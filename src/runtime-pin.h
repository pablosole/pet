#ifdef TARGET_WINDOWS
/* list of Runtime Functions (this ones can create and return objects) */

#define RUNTIME_FUNCTION_LIST_PIN(F) \
  F(WrapPtr, 1, 1)
#else
#define RUNTIME_FUNCTION_LIST_PIN(F)
#endif
