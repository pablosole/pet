ofstream OutFile;
ofstream DebugFile;

Persistent<Context> default_context;
ContextsMap contexts;
PIN_MUTEX contexts_lock;
PIN_SEMAPHORE contexts_changed;
bool kill_contexts = false;
THREADID context_manager_tid;
PIN_SEMAPHORE context_manager_ready;
TLS_KEY per_thread_context_key;
