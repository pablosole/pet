/*
 copyright 2012 Pablo Sole
 */

#include "pintool.h"
#include "sorrow.h"
#include <string>

/*
Pin main objects:
current
	*pid
	*tid
	*threadId
	*threadUid
	thread
	instruction
	basicblock
	trace
	af
	event
	context
	jscontext
	isAf()
	isReplace()
*process
	pid
	exiting
	exit
	forcedExit
images
	findByAddress() [DONT_ENUM]
	findById() [DONT_ENUM]
	open() [DONT_ENUM]
	return an array of Image objects
threads
	return an array of Thread objects
events
	return an array of Event objects
routines
	findRoutineNameByAddress
	findRoutineByAddress
	createRoutine(address, name)
control
	isAttaching
	callAppFunction(context, tid, callconv, fptr, arguments)
	removeAllInstrumentation
	detach
	*yield
	spawnThread(fun, args) (only from main isolate, create a new context over this isolate per thread)
	*sleep

Pin constructible objects:
Image (can be constructed by providing a filename [use IMG_Open])
	sections
	symbols
*	valid
	entryPoint
*	name
	loadOffset
	startAddress
	lowAddress
	highAddress
	sizeMapped
	type
	mainExecutable
	staticExecutable
	id
	findRoutineByName()
*	previous
*	next
*	destroy()

Section
	image
	valid
	routines()
	name
	type
	address
	size
	bytes
	flags (Readable, Writeable, Executable)

Routine
	section
	valid
	name
	symbol
	address
	id
	range
	size
	instructions()
	attach(point, AF)
	replace(replace_js_fun, proto, arguments) (RTN_ReplaceSignature)

Prototype
	toString()
	name
	returnType
	callingConvention
	arguments

Symbol
	valid
	name
	value
	address
	isDynamic
	isIFunc
	index
	undecorateName(style) (complete, nameonly)

Thread
	valid
	tid
	threadId
	threadUid
	isActionPending
	exit
	isApplicationThread
	waitForTermination(timeout)

ThreadData(destructor_fun)
	destructor
	set(threadId)
	get(threadId)

ChildProcess
	pid
	cmdline (GetCommandLine)
	setPinCmdline (SetPinCommandLine)

Event
	type (StartThread, FiniThread, LoadImage, UnloadImage, FiniApp, Routine, Trace, Instruction, FollowChild, StartApp, Detach, Fetch, MemoryAddressTrans, ContextChange, SyscallEnter, SyscallExit)

Instruction
	attach(point, AF)
	attachFillBuffer(point, FillBuffer)
	valid
	category (CATEGORY_StringShort)
	isCategory
	extension (EXTENSION_StringShort)
	isExtension
	opcode (OPCODE_StringShort)
	isOpcode
	routine
	toString() (Dissasemble)
	mnemonic
	address
	targetAddress
	size
	next
	nextAddress
	previous
	operands (OperandCount)
	memoryOperands (MemoryOperandCount)
	readRegs (MaxNumRRegs, RegR)
	writtenRegs (MaxNumWRegs, RegW)
	flags (IsMemoryRead, IsMemoryWrite, HasFallThrough, IsLea, IsNop, Is*, IsAddedForFunctionReplacement)
	is(type) (existence checking, faster than flags)
	getFarPointer()
	syscallStandard()
	segPrefix
	delete()
	insertJump(point, addr|reg)
	insertVersionCase(reg, case_value, version, [call_order])

Operand
	flags (IsMemory, IsReg, IsImmediate, IsImplicit, IsGsOrFsReg)
	memory (baseReg, IndexReg, segmentReg, Scale, Offset)
	reg
	immediate
	width
	is(type)

MemoryOperand
	operand (INS_MemoryOperandIndexToOperandIndex)
	size
	rewrite(reg)
	is(type)
	flags (IsRead, IsWritten)

Register (new Register() uses PIN_ClaimToolRegister)
	valid
	name
	toString() (same as name)
	fullName
	size
	type (segment, GP64, GP32, GP16, GP8, FP, PIN32, PIN64)
	is(type)
	transformXMMToYMM

Syscall
	arg(num) (return an accesor object that uses GetSyscallArgument and SetSyscallArgument)
	number
	return
	errno

Context
	

Trace
	valid
	assert()
	attach(point, AF)
	basicBlocks
	isOriginal
	address
	size
	routine
	hasFallThrough
	numInstructions
	stubSize
	version (TRACE_Version)

BasicBlock
	valid
	instructions() (BBL_NumIns)
	address
	size
	isOriginal
	hasFallThrough
	attach(point, AF)
	setVersion() (BBL_SetTargetVersion)

FillBuffer (The unsupported IARG_TYPEs are: IARG_CONTEXT, IARG_REG_REFERENCE, and IARG_REG_CONST_REFERENCE.)
	valid (false after free())
	callback
	pages
	arguments
	clone
	free
	getBuffer(context)

AnalysisFunction
	arguments
	callback  (callback might be a string, a JS function or an External obj, for C++ plugins that define AFs)
	initCallback (same as callback)
	enabled
	exceptionLimit
	argsFixed (bool)

ArgumentsArray
	order (CALL_ORDER)
	returnRegister
	arguments (ordered array. elements might be a string or an obj with key being the arg type and value being the arg value)

Lock
	lock
	unlock

Mutex
	lock (bool try)
	unlock

RWMutex
	readLock(bool try)
	writeLock(bool try)
	unlock

* Semaphore
	set
	clear
	isSet
	wait(timeout)
*/

//Routine, Trace and Instruction fast boolean checks for instrumentation and dispatcher functions
uint32_t routine_instrumentation_enabled = 0;
Persistent<Function> routine_function;
uint32_t trace_instrumentation_enabled = 0;
Persistent<Function> trace_function;
uint32_t ins_instrumentation_enabled = 0;
Persistent<Function> ins_function;
Isolate *main_ctx_isolate;
Persistent<Context> main_ctx;
Persistent<Object> main_ctx_global;

VOID RoutineProxy(RTN rtn, VOID *v) {
	if (!routine_instrumentation_enabled)
		return;

	Locker locker;
	HandleScope hscope;
	Context::Scope context_scope(main_ctx);

	const int argc = 1;
	Local<Value> argv[argc];

	argv[0] = Integer::NewFromUnsigned(rtn.index);

	routine_function->FastCall(main_ctx_isolate, *main_ctx, main_ctx_global, argc, argv);
}

VOID TraceProxy(TRACE trace, VOID *v) {
	if (!trace_instrumentation_enabled)
		return;

	Locker locker;
	HandleScope hscope;
	Context::Scope context_scope(main_ctx);

	const int argc = 1;
	Local<Value> argv[argc];

	argv[0] = Integer::NewFromUnsigned((uint32_t)trace);

	trace_function->FastCall(main_ctx_isolate, *main_ctx, main_ctx_global, argc, argv);
}

VOID InstructionProxy(INS ins, VOID *v) {
	if (!ins_instrumentation_enabled)
		return;

	Locker locker;
	HandleScope hscope;
	Context::Scope context_scope(main_ctx);

	const int argc = 1;
	Local<Value> argv[argc];

	argv[0] = Integer::NewFromUnsigned(ins.index);

	ins_function->FastCall(main_ctx_isolate, *main_ctx, main_ctx_global, argc, argv);
}

VOID ApplicationStartProxy(VOID *) {
	Locker locker;
	Context::Scope context_scope(ctxmgr->GetDefaultContext());
	HandleScope hscope;
	Local<Object> global = ctxmgr->GetDefaultContext()->Global();
	Local<Value> funval = global->Get(String::New("startAppDispatcher"));
	if (!funval->IsFunction()) {
		DEBUG("startAppDispatcher not found");
		KillPinTool();
	}

	Local<Function> fun = Local<Function>::Cast(funval);
	fun->Call(global, 0, NULL);
}

VOID DetachProxy(VOID *) {
	Locker locker;
	Context::Scope context_scope(ctxmgr->GetDefaultContext());
	HandleScope hscope;
	Local<Object> global = ctxmgr->GetDefaultContext()->Global();
	Local<Value> funval = global->Get(String::New("detachDispatcher"));
	if (!funval->IsFunction()) {
		DEBUG("detachDispatcher not found");
		KillPinTool();
	}

	Local<Function> fun = Local<Function>::Cast(funval);
	fun->Call(global, 0, NULL);
}

VOID StartThreadProxy(THREADID tid, CONTEXT *ctx, INT32 flags, VOID *v)
{
	if (!PIN_IsApplicationThread())
		return;

	Locker locker;
	Context::Scope context_scope(ctxmgr->GetDefaultContext());
	HandleScope hscope;
	Local<Object> global = ctxmgr->GetDefaultContext()->Global();
	Local<Value> funval = global->Get(String::New("startThreadDispatcher"));
	if (!funval->IsFunction()) {
		DEBUG("startThreadDispatcher not found");
		KillPinTool();
	}

	const int argc = 3;
	Local<Value> argv[argc];

	argv[0] = Integer::NewFromUnsigned(tid);

	Local<Value> args[1];
	args[0] = Integer::NewFromUnsigned((uint32_t)ctx);

	argv[1] = ctxmgr->GetSorrowContext()->GetPointerTypes()->GetExternalPointerFunct()->NewInstance(1, args);

	argv[2] = Integer::New(flags);

	Local<Function> fun = Local<Function>::Cast(funval);
	fun->Call(global, argc, argv);
}

VOID FiniThreadProxy(THREADID tid, const CONTEXT *ctx, INT32 code, VOID *v)
{
	if (!PIN_IsApplicationThread())
		return;

	Locker locker;
	Context::Scope context_scope(ctxmgr->GetDefaultContext());
	HandleScope hscope;
	Local<Object> global = ctxmgr->GetDefaultContext()->Global();
	Local<Value> funval = global->Get(String::New("finiThreadDispatcher"));
	if (!funval->IsFunction()) {
		DEBUG("finiThreadDispatcher not found");
		KillPinTool();
	}

	const int argc = 3;
	Local<Value> argv[argc];

	argv[0] = Integer::NewFromUnsigned(tid);

	Local<Value> args[1];
	args[0] = Integer::NewFromUnsigned((uint32_t)ctx);

	argv[1] = ctxmgr->GetSorrowContext()->GetPointerTypes()->GetExternalPointerFunct()->NewInstance(1, args);

	argv[2] = Integer::New(code);

	Local<Function> fun = Local<Function>::Cast(funval);
	fun->Call(global, argc, argv);
}

VOID ImageProxy(IMG img, VOID *v)
{
	Locker locker;
	Context::Scope context_scope(ctxmgr->GetDefaultContext());
	HandleScope hscope;
	Local<Object> global = ctxmgr->GetDefaultContext()->Global();
	Local<Value> funval;
	if (!v) {
		funval = global->Get(String::New("loadImageDispatcher"));
	} else {
		funval = global->Get(String::New("unloadImageDispatcher"));
	}
	if (!funval->IsFunction()) {
		DEBUG("load|unloadImageDispatcher not found");
		KillPinTool();
	}

	const int argc = 1;
	Local<Value> argv[argc];

	argv[0] = Integer::NewFromUnsigned((uint32_t)img.index);

	Local<Function> fun = Local<Function>::Cast(funval);
	fun->Call(global, argc, argv);
}

VOID InternalThreadProxy(VOID *arg) {
	Locker locker;
	HandleScope hscope;
	Context::Scope context_scope(ctxmgr->GetDefaultContext());
	SorrowContext sorrow(0, NULL);

	Local<Object> global = ctxmgr->GetDefaultContext()->Global();
	Local<Value> funval = global->Get(String::New("spawnedThreadDispatcher"));
	if (!funval->IsFunction()) {
		DEBUG("spawnedThreadDispatcher not found");
		KillPinTool();
	}

	Persistent<Value> pobj((Value *)arg);
	const int argc = 1;
	Local<Value> argv[argc];

	argv[0] = Local<Value>::New(pobj);

	Local<Function> fun = Local<Function>::Cast(funval);
	fun->Call(global, argc, argv);
	pobj.Dispose();
}

VOID InsertCallProxy(PinContext *context, AnalysisFunction *af, uint32_t argc, ...)
{
	va_list argptr;

	//bailout before doing anything else
	if (!af->IsEnabled())
		return;

	//bailout if the JS context is not ready yet
	if (context->GetState() != PinContext::INITIALIZED_CONTEXT)
		return;

	Isolate::Scope iscope(context->GetIsolate());
	Locker lock(context->GetIsolate());
	HandleScope hscope;
	Context::Scope cscope(context->GetContext());

	Persistent<Function> fun = context->EnsureFunction(af);

	va_start(argptr, argc);

	//_malloca allocates from the stack unless the size is >1024
	//stack allocation is faster than heap and in that case _freea becomes basically a NOOP
	Handle<Value> *argv = reinterpret_cast<Handle<Value> *>(_malloca(sizeof(char *) * argc));

	for (uint32_t x=0; x < argc; x++)
		argv[x] = Uint32::NewFromUnsigned(va_arg(argptr, uint32_t));

	va_end(argptr);

	TryCatch trycatch;

	fun->FastCall(context->GetIsolate(), *(context->GetContext()), context->GetGlobal(), argc, argv);

	if (trycatch.HasCaught())
	{
		af->IncException();
		af->SetLastException(ReportExceptionToString(&trycatch));
		DEBUG("Exception on AF: " << af->GetLastException());

		//Disable the function if there's too many exceptions
		if (af->GetNumExceptions() > af->GetThreshold() || !trycatch.CanContinue())
			af->Disable();
	}

	_freea(argv);
}


namespace sorrow {
	using namespace v8;

    V8_FUNCTN(InsertCall) {
        HandleScope scope;

		if (args.Length() != 4 || !args[0]->IsNumber() || !args[1]->IsNumber() || !args[2]->IsObject() || !args[3]->IsNumber())
			return ThrowException(String::New("wrong arguments"));

		TryCatch try_catch;

		RTN rtn;
		INS ins;
		BBL bbl;
		TRACE trace;

		uint32_t type = NumberToUint32(args[3], &try_catch);
		if (try_catch.HasCaught()) return try_catch.Exception();
		IPOINT point = (IPOINT)NumberToUint32(args[1], &try_catch);
		if (try_catch.HasCaught()) return try_catch.Exception();

		Local<Object> obj = args[2]->ToObject();
		AnalysisFunction *af = (AnalysisFunction *)obj->GetPointerFromInternalField(0);
		uint32_t index=0;

		index = NumberToUint32(args[0], &try_catch);
		if (try_catch.HasCaught()) return try_catch.Exception();

		switch (type) {
			case 0:
				rtn.index = index;
				RTN_InsertCall(rtn, point, (AFUNPTR)InsertCallProxy, \
				IARG_PRESERVE, &ctxmgr->GetPreservedRegset(), \
				IARG_REG_VALUE, ctxmgr->GetPerThreadContextReg(), \
				IARG_PTR, af, \
				IARG_UINT32, af->GetArgumentCount(), \
				IARG_IARGLIST, af->GetArguments(), \
				IARG_END);
				break;
			case 1:
				ins.index = index;
				INS_InsertCall(ins, point, (AFUNPTR)InsertCallProxy, \
				IARG_PRESERVE, &ctxmgr->GetPreservedRegset(), \
				IARG_REG_VALUE, ctxmgr->GetPerThreadContextReg(), \
				IARG_PTR, af, \
				IARG_UINT32, af->GetArgumentCount(), \
				IARG_IARGLIST, af->GetArguments(), \
				IARG_END);
				break;
			case 2:
				bbl.index = index;
				BBL_InsertCall(bbl, point, (AFUNPTR)InsertCallProxy, \
				IARG_PRESERVE, &ctxmgr->GetPreservedRegset(), \
				IARG_REG_VALUE, ctxmgr->GetPerThreadContextReg(), \
				IARG_PTR, af, \
				IARG_UINT32, af->GetArgumentCount(), \
				IARG_IARGLIST, af->GetArguments(), \
				IARG_END);
				break;
			case 3:
				trace = (TRACE)index;
				TRACE_InsertCall(trace, point, (AFUNPTR)InsertCallProxy, \
				IARG_PRESERVE, &ctxmgr->GetPreservedRegset(), \
				IARG_REG_VALUE, ctxmgr->GetPerThreadContextReg(), \
				IARG_PTR, af, \
				IARG_UINT32, af->GetArgumentCount(), \
				IARG_IARGLIST, af->GetArguments(), \
				IARG_END);
				break;
		}

		return Undefined();
	}

    void ArgsArrayWeakCallback(Persistent<Value> object, void* ptr) {
		HandleScope scope;
		IARGLIST_Free((IARGLIST)ptr);
		object.Dispose();
    }

    V8_FUNCTN(ArgsArrayConstructor) {
        HandleScope scope;
        Handle<Object> op = args.This();

		TryCatch try_catch;
		uint32_t type;
		uint32_t value;
		uint32_t num_args = 0;

		if (args.Length() != 2 || !args[0]->IsObject())
			return ThrowException(String::New("wrong arguments"));

		uint32_t indexes = NumberToUint32(args[1], &try_catch);
		if (try_catch.HasCaught()) return try_catch.Exception();
		Local<Object> args_arr = args[0]->ToObject();
		Local<Value> item;

		IARGLIST list = IARGLIST_Alloc();
		for (uint32_t idx=0; idx < indexes; idx++) {
			item = args_arr->Get(idx);

			//this is an argument pair
			if (item->IsArray()) {
				Local<Object> pair = item->ToObject();

				type = NumberToUint32(pair->Get(0), &try_catch);
				if (try_catch.HasCaught()) {
					IARGLIST_Free(list);
					return try_catch.Exception();
				}
				value = NumberToUint32(pair->Get(1), &try_catch);
				if (try_catch.HasCaught()) {
					IARGLIST_Free(list);
					return try_catch.Exception();
				}

				IARGLIST_AddArguments(list, type, value, IARG_END);
				num_args++;
			} else {
				type = NumberToUint32(item, &try_catch);
				if (try_catch.HasCaught()) {
					IARGLIST_Free(list);
					return try_catch.Exception();
				}

				IARGLIST_AddArguments(list, type, IARG_END);
				num_args++;
			}
		}

		Persistent<Object> persistent_op = Persistent<Object>::New(op);
		persistent_op.MakeWeak(list, ArgsArrayWeakCallback);
		persistent_op.MarkIndependent();

		op->SetPointerInInternalField(0, list);
		op->SetPointerInInternalField(1, (void *)num_args);
		return op;
	}

    V8_FUNCTN(GetEnabledAnalysisFunction) {
        HandleScope scope;

		if (args.Length() != 1)
			return ThrowException(String::New("GetEnabledAnalysisFunction needs 1 parameter"));

		if (!args[0]->IsObject())
			return ThrowException(String::New("wrong argument types"));

		Local<Object> obj = args[0]->ToObject();
		AnalysisFunction *af = (AnalysisFunction *)obj->GetPointerFromInternalField(0);
		Handle<Value> ret = Boolean::New(af->IsEnabled());

		return scope.Close(ret);
	}

    V8_FUNCTN(SetEnabledAnalysisFunction) {
        HandleScope scope;

		if (args.Length() != 2)
			return ThrowException(String::New("SetEnabledAnalysisFunction needs 2 parameters"));

		if (!args[0]->IsObject() || !args[1]->IsBoolean())
			return ThrowException(String::New("wrong argument types"));

		Local<Object> obj = args[0]->ToObject();
		AnalysisFunction *af = (AnalysisFunction *)obj->GetPointerFromInternalField(0);
		
		if (args[1]->IsTrue())
			af->Enable();
		else
			af->Disable();

		return Undefined();
	}

    V8_FUNCTN(CreateAnalysisFunction) {
        HandleScope scope;

		if (args.Length() != 3)
			return ThrowException(String::New("createAF needs 3 parameters"));

		if (!args[0]->IsString() || !args[1]->IsString() || !args[2]->IsObject())
			return ThrowException(String::New("wrong argument types"));

		Local<String> body = args[0]->ToString();
		Local<String> init = args[1]->ToString();
		Local<Object> args_obj = args[2]->ToObject();
		IARGLIST arglist = (IARGLIST) args_obj->GetPointerFromInternalField(0);
		uint32_t nargs = (uint32_t) args_obj->GetPointerFromInternalField(1);

		v8::String::Utf8Value body_utf8(body);
		v8::String::Utf8Value init_utf8(init);

		AnalysisFunction *af = ctxmgr->AddFunction(ToCString(body_utf8), ToCString(init_utf8));
		af->SetArguments(arglist, nargs);
		
		Local<Value> arg[1];
		arg[0] = Integer::NewFromUnsigned((uint32_t)af);
		Local<Object> ptr = ctxmgr->GetSorrowContext()->GetPointerTypes()->GetExternalPointerFunct()->NewInstance(1, arg);

		return scope.Close(ptr);
	}

    V8_FUNCTN(SpawnThread) {
        HandleScope scope;

		if (Isolate::GetCurrent() != ctxmgr->GetDefaultIsolate()) {
			return ThrowException(String::New("SpawnThread can only work on the default isolate"));
		}

		if (args.Length() < 1)
			return ThrowException(String::New("SpawnThread needs 1 parameter"));

		Handle<Value> obj = args[0];
		Persistent<Value> pobj = Persistent<Value>::New(obj);

		Local<Value> arg_ptr[1];
		arg_ptr[0] = Integer::NewFromUnsigned(8);
		Local<Object> ptr = ctxmgr->GetSorrowContext()->GetPointerTypes()->GetOwnPointerFunct()->NewInstance(1, arg_ptr);
		PIN_THREAD_UID *p_uid = (PIN_THREAD_UID *)ptr->GetPointerFromInternalField(0);

		THREADID tid = PIN_SpawnInternalThread(InternalThreadProxy, *pobj, 0, p_uid);

		if (tid == INVALID_THREADID)
			return Null();

		Local<Array> arr = Array::New(2);
		arr->Set(0, Integer::NewFromUnsigned(tid));
		arr->Set(1, ptr);

		return scope.Close(arr);
	}

    V8_FUNCTN(AddEventProxy) {
        HandleScope scope;

		if (args.Length() < 1)
			return ThrowException(String::New("addEventProxy needs 1 parameter"));

		TryCatch try_catch;

		uint32_t type = convertToUint(args[0], &try_catch);
		if (try_catch.HasCaught()) return try_catch.Exception();
		
		if (ctxmgr->IsInstrumentationEnabled(type))
			return Undefined();
		
		switch (type) {
			case STARTAPP:
				PIN_AddApplicationStartFunction(ApplicationStartProxy, 0);
				break;

			case DETACH:
				PIN_AddDetachFunction(DetachProxy, 0);
				break;

			case STARTTHREAD:
				PIN_AddThreadStartFunction(StartThreadProxy, 0);
				break;

			case FINITHREAD:
				PIN_AddThreadFiniFunction(FiniThreadProxy, 0);
				break;

			case LOADIMAGE:
				IMG_AddInstrumentFunction(ImageProxy, 0);
				break;

			case UNLOADIMAGE:
				IMG_AddUnloadFunction(ImageProxy, (VOID *)1);
				break;

			case NEWROUTINE:
				RTN_AddInstrumentFunction(RoutineProxy, 0);
				break;

			case NEWTRACE:
				TRACE_AddInstrumentFunction(TraceProxy, 0);
				break;

			case NEWINSTRUCTION:
				INS_AddInstrumentFunction(InstructionProxy, 0);
				break;

			//non-supported types
			default:
				return Undefined();
		}

		ctxmgr->EnableInstrumentation(type);
		return Undefined();
	}

	Handle<Value> GetOwnString(Local<String> property,
                          const AccessorInfo &info) {
		Local<Object> self = info.Holder();
		string *ptr = (string *)self->GetPointerFromInternalField(0);
		if (ptr)
			return String::New(ptr->c_str(), ptr->size());
		else
			return Undefined();
	}

	void SetOwnString(Local<String> property, Local<Value> value,
					 const AccessorInfo& info) {
		Local<Object> self = info.Holder();
		string *ptr = (string *)self->GetPointerFromInternalField(0);
		if (ptr)
			delete ptr;

		String::Utf8Value strvalue(value);
		const char *strchar = ToCString(strvalue);
		ptr = new string(strchar);
		self->SetPointerInInternalField(0, reinterpret_cast<void *>(ptr));
	}

    void OwnStringWeakCallback(Persistent<Value> object, void* ptr) {
		HandleScope scope;
		Local<Object> obj = object->ToObject();

		string *p = reinterpret_cast<string *>(obj->GetPointerFromInternalField(0));
		if (p)
			delete p;

		object.Dispose();
    }

    V8_FUNCTN(OwnStringConstructor) {
        HandleScope scope;
        Handle<Object> op = args.This();

		if (args.Length() > 1)
			return ThrowException(String::New("OwnString constructor can have one parameter at most (string)."));

		string *strstring = (string *)0;
		if (args.Length()) {
			String::Utf8Value strvalue(args[0]);
			const char *strchar = ToCString(strvalue);
			strstring = new string(strchar);
		}

		Persistent<Object> persistent_op = Persistent<Object>::New(op);
		persistent_op.MakeWeak(0, OwnStringWeakCallback);
		persistent_op.MarkIndependent();

		op->SetPointerInInternalField(0, strstring);
		return op;
	}

    void OwnPointerWeakCallback(Persistent<Value> object, void* ptr) {
		HandleScope scope;
		free(ptr);
		object.Dispose();
    }

    V8_FUNCTN(OwnPointerConstructor) {
        HandleScope scope;
        Handle<Object> op = args.This();

		if (args.Length() != 1)
			return ThrowException(String::New("OwnPointer constructor must have one parameter (size)."));

		void *ptr;
		TryCatch try_catch;

		uint32_t size = convertToUint(args[0], &try_catch);
		if (try_catch.HasCaught()) return try_catch.Exception();
		ptr = malloc(size);
		if (!ptr)
			return ThrowException(String::New("ExternalPointer allocation failed"));

		Persistent<Object> persistent_op = Persistent<Object>::New(op);
		persistent_op.MakeWeak(ptr, OwnPointerWeakCallback);
		persistent_op.MarkIndependent();

		op->SetPointerInInternalField(0, ptr);
		return op;
	}

    V8_FUNCTN(ExternalPointerConstructor) {
        HandleScope scope;
        Handle<Object> ep = args.This();

		if (args.Length() > 1)
			return ThrowException(String::New("ExternalPointer constructor can have one parameter at most (ptr address)."));

		//an unaligned ptr is assured to NOT be encoded as a SMI, giving the chance to our
		//WrapPointer function to work properly.
		void *ptr = (void *)1;

		if (args.Length() > 0) {
			TryCatch try_catch;
			ptr = reinterpret_cast<void *>(args[0]->Uint32Value());
			if (try_catch.HasCaught()) return try_catch.Exception();
		}

		ep->SetPointerInInternalField(0, ptr);
		return ep;
	}

	Handle<Value> GetPointerAddress(Local<String> property,
                          const AccessorInfo &info) {
		Local<Object> self = info.Holder();
		void* ptr = self->GetPointerFromInternalField(0);
		return Integer::New(reinterpret_cast<uintptr_t>(ptr));
	}

	void SetPointerAddress(Local<String> property, Local<Value> value,
					 const AccessorInfo& info) {
		Local<Object> self = info.Holder();
		self->SetPointerInInternalField(0, reinterpret_cast<void *>(value->Uint32Value()));
	}

    V8_FUNCTN(PIN_Sleep_v8) {
        HandleScope scope;

		if (args.Length() != 1)
			return ThrowException(String::New("Sleep needs 1 parameter(ms)"));

		TryCatch try_catch;

		uint32_t ms = convertToUint(args[0], &try_catch);
		if (try_catch.HasCaught()) return try_catch.Exception();

		{
			Unlocker unlocker;
			PIN_Sleep(ms);
		}

		return Undefined();
	}

	PointerTypes::PointerTypes(Handle<Object> target) {
		HandleScope scope;

		extptr_t = Persistent<FunctionTemplate>::New(FunctionTemplate::New(ExternalPointerConstructor));
        Local<ObjectTemplate> extptr_ot = extptr_t->InstanceTemplate();
		extptr_ot->SetInternalFieldCount(1);
		extptr_ot->SetAccessor(String::New("pointer"), GetPointerAddress, SetPointerAddress);

		extptr = Persistent<Function>::New(extptr_t->GetFunction());
		extptr_t->SetClassName(V8_STR("ExternalPointer"));
		target->Set(V8_STR("ExternalPointer"), extptr);

		ownptr_t = Persistent<FunctionTemplate>::New(FunctionTemplate::New(OwnPointerConstructor));
        Local<ObjectTemplate> ownptr_ot = ownptr_t->InstanceTemplate();
		ownptr_ot->SetInternalFieldCount(1);
		ownptr_ot->SetAccessor(String::New("pointer"), GetPointerAddress);

		ownptr = Persistent<Function>::New(ownptr_t->GetFunction());
		ownptr_t->SetClassName(V8_STR("OwnPointer"));
		target->Set(V8_STR("OwnPointer"), ownptr);

		ownstr_t = Persistent<FunctionTemplate>::New(FunctionTemplate::New(OwnStringConstructor));
        Local<ObjectTemplate> ownstr_ot = ownstr_t->InstanceTemplate();
		ownstr_ot->SetInternalFieldCount(1);
		ownstr_ot->SetAccessor(String::New("pointer"), GetPointerAddress, SetPointerAddress);
		ownstr_ot->SetAccessor(String::New("string"), GetOwnString, SetOwnString);

		ownstr = Persistent<Function>::New(ownstr_t->GetFunction());
		ownstr_t->SetClassName(V8_STR("OwnString"));
		target->Set(V8_STR("OwnString"), ownstr);

		Local<FunctionTemplate> argsarray_t = FunctionTemplate::New(ArgsArrayConstructor);
        Local<ObjectTemplate> argsarray_ot = argsarray_t->InstanceTemplate();
		argsarray_ot->SetInternalFieldCount(2);  //IARGLIST, num_args
		argsarray_t->SetClassName(V8_STR("IntArgsArray"));
		target->Set(V8_STR("IntArgsArray"), argsarray_t->GetFunction());

		SET_METHOD(target, "addEventProxy", AddEventProxy);
		SET_METHOD(target, "SpawnThread", SpawnThread);
		SET_METHOD(target, "PIN_Sleep", PIN_Sleep_v8);
		SET_METHOD(target, "createAF", CreateAnalysisFunction);
		SET_METHOD(target, "getEnabledAF", GetEnabledAnalysisFunction);
		SET_METHOD(target, "setEnabledAF", SetEnabledAnalysisFunction);
		SET_METHOD(target, "InsertCall", InsertCall);
	}

	PointerTypes::~PointerTypes() {
		extptr_t.Dispose();
		ownptr_t.Dispose();
		ownstr_t.Dispose();
		extptr.Dispose();
		ownptr.Dispose();
		ownstr.Dispose();
	}
}
