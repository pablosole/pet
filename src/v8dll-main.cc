#include "pintool.h"

ofstream OutFile;
ofstream DebugFile;

ContextsMap contexts;
PIN_MUTEX contexts_lock;
PIN_SEMAPHORE contexts_changed;
bool kill_contexts = false;
THREADID context_manager_tid;
PIN_SEMAPHORE context_manager_ready;
TLS_KEY per_thread_context_key;

WINDOWS::HANDLE WINAPI MyCreateThread(
			WINDOWS::LPSECURITY_ATTRIBUTES lpThreadAttributes,
			WINDOWS::SIZE_T dwStackSize,
			ROOT_THREAD_FUNC lpStartAddress,
			WINDOWS::LPVOID lpParameter,
			WINDOWS::DWORD dwCreationFlags,
			WINDOWS::LPDWORD lpThreadId
	  )
{
	THREADID ret = (PIN_SpawnInternalThread((lpStartAddress),
					lpParameter,
					dwStackSize,
					reinterpret_cast<PIN_THREAD_UID *>(lpThreadId)));

	return (ret == INVALID_THREADID) ? 0 : reinterpret_cast<WINDOWS::HANDLE>(ret);
}

VOID OnThreadStart(PinContext *context)
{
	DEBUG("OnThreadStart for tid:" << context->tid);
}

void ThreadStart(THREADID tid, CONTEXT *ctx, INT32 flags, VOID *v)
{
	if (!PIN_IsApplicationThread())
		return;

	DEBUG("ThreadStart called:" << tid);
	if (!EnsurePinContextCallback(tid, OnThreadStart))
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

	kill_contexts = true;
	PIN_SemaphoreSet(&contexts_changed);

	OutFile.close();
	DebugFile.close();
}

void AddGenericInstrumentation(void *)
{
	PIN_AddThreadStartFunction(&ThreadStart, 0);
	PIN_AddThreadFiniFunction(&ThreadFini, 0);
	PIN_AddFiniUnlockedFunction(&Fini, 0);

	//this is executed from the app's main thread
	//before the first ThreadStart
	DEBUG("Main TID:" << PIN_ThreadId());
}

KNOB<string> KnobOutputFile(KNOB_MODE_WRITEONCE, "pintool", "o", "pet-output.log", "specify output file name");
KNOB<string> KnobDebugFile(KNOB_MODE_WRITEONCE, "pintool", "d", "pet-debug.log", "specify debug file name");
KNOB<string> KnobV8Options(KNOB_MODE_WRITEONCE, "pintool", "e", "--preemption", "v8 engine options");

int main(int argc, char * argv[])
{
    // Initialize pin
    if (PIN_Init(argc, argv)) return -1;

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

	if (!InitializePinContexts())
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
