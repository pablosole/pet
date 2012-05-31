#include "pintool.h"
#include <iostream>
#include <process.h>

bool DestroyPinContext(PinContext *context);
void DeinitializePinContexts();

void PinContextManager(void *notused)
{
	ContextsMap::iterator it;

	DEBUG("PinContextManager initiated...");

	//The thread is started after PIN initialization, but there's still a race
	PIN_Sleep(2000);

	PIN_SemaphoreSet(&context_manager_ready);

	while (true)
	{
		DEBUG("wait here until something happens");
		PIN_SemaphoreWait(&contexts_changed);
		PIN_SemaphoreClear(&contexts_changed);

		if (kill_contexts)
			break;

		DEBUG("something to do in the context manager");
		PIN_MutexLock(&contexts_lock);

		it = contexts.begin();
		while (it != contexts.end())
		{
			THREADID tid = it->first;
			PinContext *context = it->second;
			
			switch (context->state)
			{
				case NEW_CONTEXT:
					DEBUG("create new context for tid:" << tid);

					PIN_MutexLock(&context->lock);
					DEBUG("after mutex");
					{
						Locker lock;
						context->context = Context::New();

						//All contexts can talk with everybody
						context->context->SetSecurityToken(Undefined());
					}
					DEBUG("after context new");
					if (context->context.IsEmpty())
						context->state = ERROR_CONTEXT;
					else
						context->state = INITIALIZED_CONTEXT;
					PIN_MutexUnlock(&context->lock);
					PIN_SemaphoreSet(&context->state_changed);
					++it;
					break;

				case KILLING_CONTEXT:
					DEBUG("destroy context for tid:" << tid);

					DestroyPinContext(context);
					++it;
					break;

				case DEAD_CONTEXT:
				case ERROR_CONTEXT:
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

		PIN_MutexUnlock(&contexts_lock);
	}

	//If we're here, it means our tool is going to die soon, kill all contexts as gracefully as possible.

	DEBUG("about to die, kill everything...");
	PIN_MutexLock(&contexts_lock);
	it = contexts.begin();
	while (it != contexts.end())
	{
		THREADID tid = it->first;
		PinContext *context = it->second;
		
		if (context->state == INITIALIZED_CONTEXT) {
			DEBUG("destroying context for tid:" << tid);
			DestroyPinContext(context);
		}
		DEBUG("deleting context for tid:" << tid);
		delete context;
		contexts.erase(it++);
	}

	PIN_MutexUnlock(&contexts_lock);
	DeinitializePinContexts();

	DEBUG("Terminating scripts...");
	V8::TerminateExecution();

	DEBUG("PinContextManager exit");
	PIN_ExitProcess(0);
}

//Enter this function without the specific context's lock held
bool DestroyPinContext(PinContext *context)
{
	PIN_MutexLock(&context->lock);
	{
		Locker lock;
		context->context.Dispose();
		context->context.Clear();
		V8::ContextDisposedNotification();
	}
	context->state = DEAD_CONTEXT;
	PIN_MutexUnlock(&context->lock);
	PIN_SemaphoreSet(&context->state_changed);

	return true;
}

VOID ThreadContextDestructor(VOID *v)
{
	PinContext *context = reinterpret_cast<PinContext *>(v);

	if (!IsValidContext(context))
		return;

	DEBUG("set context state to kill");
	PIN_MutexLock(&context->lock);
	context->state = KILLING_CONTEXT;
	PIN_MutexUnlock(&context->lock);

	PIN_SemaphoreSet(&contexts_changed);

	//wait until the context is actually destroyed (just for debugging)
	PIN_SemaphoreWait(&context->state_changed);
	PIN_SemaphoreClear(&context->state_changed);
}

bool InitializePinContexts()
{
	if (!PIN_MutexInit(&contexts_lock) || 
		!PIN_SemaphoreInit(&contexts_changed) || 
		!PIN_SemaphoreInit(&context_manager_ready))
		return false;

	per_thread_context_key = PIN_CreateThreadDataKey(ThreadContextDestructor);
	if (per_thread_context_key == -1)
		return false;

	return (context_manager_tid = PIN_SpawnInternalThread(PinContextManager, 0, 0, NULL)) != INVALID_THREADID;
}

void DeinitializePinContexts()
{
	PIN_MutexFini(&contexts_lock);
	PIN_SemaphoreFini(&contexts_changed);
	PIN_SemaphoreFini(&context_manager_ready);
	PIN_DeleteThreadDataKey(per_thread_context_key);
}

//this function returns a new context and adds it to the map of contexts or returns
//INVALID_PIN_CONTEXT if cannot allocate a new context or
//EXISTS_PIN_CONTEXT if there's a context for this tid in the map already.
PinContext *CreatePinContext(THREADID tid)
{
	PinContext *context;
	pair<ContextsMap::iterator, bool> ret;
	ContextState state;

	DEBUG("CreatePinContext called");

	context = new PinContext(tid);
	if (!context)
		return INVALID_PIN_CONTEXT;

	PIN_MutexLock(&contexts_lock);
	ret = contexts.insert(pair<THREADID, PinContext *>(tid, context));
	PIN_MutexUnlock(&contexts_lock);

	if (ret.second == false)
	{
		delete context;
		return EXISTS_PIN_CONTEXT;
	}

	DEBUG("waiting for the context manager to wake up");
	PIN_SemaphoreSet(&contexts_changed);

	//wait until the new context is created
	PIN_SemaphoreWait(&context->state_changed);
	PIN_SemaphoreClear(&context->state_changed);

	PIN_MutexLock(&context->lock);
	state = context->state;
	PIN_MutexUnlock(&context->lock);

	return (state == INITIALIZED_CONTEXT) ? context : INVALID_PIN_CONTEXT;
}

VOID EnsureContextCallbackHelper(VOID *v)
{
	if (kill_contexts)
		return;

	EnsureCallback *info = reinterpret_cast<EnsureCallback *>(v);
	PIN_SemaphoreWait(&context_manager_ready);

	PinContext *context = EnsurePinContext(info->tid, info->create);

	if (!IsValidContext(context))
	{
		DEBUG("Couldn't ensure a valid context for callback on tid " << info->tid);
		return;
	}
	info->callback(context);
}

//This function is used to queue a callback to run whenever the Context Manager is ready to work.
//The function is executed from an internal thread and it receives a PinContext * as its only argument.
bool EnsurePinContextCallback(THREADID tid, bool create, ENSURE_CALLBACK_FUNC *callback)
{
	if (kill_contexts)
		return false;

	if (PIN_SemaphoreIsSet(&context_manager_ready))
	{
		PinContext *context = EnsurePinContext(tid, create);
		callback(context);
		return true;
	}

	EnsureCallback *info = new EnsureCallback();
	info->callback = callback;
	info->tid = tid;
	info->create = create;

	return PIN_SpawnInternalThread(EnsureContextCallbackHelper, info, 0, 0) != INVALID_THREADID;
}

PinContext *EnsurePinContext(THREADID tid, bool create)
{
	DEBUG("EnsurePinContext called");

	if (!PIN_SemaphoreIsSet(&context_manager_ready))
		return NO_MANAGER_CONTEXT;

	PinContext *context = reinterpret_cast<PinContext *>(PIN_GetThreadData(per_thread_context_key, tid));

	if (!IsValidContext(context))
		if (create)
			context = CreatePinContext(tid);
		else
			context = INVALID_PIN_CONTEXT;

	PIN_SetThreadData(per_thread_context_key, context, tid);

	return context;
}
