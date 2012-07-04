#include "pin.H"
#undef LOG
#undef ASSERT
#define LOG_PIN(message) QMESSAGE(MessageTypeLog, message)
#define ASSERT_PIN(condition,message)   \
    do{ if(!(condition)) \
        ASSERTQ(LEVEL_BASE::AssertString(PIN_ASSERT_FILE, __FUNCTION__, __LINE__, std::string("") + message));} while(0)

#undef USING_V8_SHARED
#define WINDOWS_H_IN_NAMESPACE
#include "v8.h"

using namespace v8;
namespace i = v8::internal;
#include <map>
#include <iostream>
#include <fstream>
#include <sstream>
#include <list>
#include <iomanip>

#define DEBUG(m) do { \
DebugFile << m << endl; \
cerr << m << endl; \
DebugFile.flush(); \
cerr.flush(); \
} while (false);

#define DEBUG_NO_NL(m) do { \
DebugFile << m; \
cerr << m; \
} while (false);

#define CACHE_LINE  64
#define CACHE_ALIGN __declspec(align(CACHE_LINE))

class PinContext;
class AnalysisFunction;
class ContextManager;

//Use 4 bits for TID and 8 bits for function id
//That give us a fast access cache for the first 256 functions over the first 16 threads.
const int kFastCacheMaxTid = 15;
const int kFastCacheMaxFuncId = 255;
const int kFastCacheTidShift = 8;
const int kFastCacheSize = (kFastCacheMaxFuncId | (kFastCacheMaxTid << kFastCacheTidShift)) + 1;
const unsigned int kInvalidFastCacheIndex = -1;
extern Persistent<Function> AnalysisFunctionFastCache[kFastCacheSize];

typedef VOID ENSURE_CALLBACK_FUNC(PinContext * arg, VOID *v);
struct EnsureCallback {
	THREADID tid;
	ENSURE_CALLBACK_FUNC *callback;
	bool create;
	VOID *data;
};

PinContext * const INVALID_PIN_CONTEXT = reinterpret_cast<PinContext *>(0);
PinContext * const NO_MANAGER_CONTEXT = reinterpret_cast<PinContext *>(-1);

//global accessible data and functions
extern ofstream OutFile;
extern ofstream DebugFile;
extern ContextManager *ctxmgr;

VOID AddGenericInstrumentation(VOID *);
void KillPinTool();
void ReportException (TryCatch* try_catch);
const string ReportExceptionToString(TryCatch* try_catch);
Handle<Value> evalOnContext(Isolate *isolate_src, Handle<Context> context_src, Isolate *isolate_dst, Handle<Context> context_dst, const string& source);

//we associate each PinContext with an application thread.
typedef std::map<uint32_t, Persistent<Function>> FunctionsCacheMap;
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

	ContextState inline GetState() { return state; }
	void inline SetState(ContextState s) { state = s; }

	void inline Lock() { PIN_MutexLock(&lock); }
	void inline Unlock() { PIN_MutexUnlock(&lock); }
	void inline Wait() {
		PIN_SemaphoreWait(&changed);
		PIN_SemaphoreClear(&changed);
	}
	void inline WaitIfNotReady() {
		if (GetState() == NEW_CONTEXT)
			Wait();
	}
	void inline Notify() { PIN_SemaphoreSet(&changed); }

	THREADID inline GetTid() { return tid; }

	bool CreateJSContext();
	void DestroyJSContext();

	static VOID ThreadInfoDestructor(VOID *);

	static bool inline IsValid(PinContext *context) { return !(context == 0 || context == INVALID_PIN_CONTEXT || context == NO_MANAGER_CONTEXT); }

	inline Isolate *GetIsolate() { return isolate; }
	inline Persistent<Context> &GetContext() { return context; }
	inline Persistent<Object> &GetGlobal() { return global; }
	Persistent<Function> PinContext::EnsureFunction(AnalysisFunction *af);
	Persistent<Function> PinContext::EnsureFunctionSlow(AnalysisFunction *af);
	inline unsigned int GetFastCacheIndex(unsigned int funcId) {
		if (funcId > kFastCacheMaxFuncId || GetTid() > kFastCacheMaxTid)
			return kInvalidFastCacheIndex;
		return funcId | (GetTid() << kFastCacheTidShift);
	}

private:
	Isolate *isolate;
	Persistent<Context> context;
	Persistent<Object> global;
	THREADID tid;
	enum ContextState state;
	PIN_MUTEX lock;
	PIN_SEMAPHORE changed;
	FunctionsCacheMap funcache;
};


typedef std::map<THREADID, PinContext *> ContextsMap;
typedef std::map<unsigned int, AnalysisFunction *> FunctionsMap;
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

	REG inline GetPerThreadContextReg() { return per_thread_context_reg; }
	REGSET inline GetPreservedRegset() { return preserved_regs; }

	void inline SetPerformanceCounter() { WINDOWS::QueryPerformanceCounter(&performancecounter_start); }
	bool GetPerformanceCounterDiff(WINDOWS::LARGE_INTEGER *out);

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
	void inline LockFunctions() { PIN_RWMutexWriteLock(&lock_functions); }
	void inline UnlockFunctions() { PIN_RWMutexUnlock(&lock_functions); }
	void inline LockFunctionsRead() { PIN_RWMutexReadLock(&lock_functions); }


	//Context Manager thread function
	static VOID Run(VOID *);

	//Helper functions
	void ProcessCallbacks();
	void ProcessChanges();
	void KillAllContexts();
	PinContext *CreateContext(THREADID tid, BOOL waitforit = true);
	static VOID ProcessCallbacksHelper(VOID *v);

	//API
	bool EnsurePinContextCallback(THREADID tid, ENSURE_CALLBACK_FUNC *callback, VOID *v, bool create = true);
	PinContext *EnsurePinContext(THREADID tid, bool create = true);

	//for fast access, this should be equivalent to: TlsGetValue(per_thread_context)
	inline PinContext *LoadPinContext(THREADID tid) {
		return EnsurePinContext(tid, false);
	}
	inline PinContext *LoadPinContext() { return LoadPinContext(PIN_ThreadId()); }

	//AnalysisFunction API
	AnalysisFunction *AddFunction(const string& body, const string& init=string());
	bool RemoveFunction(unsigned int funcId);
	AnalysisFunction *ContextManager::GetFunction(unsigned int funcId);

	//Global and SharedData context manager functions
	inline Handle<Context> GetDefaultContext() { return default_context; }
	inline Handle<Context> GetSharedDataContext() { return shareddata_context; }
	inline Isolate *GetDefaultIsolate() { return default_isolate; }

 private:
	ContextManagerState state;
	Persistent<Context> default_context;
	Persistent<Context> shareddata_context;
	Isolate *default_isolate;
	ContextsMap contexts;
	list<EnsureCallback *> callbacks;
	FunctionsMap functions;
	PIN_MUTEX lock;
	PIN_MUTEX lock_callbacks;
	PIN_RWMUTEX lock_functions;
	PIN_SEMAPHORE changed;
	THREADID tid;
	PIN_SEMAPHORE ready;
	TLS_KEY per_thread_context_key;
	unsigned int last_function_id;

	//Scratch register reserved for PinContext passing for the analysis functions.
	//make it static to avoid an extra deref.
	REG per_thread_context_reg;

	//Set of registers known to be preserved by our pintools, the more we can put here the better.
	REGSET preserved_regs;

	WINDOWS::LARGE_INTEGER performancecounter_start;
};

class CACHE_ALIGN AnalysisFunction {
public:
	AnalysisFunction(const string& _body, const string& _init = string()) :
		num_args(0), 
		enabled(true),
		num_exceptions(0),
		exception_threshold(10),
		args_fixed(false),
		init(_init)
	{
		arguments = IARGLIST_Alloc();
		SetBody(_body);
	}

	~AnalysisFunction()
	{
		IARGLIST_Free(arguments);
	}

	inline unsigned int GetFuncId() { return funcId; }
	inline void SetFuncId(unsigned int f) { funcId = f; }
	inline unsigned int GetHash() { return hash; }
	uint32_t HashBody();
	inline const string& GetBody() { return body; }
	inline void SetBody(const string& _body) {
		body = _body;
		HashBody();
	}
	inline const string& GetInit() { return init; }
	inline void SetInit(const string& _init) { init = _init; }
	inline const string& GetLastException() { return last_exception; }
	inline void SetLastException(const string& _last_exc) { last_exception = _last_exc; }

	inline void Enable() { enabled = true; }
	inline void Disable() { enabled = false; }
	inline bool IsEnabled() { return enabled; }

	inline void SetThreshold(uint32_t x) { exception_threshold = x; }
	inline uint32_t GetThreshold() { return exception_threshold; }

	inline void IncException() { ++num_exceptions; }
	inline uint32_t GetNumExceptions() { return num_exceptions; }
	inline void ResetNumExceptions() { num_exceptions = 0; }

	inline bool ArgsAreFixed() { return args_fixed; }
	inline void FixArgs() { args_fixed = true; }

	inline uint32_t GetArgumentCount() { return num_args; }
	IARGLIST GetArguments() { return arguments; }

	template <class T>
	void AddArgument(IARG_TYPE arg, T argvalue) {
		if (!ArgsAreFixed()) {
			IARGLIST_AddArguments(arguments, arg, argvalue, IARG_END);
			++num_args;
		}
	}
	void AddArgument(IARG_TYPE arg) {
		if (!ArgsAreFixed()) {
			IARGLIST_AddArguments(arguments, arg, IARG_END);
			++num_args;
		}
	}

private:
	unsigned int funcId;
	unsigned int hash;
	string body;
	string init;
	IARGLIST arguments;
	uint32_t num_args;
	bool args_fixed;
	bool enabled;
	uint32_t num_exceptions;
	uint32_t exception_threshold;
	string last_exception;
};

