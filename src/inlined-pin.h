#if defined(FROM_SERIALIZE_CC) && !defined(PIN_FUN_SCC)
#define PIN_FUN_SCC(NAME, ARGS, RETARGS, APINUM, F) Add(ExternalReference::NAME##_function(isolate).address(), RUNTIME_ENTRY, APINUM, #NAME);
#endif
#if defined(FROM_HYDROGEN_CC) && !defined(PIN_FUN_HCC)
#define PIN_FUN_HCC(NAME, ARGS, RETARGS, APINUM, F) \
	void HGraphBuilder::Generate##NAME(CallRuntime* call) { \
	return Bailout("inlined runtime function: " #NAME); \
	} \

#endif
#if defined(FROM_RUNTIME_H) && !defined(PIN_FUN_RUH)
#define PIN_FUN_RUH(NAME, ARGS, RETARGS, APINUM, F) F(NAME, ARGS, RETARGS)
#endif
#if defined(FROM_ASSEMBLER_CC) && !defined(PIN_FUN_ACC)
#define PIN_FUN_ACC(NAME, ARGS, RETARGS, APINUM, F) ExternalReference ExternalReference::NAME##_function(Isolate* isolate) { \
	return ExternalReference(Redirect(isolate, FUNCTION_ADDR(NAME))); \
} \

#endif
#if defined(FROM_ASSEMBLER_H) && !defined(PIN_FUN_ASH)
#define PIN_FUN_ASH(NAME, ARGS, RETARGS, APINUM, F) static ExternalReference NAME##_function(Isolate* isolate);
#endif
#ifndef PIN_DECLARATIONS



/* Add all function declarations here */
/* name, args, args returned, API number for the ExternalReference (keep in sync with +1 the latest RUNTIME_ENTRY in serialize.cc) */
#define PIN_DECLARATIONS(F, PIN_FUN) \
	PIN_FUN(PIN_SemaphoreInit, 1, 1, 8, F) \
	PIN_FUN(PIN_SemaphoreFini, 1, 0, 9, F) \
	PIN_FUN(PIN_SemaphoreIsSet, 1, 1, 10, F) \
	PIN_FUN(PIN_SemaphoreSet, 1, 0, 11, F) \
	PIN_FUN(PIN_SemaphoreWait, 1, 0, 12, F) \
	PIN_FUN(PIN_SemaphoreTimedWait, 2, 1, 13, F) \
	PIN_FUN(PIN_SemaphoreClear, 1, 0, 14, F) \
	PIN_FUN(PIN_GetPid, 0, 1, 15, F) \
	PIN_FUN(PIN_Yield, 0, 0, 16, F) \
	PIN_FUN(PIN_Sleep, 1, 0, 17, F) \
	PIN_FUN(PIN_GetTid, 0, 1, 18, F) \
	PIN_FUN(PIN_ThreadId, 0, 1, 19, F) \
	PIN_FUN(PIN_ThreadUid, 0, 1, 20, F) \
	PIN_FUN(PIN_IsProcessExiting, 0, 1, 21, F) \
	PIN_FUN(PIN_ExitProcess, 1, 0, 22, F) \
	PIN_FUN(PIN_ExitApplication, 1, 0, 23, F) \
	PIN_FUN(IMG_Invalid, 0, 1, 24, F) \
	PIN_FUN(APP_ImgHead, 0, 1, 25, F) \
	PIN_FUN(APP_ImgTail, 0, 1, 26, F) \
	PIN_FUN(IMG_Prev, 1, 1, 27, F) \
	PIN_FUN(IMG_Next, 1, 1, 28, F) \
	PIN_FUN(IMG_Name, 1, 1, 29, F) \
	PIN_FUN(IMG_Entry, 1, 1, 30, F) \
	PIN_FUN(IMG_LoadOffset, 1, 1, 31, F) \
	PIN_FUN(IMG_FindByAddress, 1, 1, 32, F) \
	PIN_FUN(IMG_FindImgById, 1, 1, 33, F) \
	PIN_FUN(IMG_Open, 1, 1, 34, F) \
	PIN_FUN(IMG_Close, 1, 0, 35, F) \
	PIN_FUN(IMG_SecHead, 1, 1, 36, F) \
	PIN_FUN(IMG_SecTail, 1, 1, 37, F) \
	PIN_FUN(SEC_Invalid, 0, 1, 38, F) \
	PIN_FUN(SEC_Prev, 1, 1, 39, F) \
	PIN_FUN(SEC_Next, 1, 1, 40, F) \
	PIN_FUN(SEC_Name, 1, 1, 41, F) \
	PIN_FUN(SYM_Invalid, 0, 1, 42, F) \
	PIN_FUN(SYM_Prev, 1, 1, 43, F) \
	PIN_FUN(SYM_Next, 1, 1, 44, F) \
	PIN_FUN(SYM_Name, 1, 1, 45, F) \
	PIN_FUN(IMG_RegsymHead, 1, 1, 46, F) \
	PIN_FUN(SEC_RtnHead, 1, 1, 47, F) \
	PIN_FUN(SEC_RtnTail, 1, 1, 48, F) \
	PIN_FUN(RTN_Invalid, 0, 1, 49, F) \
	PIN_FUN(RTN_Prev, 1, 1, 50, F) \
	PIN_FUN(RTN_Next, 1, 1, 51, F) \
	PIN_FUN(RTN_Name, 1, 1, 52, F) \
	PIN_FUN(RTN_InsHead, 1, 1, 53, F) \
	PIN_FUN(RTN_InsHeadOnly, 1, 1, 54, F) \
	PIN_FUN(RTN_InsTail, 1, 1, 55, F) \
	PIN_FUN(INS_Invalid, 0, 1, 56, F) \
	PIN_FUN(INS_Prev, 1, 1, 57, F) \
	PIN_FUN(INS_Next, 1, 1, 58, F) \
	PIN_FUN(INS_Disassemble, 2, 0, 59, F) \
	PIN_FUN(RTN_Open, 1, 0, 60, F) \
	PIN_FUN(RTN_Close, 1, 0, 61, F) \
	PIN_FUN(RTN_FindNameByAddress, 2, 0, 62, F) \
	PIN_FUN(RTN_FindByAddress, 1, 1, 63, F) \
	PIN_FUN(INS_Address, 1, 1, 64, F) \
	PIN_FUN(INS_Size, 1, 1, 65, F) \
	PIN_FUN(RTN_CreateAt, 2, 1, 66, F) \
	PIN_FUN(PIN_SaveContext, 2, 0, 67, F) \
	PIN_FUN(PIN_ContextContainsState, 2, 1, 68, F) \
	PIN_FUN(PIN_GetContextReg, 2, 1, 69, F) \
	PIN_FUN(PIN_SetContextReg, 3, 0, 70, F) \
	PIN_FUN(PIN_ClaimToolRegister, 0, 1, 71, F) \
	PIN_FUN(REG_FullRegName, 1, 1, 72, F) \
	PIN_FUN(REG_StringShort, 2, 0, 73, F) \
	PIN_FUN(TRACE_Rtn, 1, 1, 74, F) \
	PIN_FUN(TRACE_Address, 1, 1, 75, F) \
	PIN_FUN(TRACE_Size, 1, 1, 76, F) \
	PIN_FUN(PIN_RemoveInstrumentation, 0, 0, 77, F) \
	PIN_FUN(PIN_WaitForThreadTermination, 2, 1, 78, F) \
	




#endif
#if defined(FROM_RUNTIME_H) && !defined(INLINE_FUNCTION_LIST_PIN)
#define INLINE_FUNCTION_LIST_PIN(F) PIN_DECLARATIONS(F, PIN_FUN_RUH)
#endif
#if defined(FROM_SERIALIZE_CC)
PIN_DECLARATIONS(0, PIN_FUN_SCC)
#endif
#if defined(FROM_HYDROGEN_CC)
PIN_DECLARATIONS(0, PIN_FUN_HCC)
#endif
#if defined(FROM_ASSEMBLER_CC)
PIN_DECLARATIONS(0, PIN_FUN_ACC)
#endif
#if defined(FROM_ASSEMBLER_H)
PIN_DECLARATIONS(0, PIN_FUN_ASH)
#endif
