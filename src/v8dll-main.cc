#include "pintool.h"
#include "v8dll-main.h"

KNOB<string> KnobOutputFile(KNOB_MODE_WRITEONCE, "pintool", "o", "pet-output.log", "specify output file name");
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

	PinContext *pincontext = ctxmgr->CreateContext(tid, false);

	//set the per-thread context class to the per-thread scratch register allocated for it.
	PIN_SetContextReg(ctx, ctxmgr->GetPerThreadContextReg(), reinterpret_cast<ADDRINT>(pincontext));

	DEBUG("JS Context for " << tid << " queued for initialization");
}

VOID ContextFini(INT32 code, VOID *v)
{
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
}

int main(int argc, char * argv[])
{
    // Initialize pin
	if (PIN_Init(argc, argv)) {
		return Usage();
	}

	OutFile.open(KnobOutputFile.Value().c_str());
	OutFile.setf(ios::showbase);
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
	PIN_AddFiniUnlockedFunction(ContextFini, 0);

	const char *args[2] = { "dummy", "dummy" };
	int argsc = 0;

	if (KnobJSFile.NumberOfValues() > 0) {
		args[1] = KnobJSFile.Value().c_str();
		argsc = 2;
	}
	ctxmgr->InitializeSorrowContext(argsc, args);

    // Start the program, never returns
    PIN_StartProgram();
    
    return 0;
}
