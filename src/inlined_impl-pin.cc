#define FUN(NAME) void FullCodeGenerator::Emit##NAME(CallRuntime* expr)

FUN(PIN_GetPid)
{
  const int argument_count = 0;
  __ PrepareCallCFunction(argument_count, ebx);
  __ CallCFunction(ExternalReference::PIN_GetPid_function(isolate()), argument_count);

  __ SmiTag(eax);  //transform output into a tagged small integer
  context()->Plug(eax);
}

FUN(PIN_GetTid)
{
  const int argument_count = 0;
  __ PrepareCallCFunction(argument_count, ebx);
  __ CallCFunction(ExternalReference::PIN_GetTid_function(isolate()), argument_count);

  __ SmiTag(eax);
  context()->Plug(eax);
}

FUN(PIN_ThreadId)
{
  const int argument_count = 0;
  __ PrepareCallCFunction(argument_count, ebx);
  __ CallCFunction(ExternalReference::PIN_ThreadId_function(isolate()), argument_count);

  __ SmiTag(eax);
  context()->Plug(eax);
}

FUN(PIN_ThreadUid)
{
  const int argument_count = 0;
  __ PrepareCallCFunction(argument_count, ebx);
  __ CallCFunction(ExternalReference::PIN_ThreadUid_function(isolate()), argument_count);

  __ SmiTag(eax);
  context()->Plug(eax);
}

FUN(PIN_IsProcessExiting)
{
	Label materialize_true, materialize_false;
	Label *if_true = NULL;
	Label *if_false = NULL;
	Label *fall_through = NULL;

	const int argument_count = 0;
	__ PrepareCallCFunction(argument_count, ebx);
	__ CallCFunction(ExternalReference::PIN_IsProcessExiting_function(isolate()), argument_count);

	context()->PrepareTest(&materialize_true, &materialize_false, &if_true, &if_false, &fall_through);
	PrepareForBailoutBeforeSplit(expr, true, if_true, if_false);
	__ test(eax, eax);
	Split(not_zero, if_true, if_false, fall_through);
	context()->Plug(if_true, if_false);
}

FUN(PIN_ExitProcess)
{
	ZoneList<Expression*>* args = expr->arguments();
	ASSERT(args->length == 1);

	VisitForAccumulatorValue(args->at(0));
	__ SmiUntag(eax);

	const int argument_count = 1;

	__ PrepareCallCFunction(argument_count, ebx);
	__ mov (Operand(esp, 0), eax);
	__ CallCFunction(ExternalReference::PIN_ExitProcess_function(isolate()), argument_count);
}

FUN(PIN_ExitApplication)
{
	ZoneList<Expression*>* args = expr->arguments();
	ASSERT(args->length == 1);

	VisitForAccumulatorValue(args->at(0));
	__ SmiUntag(eax);

	const int argument_count = 1;

	__ PrepareCallCFunction(argument_count, ebx);
	__ mov (Operand(esp, 0), eax);
	__ CallCFunction(ExternalReference::PIN_ExitApplication_function(isolate()), argument_count);
}

FUN(PIN_Yield)
{
	const int argument_count = 0;
	__ PrepareCallCFunction(argument_count, ebx);
	__ CallCFunction(ExternalReference::PIN_Yield_function(isolate()), argument_count);
}

FUN(PIN_Sleep)
{
	ZoneList<Expression*>* args = expr->arguments();
	ASSERT(args->length == 1);

	VisitForAccumulatorValue(args->at(0));
	__ SmiUntag(eax);

	const int argument_count = 1;

	__ PrepareCallCFunction(argument_count, ebx);
	__ mov (Operand(esp, 0), eax);
	__ CallCFunction(ExternalReference::PIN_Sleep_function(isolate()), argument_count);
}

//js side check that arg %_IsObject
FUN(PIN_SemaphoreInit)
{
	ZoneList<Expression*>* args = expr->arguments();
	ASSERT(args->length == 1);

	Label materialize_true, materialize_false;
	Label *if_true = NULL;
	Label *if_false = NULL;
	Label *fall_through = NULL;

	VisitForAccumulatorValue(args->at(0));
	__ UnwrapPointer(eax);

	const int argument_count = 1;

	__ PrepareCallCFunction(argument_count, ebx);
	__ mov (Operand(esp, 0), eax);
	__ CallCFunction(ExternalReference::PIN_SemaphoreInit_function(isolate()), argument_count);

	context()->PrepareTest(&materialize_true, &materialize_false, &if_true, &if_false, &fall_through);
	PrepareForBailoutBeforeSplit(expr, true, if_true, if_false);
	__ test(eax, eax);
	Split(not_zero, if_true, if_false, fall_through);
	context()->Plug(if_true, if_false);
}

FUN(PIN_SemaphoreFini)
{
	ZoneList<Expression*>* args = expr->arguments();
	ASSERT(args->length == 1);

	VisitForAccumulatorValue(args->at(0));
	__ UnwrapPointer(eax);

	const int argument_count = 1;

	__ PrepareCallCFunction(argument_count, ebx);
	__ mov (Operand(esp, 0), eax);
	__ CallCFunction(ExternalReference::PIN_SemaphoreFini_function(isolate()), argument_count);
}

FUN(PIN_SemaphoreIsSet)
{
	ZoneList<Expression*>* args = expr->arguments();
	ASSERT(args->length == 1);

	Label materialize_true, materialize_false;
	Label *if_true = NULL;
	Label *if_false = NULL;
	Label *fall_through = NULL;

	VisitForAccumulatorValue(args->at(0));
	__ UnwrapPointer(eax);

	const int argument_count = 1;

	__ PrepareCallCFunction(argument_count, ebx);
	__ mov (Operand(esp, 0), eax);

	__ CallCFunction(ExternalReference::PIN_SemaphoreIsSet_function(isolate()), argument_count);

	context()->PrepareTest(&materialize_true, &materialize_false, &if_true, &if_false, &fall_through);
	PrepareForBailoutBeforeSplit(expr, true, if_true, if_false);
	__ test(eax, eax);
	Split(not_zero, if_true, if_false, fall_through);
	context()->Plug(if_true, if_false);
}

FUN(PIN_SemaphoreSet)
{
	ZoneList<Expression*>* args = expr->arguments();
	ASSERT(args->length == 1);

	VisitForAccumulatorValue(args->at(0));
	__ UnwrapPointer(eax);

	const int argument_count = 1;

	__ PrepareCallCFunction(argument_count, ebx);
	__ mov (Operand(esp, 0), eax);
	__ CallCFunction(ExternalReference::PIN_SemaphoreSet_function(isolate()), argument_count);
}

FUN(PIN_SemaphoreWait)
{
	ZoneList<Expression*>* args = expr->arguments();
	ASSERT(args->length == 1);

	VisitForAccumulatorValue(args->at(0));
	__ UnwrapPointer(eax);

	const int argument_count = 1;

	__ PrepareCallCFunction(argument_count, ebx);
	__ mov (Operand(esp, 0), eax);
	__ CallCFunction(ExternalReference::PIN_SemaphoreWait_function(isolate()), argument_count);
}

FUN(PIN_SemaphoreTimedWait)
{
	ZoneList<Expression*>* args = expr->arguments();
	ASSERT(args->length == 2);

	Label materialize_true, materialize_false;
	Label *if_true = NULL;
	Label *if_false = NULL;
	Label *fall_through = NULL;

	VisitForStackValue(args->at(0));
	VisitForAccumulatorValue(args->at(1));
	__ pop(edx);
	__ UnwrapPointer(edx);
	__ SmiUntag(eax);

	const int argument_count = 2;

	__ PrepareCallCFunction(argument_count, ebx);
	__ mov (Operand(esp, kPointerSize * 0), edx);
	__ mov (Operand(esp, kPointerSize * 1), eax);
	__ CallCFunction(ExternalReference::PIN_SemaphoreTimedWait_function(isolate()), argument_count);

	context()->PrepareTest(&materialize_true, &materialize_false, &if_true, &if_false, &fall_through);
	PrepareForBailoutBeforeSplit(expr, true, if_true, if_false);
	//it sounds like a bug on PIN: FALSE == 0x100
	__ cmp(eax, Immediate(0x100));
	Split(not_equal, if_true, if_false, fall_through);
	context()->Plug(if_true, if_false);
}

FUN(PIN_SemaphoreClear)
{
	ZoneList<Expression*>* args = expr->arguments();
	ASSERT(args->length == 1);

	VisitForAccumulatorValue(args->at(0));
	__ UnwrapPointer(eax);

	const int argument_count = 1;

	__ PrepareCallCFunction(argument_count, ebx);
	__ mov (Operand(esp, 0), eax);
	__ CallCFunction(ExternalReference::PIN_SemaphoreClear_function(isolate()), argument_count);
}

#undef FUN