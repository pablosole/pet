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
#include <sorrow.h>
using namespace sorrow;

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

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

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
extern ofstream DebugFile;
extern ContextManager *ctxmgr;

//Routine, Trace and Instruction fast boolean checks for instrumentation
extern uint32_t routine_instrumentation_enabled;
extern Persistent<Function> routine_function;
extern uint32_t trace_instrumentation_enabled;
extern Persistent<Function> trace_function;
extern uint32_t ins_instrumentation_enabled;
extern Persistent<Function> ins_function;
extern Isolate *main_ctx_isolate;
extern Persistent<Context> main_ctx;
extern Persistent<Object> main_ctx_global;

void KillPinTool();
const char* ToCString(const v8::String::Utf8Value& value);
void ReportException (TryCatch* try_catch);
const string ReportExceptionToString(TryCatch* try_catch);
Handle<Value> evalOnContext(Isolate *isolate_src, Handle<Context> context_src, Isolate *isolate_dst, Handle<Context> context_dst, const string& source);
Handle<Value> evalOnContext(PinContext *pincontext_src, PinContext *pincontext_dst, const string& source);
Handle<Value> evalOnDefaultContext(PinContext *context_src, const string& source);
void forceGarbageCollection();
size_t convertToUint(Local<Value> value_in, TryCatch* try_catch);
uint32_t NumberToUint32(Local<Value> value_in, TryCatch* try_catch);

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

	inline SorrowContext *GetSorrowContext() { return sorrowctx; }
	inline Isolate *GetIsolate() { return isolate; }
	inline Persistent<Context> &GetContext() { return context; }
	inline Persistent<Object> &GetGlobal() { return global; }
	Persistent<Function> PinContext::EnsureFunction(AnalysisFunction *af);
	Persistent<Function> PinContext::EnsureFunctionSlow(AnalysisFunction *af);
	inline unsigned int GetFastCacheIndex(unsigned int funcId) {
		return GetFastCacheIndex(funcId, GetTid());
	}
	static inline unsigned int GetFastCacheIndex(unsigned int funcId, unsigned int tid) {
		if (funcId > kFastCacheMaxFuncId || tid > kFastCacheMaxTid)
			return kInvalidFastCacheIndex;
		return funcId | (tid << kFastCacheTidShift);
	}

private:
	Isolate *isolate;
	Persistent<Context> context;
	Persistent<Object> global;
	SorrowContext *sorrowctx;
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
	inline bool IsDieing() { return state == KILLING_MANAGER; }
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
	inline uint32_t IsInstrumentationEnabled(uint32_t idx) { return (instrumentation_flags & (1 << idx)) != 0; }
	inline void EnableInstrumentation(uint32_t idx) { instrumentation_flags |= 1 << idx; }

	//for fast access, this should be equivalent to: TlsGetValue(per_thread_context)
	inline PinContext *LoadPinContext(THREADID tid) {
		return EnsurePinContext(tid, false);
	}
	inline PinContext *LoadPinContext() { return LoadPinContext(PIN_ThreadId()); }

	//AnalysisFunction API
	AnalysisFunction *AddFunction(const string& body, const string& init=string(), const string& dtor=string());
	bool RemoveFunction(unsigned int funcId);
	AnalysisFunction *GetFunction(unsigned int funcId);
	inline FunctionsMap &GetFunctionsMap() { return functions; }
	inline unsigned int GetLastFunctionId() { return last_function_id; }

	//Global and SharedData context manager functions
	inline Handle<Context> GetDefaultContext() { return default_context; }
	inline Handle<Context> GetSharedDataContext() { return shareddata_context; }
	inline Isolate *GetDefaultIsolate() { return default_isolate; }
	inline SorrowContext *GetSorrowContext() { return sorrrowctx; }
	void ContextManager::InitializeSorrowContext(int argc, const char *argv[]);

 private:
	ContextManagerState state;
	Persistent<Context> default_context;
	Persistent<Context> shareddata_context;
	SorrowContext *sorrrowctx;
	SorrowContext *sorrrowctx_shareddata;
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

	uint32_t instrumentation_flags;
};

class CACHE_ALIGN AnalysisFunction {
public:
	AnalysisFunction(const string& _body, const string& _init, const string& _dtor) :
		num_args(0), 
		enabled(true),
		num_exceptions(0),
		exception_threshold(10),
		args_fixed(false),
		init(_init),
		dtor(_dtor),
		arguments(0)
	{
		SetBody(_body);
	}

	~AnalysisFunction()
	{
	}

	inline unsigned int GetFuncId() { return funcId; }
	inline void SetFuncId(unsigned int f) { funcId = f; }
	inline unsigned int GetHash() { return hash; }
	uint32_t HashBody();
	inline const string& GetBody() { return body; }
	inline void SetBody(const string& _body) {
		body = _body;
		HashBody();

		if (GetFuncId() <= kFastCacheMaxFuncId) {
			for (int x=0; x <= kFastCacheMaxTid; x++) {
				Persistent<Function>& tmp = AnalysisFunctionFastCache[PinContext::GetFastCacheIndex(GetFuncId(), x)];
				if (!tmp.IsEmpty()) {
					tmp.Dispose();
					tmp.Clear();
				}
			}
		}
	}
	inline const string& GetDtor() { return dtor; }
	inline void SetDtor(const string& _dtor) { dtor = _dtor; }
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
	//XXX: If I implement PIN_RemoveInstrumentation I should make all AFs 'arguments R/W again.

	inline uint32_t GetArgumentCount() { return num_args; }
	IARGLIST GetArguments() { return arguments; }
	void SetArguments(IARGLIST args, uint32_t n) { arguments = args; num_args = n; }

private:
	unsigned int funcId;
	unsigned int hash;
	string body;
	string init;
	string dtor;
	IARGLIST arguments;
	uint32_t num_args;
	bool args_fixed;
	bool enabled;
	uint32_t num_exceptions;
	uint32_t exception_threshold;
	string last_exception;
};
