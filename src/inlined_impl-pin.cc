#define FUN(NAME) void FullCodeGenerator::Emit##NAME(CallRuntime* expr)

FUN(UnwrapPointer)
{
	ZoneList<Expression*>* args = expr->arguments();
	ASSERT(args->length == 1);

	VisitForAccumulatorValue(args->at(0));
	__ UnwrapPointer(eax);

	__ SmiTag(eax);
	context()->Plug(eax);
}

//arg0: ExternalPointer obj, arg1: pointer addr
FUN(WrapPointer)
{
	ZoneList<Expression*>* args = expr->arguments();
	ASSERT(args->length == 2);

	VisitForAccumulatorValue(args->at(0));
	VisitForStackValue(args->at(1));
	__ pop(edx);
	__ SmiUntag(edx);
	__ WrapPointer(eax, edx, ecx);
}

//arg0: c++ string address
FUN(JSStringFromCString)
{
	ZoneList<Expression*>* args = expr->arguments();
	ASSERT(args->length == 1);

	VisitForAccumulatorValue(args->at(0));
	__ SmiUntag(eax);
	__ mov(edi, eax);

	__ push (esi);
	__ push (edi);
	__ AllocateStringFromCString(eax, edi, esi, ecx, edx);
	__ pop (edi);
	__ pop (esi);

	context()->Plug(eax);
}

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

FUN(IMG_Invalid)
{
  const int argument_count = 0;
  __ PrepareCallCFunction(argument_count, ebx);
  __ CallCFunction(ExternalReference::IMG_Invalid_function(isolate()), argument_count);

  __ SmiTag(eax);  //transform output into a tagged small integer
  context()->Plug(eax);
}

FUN(APP_ImgHead)
{
	ZoneList<Expression*>* args = expr->arguments();
	ASSERT(args->length == 1);

	VisitForAccumulatorValue(args->at(0));
	__ mov(edx, eax);

	const int argument_count = 0;
	__ PrepareCallCFunction(argument_count, ebx);
	__ CallCFunction(ExternalReference::APP_ImgHead_function(isolate()), argument_count);
	__ WrapPointer(edx, eax, ecx);
}

FUN(APP_ImgTail)
{
	ZoneList<Expression*>* args = expr->arguments();
	ASSERT(args->length == 1);

	VisitForAccumulatorValue(args->at(0));
	__ mov(edx, eax);

	const int argument_count = 0;
	__ PrepareCallCFunction(argument_count, ebx);
	__ CallCFunction(ExternalReference::APP_ImgTail_function(isolate()), argument_count);
	__ WrapPointer(edx, eax, ecx);
}

FUN(IMG_Next)
{
	ZoneList<Expression*>* args = expr->arguments();
	ASSERT(args->length == 2);

	VisitForAccumulatorValue(args->at(0));  //IMG_Next argument
	VisitForStackValue(args->at(1));        //return value pointer
	__ UnwrapPointer(eax);

	const int argument_count = 1;

	__ PrepareCallCFunction(argument_count, ebx);
	__ mov (Operand(esp, kPointerSize * 0), eax);
	__ CallCFunction(ExternalReference::IMG_Next_function(isolate()), argument_count);
	__ pop(edx);
	__ WrapPointer(edx, eax, ecx);
}

FUN(IMG_Prev)
{
	ZoneList<Expression*>* args = expr->arguments();
	ASSERT(args->length == 2);

	VisitForAccumulatorValue(args->at(0));  //IMG_Prev argument
	VisitForStackValue(args->at(1));        //return value pointer
	__ UnwrapPointer(eax);

	const int argument_count = 1;

	__ PrepareCallCFunction(argument_count, ebx);
	__ mov (Operand(esp, kPointerSize * 0), eax);
	__ CallCFunction(ExternalReference::IMG_Prev_function(isolate()), argument_count);
	__ pop(edx);
	__ WrapPointer(edx, eax, ecx);
}

FUN(IMG_Name)
{
	ZoneList<Expression*>* args = expr->arguments();
	ASSERT(args->length == 1);

	VisitForAccumulatorValue(args->at(0));
	__ UnwrapPointer(eax);

	const int argument_count = 1;

	__ PrepareCallCFunction(argument_count, ebx);
	__ mov (Operand(esp, 0), eax);
	__ CallCFunction(ExternalReference::IMG_Name_function(isolate()), argument_count);

	__ SmiTag(eax);
	context()->Plug(eax);
}

FUN(IMG_Entry)
{
	ZoneList<Expression*>* args = expr->arguments();
	ASSERT(args->length == 1);

	VisitForAccumulatorValue(args->at(0));
	__ UnwrapPointer(eax);

	const int argument_count = 1;

	__ PrepareCallCFunction(argument_count, ebx);
	__ mov (Operand(esp, 0), eax);
	__ CallCFunction(ExternalReference::IMG_Entry_function(isolate()), argument_count);

	__ SmiTag(eax);
	context()->Plug(eax);
}

FUN(IMG_LoadOffset)
{
	ZoneList<Expression*>* args = expr->arguments();
	ASSERT(args->length == 1);

	VisitForAccumulatorValue(args->at(0));
	__ UnwrapPointer(eax);

	const int argument_count = 1;

	__ PrepareCallCFunction(argument_count, ebx);
	__ mov (Operand(esp, 0), eax);
	__ CallCFunction(ExternalReference::IMG_LoadOffset_function(isolate()), argument_count);

	__ SmiTag(eax);
	context()->Plug(eax);
}

FUN(IMG_FindByAddress)
{
	ZoneList<Expression*>* args = expr->arguments();
	ASSERT(args->length == 2);

	VisitForAccumulatorValue(args->at(0));
	VisitForStackValue(args->at(1));        //return value pointer
	__ SmiUntag(eax);

	const int argument_count = 1;

	__ PrepareCallCFunction(argument_count, ebx);
	__ mov (Operand(esp, kPointerSize * 0), eax);
	__ CallCFunction(ExternalReference::IMG_FindByAddress_function(isolate()), argument_count);
	__ pop(edx);
	__ WrapPointer(edx, eax, ecx);
}

FUN(IMG_FindImgById)
{
	ZoneList<Expression*>* args = expr->arguments();
	ASSERT(args->length == 2);

	VisitForAccumulatorValue(args->at(0));
	VisitForStackValue(args->at(1));        //return value pointer
	__ SmiUntag(eax);

	const int argument_count = 1;

	__ PrepareCallCFunction(argument_count, ebx);
	__ mov (Operand(esp, kPointerSize * 0), eax);
	__ CallCFunction(ExternalReference::IMG_FindImgById_function(isolate()), argument_count);
	__ pop(edx);
	__ WrapPointer(edx, eax, ecx);
}

FUN(IMG_Open)
{
	ZoneList<Expression*>* args = expr->arguments();
	ASSERT(args->length == 2);

	VisitForAccumulatorValue(args->at(0));
	VisitForStackValue(args->at(1));        //return value pointer
	__ UnwrapPointer(eax);

	const int argument_count = 1;

	__ PrepareCallCFunction(argument_count, ebx);
	__ mov (Operand(esp, kPointerSize * 0), eax);
	__ CallCFunction(ExternalReference::IMG_Open_function(isolate()), argument_count);
	__ pop(edx);
	__ WrapPointer(edx, eax, ecx);
}

FUN(IMG_Close)
{
	ZoneList<Expression*>* args = expr->arguments();
	ASSERT(args->length == 1);

	VisitForAccumulatorValue(args->at(0));
	__ UnwrapPointer(eax);

	const int argument_count = 1;

	__ PrepareCallCFunction(argument_count, ebx);
	__ mov (Operand(esp, 0), eax);
	__ CallCFunction(ExternalReference::IMG_Close_function(isolate()), argument_count);
}

FUN(SEC_Invalid)
{
  const int argument_count = 0;
  __ PrepareCallCFunction(argument_count, ebx);
  __ CallCFunction(ExternalReference::SEC_Invalid_function(isolate()), argument_count);

  __ SmiTag(eax);  //transform output into a tagged small integer
  context()->Plug(eax);
}

FUN(SYM_Invalid)
{
  const int argument_count = 0;
  __ PrepareCallCFunction(argument_count, ebx);
  __ CallCFunction(ExternalReference::SYM_Invalid_function(isolate()), argument_count);

  __ SmiTag(eax);  //transform output into a tagged small integer
  context()->Plug(eax);
}

FUN(IMG_SecHead)
{
	ZoneList<Expression*>* args = expr->arguments();
	ASSERT(args->length == 2);

	VisitForAccumulatorValue(args->at(0));
	VisitForStackValue(args->at(1));        //return value pointer
	__ UnwrapPointer(eax);

	const int argument_count = 1;

	__ PrepareCallCFunction(argument_count, ebx);
	__ mov (Operand(esp, kPointerSize * 0), eax);
	__ CallCFunction(ExternalReference::IMG_SecHead_function(isolate()), argument_count);
	__ pop(edx);
	__ WrapPointer(edx, eax, ecx);
}

FUN(IMG_SecTail)
{
	ZoneList<Expression*>* args = expr->arguments();
	ASSERT(args->length == 2);

	VisitForAccumulatorValue(args->at(0));
	VisitForStackValue(args->at(1));        //return value pointer
	__ UnwrapPointer(eax);

	const int argument_count = 1;

	__ PrepareCallCFunction(argument_count, ebx);
	__ mov (Operand(esp, kPointerSize * 0), eax);
	__ CallCFunction(ExternalReference::IMG_SecTail_function(isolate()), argument_count);
	__ pop(edx);
	__ WrapPointer(edx, eax, ecx);
}

FUN(SEC_Next)
{
	ZoneList<Expression*>* args = expr->arguments();
	ASSERT(args->length == 2);

	VisitForAccumulatorValue(args->at(0));
	VisitForStackValue(args->at(1));        //return value pointer
	__ UnwrapPointer(eax);

	const int argument_count = 1;

	__ PrepareCallCFunction(argument_count, ebx);
	__ mov (Operand(esp, kPointerSize * 0), eax);
	__ CallCFunction(ExternalReference::SEC_Next_function(isolate()), argument_count);
	__ pop(edx);
	__ WrapPointer(edx, eax, ecx);
}

FUN(SEC_Prev)
{
	ZoneList<Expression*>* args = expr->arguments();
	ASSERT(args->length == 2);

	VisitForAccumulatorValue(args->at(0));
	VisitForStackValue(args->at(1));        //return value pointer
	__ UnwrapPointer(eax);

	const int argument_count = 1;

	__ PrepareCallCFunction(argument_count, ebx);
	__ mov (Operand(esp, kPointerSize * 0), eax);
	__ CallCFunction(ExternalReference::SEC_Prev_function(isolate()), argument_count);
	__ pop(edx);
	__ WrapPointer(edx, eax, ecx);
}

FUN(SEC_Name)
{
	ZoneList<Expression*>* args = expr->arguments();
	ASSERT(args->length == 1);

	VisitForAccumulatorValue(args->at(0));
	__ UnwrapPointer(eax);

	const int argument_count = 1;

	__ PrepareCallCFunction(argument_count, ebx);
	__ mov (Operand(esp, 0), eax);
	__ CallCFunction(ExternalReference::SEC_Name_function(isolate()), argument_count);

	__ SmiTag(eax);
	context()->Plug(eax);
}

FUN(IMG_RegsymHead)
{
	ZoneList<Expression*>* args = expr->arguments();
	ASSERT(args->length == 2);

	VisitForAccumulatorValue(args->at(0));
	VisitForStackValue(args->at(1));        //return value pointer
	__ UnwrapPointer(eax);

	const int argument_count = 1;

	__ PrepareCallCFunction(argument_count, ebx);
	__ mov (Operand(esp, kPointerSize * 0), eax);
	__ CallCFunction(ExternalReference::IMG_RegsymHead_function(isolate()), argument_count);
	__ pop(edx);
	__ WrapPointer(edx, eax, ecx);
}

FUN(SYM_Next)
{
	ZoneList<Expression*>* args = expr->arguments();
	ASSERT(args->length == 2);

	VisitForAccumulatorValue(args->at(0));
	VisitForStackValue(args->at(1));        //return value pointer
	__ UnwrapPointer(eax);

	const int argument_count = 1;

	__ PrepareCallCFunction(argument_count, ebx);
	__ mov (Operand(esp, kPointerSize * 0), eax);
	__ CallCFunction(ExternalReference::SYM_Next_function(isolate()), argument_count);
	__ pop(edx);
	__ WrapPointer(edx, eax, ecx);
}

FUN(SYM_Prev)
{
	ZoneList<Expression*>* args = expr->arguments();
	ASSERT(args->length == 2);

	VisitForAccumulatorValue(args->at(0));
	VisitForStackValue(args->at(1));        //return value pointer
	__ UnwrapPointer(eax);

	const int argument_count = 1;

	__ PrepareCallCFunction(argument_count, ebx);
	__ mov (Operand(esp, kPointerSize * 0), eax);
	__ CallCFunction(ExternalReference::SYM_Prev_function(isolate()), argument_count);
	__ pop(edx);
	__ WrapPointer(edx, eax, ecx);
}

FUN(SYM_Name)
{
	ZoneList<Expression*>* args = expr->arguments();
	ASSERT(args->length == 1);

	VisitForAccumulatorValue(args->at(0));
	__ UnwrapPointer(eax);

	const int argument_count = 1;

	__ PrepareCallCFunction(argument_count, ebx);
	__ mov (Operand(esp, 0), eax);
	__ CallCFunction(ExternalReference::SYM_Name_function(isolate()), argument_count);

	__ SmiTag(eax);
	context()->Plug(eax);
}

FUN(RTN_Invalid)
{
  const int argument_count = 0;
  __ PrepareCallCFunction(argument_count, ebx);
  __ CallCFunction(ExternalReference::RTN_Invalid_function(isolate()), argument_count);

  __ SmiTag(eax);  //transform output into a tagged small integer
  context()->Plug(eax);
}

FUN(SEC_RtnHead)
{
	ZoneList<Expression*>* args = expr->arguments();
	ASSERT(args->length == 2);

	VisitForAccumulatorValue(args->at(0));
	VisitForStackValue(args->at(1));        //return value pointer
	__ UnwrapPointer(eax);

	const int argument_count = 1;

	__ PrepareCallCFunction(argument_count, ebx);
	__ mov (Operand(esp, kPointerSize * 0), eax);
	__ CallCFunction(ExternalReference::SEC_RtnHead_function(isolate()), argument_count);
	__ pop(edx);
	__ WrapPointer(edx, eax, ecx);
}

FUN(SEC_RtnTail)
{
	ZoneList<Expression*>* args = expr->arguments();
	ASSERT(args->length == 2);

	VisitForAccumulatorValue(args->at(0));
	VisitForStackValue(args->at(1));        //return value pointer
	__ UnwrapPointer(eax);

	const int argument_count = 1;

	__ PrepareCallCFunction(argument_count, ebx);
	__ mov (Operand(esp, kPointerSize * 0), eax);
	__ CallCFunction(ExternalReference::SEC_RtnTail_function(isolate()), argument_count);
	__ pop(edx);
	__ WrapPointer(edx, eax, ecx);
}

FUN(RTN_Next)
{
	ZoneList<Expression*>* args = expr->arguments();
	ASSERT(args->length == 2);

	VisitForAccumulatorValue(args->at(0));
	VisitForStackValue(args->at(1));        //return value pointer
	__ UnwrapPointer(eax);

	const int argument_count = 1;

	__ PrepareCallCFunction(argument_count, ebx);
	__ mov (Operand(esp, kPointerSize * 0), eax);
	__ CallCFunction(ExternalReference::RTN_Next_function(isolate()), argument_count);
	__ pop(edx);
	__ WrapPointer(edx, eax, ecx);
}

FUN(RTN_Prev)
{
	ZoneList<Expression*>* args = expr->arguments();
	ASSERT(args->length == 2);

	VisitForAccumulatorValue(args->at(0));
	VisitForStackValue(args->at(1));        //return value pointer
	__ UnwrapPointer(eax);

	const int argument_count = 1;

	__ PrepareCallCFunction(argument_count, ebx);
	__ mov (Operand(esp, kPointerSize * 0), eax);
	__ CallCFunction(ExternalReference::RTN_Prev_function(isolate()), argument_count);
	__ pop(edx);
	__ WrapPointer(edx, eax, ecx);
}

FUN(RTN_Name)
{
	ZoneList<Expression*>* args = expr->arguments();
	ASSERT(args->length == 1);

	VisitForAccumulatorValue(args->at(0));
	__ UnwrapPointer(eax);

	const int argument_count = 1;

	__ PrepareCallCFunction(argument_count, ebx);
	__ mov (Operand(esp, 0), eax);
	__ CallCFunction(ExternalReference::RTN_Name_function(isolate()), argument_count);

	__ SmiTag(eax);
	context()->Plug(eax);
}

FUN(INS_Invalid)
{
  const int argument_count = 0;
  __ PrepareCallCFunction(argument_count, ebx);
  __ CallCFunction(ExternalReference::INS_Invalid_function(isolate()), argument_count);

  __ SmiTag(eax);  //transform output into a tagged small integer
  context()->Plug(eax);
}

FUN(RTN_InsHead)
{
	ZoneList<Expression*>* args = expr->arguments();
	ASSERT(args->length == 2);

	VisitForAccumulatorValue(args->at(0));
	VisitForStackValue(args->at(1));        //return value pointer
	__ UnwrapPointer(eax);

	const int argument_count = 1;

	__ PrepareCallCFunction(argument_count, ebx);
	__ mov (Operand(esp, kPointerSize * 0), eax);
	__ CallCFunction(ExternalReference::RTN_InsHead_function(isolate()), argument_count);
	__ pop(edx);
	__ WrapPointer(edx, eax, ecx);
}

FUN(RTN_InsHeadOnly)
{
	ZoneList<Expression*>* args = expr->arguments();
	ASSERT(args->length == 2);

	VisitForAccumulatorValue(args->at(0));
	VisitForStackValue(args->at(1));        //return value pointer
	__ UnwrapPointer(eax);

	const int argument_count = 1;

	__ PrepareCallCFunction(argument_count, ebx);
	__ mov (Operand(esp, kPointerSize * 0), eax);
	__ CallCFunction(ExternalReference::RTN_InsHeadOnly_function(isolate()), argument_count);
	__ pop(edx);
	__ WrapPointer(edx, eax, ecx);
}

FUN(RTN_InsTail)
{
	ZoneList<Expression*>* args = expr->arguments();
	ASSERT(args->length == 2);

	VisitForAccumulatorValue(args->at(0));
	VisitForStackValue(args->at(1));        //return value pointer
	__ UnwrapPointer(eax);

	const int argument_count = 1;

	__ PrepareCallCFunction(argument_count, ebx);
	__ mov (Operand(esp, kPointerSize * 0), eax);
	__ CallCFunction(ExternalReference::RTN_InsTail_function(isolate()), argument_count);
	__ pop(edx);
	__ WrapPointer(edx, eax, ecx);
}

FUN(INS_Next)
{
	ZoneList<Expression*>* args = expr->arguments();
	ASSERT(args->length == 2);

	VisitForAccumulatorValue(args->at(0));
	VisitForStackValue(args->at(1));        //return value pointer
	__ UnwrapPointer(eax);

	const int argument_count = 1;

	__ PrepareCallCFunction(argument_count, ebx);
	__ mov (Operand(esp, kPointerSize * 0), eax);
	__ CallCFunction(ExternalReference::INS_Next_function(isolate()), argument_count);
	__ pop(edx);
	__ WrapPointer(edx, eax, ecx);
}

FUN(INS_Prev)
{
	ZoneList<Expression*>* args = expr->arguments();
	ASSERT(args->length == 2);

	VisitForAccumulatorValue(args->at(0));
	VisitForStackValue(args->at(1));        //return value pointer
	__ UnwrapPointer(eax);

	const int argument_count = 1;

	__ PrepareCallCFunction(argument_count, ebx);
	__ mov (Operand(esp, kPointerSize * 0), eax);
	__ CallCFunction(ExternalReference::INS_Prev_function(isolate()), argument_count);
	__ pop(edx);
	__ WrapPointer(edx, eax, ecx);
}

FUN(INS_Disassemble)
{
	ZoneList<Expression*>* args = expr->arguments();
	ASSERT(args->length == 2);

	VisitForAccumulatorValue(args->at(0));
	VisitForStackValue(args->at(1));        //wrapped empty string buffer
	__ UnwrapPointer(eax);
	__ pop(edx);
	__ UnwrapPointer(edx);

	const int argument_count = 2;

	__ PrepareCallCFunction(argument_count, ebx);
	__ mov (Operand(esp, kPointerSize * 0), edx);
	__ mov (Operand(esp, kPointerSize * 1), eax);
	__ CallCFunction(ExternalReference::INS_Disassemble_function(isolate()), argument_count);
}

FUN(RTN_Open)
{
	ZoneList<Expression*>* args = expr->arguments();
	ASSERT(args->length == 1);

	VisitForAccumulatorValue(args->at(0));
	__ UnwrapPointer(eax);

	const int argument_count = 1;

	__ PrepareCallCFunction(argument_count, ebx);
	__ mov (Operand(esp, 0), eax);
	__ CallCFunction(ExternalReference::RTN_Open_function(isolate()), argument_count);
}

FUN(RTN_Close)
{
	ZoneList<Expression*>* args = expr->arguments();
	ASSERT(args->length == 1);

	VisitForAccumulatorValue(args->at(0));
	__ UnwrapPointer(eax);

	const int argument_count = 1;

	__ PrepareCallCFunction(argument_count, ebx);
	__ mov (Operand(esp, 0), eax);
	__ CallCFunction(ExternalReference::RTN_Close_function(isolate()), argument_count);
}

FUN(RTN_FindNameByAddress)
{
	ZoneList<Expression*>* args = expr->arguments();
	ASSERT(args->length == 2);

	VisitForAccumulatorValue(args->at(0));
	VisitForStackValue(args->at(1));        //wrapped empty string buffer
	__ SmiUntag(eax);
	__ pop(edx);
	__ UnwrapPointer(edx);

	const int argument_count = 2;

	__ PrepareCallCFunction(argument_count, ebx);
	__ mov (Operand(esp, kPointerSize * 0), edx);
	__ mov (Operand(esp, kPointerSize * 1), eax);
	__ CallCFunction(ExternalReference::RTN_FindNameByAddress_function(isolate()), argument_count);
}

FUN(RTN_FindByAddress)
{
	ZoneList<Expression*>* args = expr->arguments();
	ASSERT(args->length == 2);

	VisitForAccumulatorValue(args->at(0));
	VisitForStackValue(args->at(1));        //return value pointer
	__ SmiUntag(eax);

	const int argument_count = 1;

	__ PrepareCallCFunction(argument_count, ebx);
	__ mov (Operand(esp, 0), eax);
	__ CallCFunction(ExternalReference::RTN_FindByAddress_function(isolate()), argument_count);
	__ pop(edx);
	__ WrapPointer(edx, eax, ecx);
}

FUN(INS_Address)
{
	ZoneList<Expression*>* args = expr->arguments();
	ASSERT(args->length == 1);

	VisitForAccumulatorValue(args->at(0));
	__ UnwrapPointer(eax);

	const int argument_count = 1;

	__ PrepareCallCFunction(argument_count, ebx);
	__ mov (Operand(esp, 0), eax);
	__ CallCFunction(ExternalReference::INS_Address_function(isolate()), argument_count);

	__ SmiTag(eax);
	context()->Plug(eax);
}

FUN(INS_Size)
{
	ZoneList<Expression*>* args = expr->arguments();
	ASSERT(args->length == 1);

	VisitForAccumulatorValue(args->at(0));
	__ UnwrapPointer(eax);

	const int argument_count = 1;

	__ PrepareCallCFunction(argument_count, ebx);
	__ mov (Operand(esp, 0), eax);
	__ CallCFunction(ExternalReference::INS_Size_function(isolate()), argument_count);

	__ SmiTag(eax);
	context()->Plug(eax);
}

//[addr, name, output]
FUN(RTN_CreateAt)
{
	ZoneList<Expression*>* args = expr->arguments();
	ASSERT(args->length == 3);

	VisitForAccumulatorValue(args->at(0));
	VisitForStackValue(args->at(1));
	VisitForStackValue(args->at(2));
	__ SmiUntag(eax);
	__ pop(edx);
	__ UnwrapPointer(edx);

	const int argument_count = 2;

	__ PrepareCallCFunction(argument_count, ebx);
	__ mov (Operand(esp, kPointerSize * 0), edx);
	__ mov (Operand(esp, kPointerSize * 1), eax);
	__ CallCFunction(ExternalReference::RTN_CreateAt_function(isolate()), argument_count);
	__ pop(edx);
	__ WrapPointer(edx, eax, ecx);
}


#undef FUN