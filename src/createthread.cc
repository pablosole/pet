// Platform specific code for Win32 on PIN.

#ifdef TARGET_WINDOWS

#include "pin.H"
#undef ASSERT
#undef LOG

namespace WINDOWS {
#include <windows.h>
}

typedef WINDOWS::HANDLE (WINAPI *MyCreateThread_type)(
        WINDOWS::LPSECURITY_ATTRIBUTES lpThreadAttributes,
        WINDOWS::SIZE_T dwStackSize,
		ROOT_THREAD_FUNC lpStartAddress,
		WINDOWS::LPVOID lpParameter,
        WINDOWS::DWORD dwCreationFlags,
        WINDOWS::LPDWORD lpThreadId
  );

extern "C" {
_declspec(dllexport) WINDOWS::HANDLE WINAPI CreateThread(
        WINDOWS::LPSECURITY_ATTRIBUTES lpThreadAttributes,
        WINDOWS::SIZE_T dwStackSize,
		ROOT_THREAD_FUNC lpStartAddress,
		WINDOWS::LPVOID lpParameter,
        WINDOWS::DWORD dwCreationFlags,
        WINDOWS::LPDWORD lpThreadId
  ) {
	  MyCreateThread_type MyCreateThread;
	  WINDOWS::HMODULE v8 = WINDOWS::GetModuleHandle("v8.dll");
	  if (!v8)
		  return 0;

	  MyCreateThread = (MyCreateThread_type) WINDOWS::GetProcAddress(v8, "MyCreateThread");
	  if (!MyCreateThread)
		  return 0;

	  return MyCreateThread(lpThreadAttributes, dwStackSize, lpStartAddress, lpParameter, dwCreationFlags, lpThreadId);
}
}

#endif
