#include "pin.H"
#undef LOG
#undef ASSERT
#define LOG_PIN(message) QMESSAGE(MessageTypeLog, message)
#define ASSERT_PIN(condition,message)   \
    do{ if(!(condition)) \
        ASSERTQ(LEVEL_BASE::AssertString(PIN_ASSERT_FILE, __FUNCTION__, __LINE__, std::string("") + message));} while(0)

#undef USING_V8_SHARED
#include "../include/v8.h"
using namespace v8;
namespace WINDOWS {
#include <windows.h>
}
#include <map>
#include <iostream>
#include <fstream>
#include <list>

#define DEBUG(m) do { \
DebugFile << m << endl; \
cerr << m << endl; \
} while (false);

#define CACHE_LINE  64
#define CACHE_ALIGN __declspec(align(CACHE_LINE))

class PinContext;

typedef VOID ENSURE_CALLBACK_FUNC(PinContext * arg);
struct EnsureCallback {
	THREADID tid;
	ENSURE_CALLBACK_FUNC *callback;
	bool create;
};

PinContext * const INVALID_PIN_CONTEXT = reinterpret_cast<PinContext *>(0);
PinContext * const EXISTS_PIN_CONTEXT = reinterpret_cast<PinContext *>(-1);
PinContext * const NO_MANAGER_CONTEXT = reinterpret_cast<PinContext *>(-2);

//we associate each PinContext with an application thread
class CACHE_ALIGN PinContext {
public:
	enum ContextState { NEW_CONTEXT, INITIALIZED_CONTEXT, KILLING_CONTEXT, DEAD_CONTEXT, ERROR_CONTEXT };

	PinContext(THREADID _tid) {
		tid = _tid;
		PIN_MutexInit(&lock);
		PIN_SemaphoreInit(&changed);
		PIN_SemaphoreClear(&changed);
		state = NEW_CONTEXT;
		context.Clear();
		isolate = 0;
	}
	~PinContext() {
		PIN_MutexFini(&lock);
		PIN_SemaphoreFini(&changed);
	}

	ContextState inline GetState(bool locked = false) {
		ContextState tmp;
		if (locked) Lock();
		tmp = state;
		if (locked) Unlock();
		return tmp;
	}
	void inline SetState(ContextState s) { state = s; }

	void inline Lock() { PIN_MutexLock(&lock); }
	void inline Unlock() { PIN_MutexUnlock(&lock); }
	void inline Wait() {
		PIN_SemaphoreWait(&changed);
		PIN_SemaphoreClear(&changed);
	}
	void inline Notify() { PIN_SemaphoreSet(&changed); }

	THREADID inline GetTid() { return tid; }

	bool CreateJSContext();
	void DestroyJSContext();

	static VOID ThreadInfoDestructor(VOID *);

	static bool inline IsValid(PinContext *context) { return !(context == 0 || context == INVALID_PIN_CONTEXT || context == EXISTS_PIN_CONTEXT || context == NO_MANAGER_CONTEXT); }

	//doesn't make sense to have an accessor for this ones
	Isolate *isolate;
	Persistent<Context> context;

private:
	THREADID tid;
	enum ContextState state;
	PIN_MUTEX lock;
	PIN_SEMAPHORE changed;
};

typedef std::map<THREADID, PinContext *> ContextsMap;
class ContextManager {
 public:
	enum ContextManagerState { RUNNING_MANAGER, ERROR_MANAGER, KILLING_MANAGER, DEAD_MANAGER };
	ContextManager();
	~ContextManager();

	inline bool IsValid() { return (state == RUNNING_MANAGER || state == KILLING_MANAGER); }
	inline bool IsRunning() { return state == RUNNING_MANAGER; }
	ContextManagerState inline GetState() { return state; }
	void inline SetState(ContextManagerState s) { state = s; }

	void inline Ready() { PIN_SemaphoreSet(&ready); }
	void inline WaitReady() { PIN_SemaphoreWait(&ready); }
	bool inline IsReady() { return PIN_SemaphoreIsSet(&ready); }

	void inline Wait() {
		PIN_SemaphoreWait(&changed);
		PIN_SemaphoreClear(&changed);
	}
	void inline Notify() { PIN_SemaphoreSet(&changed); }
	void inline Abort() {
		SetState(KILLING_MANAGER);
		Notify();
	}

	void inline Lock() { PIN_MutexLock(&lock); }
	void inline Unlock() { PIN_MutexUnlock(&lock); }
	void inline LockCallbacks() { PIN_MutexLock(&lock_callbacks); }
	void inline UnlockCallbacks() { PIN_MutexUnlock(&lock_callbacks); }
	bool inline CallbacksIsProcessing() {
		if (PIN_MutexTryLock(&lock_callbacks) == false)
			return true;
		else
			PIN_MutexUnlock(&lock_callbacks);
		return false;
	}


	//Context Manager thread function
	static VOID Run(VOID *);

	//Helper functions
	void ProcessCallbacks();
	void ProcessChanges();
	void KillAllContexts();
	PinContext *CreateContext(THREADID tid);
	static VOID ProcessCallbacksHelper(VOID *v);

	//API
	bool EnsurePinContextCallback(THREADID tid, ENSURE_CALLBACK_FUNC *callback, bool create = true);
	PinContext *EnsurePinContext(THREADID tid, bool create = true);

	//for fast access, this should be equivalent to: TlsGetValue(per_thread_context)
	inline PinContext *LoadPinContext(THREADID tid) {
		return EnsurePinContext(tid, false);
	}
	inline PinContext *LoadPinContext() { return LoadPinContext(PIN_ThreadId()); }

 private:
	ContextManagerState state;
	Persistent<Context> default_context;
	ContextsMap contexts;
	list<EnsureCallback *> callbacks;
	PIN_MUTEX lock;
	PIN_MUTEX lock_callbacks;
	PIN_SEMAPHORE changed;
	THREADID tid;
	PIN_SEMAPHORE ready;
	TLS_KEY per_thread_context_key;
};

extern ofstream OutFile;
extern ofstream DebugFile;
extern ContextManager *ctxmgr;

VOID AddGenericInstrumentation(VOID *);
void KillPinTool();
