#include "pintool.h"
#include <iostream>
#include <process.h>


VOID ContextManager::Run(VOID *_ctx)
{
	ContextManager *ctx = static_cast<ContextManager *>(_ctx);

	//just for extra safety
	if (!ctx || !ctx->IsValid())
		return;

	DEBUG("ContextManager thread initiated");
	ctx->Ready();

	while (true)
	{
		ctx->Wait();

		//check if we're about to die
		if (!ctx->IsRunning())
			break;

		ctx->ProcessCallbacks();

		ctx->ProcessChanges();
	}

	DEBUG("ContextManager thread exit");
}


void ContextManager::ProcessCallbacksHelper(VOID *v)
{
	ContextManager *ctx = static_cast<ContextManager *>(v);

	ctx->LockCallbacks();
	while (!ctx->callbacks.empty())
	{
		EnsureCallback *info = ctx->callbacks.back();
		ctx->callbacks.pop_back();
		PinContext *context = ctx->EnsurePinContext(info->tid, info->create);
		context->WaitIfNotReady();

		if (!PinContext::IsValid(context))
		{
			DEBUG("Couldn't ensure a valid context for callback on tid " << info->tid);
		} else {
			info->callback(context, info->data);
		}
	}
	ctx->UnlockCallbacks();
}


void ContextManager::ProcessCallbacks()
{
	if (callbacks.empty() || CallbacksIsProcessing())
		return;

	PIN_SpawnInternalThread(ContextManager::ProcessCallbacksHelper, this, 0, NULL);
}


void ContextManager::ProcessChanges()
{
	ContextsMap::iterator it;

	Lock();

	it = contexts.begin();
	while (it != contexts.end())
	{
		THREADID tid = it->first;
		PinContext *context = it->second;
		
		switch (context->GetState())
		{
			case PinContext::NEW_CONTEXT:
				context->CreateJSContext();
				++it;
				break;

			case PinContext::KILLING_CONTEXT:
				context->DestroyJSContext();
				++it;
				break;

			case PinContext::DEAD_CONTEXT:
			case PinContext::ERROR_CONTEXT:
				//this is a thread that was killed or became erroneous in a previous round, erase it.
				contexts.erase(it++);
				delete context;
				break;
			default:
				//unknown state, move to the next anyway
				++it;
		}
	}

	Unlock();
}


void ContextManager::KillAllContexts()
{
	ContextsMap::iterator it;

	//Last chance to process contexts dieing
	ProcessChanges();

	Lock();

	it = contexts.begin();
	while (it != contexts.end())
	{
		THREADID tid = it->first;
		PinContext *context = it->second;
		
		if (context->GetState() == PinContext::INITIALIZED_CONTEXT) {
			context->DestroyJSContext();
		}
		contexts.erase(it++);
		delete context;
	}

	Unlock();
}

//It's mandatory to check the result of this initialization using IsValid()
//after construction.
ContextManager::ContextManager() :
last_function_id(0),
instrumentation_flags(0)
{
	SetPerformanceCounter();

	if (!PIN_MutexInit(&lock) || 
		!PIN_MutexInit(&lock_callbacks) || 
		!PIN_RWMutexInit(&lock_functions) || 
		!PIN_SemaphoreInit(&changed) || 
		!PIN_SemaphoreInit(&ready))
	{
		SetState(ERROR_MANAGER);
		return;
	}

	per_thread_context_key = PIN_CreateThreadDataKey(PinContext::ThreadInfoDestructor);
	if (per_thread_context_key == -1)
	{
		SetState(ERROR_MANAGER);
		return;
	}

	per_thread_context_reg = PIN_ClaimToolRegister();
	if (!REG_valid(per_thread_context_reg)) {
		DEBUG("Failed to allocate a scratch register for PinContexts.");
		SetState(ERROR_MANAGER);
		return;
	}

	//calling convention assured preserved regs. (STDCALL)
	REGSET_Clear(preserved_regs);
	REGSET_Insert(preserved_regs, REG_EBX);
	REGSET_Insert(preserved_regs, REG_ESI);
	REGSET_Insert(preserved_regs, REG_EDI);
	REGSET_Insert(preserved_regs, REG_EBP);
	REGSET_Insert(preserved_regs, REG_X87);
	
	//we use XMM0 to create heap numbers
	//REGSET_Insert(preserved_regs, REG_XMM0);

	REGSET_Insert(preserved_regs, REG_XMM1);
	REGSET_Insert(preserved_regs, REG_XMM2);
	REGSET_Insert(preserved_regs, REG_XMM3);
	REGSET_Insert(preserved_regs, REG_XMM4);
	REGSET_Insert(preserved_regs, REG_XMM5);
	REGSET_Insert(preserved_regs, REG_XMM6);
	REGSET_Insert(preserved_regs, REG_XMM7);

	//A default context for the instrumentation functions over the default isolate.
	//And a diff context used only for eval() over this default isolate from other isolates.
	{
		Locker lock;
		HandleScope hscope;

		default_context = Context::New();
		if (default_context.IsEmpty())
		{
			SetState(ERROR_MANAGER);
			return;
		}
		default_context->SetSecurityToken(Undefined());

		shareddata_context = Context::New();
		if (shareddata_context.IsEmpty())
		{
			SetState(ERROR_MANAGER);
			return;
		}
		shareddata_context->SetSecurityToken(Undefined());

		{
			Context::Scope cscope(default_context);
			Local<ObjectTemplate> objt = ObjectTemplate::New();
			objt->SetInternalFieldCount(1);
			Local<Object> obj = objt->NewInstance();
			default_context->Global()->SetHiddenValue(String::New("SorrowInstance"), obj);
		}
		{
			Context::Scope cscope(shareddata_context);
			Local<ObjectTemplate> objt = ObjectTemplate::New();
			objt->SetInternalFieldCount(1);
			Local<Object> obj = objt->NewInstance();
			shareddata_context->Global()->SetHiddenValue(String::New("SorrowInstance"), obj);
		}

		Locker::StartPreemption(50);
	}

	default_isolate = Isolate::GetCurrent();

	SetState(RUNNING_MANAGER);
	tid = PIN_SpawnInternalThread(Run, this, 0, NULL);
	
	if (tid == INVALID_THREADID)
		SetState(ERROR_MANAGER);
}

ContextManager::~ContextManager()
{
	WINDOWS::LARGE_INTEGER diff;
	WINDOWS::LARGE_INTEGER freq;
	GetPerformanceCounterDiff(&diff);
	WINDOWS::QueryPerformanceFrequency(&freq);
	DEBUG("Enlapsed time inside ContextManager (secs -- ticks): " << std::setprecision(6) << ((double)diff.QuadPart / (double)freq.QuadPart) << " -- " << diff.QuadPart );

	if (GetState() == ERROR_MANAGER)
		return;

	PIN_MutexFini(&lock);
	PIN_MutexFini(&lock_callbacks);
	PIN_RWMutexFini(&lock_functions);
	PIN_SemaphoreFini(&changed);
	PIN_SemaphoreFini(&ready);
	PIN_DeleteThreadDataKey(per_thread_context_key);

	{
		Locker lock;
		Locker::StopPreemption();
	}

	V8::TerminateExecution();
	delete sorrrowctx;
	delete sorrrowctx_shareddata;
	routine_function.Dispose();
	trace_function.Dispose();
	ins_function.Dispose();
	main_ctx.Dispose();
	main_ctx_global.Dispose();
	default_context.Dispose();
	shareddata_context.Dispose();
	
	V8::Dispose();
}

//this function returns a new context and adds it to the map of contexts or returns
//INVALID_PIN_CONTEXT if it cannot allocate a new context.
PinContext *ContextManager::CreateContext(THREADID tid, BOOL waitforit)
{
	PinContext *context;
	pair<ContextsMap::iterator, bool> ret;

	context = new PinContext(tid);
	if (!context)
		return INVALID_PIN_CONTEXT;

	Lock();
	ret = contexts.insert(pair<THREADID, PinContext *>(tid, context));
	Unlock();

	if (ret.second == false)
	{
		delete context;
		Lock();
		context = contexts[tid];
		Unlock();
	} else {
		Notify();
		if (waitforit) {
			//wait until the new context is created
			context->Wait();
		}
	}

	PIN_SetThreadData(per_thread_context_key, context, tid);

	if (context->GetState() == PinContext::INITIALIZED_CONTEXT || \
		(!waitforit && context->GetState() == PinContext::NEW_CONTEXT))
		return context;
	else
		return INVALID_PIN_CONTEXT;
}

//This function is used to queue a callback to run whenever the Context Manager is ready to work.
//The callback is executed from an internal thread and it receives a PinContext * and the VOID *v passed as their arguments.
bool ContextManager::EnsurePinContextCallback(THREADID tid, ENSURE_CALLBACK_FUNC *callback, VOID *v, bool create)
{
	if (!IsRunning())
		return false;

	if (IsReady())
	{
		PinContext *context = EnsurePinContext(tid, create);
		context->WaitIfNotReady();
		callback(context, v);
		return true;
	}

	EnsureCallback *info = new EnsureCallback();
	info->callback = callback;
	info->tid = tid;
	info->create = create;
	info->data = v;

	LockCallbacks();
	callbacks.push_front(info);
	UnlockCallbacks();

	Notify();
	return true;
}

inline PinContext *ContextManager::EnsurePinContext(THREADID tid, bool create)
{
	PinContext *context = static_cast<PinContext *>(PIN_GetThreadData(per_thread_context_key, tid));

	if (!PinContext::IsValid(context) && create) {
		if (!IsReady())
			return NO_MANAGER_CONTEXT;

		context = CreateContext(tid);
	}

	return context;
}


AnalysisFunction *ContextManager::AddFunction(const string &body, const string& init, const string& dtor)
{
	AnalysisFunction *af = new AnalysisFunction(body, init, dtor);

	DEBUG("Adding AF with hash:" << af->GetHash());

	if (!af)
		return 0;

	LockFunctions();
	unsigned int funcId = last_function_id;
	functions[last_function_id++] = af;
	UnlockFunctions();

	af->SetFuncId(funcId);

	return af;
}

bool ContextManager::RemoveFunction(unsigned int funcId)
{
	FunctionsMap::iterator it;
	bool ret=false;

	LockFunctions();
	it = functions.find(funcId);
	if (it != functions.end())
	{
		AnalysisFunction *af = it->second;
		functions.erase(it);
		delete af;
		ret=true;
	}
	UnlockFunctions();

	return ret;
}

AnalysisFunction *ContextManager::GetFunction(unsigned int funcId)
{
	AnalysisFunction *af = 0;

	LockFunctionsRead();
	FunctionsMap::const_iterator it;
	it = functions.find(funcId);
	if (it != functions.end())
		af = it->second;
	UnlockFunctions();

	return af;
}


uint32_t AnalysisFunction::HashBody()
{
	hash = 0;
	const char *s = body.c_str();

	while (*s)
		hash = hash * 101 + *s++;

	return hash;
}

bool ContextManager::GetPerformanceCounterDiff(WINDOWS::LARGE_INTEGER *out)
{
	WINDOWS::LARGE_INTEGER tmp;
	if (!WINDOWS::QueryPerformanceCounter(&tmp))
		return false;

	out->QuadPart = tmp.QuadPart - performancecounter_start.QuadPart;

	return true;
}

void ContextManager::InitializeSorrowContext(int argc, const char *argv[])
{
	{
		Locker locker;
		Context::Scope context_scope(GetSharedDataContext());
		sorrrowctx_shareddata = new SorrowContext(0, NULL);
	}

	{
		Locker locker;
		Context::Scope context_scope(GetDefaultContext());
		sorrrowctx = new SorrowContext(argc, argv);

		HandleScope hscope;
		Local<Value> funval = GetDefaultContext()->Global()->Get(String::New("setupEventBooleans"));
		if (!funval->IsFunction()) {
			DEBUG("setupEventBooleans not found");
			KillPinTool();
		}

		Local<Function> fun = Local<Function>::Cast(funval);
		Local<Value> argv[3];

		Local<Value> args[1];
		args[0] = Integer::NewFromUnsigned((uint32_t)&routine_instrumentation_enabled);
		argv[0] = sorrrowctx->GetPointerTypes()->GetExternalPointerFunct()->NewInstance(1, args);

		args[0] = Integer::NewFromUnsigned((uint32_t)&trace_instrumentation_enabled);
		argv[1] = sorrrowctx->GetPointerTypes()->GetExternalPointerFunct()->NewInstance(1, args);

		args[0] = Integer::NewFromUnsigned((uint32_t)&ins_instrumentation_enabled);
		argv[2] = sorrrowctx->GetPointerTypes()->GetExternalPointerFunct()->NewInstance(1, args);

		fun->Call(GetDefaultContext()->Global(), 4, argv);

		funval = GetDefaultContext()->Global()->Get(String::New("fastDispatcher_0"));
		if (!funval->IsFunction()) {
			DEBUG("fastDispatcher_0 not found");
			KillPinTool();
		}
		routine_function = Persistent<Function>::New(Handle<Function>::Cast(funval));

		funval = GetDefaultContext()->Global()->Get(String::New("fastDispatcher_1"));
		if (!funval->IsFunction()) {
			DEBUG("fastDispatcher_1 not found");
			KillPinTool();
		}
		trace_function = Persistent<Function>::New(Handle<Function>::Cast(funval));

		funval = GetDefaultContext()->Global()->Get(String::New("fastDispatcher_2"));
		if (!funval->IsFunction()) {
			DEBUG("fastDispatcher_2 not found");
			KillPinTool();
		}
		ins_function = Persistent<Function>::New(Handle<Function>::Cast(funval));

		main_ctx = Persistent<Context>::New(GetDefaultContext());
		main_ctx_isolate = GetDefaultIsolate();

		//this is the "receiver" argument for Invoke, it's the "this" of the function called
		//used for the "FastCall" hack
		i::Object** ctx = reinterpret_cast<i::Object**>(*main_ctx);
		i::Handle<i::Context> internal_context = i::Handle<i::Context>::cast(i::Handle<i::Object>(ctx));
		i::GlobalObject *internal_global = internal_context->global();
		i::Handle<i::JSObject> receiver(internal_global->global_receiver());
		Local<Object> receiver_local(Utils::ToLocal(receiver));
		main_ctx_global = Persistent<Object>::New(receiver_local);


		if (argc)
			sorrrowctx->LoadMain();
	}
}
