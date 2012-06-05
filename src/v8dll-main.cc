#include "pintool.h"
#include "v8dll-main.h"

KNOB<string> KnobOutputFile(KNOB_MODE_WRITEONCE, "pintool", "o", "pet-output.log", "specify output file name");
KNOB<string> KnobDebugFile(KNOB_MODE_WRITEONCE, "pintool", "d", "pet-debug.log", "specify debug file name");
KNOB<string> KnobV8Options(KNOB_MODE_WRITEONCE, "pintool", "e", "", "v8 engine options");

INT32 Usage()
{
    cerr <<
        "This pin tool exports PIN to javascript and allows to execute scripts for instrumentation.\n"
        "\n";

    cerr << KNOB_BASE::StringKnobSummary() << endl;
    return -1;
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

	//Initialize default isolate
	V8::Initialize();

	//Just in case we want to mangle with any V8 flag
	V8::SetFlagsFromString(KnobV8Options.Value().c_str(), KnobV8Options.Value().length());
	DEBUG("V8Options=" << KnobV8Options.Value());

	ctxmgr = new ContextManager();

	if (!ctxmgr || !ctxmgr->IsValid())
	{
		DEBUG("Failed to initialize the Context Manager");
		return -1;
	}

	PIN_AddApplicationStartFunction(&AddGenericInstrumentation, 0);

	PIN_InitSymbols();

    // Start the program, never returns
    PIN_StartProgram();
    
    return 0;
}
