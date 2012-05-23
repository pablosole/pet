#include "pintool.h"
#include <iostream>
#include <process.h>

bool DestroyPinContext(PinContext *context);

void PinContextManager(void *notused)
{
	ContextsMap::iterator it;

	DEBUG("PinContextManager initiated...");

	//The thread is started after PIN initialization, but there's still a race
	PIN_Sleep(2000);

	PIN_SemaphoreSet(&context_manager_ready);

	while (!kill_contexts)
	{
		DEBUG("wait here until something happens");
		PIN_SemaphoreWait(&contexts_changed);
		PIN_SemaphoreClear(&contexts_changed);

		DEBUG("something to do in the context manager");
		PIN_RWMutexWriteLock(&contexts_lock);

		it = contexts.begin();
		while (it != contexts.end())
		{
			PIN_THREAD_UID tid = it->first;
			PinContext *context = it->second;
			
			//Create a context for a new thread
			switch (context->state)
			{
				case NEW_CONTEXT:
					DEBUG("create new context for tid:" << tid);

					PIN_MutexLock(&context->lock);
					DEBUG("after mutex");
					context->context = Context::New();
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

		PIN_RWMutexUnlock(&contexts_lock);
	}

	//If we're here, it means our tool is going to die soon, kill all contexts as gracefully as possible.

	DEBUG("about to die, kill everything...");
	PIN_RWMutexWriteLock(&contexts_lock);
	it = contexts.begin();
	while (it != contexts.end())
	{
		PinContext *context = it->second;
		
		DestroyPinContext(context);
		delete context;
		contexts.erase(it++);
	}
	PIN_RWMutexUnlock(&contexts_lock);
}

//Enter this function without the specific context's lock held
bool DestroyPinContext(PinContext *context)
{
	PIN_MutexLock(&context->lock);
	context->context.Dispose();
	context->context.Clear();
	context->state = DEAD_CONTEXT;
	PIN_MutexUnlock(&context->lock);
	PIN_SemaphoreSet(&context->state_changed);

	return true;
}

bool InitializePinContexts()
{
	if (!PIN_RWMutexInit(&contexts_lock) || 
		!PIN_SemaphoreInit(&contexts_changed) || 
		!PIN_SemaphoreInit(&context_manager_ready))
		return false;

	return PIN_SpawnInternalThread(PinContextManager, 0, 0, &context_manager_tid) != INVALID_THREADID;
}

//this function returns a new context and adds it to the map of contexts or returns
//INVALID_PIN_CONTEXT if cannot allocate a new context or
//EXISTS_PIN_CONTEXT if there's a context for this tid in the map already.
PinContext *CreatePinContext(PIN_THREAD_UID tid)
{
	PinContext *context;
	pair<ContextsMap::iterator, bool> ret;
	ContextState state;

	DEBUG("CreatePinContext called");

	context = new PinContext(tid);
	if (!context)
		return INVALID_PIN_CONTEXT;

	PIN_RWMutexWriteLock(&contexts_lock);
	ret = contexts.insert(pair<PIN_THREAD_UID, PinContext *>(tid, context));
	PIN_RWMutexUnlock(&contexts_lock);

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
	EnsureCallback *info = reinterpret_cast<EnsureCallback *>(v);
	PIN_SemaphoreWait(&context_manager_ready);

	PinContext *context = EnsurePinContext(info->tid, info->create);
	info->callback(context);
}

//This function is used to queue a callback to run whenever the Context Manager is ready to work.
//The function is executed from an internal thread and it receives a PinContext * as its only argument.
bool EnsurePinContextCallback(PIN_THREAD_UID tid, bool create, ENSURE_CALLBACK_FUNC *callback)
{
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

PinContext *EnsurePinContext(PIN_THREAD_UID tid, bool create)
{
	DEBUG("EnsurePinContext called");

	if (!PIN_SemaphoreIsSet(&context_manager_ready))
		return NO_MANAGER_CONTEXT;

	ContextsMap::const_iterator it;
	PinContext *context;
	bool notfound;

	PIN_RWMutexReadLock(&contexts_lock);
	it = contexts.find(tid);
	notfound = it == contexts.end();
	PIN_RWMutexUnlock(&contexts_lock);

	if (notfound)
	{
		if (create == false)
			return INVALID_PIN_CONTEXT;

		context = CreatePinContext(tid);
		if (context == INVALID_PIN_CONTEXT)
		{
			LOG_PIN("Error allocating a new context for a thread, bailing out\n");
			exit(-1);
		} else if (context == EXISTS_PIN_CONTEXT)
		{
			//this shouldn't happen, but address it anyway.
			//It means there was a TOCTOU between our find() and the create call.
			PIN_RWMutexReadLock(&contexts_lock);
			it = contexts.find(tid);
			PIN_RWMutexUnlock(&contexts_lock);
		} else
			return context;
	}

	return it->second;
}
