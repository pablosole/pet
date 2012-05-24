// Platform specific code for Win32 on PIN.

#ifdef TARGET_WINDOWS

#include "pin.H"
#undef ASSERT
#undef LOG
#define WINDOWS_H_IN_NAMESPACE

#include "v8.h"

#include "codegen.h"
#include "platform.h"
#include "vm-state-inl.h"
#include <process.h>  // For _beginthreadex().

namespace v8 {
namespace internal {

// Returns the accumulated user time for thread.
int OS::GetUserTime(uint32_t* secs,  uint32_t* usecs) {
  *secs = 0;
  *usecs = 0;
  return 0;
}

void OS::Sleep(int milliseconds) {
  PIN_Sleep(milliseconds);
}

// Definition of invalid thread handle and id.
static const THREADID kNoThread = INVALID_THREADID;

// Entry point for threads. The supplied argument is a pointer to the thread
// object. The entry function dispatches to the run method in the thread
// object. It is important that this function has __stdcall calling
// convention.
static unsigned int __stdcall ThreadEntry(void* arg) {
  Thread* thread = reinterpret_cast<Thread*>(arg);
  thread->Run();
  return 0;
}

//Thread handling
class Thread::PlatformData : public Malloced {
 public:
  explicit PlatformData(THREADID thread) : thread_(thread) {}
  THREADID thread_;
  PIN_THREAD_UID thread_id_;
};

Thread::Thread(const Options& options)
    : stack_size_(options.stack_size()) {
  data_ = new PlatformData(kNoThread);
  set_name(options.name());
}

Thread::~Thread() {
  delete data_;
}

void Thread::Start() {
  data_->thread_ = _beginthreadex(NULL,
                     static_cast<unsigned>(stack_size_),
                     ThreadEntry,
                     this,
                     0,
                     reinterpret_cast<unsigned int *>(&data_->thread_id_));
}

// Wait for thread to terminate.
void Thread::Join() {
  if (data_->thread_id_ != PIN_ThreadUid()) {
    PIN_WaitForThreadTermination(data_->thread_id_, PIN_INFINITE_TIMEOUT, NULL);
  }
}

void Thread::YieldCPU() {
  PIN_Yield();
}

class Win32PinSemaphore : public Semaphore {
 public:
  explicit Win32PinSemaphore(int count) {
    // There's only uses where count is 0 or 1, support those.
    PIN_SemaphoreInit(&sem);
	if (count)
		PIN_SemaphoreSet(&sem);
  }

  ~Win32PinSemaphore() {
	  PIN_SemaphoreFini(&sem);
  }

  void Wait() {
    PIN_SemaphoreWait(&sem);
  }

  bool Wait(int timeout) {
    // Timeout in Windows API is in milliseconds.
    DWORD millis_timeout = timeout / 1000;
	return PIN_SemaphoreTimedWait(&sem, millis_timeout);
  }

  void Signal() {
    PIN_SemaphoreSet(&sem);
  }

 private:
  PIN_SEMAPHORE sem;
};

Semaphore* OS::CreateSemaphore(int count) {
  return new Win32PinSemaphore(count);
}

} }  // namespace v8::internal

#undef WINDOWS_H_IN_NAMESPACE
#endif // TARGET_WINDOWS
