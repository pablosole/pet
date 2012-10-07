#include "pintool.h"
#include <iostream>
#include <process.h>

//It's mandatory to check the result of this initialization using IsValid()
//after construction.
ContextManager::ContextManager() :
last_function_id(0),
instrumentation_flags(0)
{
	SetPerformanceCounter();

	if (!PIN_MutexInit(&lock) ||
		!PIN_RWMutexInit(&lock_functions))
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

	//XMM0-4 are used by V8 JIT
	/*REGSET_Insert(preserved_regs, REG_XMM5);
	REGSET_Insert(preserved_regs, REG_XMM6);
	REGSET_Insert(preserved_regs, REG_XMM7);
	*/

	//A default context for the instrumentation functions over the default isolate.
	{
		Locker lock;
		HandleScope hscope;

		default_context = Context::New();
		if (default_context.IsEmpty())
		{
			SetState(ERROR_MANAGER);
			return;
		}

		{
			Context::Scope cscope(default_context);
			Local<ObjectTemplate> objt = ObjectTemplate::New();
			objt->SetInternalFieldCount(1);
			Local<Object> obj = objt->NewInstance();
			default_context->Global()->SetHiddenValue(String::New("SorrowInstance"), obj);
		}
	}

	default_isolate = Isolate::GetCurrent();

	SetState(RUNNING_MANAGER);
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
	PIN_RWMutexFini(&lock_functions);
	PIN_DeleteThreadDataKey(per_thread_context_key);

	V8::TerminateExecution();
	delete sorrrowctx;
	routine_function.Dispose();
	trace_function.Dispose();
	ins_function.Dispose();
	main_ctx.Dispose();
	main_ctx_global.Dispose();
	default_context.Dispose();
	
	V8::Dispose();
}

//this function returns a new context and adds it to the map of contexts or returns
//INVALID_PIN_CONTEXT if it cannot allocate a new context.
PinContext *ContextManager::CreateContext(THREADID tid)
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
	}

	PIN_SetThreadData(per_thread_context_key, context, tid);

	return context;
}

bool ContextManager::DestroyContext(THREADID tid)
{
	ContextsMap::iterator it;
	bool ret=false;

	Lock();
	it = contexts.find(tid);
	if (it != contexts.end())
	{
		PinContext *ctx = it->second;
		contexts.erase(it);
		delete ctx;
		ret=true;
	}
	Unlock();

	return ret;
}


void ContextManager::KillAllContexts()
{
	ContextsMap::iterator it;

	SetState(ContextManager::DEAD_MANAGER);

	Lock();

	it = contexts.begin();
	while (it != contexts.end())
	{
		THREADID tid = it->first;
		PinContext *context = it->second;
		contexts.erase(it++);
		delete context;
	}

	Unlock();
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
	Local<Value> argv_f[3];

	Local<Value> args[1];
	args[0] = Integer::NewFromUnsigned((uint32_t)&routine_instrumentation_enabled);
	argv_f[0] = sorrrowctx->GetPointerTypes()->GetExternalPointerFunct()->NewInstance(1, args);

	args[0] = Integer::NewFromUnsigned((uint32_t)&trace_instrumentation_enabled);
	argv_f[1] = sorrrowctx->GetPointerTypes()->GetExternalPointerFunct()->NewInstance(1, args);

	args[0] = Integer::NewFromUnsigned((uint32_t)&ins_instrumentation_enabled);
	argv_f[2] = sorrrowctx->GetPointerTypes()->GetExternalPointerFunct()->NewInstance(1, args);

	fun->Call(GetDefaultContext()->Global(), 4, argv_f);



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

uint32_t AnalysisFunction::HashBody()
{
	hash = 0;
	const char *s = body.c_str();

	while (*s)
		hash = hash * 101 + *s++;

	return hash;
}
