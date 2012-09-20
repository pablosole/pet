#include "pintool.h"
#include "v8dll-main.h"

KNOB<string> KnobDebugFile(KNOB_MODE_WRITEONCE, "pintool", "d", "pet-debug.log", "specify debug file name");
KNOB<string> KnobV8Options(KNOB_MODE_WRITEONCE, "pintool", "e", "--always_opt --break_on_abort --harmony_collections --harmony_proxies", "v8 engine options");
KNOB<string> KnobJSFile(KNOB_MODE_WRITEONCE, "pintool", "f", "main.js", "JS file to execute on init");

INT32 Usage()
{
    cerr <<
        "This pin tool exports PIN to javascript and allows to execute scripts for instrumentation.\n"
        "\n";

    cerr << KNOB_BASE::StringKnobSummary() << endl;
    return -1;
}

VOID InitializePerThreadContexts(THREADID tid, CONTEXT *ctx, INT32 flags, VOID *v)
{
	if (!PIN_IsApplicationThread())
		return;

	if (ctxmgr->IsDieing())
		return;

	PinContext *pincontext = ctxmgr->CreateContext(tid, false);

	//set the per-thread context class to the per-thread scratch register allocated for it.
	PIN_SetContextReg(ctx, ctxmgr->GetPerThreadContextReg(), reinterpret_cast<ADDRINT>(pincontext));

	DEBUG("JS Context for " << tid << " queued for initialization");
}

VOID AddThreads(THREADID tid, CONTEXT *ctx, INT32 flags, VOID *v)
{
	if (!ctxmgr->IsRunning())
		return;

	Locker locker;
	Context::Scope context_scope(ctxmgr->GetDefaultContext());
	HandleScope hscope;
	Local<Object> global = ctxmgr->GetDefaultContext()->Global();
	Local<Value> funval = global->Get(String::New("addThread"));
	if (!funval->IsFunction()) {
		DEBUG("addThread not found");
		KillPinTool();
	}

	Local<Value> args[1];
	args[0] = Integer::NewFromUnsigned(8);
	Local<Object> ptr = ctxmgr->GetSorrowContext()->GetPointerTypes()->GetOwnPointerFunct()->NewInstance(1, args);
	PIN_THREAD_UID *p_uid = (PIN_THREAD_UID *)ptr->GetPointerFromInternalField(0);
	*p_uid = PIN_ThreadUid();

	const int argc = 4;
	Local<Value> argv[argc];

	argv[0] = Integer::NewFromUnsigned(PIN_GetTid());
	argv[1] = Integer::NewFromUnsigned(tid);
	argv[2] = ptr;
	argv[3] = Local<Value>::New(Boolean::New(PIN_IsApplicationThread()));

	Local<Function> fun = Local<Function>::Cast(funval);
	fun->Call(global, argc, argv);
}

VOID RemoveThreads(THREADID tid, const CONTEXT *ctx, INT32 code, VOID *v)
{
	if (!ctxmgr->IsRunning())
		return;

	Locker locker;
	Context::Scope context_scope(ctxmgr->GetDefaultContext());
	HandleScope hscope;
	Local<Object> global = ctxmgr->GetDefaultContext()->Global();
	Local<Value> funval = global->Get(String::New("removeThread"));
	if (!funval->IsFunction()) {
		DEBUG("removeThread not found");
		KillPinTool();
	}

	const int argc = 1;
	Local<Value> argv[argc];

	argv[0] = Integer::NewFromUnsigned(tid);

	Local<Function> fun = Local<Function>::Cast(funval);
	fun->Call(global, argc, argv);
}

VOID ContextFini(INT32 code, VOID *v)
{
	DEBUG("Application is about to finish");
	{
		Locker locker;
		Context::Scope context_scope(ctxmgr->GetDefaultContext());
		HandleScope hscope;
		Local<Object> global = ctxmgr->GetDefaultContext()->Global();
		Local<Value> funval = global->Get(String::New("finiAppDispatcher"));
		if (!funval->IsFunction()) {
			DEBUG("finiAppDispatcher not found");
			KillPinTool();
		}

		Local<Function> fun = Local<Function>::Cast(funval);
		fun->Call(global, 0, NULL);
	}

	ctxmgr->Abort();

	//If we're here, it means our tool is going to die soon, kill all contexts as gracefully as possible.
	ctxmgr->KillAllContexts();
	delete ctxmgr;
}

int main(int argc, char * argv[])
{
    // Initialize pin
	if (PIN_Init(argc, argv)) {
		return Usage();
	}

	DebugFile.open(KnobDebugFile.Value().c_str());
	DebugFile.setf(ios::showbase);

	//Must be executed before ANY v8 function
	internal::InitializeIsolation();

	//Just in case we want to mangle with any V8 flag
	V8::SetFlagsFromString(KnobV8Options.Value().c_str(), KnobV8Options.Value().length());
	DEBUG("V8Options=" << KnobV8Options.Value());

	ctxmgr = new ContextManager();

	if (!ctxmgr || !ctxmgr->IsValid())
	{
		DEBUG("Failed to initialize the Context Manager");
		return -1;
	}

	if (!WINDOWS::IsDebuggerPresent())
		PIN_AddInternalExceptionHandler(InternalExceptionHandler, 0);

	PIN_InitSymbols();

	//Callbacks for per-thread context initialization and termination
	PIN_AddThreadStartFunction(InitializePerThreadContexts, 0);
	PIN_AddThreadStartFunction(AddThreads, 0);
	PIN_AddThreadFiniFunction(RemoveThreads, 0);
	PIN_AddFiniUnlockedFunction(ContextFini, 0);

	const char *args[2] = { "dummy", "dummy" };
	int argsc = 0;

	if (KnobJSFile.NumberOfValues() > 0) {
		DEBUG("Running script " << KnobJSFile.Value().c_str());
		args[1] = KnobJSFile.Value().c_str();
		argsc = 2;
	}
	ctxmgr->InitializeSorrowContext(argsc, args);

    // Start the program, never returns
    PIN_StartProgram();
    
    return 0;
}
