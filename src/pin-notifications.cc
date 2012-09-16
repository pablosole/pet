#include "pintool.h"
#include <stdarg.h>
#include <malloc.h>

UINT32 docount(PinContext *context, AnalysisFunction *af, uint32_t argc, ...)
{
	va_list argptr;

	//bailout before doing anything else
	if (!af->IsEnabled())
		return 0;

	Isolate::Scope iscope(context->GetIsolate());
	Locker lock(context->GetIsolate());
	HandleScope hscope;
	Context::Scope cscope(context->GetContext());

	Persistent<Function> fun = context->EnsureFunction(af);
	if (fun.IsEmpty()) {
		return 0;
	}

	va_start(argptr, argc);

	//_malloca allocates from the stack unless the size is >1024
	//stack allocation is faster than heap and in that case _freea becomes basically a NOOP
	Handle<Value> *argv = reinterpret_cast<Handle<Value> *>(_malloca(sizeof(char *) * argc));

	//XXX: mangle different IARG_TYPEs differently
	for (uint32_t x=0; x < argc; x++)
		argv[x] = Uint32::NewFromUnsigned(va_arg(argptr, uint32_t));

	va_end(argptr);

	TryCatch trycatch;

	fun->FastCall(context->GetIsolate(), *(context->GetContext()), context->GetGlobal(), argc, argv);

	if (trycatch.HasCaught())
	{
		DEBUG("Exception on AF");
		af->IncException();

		//Disable the function if there's too many exceptions
		if (af->GetNumExceptions() > af->GetThreshold() || !trycatch.CanContinue())
			af->Disable();
	}

	_freea(argv);

	return 0;
}

VOID Trace(INS ins, VOID *v)
{
	static int count=0;

	if (count > 10000) {

		return;
	}

	static int done=0;
	static AnalysisFunction *af1 = ctxmgr->AddFunction("function update(isread) { if (isread == 1) read++; else write++; }", "var read=0; var write=0;");
	static AnalysisFunction *af2 = ctxmgr->AddFunction("function update(isread) { if (isread == 1) read++; else write++; }", "var read=0; var write=0;");
	if (!done) {
		af1->AddArgument<uint32_t>(IARG_UINT32, 1);
		af2->AddArgument<uint32_t>(IARG_UINT32, 0);
		done=1;
	}

	if (INS_IsMemoryRead(ins)) {
		INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)docount, \
		IARG_PRESERVE, &ctxmgr->GetPreservedRegset(), \
		IARG_REG_VALUE, ctxmgr->GetPerThreadContextReg(), \
		IARG_PTR, af1, \
		IARG_UINT32, af1->GetArgumentCount(), \
		IARG_IARGLIST, af1->GetArguments(), \
		IARG_END);

		af1->FixArgs();
		count++;
	}
	if (INS_IsMemoryWrite(ins)) {
		INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)docount, \
		IARG_PRESERVE, &ctxmgr->GetPreservedRegset(), \
		IARG_REG_VALUE, ctxmgr->GetPerThreadContextReg(), \
		IARG_PTR, af2, \
		IARG_UINT32, af2->GetArgumentCount(), \
		IARG_IARGLIST, af2->GetArguments(), \
		IARG_END);

		af2->FixArgs();
		count++;
	}

}

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
		INS_AddInstrumentFunction(Trace, f);
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

