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

WINDOWS::HANDLE WINAPI MyCreateThread(
			WINDOWS::LPSECURITY_ATTRIBUTES lpThreadAttributes,
			WINDOWS::SIZE_T dwStackSize,
			ROOT_THREAD_FUNC lpStartAddress,
			WINDOWS::LPVOID lpParameter,
			WINDOWS::DWORD dwCreationFlags,
			WINDOWS::LPDWORD lpThreadId
	  )
{
	  return reinterpret_cast<WINDOWS::HANDLE> (PIN_SpawnInternalThread((lpStartAddress),
					lpParameter,
					dwStackSize,
					reinterpret_cast<PIN_THREAD_UID *>(lpThreadId)));
}


int main(int argc, char * argv[])
{
	LOG_PIN("before PIN_INIT\n");
    // Initialize pin
    if (PIN_Init(argc, argv)) return -1;

	//Must be executed before ANY v8 function
	internal::InitializeIsolation();

	// Create a stack-allocated handle scope.
	HandleScope handle_scope;

	// Create a new context.
	Persistent<Context> context = Context::New();
	

	LOG_PIN("before StartProgram\n");
    // Start the program, never returns
    PIN_StartProgram();
    
    return 0;
}
