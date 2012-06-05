#include "pintool.h"

bool PinContext::CreateJSContext()
{
	DEBUG("create new context for tid:" << tid);

	Lock();

	isolate = Isolate::New();
	if (isolate != 0)
	{
		//Enter and lock the new isolate
		Isolate::Scope iscope(isolate);
		Locker lock(isolate);

		context = Context::New();

		//inform PinContext class in case we need it from a javascript related function
		isolate->SetData(this);

		//Scope and Locker are destroyed here.
	}

	if (context.IsEmpty())
		SetState(ERROR_CONTEXT);
	else
		SetState(INITIALIZED_CONTEXT);

	Unlock();
	Notify();

	return GetState() == INITIALIZED_CONTEXT;
}

void PinContext::DestroyJSContext()
{
	DEBUG("destroy context for tid:" << tid);

	Lock();

	if (isolate != 0)
	{
		Isolate::Scope iscope(isolate);
		Locker lock(isolate);

		if (!context.IsEmpty())
		{
			V8::TerminateExecution(isolate);
			context.Dispose();
			context.Clear();
			V8::ContextDisposedNotification();
		}

		isolate->Dispose();
	}

	SetState(DEAD_CONTEXT);
	Unlock();
	Notify();
}

VOID PinContext::ThreadInfoDestructor(VOID *v)
{
	PinContext *context = static_cast<PinContext *>(v);

	if (!PinContext::IsValid(context))
		return;

	DEBUG("set context state to kill");
	context->Lock();
	context->SetState(PinContext::KILLING_CONTEXT);
	context->Unlock();
	context->Notify();
}
