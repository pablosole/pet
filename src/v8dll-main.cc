#include "pintool.h"

ofstream OutFile;
ofstream DebugFile;

ContextsMap contexts;
PIN_RWMUTEX contexts_lock;
PIN_SEMAPHORE contexts_changed;
bool kill_contexts = false;
PIN_THREAD_UID context_manager_tid;
PIN_SEMAPHORE context_manager_ready;

WINDOWS::HANDLE WINAPI MyCreateThread(
			WINDOWS::LPSECURITY_ATTRIBUTES lpThreadAttributes,
			WINDOWS::SIZE_T dwStackSize,
			ROOT_THREAD_FUNC lpStartAddress,
			WINDOWS::LPVOID lpParameter,
			WINDOWS::DWORD dwCreationFlags,
			WINDOWS::LPDWORD lpThreadId
	  )
{
	  return reinterpret_cast<WINDOWS::HANDLE> (PIN_SpawnInternalThread((lpStartAddress),
					lpParameter,
					dwStackSize,
					reinterpret_cast<PIN_THREAD_UID *>(lpThreadId)));
}

VOID OnThreadStart(PinContext *context)
{
	if (!IsValidContext(context))
	{
		DEBUG("OnThreadStart executed over an invalid context");
		return;
	}

	DEBUG("OnThreadStart for tid:" << context->tid);
}

void ThreadStart(THREADID tid, CONTEXT *ctx, INT32 flags, VOID *v)
{
	if (!PIN_IsApplicationThread())
		return;

	DEBUG("ThreadStart called:" << PIN_ThreadUid());
	EnsurePinContextCallback(PIN_ThreadUid(), OnThreadStart);
}

void ThreadFini(THREADID tid, const CONTEXT *ctxt, INT32 code, VOID *v)
{
	if (!PIN_IsApplicationThread())
		return;

	DEBUG("ThreadFini called:" << PIN_ThreadUid());

	//dont create a context if we weren't following this thread
	PinContext *context = EnsurePinContext(PIN_ThreadUid(), false);

	if (!IsValidContext(context))
		return;

	DEBUG("set context state to kill");
	PIN_MutexLock(&context->lock);
	context->state = KILLING_CONTEXT;
	PIN_MutexUnlock(&context->lock);

	PIN_SemaphoreSet(&contexts_changed);

	//wait until the context is actually destroyed (just for debugging)
	PIN_SemaphoreWait(&context->state_changed);
	PIN_SemaphoreClear(&context->state_changed);
}

void Fini(INT32 code, void *v)
{
	DEBUG("Fini called");

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
	DEBUG("Main TID:" << PIN_ThreadUid());
}

KNOB<string> KnobOutputFile(KNOB_MODE_WRITEONCE, "pintool", "o", "pet-output.log", "specify output file name");
KNOB<string> KnobDebugFile(KNOB_MODE_WRITEONCE, "pintool", "d", "pet-debug.log", "specify debug file name");

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

	if (!InitializePinContexts())
	{
		DEBUG("Failed to initialize the Context Manager");
		return -1;
	}

	PIN_AddApplicationStartFunction(&AddGenericInstrumentation, 0);

    // Start the program, never returns
    PIN_StartProgram();
    
    return 0;
}
