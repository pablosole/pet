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

#define DEBUG(m) do { \
DebugFile << m << endl; \
cerr << m << endl; \
} while (false);

enum ContextState { NEW_CONTEXT, INITIALIZED_CONTEXT, KILLING_CONTEXT, DEAD_CONTEXT, ERROR_CONTEXT };

//we associate each PinContext with an application thread
struct PinContext {
	PIN_THREAD_UID tid;
	Persistent<Context> context;
	enum ContextState state;
	PIN_MUTEX lock;
	PIN_SEMAPHORE state_changed;

	PinContext(PIN_THREAD_UID _tid) {
		PIN_MutexInit(&lock);
		PIN_SemaphoreInit(&state_changed);
		PIN_SemaphoreClear(&state_changed);
		state = NEW_CONTEXT;
		context.Clear();
		tid = _tid;
	}
	~PinContext() {
		PIN_MutexFini(&lock);
		PIN_SemaphoreFini(&state_changed);
	}
};
typedef VOID ENSURE_CALLBACK_FUNC(PinContext * arg);
struct EnsureCallback {
	PIN_THREAD_UID tid;
	ENSURE_CALLBACK_FUNC *callback;
	bool create;
};

typedef std::map<PIN_THREAD_UID, PinContext *> ContextsMap;
extern ContextsMap contexts;
extern PIN_RWMUTEX contexts_lock;
extern PIN_SEMAPHORE contexts_changed;
extern bool kill_contexts;
extern PIN_THREAD_UID context_manager_tid;
extern PIN_SEMAPHORE context_manager_ready;
static PinContext * const INVALID_PIN_CONTEXT = reinterpret_cast<PinContext *>(0);
static PinContext * const EXISTS_PIN_CONTEXT = reinterpret_cast<PinContext *>(-1);
static PinContext * const NO_MANAGER_CONTEXT = reinterpret_cast<PinContext *>(-2);
extern ofstream OutFile;
extern ofstream DebugFile;

PinContext *EnsurePinContext(PIN_THREAD_UID tid, bool create = true);
bool EnsurePinContextCallback(PIN_THREAD_UID tid, bool create, ENSURE_CALLBACK_FUNC *callback);
inline bool EnsurePinContextCallback(PIN_THREAD_UID tid, ENSURE_CALLBACK_FUNC *callback) {
	return EnsurePinContextCallback(tid, true, callback);
}
bool InitializePinContexts();
void PinContextManager(void *notused);
bool inline IsValidContext(PinContext *context) { return !(context == INVALID_PIN_CONTEXT || context == EXISTS_PIN_CONTEXT || context == NO_MANAGER_CONTEXT); }
