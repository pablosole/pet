/*
 copyright 2012 Pablo Sole
 */

#include "pintool.h"
#include "sorrow.h"
#include <string>

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
	//bailout before doing anything else
	if (!af->IsEnabled())
		return;

	PinContextSwitch *ctxswitch = context->GetContextSwitchBuffer();

	if (!context->IsAFCtorCalled()) {
		context->SetAFCtorCalled();

		DEBUG("Calling ConstructJSProxy for Context id=" << context->GetTid());
		EnterContext(ctxswitch);

		for (uint32_t x=0; x < ctxmgr->GetLastFunctionId(); x++) {
			AnalysisFunction *tmp = ctxmgr->GetFunction(x);
			if (!tmp)
				continue;

			DEBUG("Calling constructor for AF id=" << tmp->GetFuncId() << " on TID=" << context->GetTid());
			PinContextAction *action = ctxswitch->AllocateAction();
			action->u.PrepareAF.afid = x;
			action->u.PrepareAF.cback= (uintptr_t)tmp->GetBody().c_str();
			action->u.PrepareAF.ctor = (uintptr_t)tmp->GetInit().c_str();
			action->u.PrepareAF.dtor = (uintptr_t)tmp->GetDtor().c_str();
			action->type             = PinContextAction::PREPARE_AF;
			EnterContext(ctxswitch);
		}
	}

	PinContextAction *action = ctxswitch->AllocateAction();
	action->u.CallAF.afid    = af->GetFuncId();
	action->u.CallAF.c_args  = (uintptr_t) &argc;
	action->type             = PinContextAction::EXECUTE_AF;

	EnterContext(ctxswitch);
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

		if (args.Length() != 4)
			return ThrowException(String::New("createAF needs 4 parameters"));

		if (!args[0]->IsString() || !args[1]->IsString() || !args[2]->IsString() || !args[3]->IsObject())
			return ThrowException(String::New("wrong argument types"));

		Local<String> body = args[0]->ToString();
		Local<String> init = args[1]->ToString();
		Local<String> dtor = args[2]->ToString();
		Local<Object> args_obj = args[3]->ToObject();
		IARGLIST arglist = (IARGLIST) args_obj->GetPointerFromInternalField(0);
		uint32_t nargs = (uint32_t) args_obj->GetPointerFromInternalField(1);

		v8::String::Utf8Value body_utf8(body);
		v8::String::Utf8Value init_utf8(init);
		v8::String::Utf8Value dtor_utf8(dtor);

		AnalysisFunction *af = ctxmgr->AddFunction(ToCString(body_utf8), ToCString(init_utf8), ToCString(dtor_utf8));
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
