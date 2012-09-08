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
	PIN_FUN(PIN_ExitApplication, 1, 0, 23, F)




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
