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


namespace sorrow {
	using namespace v8;

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
