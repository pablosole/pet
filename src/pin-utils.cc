#include "pintool.h"

//This needs to be exported from the main dll that pin uses. Calling any PIN API is not allowed from a third party DLL.
//We trick the MSVC linker to override CreateThread with our own version of it (only in the pintool), provided in a separate
//project (and a separate dll actually) called pin_threading. All it does is to find this export and call to it.
//Notice that PIN_SpawnInternalThread doesn't support all options that CreateThread does, but for our use (to support _beginthreadex)
//is enough.
WINDOWS::HANDLE WINAPI MyCreateThread(
			WINDOWS::LPSECURITY_ATTRIBUTES lpThreadAttributes,
			WINDOWS::SIZE_T dwStackSize,
			ROOT_THREAD_FUNC lpStartAddress,
			WINDOWS::LPVOID lpParameter,
			WINDOWS::DWORD dwCreationFlags,
			WINDOWS::LPDWORD lpThreadId
	  )
{
	THREADID ret = (PIN_SpawnInternalThread((lpStartAddress),
					lpParameter,
					dwStackSize,
					reinterpret_cast<PIN_THREAD_UID *>(lpThreadId)));

	return (ret == INVALID_THREADID) ? 0 : reinterpret_cast<WINDOWS::HANDLE>(ret);
}

void KillPinTool()
{
	//this is the last thing our pintool will be able to do.
	OutFile.close();
	DebugFile.close();
	PIN_ExitProcess(0);
}

EXCEPT_HANDLING_RESULT InternalExceptionHandler(THREADID tid, EXCEPTION_INFO *pExceptInfo, PHYSICAL_CONTEXT *pPhysCtxt, VOID *v)
{
	DEBUG("Uncaught internal exception on tid: " << tid);
	KillPinTool();

	return EHR_UNHANDLED;
}
