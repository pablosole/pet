#define FUN(NAME) void FullCodeGenerator::Emit##NAME(CallRuntime* expr)

/* Add all function definitions here */
FUN(PIN_GetPid)
{
  ZoneList<Expression*>* args = expr->arguments();
  ASSERT_EQ(0, args->length());

  const int argument_count = 0;
  {
	AllowExternalCallThatCantCauseGC scope(masm_);
	__ PrepareCallCFunction(argument_count, ebx);
	__ CallCFunction(ExternalReference::PIN_GetPid_function(isolate()), argument_count);
  }

  __ SmiTag(eax);  //transform output into a tagged small integer
  context()->Plug(eax);
}

#undef FUN