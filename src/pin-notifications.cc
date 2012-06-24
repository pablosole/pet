#include "pintool.h"
#include <stdarg.h>
#include <malloc.h>

UINT32 docount(uint32_t argc, PinContext *context, AnalysisFunction *af, ...)
{
	va_list argptr;

	if (!af->IsEnabled())
		return 0;

#if 0
	if (!af) {
		DEBUG("Analysis Function not found on tid: " << context->GetTid());
		return 0;
	}
#endif

	Persistent<Function> fun = af->EnsureFunction(context);

#if 0
	if (fun.IsEmpty()) {
		DEBUG("Function object not found for af: " << func << " on tid: " << context->GetTid());
		return 0;
	}
#endif

	Isolate::Scope iscope(context->GetIsolate());
	Locker lock(context->GetIsolate());
	HandleScope hscope;
	Context::Scope cscope(context->GetContext());

	va_start(argptr, af);

	//malloca allocated from the stack unless the size is >1024
	//stack allocation is faster than heap and freea becomes basically a NOOP
	Handle<Value> *argv = reinterpret_cast<Handle<Value> *>(_malloca(sizeof(char *)*argc));

	for (uint32_t x=0; x < argc; x++)
		argv[x] = Uint32::NewFromUnsigned(va_arg(argptr, uint32_t));

	va_end(argptr);

	TryCatch trycatch;

	fun->FastCall(context->GetIsolate(), *(context->GetContext()), context->GetGlobal(), argc, argv);

	if (argv[0]->Uint32Value() != argv[1]->Uint32Value())
	{
		DEBUG("argv[0]=" << argv[0]->Uint32Value());
		DEBUG("argv[1]=" << argv[1]->Uint32Value());
	}

	if (argv[2]->Uint32Value() != 0xcafecafe)
		DEBUG("argv[2]=" << argv[2]->Uint32Value());

	if (trycatch.HasCaught())
	{
		DEBUG("Exception on AF");
	}

	_freea(argv);

	return 0;
}

VOID Trace(INS ins, VOID *v)
{
	static int count=0;

	if (count > 1000) {

		return;
	}

	AnalysisFunction *af = reinterpret_cast<AnalysisFunction *>(v);

	if (INS_IsMemoryRead(ins) || INS_IsMemoryWrite(ins)) {
		INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)docount, \
		IARG_PRESERVE, &ctxmgr->GetPreservedRegset(), \
		IARG_UINT32, af->GetArgumentCount(), \
		IARG_IARGLIST, af->GetArguments(), \
		IARG_END);

		count++;
	}

}

VOID OnThreadStart(PinContext *context, VOID *f)
{
	DEBUG("OnThreadStart for tid:" << context->GetTid());

	{
		Isolate::Scope iscope(context->GetIsolate());
		Locker lock(context->GetIsolate());
		HandleScope hscope;
		Context::Scope cscope(context->GetContext());

		Handle<String> source = String::New("var read=0; var write=0; var yahoo=0;");
		Handle<Script> script = Script::Compile(source);
		if (script.IsEmpty()) {
			DEBUG("Exception compiling.");
			return;
		}
		Handle<Value> result = script->Run();
	}

	//Add instrumentation just once
	static bool done = false;
	if (!done) {
		INS_AddInstrumentFunction(Trace, f);
		done=true;
	}
}

void ThreadStart(THREADID tid, CONTEXT *ctx, INT32 flags, VOID *v)
{
	if (!PIN_IsApplicationThread())
		return;

	PinContext *pincontext = ctxmgr->CreateContext(tid, false);

	//set the per-thread context class to the per-thread scratch register allocated for it.
	PIN_SetContextReg(ctx, ctxmgr->GetPerThreadContextReg(), reinterpret_cast<ADDRINT>(pincontext));

	DEBUG("ThreadStart called:" << tid);
	if (!ctxmgr->EnsurePinContextCallback(tid, OnThreadStart, v))
		DEBUG("Failed to create EnsurePinContext callback for tid " << tid);
}

void ThreadFini(THREADID tid, const CONTEXT *ctxt, INT32 code, VOID *v)
{
	if (!PIN_IsApplicationThread())
		return;

	PinContext *context = ctxmgr->LoadPinContext(tid);
	if (!PinContext::IsValid(context))
		return;

	Isolate::Scope iscope(context->GetIsolate());
	Locker lock(context->GetIsolate());
	HandleScope hscope;
	Context::Scope cscope(context->GetContext());

	Handle<Value> read = context->GetContext()->Global()->Get(String::New("read"));
	String::AsciiValue read_ascii(read);
	Handle<Value> write = context->GetContext()->Global()->Get(String::New("write"));
	String::AsciiValue write_ascii(write);
	Handle<Value> yahoo = context->GetContext()->Global()->Get(String::New("yahoo"));
	String::AsciiValue yahoo_ascii(yahoo);

	ofstream test;
	stringstream filename;
	filename << "rw_" << tid << ".txt";

	test.open(filename.str().c_str());
	test.setf(ios::showbase);
	test << "read=" << *read_ascii << " write=" << *write_ascii << " yahoo=" << *yahoo_ascii << " for tid=" << tid << endl;
	test.close();

	DEBUG("output: " << "read=" << *read_ascii << " write=" << *write_ascii << " yahoo=" << *yahoo_ascii << " for tid=" << tid << endl);

	DEBUG("ThreadFini called:" << tid);
}

void Fini(INT32 code, void *v)
{
	DEBUG("Fini called");

	ctxmgr->Abort();
}

VOID AddGenericInstrumentation(VOID *)
{
	//this is executed from the app's main thread
	//before the first ThreadStart
	DEBUG("Main TID:" << PIN_ThreadId());

	AnalysisFunction *af = ctxmgr->AddFunction("function update(readsize, writesize, other) { if (readsize != 0) read++; if (writesize != 0) write++; if (other == 0xcafecafe) yahoo=1; }");
	af->AddArgument(IARG_MEMORYREAD_SIZE);
	af->AddArgument(IARG_MEMORYWRITE_SIZE);
	af->AddArgument<uint32_t>(IARG_UINT32, 0xcafecafe);

	PIN_AddThreadStartFunction(&ThreadStart, af);
	PIN_AddThreadFiniFunction(&ThreadFini, 0);
	PIN_AddFiniUnlockedFunction(&Fini, 0);
}
