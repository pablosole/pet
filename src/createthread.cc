#include "pin.H"

namespace WINDOWS {
#include <windows.h>
}

typedef VOID (WINAPI *MyExitThread_type)(WINDOWS::DWORD dwExitCode);

static MyExitThread_type MyExitThread = 0;

typedef WINDOWS::HANDLE (WINAPI *MyCreateThread_type)(
        WINDOWS::LPSECURITY_ATTRIBUTES lpThreadAttributes,
        WINDOWS::SIZE_T dwStackSize,
		WINDOWS::LPTHREAD_START_ROUTINE lpStartAddress,
		WINDOWS::LPVOID lpParameter,
        WINDOWS::DWORD dwCreationFlags,
        WINDOWS::LPDWORD lpThreadId
  );

static MyCreateThread_type MyCreateThread = 0;

extern "C" {
_declspec(dllexport) WINDOWS::HANDLE WINAPI CreateThread(
        WINDOWS::LPSECURITY_ATTRIBUTES lpThreadAttributes,
        WINDOWS::SIZE_T dwStackSize,
		WINDOWS::LPTHREAD_START_ROUTINE lpStartAddress,
		WINDOWS::LPVOID lpParameter,
        WINDOWS::DWORD dwCreationFlags,
        WINDOWS::LPDWORD lpThreadId
  ) {
	  return MyCreateThread(lpThreadAttributes, dwStackSize, lpStartAddress, lpParameter, dwCreationFlags, lpThreadId);
}

_declspec(dllexport) VOID WINAPI ExitThread(WINDOWS::DWORD dwExitCode) {
	MyExitThread(dwExitCode);
}

WINDOWS::BOOL WINAPI DllMain(WINDOWS::HANDLE hinstDLL, WINDOWS::DWORD dwReason, WINDOWS::LPVOID lpvReserved) {
	//XXX: maintain updated with the name of the pintool DLL
	WINDOWS::HMODULE v8 = WINDOWS::GetModuleHandle("pinocchio.dll");
	if (!v8)
		return 0;

	if (!MyCreateThread)
	{
		MyCreateThread = (MyCreateThread_type) WINDOWS::GetProcAddress(v8, "MyCreateThread");
		if (!MyCreateThread)
		  return 0;
	}

	if (!MyExitThread)
	{
		MyExitThread = (MyExitThread_type) WINDOWS::GetProcAddress(v8, "MyExitThread");
		if (!MyExitThread)
			return 0;
	}

	return 1;
}

}

