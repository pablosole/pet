#include "pintool.h"
#include <iostream>
#include <process.h>


VOID ContextManager::Run(VOID *_ctx)
{
	ContextManager *ctx = static_cast<ContextManager *>(_ctx);

	//just for extra safety
	if (!ctx || !ctx->IsValid())
		return;

	DEBUG("PinContextManager thread initiated...");
	ctx->Ready();

	while (true)
	{
		DEBUG("wait here until something happens");
		ctx->Wait();

		//check if we're about to die
		if (!ctx->IsRunning())
			break;

		DEBUG("something to do in the context manager");
		ctx->ProcessChanges();
	}

	//If we're here, it means our tool is going to die soon, kill all contexts as gracefully as possible.
	ctx->KillAllContexts();
	delete ctx;

	DEBUG("ContextManager thread exit");

	//This should go away at some point:
	KillPinTool();
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
				DEBUG("erase context for tid:" << tid);

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

	DEBUG("about to die, kill everything...");
	Lock();

	it = contexts.begin();
	while (it != contexts.end())
	{
		THREADID tid = it->first;
		PinContext *context = it->second;
		
		if (context->GetState() == PinContext::INITIALIZED_CONTEXT) {
			DEBUG("destroying context for tid:" << tid);
			context->DestroyJSContext();
		}
		DEBUG("deleting context for tid:" << tid);
		contexts.erase(it++);
		delete context;
	}

	Unlock();
}

//It's mandatory to check the result of this initialization using IsValid()
//after construction.
ContextManager::ContextManager()
{
	if (!PIN_MutexInit(&lock) || 
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

	//A default context for the instrumentation functions over the default isolate.
	{
		Locker lock;
		default_context = Context::New();
		if (default_context.IsEmpty())
		{
			SetState(ERROR_MANAGER);
			return;
		}
	}

	SetState(RUNNING_MANAGER);
	tid = PIN_SpawnInternalThread(Run, this, 0, NULL);
	
	if (tid == INVALID_THREADID)
		SetState(ERROR_MANAGER);
}

ContextManager::~ContextManager()
{
	if (GetState() == ERROR_MANAGER)
		return;

	PIN_MutexFini(&lock);
	PIN_SemaphoreFini(&changed);
	PIN_SemaphoreFini(&ready);
	PIN_DeleteThreadDataKey(per_thread_context_key);

	V8::TerminateExecution();
	default_context.Dispose();
}

//this function returns a new context and adds it to the map of contexts or returns
//INVALID_PIN_CONTEXT if cannot allocate a new context or
//EXISTS_PIN_CONTEXT if there's a context for this tid in the map already.
PinContext *ContextManager::CreateContext(THREADID tid)
{
	PinContext *context;
	pair<ContextsMap::iterator, bool> ret;

	DEBUG("CreateContext called");

	context = new PinContext(tid);
	if (!context)
		return INVALID_PIN_CONTEXT;

	Lock();
	ret = contexts.insert(pair<THREADID, PinContext *>(tid, context));
	Unlock();

	if (ret.second == false)
	{
		delete context;
		return EXISTS_PIN_CONTEXT;
	}

	DEBUG("waiting for the context manager to wake up");
	Notify();

	//wait until the new context is created
	context->Wait();

	return (context->GetState(true) == PinContext::INITIALIZED_CONTEXT) ? context : INVALID_PIN_CONTEXT;
}

VOID ContextManager::EnsureContextCallbackHelper(VOID *v)
{
	if (!ctxmgr || !ctxmgr->IsRunning())
		return;

	EnsureCallback *info = static_cast<EnsureCallback *>(v);
	ctxmgr->WaitReady();

	PinContext *context = ctxmgr->EnsurePinContext(info->tid, info->create);

	if (!PinContext::IsValid(context))
	{
		DEBUG("Couldn't ensure a valid context for callback on tid " << info->tid);
		return;
	}
	info->callback(context);
}

//This function is used to queue a callback to run whenever the Context Manager is ready to work.
//The function is executed from an internal thread and it receives a PinContext * as its only argument.
bool ContextManager::EnsurePinContextCallback(THREADID tid, bool create, ENSURE_CALLBACK_FUNC *callback)
{
	if (!IsRunning())
		return false;

	if (IsReady())
	{
		PinContext *context = EnsurePinContext(tid, create);
		callback(context);
		return true;
	}

	EnsureCallback *info = new EnsureCallback();
	info->callback = callback;
	info->tid = tid;
	info->create = create;

	return PIN_SpawnInternalThread(ContextManager::EnsureContextCallbackHelper, info, 0, 0) != INVALID_THREADID;
}

PinContext *ContextManager::EnsurePinContext(THREADID tid, bool create)
{
	DEBUG("EnsurePinContext called");

	if (!IsReady())
		return NO_MANAGER_CONTEXT;

	PinContext *context = static_cast<PinContext *>(PIN_GetThreadData(per_thread_context_key, tid));

	if (!PinContext::IsValid(context) && create)
		context = CreateContext(tid);

	PIN_SetThreadData(per_thread_context_key, context, tid);

	return context;
}
