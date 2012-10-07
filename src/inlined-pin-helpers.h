#if defined(FROM_HYDROGEN_CC) && !defined(PIN_FUN_HCC_HELPERS)
#define PIN_FUN_HCC_HELPERS(NAME, ARGS, RETARGS, F) \
	void HGraphBuilder::Generate##NAME(CallRuntime* call) { \
	return Bailout("inlined runtime function: " #NAME); \
	} \

#endif
#if defined(FROM_RUNTIME_H) && !defined(PIN_FUN_RUH_HELPERS)
#define PIN_FUN_RUH_HELPERS(NAME, ARGS, RETARGS, F) F(NAME, ARGS, RETARGS)
#endif
#ifndef PIN_DECLARATIONS_HELPERS



/* Add all function declarations here */
/* name, args, args returned, API number for the ExternalReference (keep in sync with +1 the latest RUNTIME_ENTRY in serialize.cc) */
#define PIN_DECLARATIONS_HELPERS(F, PIN_FUN) \
	PIN_FUN(JSStringFromCString, 1, 1, F) \
	PIN_FUN(ReturnContext, 1, 0, F) \
	PIN_FUN(WriteDword, 2, 0, F) \
	PIN_FUN(ReadDword, 1, 1, F) \
	PIN_FUN(WriteWord, 2, 0, F) \
	PIN_FUN(ReadWord, 1, 1, F) \
	PIN_FUN(WriteByte, 2, 0, F) \
	PIN_FUN(ReadByte, 1, 1, F) \
	PIN_FUN(WriteDouble, 2, 0, F) \
	PIN_FUN(ReadDouble, 1, 1, F) \
	PIN_FUN(ReadAsciiString, 2, 1, F) \
	PIN_FUN(ReadTwoByteString, 2, 1, F) \
	PIN_FUN(LoadPointer, 1, 1, F) \
	PIN_FUN(StorePointer, 2, 0, F) \



#endif
#if defined(FROM_RUNTIME_H) && !defined(INLINE_FUNCTION_LIST_PIN_HELPERS)
#define INLINE_FUNCTION_LIST_PIN_HELPERS(F) PIN_DECLARATIONS_HELPERS(F, PIN_FUN_RUH_HELPERS)
#endif
#if defined(FROM_HYDROGEN_CC)
PIN_DECLARATIONS_HELPERS(0, PIN_FUN_HCC_HELPERS)
#endif
