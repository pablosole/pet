#include "pintool.h"

VOID OnThreadStart(PinContext *context)
{
	DEBUG("OnThreadStart for tid:" << context->GetTid());
}

void ThreadStart(THREADID tid, CONTEXT *ctx, INT32 flags, VOID *v)
{
	if (!PIN_IsApplicationThread())
		return;

	DEBUG("ThreadStart called:" << tid);
	if (!ctxmgr->EnsurePinContextCallback(tid, OnThreadStart))
		DEBUG("Failed to create EnsurePinContext callback for tid " << tid);
}

void ThreadFini(THREADID tid, const CONTEXT *ctxt, INT32 code, VOID *v)
{
	if (!PIN_IsApplicationThread())
		return;

	DEBUG("ThreadFini called:" << tid);
}

void Fini(INT32 code, void *v)
{
	DEBUG("Fini called");

	ctxmgr->Abort();
}

VOID AddGenericInstrumentation(VOID *)
{
	PIN_AddThreadStartFunction(&ThreadStart, 0);
	PIN_AddThreadFiniFunction(&ThreadFini, 0);
	PIN_AddFiniUnlockedFunction(&Fini, 0);

	//this is executed from the app's main thread
	//before the first ThreadStart
	DEBUG("Main TID:" << PIN_ThreadId());
}
