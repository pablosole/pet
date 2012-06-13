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
	/* Process API */ \
	PIN_FUN(PIN_GetPid, 0, 1, 8, F)




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
