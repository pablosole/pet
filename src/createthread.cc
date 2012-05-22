#include "pin.H"

namespace WINDOWS {
#include <windows.h>
}

typedef WINDOWS::HANDLE (WINAPI *MyCreateThread_type)(
        WINDOWS::LPSECURITY_ATTRIBUTES lpThreadAttributes,
        WINDOWS::SIZE_T dwStackSize,
		WINDOWS::LPTHREAD_START_ROUTINE lpStartAddress,
		WINDOWS::LPVOID lpParameter,
        WINDOWS::DWORD dwCreationFlags,
        WINDOWS::LPDWORD lpThreadId
  );

extern "C" {
_declspec(dllexport) WINDOWS::HANDLE WINAPI CreateThread(
        WINDOWS::LPSECURITY_ATTRIBUTES lpThreadAttributes,
        WINDOWS::SIZE_T dwStackSize,
		WINDOWS::LPTHREAD_START_ROUTINE lpStartAddress,
		WINDOWS::LPVOID lpParameter,
        WINDOWS::DWORD dwCreationFlags,
        WINDOWS::LPDWORD lpThreadId
  ) {
	  MyCreateThread_type MyCreateThread;

	  //XXX: maintain updated with the name of the pintool DLL
	  WINDOWS::HMODULE v8 = WINDOWS::GetModuleHandle("pet.dll");
	  if (!v8)
		  return 0;

	  MyCreateThread = (MyCreateThread_type) WINDOWS::GetProcAddress(v8, "MyCreateThread");
	  if (!MyCreateThread)
		  return 0;

	  return MyCreateThread(lpThreadAttributes, dwStackSize, lpStartAddress, lpParameter, dwCreationFlags, lpThreadId);
}
}
