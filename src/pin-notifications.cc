#include "pintool.h"
#include <stdarg.h>
#include <malloc.h>

VOID OnThreadStart(PinContext *context, VOID *f)
{
	DEBUG("OnThreadStart for tid:" << context->GetTid());
/*
	if (!ctxmgr->EnsurePinContextCallback(tid, OnThreadStart, v))
		DEBUG("Failed to create EnsurePinContext callback for tid " << tid);
*/
	//Add instrumentation just once
	static bool done = false;
	if (!done) {
		Locker lock(ctxmgr->GetDefaultIsolate());
		HandleScope hscope;
		Context::Scope cscope(ctxmgr->GetSharedDataContext());

		Handle<Value> ret = evalOnDefaultContext(context, "yahoo=10;");
		if (!ret.IsEmpty()) {
			String::Utf8Value ret_str(ret);
			DEBUG("eval returned: " << *ret_str);
		}
//		INS_AddInstrumentFunction(Trace, f);
		done=true;
	}
}


void ThreadFini(THREADID tid, const CONTEXT *ctxt, INT32 code, VOID *v)
{
	if (!PIN_IsApplicationThread())
		return;

	PinContext *context = ctxmgr->LoadPinContext(tid);
	if (!PinContext::IsValid(context))
		return;

	DEBUG("ThreadFini called:" << tid);
}

