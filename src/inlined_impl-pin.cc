#ifdef INLINED_IMPL
#define FUN(NAME) void FullCodeGenerator::Emit##NAME(CallRuntime* expr)

//Helper functions
FUN(ReturnContext)
{
	ZoneList<Expression*>* args = expr->arguments();
	ASSERT(args->length == 1);

	VisitForAccumulatorValue(args->at(0));
	__ LoadPointer(eax);

	//newstate
	__ mov(Operand(eax, kPointerSize * 6), ebx);
	__ mov(Operand(eax, kPointerSize * 7), esi);
	__ mov(Operand(eax, kPointerSize * 8), edi);
	__ mov(Operand(eax, kPointerSize * 9), ebp);
	__ mov(Operand(eax, kPointerSize * 10), esp);

	//ret address
	Label retaddr;
	__ call (&retaddr);
	__ bind(&retaddr);
	__ pop(ebx);

	//point EIP after the RET 4
	__ add(ebx, Immediate(0x18));

	//neweip
	__ mov(Operand(eax, kPointerSize * 5), ebx);

	//switch to the saved stack and registers
	__ mov(ebx, Operand(eax, kPointerSize * 0));
	__ mov(esi, Operand(eax, kPointerSize * 1));
	__ mov(edi, Operand(eax, kPointerSize * 2));
	__ mov(ebp, Operand(eax, kPointerSize * 3));
	__ mov(esp, Operand(eax, kPointerSize * 4));

	__ ret(4);
}

//ptr, size (0 means strlen, size is in two-bytes chars)
FUN(ReadTwoByteString)
{
	ZoneList<Expression*>* args = expr->arguments();
	ASSERT(args->length == 2);

	VisitForAccumulatorValue(args->at(1));
	__ ReadInteger(eax);
	__ mov(ecx, eax);  //size
	VisitForAccumulatorValue(args->at(0));
	__ LoadPointer(eax); //ptr
	__ mov(esi, eax);

	Label bailout;
	Label done;
	Label strlen;
	Label with_size;

	__ test(ecx,ecx);
	__ j(not_zero, &with_size);
	__ mov(edi,eax);
	Operand self(edi, 0, RelocInfo::NONE);
	__ bind(&strlen);
	__ cmpw(self, Immediate(0));
	__ j(zero, &with_size);
	__ inc(ecx);
	__ inc(edi);
	__ inc(edi);
	__ jmp(&strlen);

	__ bind(&with_size);
	//allocate in chars
	__ AllocateTwoByteString(eax, ecx, ebx, edi, edx, &bailout);

	__ lea(edi, Operand(eax, SeqAsciiString::kHeaderSize - kHeapObjectTag));
	//copy in bytes
	__ add(ecx,ecx);
	__ CopyBytes(esi, edi, ecx, ebx);
	__ jmp(&done);

	__ bind(&bailout);
	__ mov(eax, isolate()->factory()->undefined_value());

	__ bind(&done);
	__ mov(esi, Operand(ebp, StandardFrameConstants::kContextOffset));
	context()->Plug(eax);
}

//ptr, size (0 means strlen)
FUN(ReadAsciiString)
{
	ZoneList<Expression*>* args = expr->arguments();
	ASSERT(args->length == 2);

	VisitForAccumulatorValue(args->at(1));
	__ ReadInteger(eax);
	__ mov(ecx, eax);  //size
	VisitForAccumulatorValue(args->at(0));
	__ LoadPointer(eax); //ptr
	__ mov(esi, eax);

	Label bailout;
	Label done;
	Label strlen;
	Label with_size;

	__ test(ecx,ecx);
	__ j(not_zero, &with_size);
	__ mov(edi,eax);
	Operand self(edi, 0, RelocInfo::NONE);
	__ bind(&strlen);
	__ cmpb(self, 0);
	__ j(zero, &with_size);
	__ inc(ecx);
	__ inc(edi);
	__ jmp(&strlen);

	__ bind(&with_size);
	__ AllocateAsciiString(eax, ecx, ebx, edi, edx, &bailout);

	__ lea(edi, Operand(eax, SeqAsciiString::kHeaderSize - kHeapObjectTag));
	__ CopyBytes(esi, edi, ecx, ebx);
	__ jmp(&done);

	__ bind(&bailout);
	__ mov(eax, isolate()->factory()->undefined_value());

	__ bind(&done);
	__ mov(esi, Operand(ebp, StandardFrameConstants::kContextOffset));
	context()->Plug(eax);
}

FUN(ReadDouble)
{
	ZoneList<Expression*>* args = expr->arguments();
	ASSERT(args->length == 1);

	Label slow_alloc;
	Label allocated;

	VisitForAccumulatorValue(args->at(0));
	__ LoadPointer(eax);
	Operand self(eax, 0, RelocInfo::NONE);
	__ movdbl(xmm0, self);

	__ AllocateHeapNumber(edi, eax, ebx, &slow_alloc);
	__ jmp(&allocated);
	__ bind(&slow_alloc);
	__ CallRuntime(Runtime::kNumberAlloc, 0);
	__ mov(edi, eax);
	__ bind(&allocated);
	__ movdbl(Operand(edi, HeapNumber::kValueOffset - kHeapObjectTag), xmm0);

	context()->Plug(edi);
}

FUN(WriteDouble)
{
	ZoneList<Expression*>* args = expr->arguments();
	ASSERT(args->length == 2);

	VisitForAccumulatorValue(args->at(1));
	__ mov(edx, eax);
	VisitForAccumulatorValue(args->at(0));
	__ LoadPointer(eax);

	Label smi, end;

	__ JumpIfSmi(edx, &smi, Label::kNear);
	__ movdbl(xmm0, Operand(edx, HeapNumber::kValueOffset - kHeapObjectTag));
	__ jmp(&end);
	__ bind(&smi);
	__ SmiUntag(edx);
	//Converts a 32bit integer to the bottom 64bit double.
	__ cvtsi2sd(xmm0, edx);
	__ bind(&end);

	Operand self(eax, 0, RelocInfo::NONE);
	__ movdbl (self, xmm0);
}

FUN(ReadByte)
{
	ZoneList<Expression*>* args = expr->arguments();
	ASSERT(args->length == 1);

	VisitForAccumulatorValue(args->at(0));
	__ LoadPointer(eax);
	Operand self(eax, 0, RelocInfo::NONE);
	__ movzx_b (eax, self);
	__ SmiTag(eax);
	context()->Plug(eax);
}

FUN(WriteByte)
{
	ZoneList<Expression*>* args = expr->arguments();
	ASSERT(args->length == 2);

	VisitForAccumulatorValue(args->at(1));
	__ mov(edx, eax);
	__ ReadInteger(edx);
	VisitForAccumulatorValue(args->at(0));
	__ LoadPointer(eax);
	Operand self(eax, 0, RelocInfo::NONE);
	__ mov_b (self, edx);
}

FUN(ReadWord)
{
	ZoneList<Expression*>* args = expr->arguments();
	ASSERT(args->length == 1);

	VisitForAccumulatorValue(args->at(0));
	__ LoadPointer(eax);
	Operand self(eax, 0, RelocInfo::NONE);
	__ movzx_w (eax, self);
	__ SmiTag(eax);
	context()->Plug(eax);
}

FUN(WriteWord)
{
	ZoneList<Expression*>* args = expr->arguments();
	ASSERT(args->length == 2);

	VisitForAccumulatorValue(args->at(1));
	__ mov(edx, eax);
	__ ReadInteger(edx);
	VisitForAccumulatorValue(args->at(0));
	__ LoadPointer(eax);
	Operand self(eax, 0, RelocInfo::NONE);
	__ mov_w (self, edx);
}

FUN(ReadDword)
{
	ZoneList<Expression*>* args = expr->arguments();
	ASSERT(args->length == 1);

	VisitForAccumulatorValue(args->at(0));
	__ LoadPointer(eax);
	Operand self(eax, 0, RelocInfo::NONE);
	__ mov (ecx, self);
	__ CreateInteger(ecx, eax, edi, ebx);
	context()->Plug(eax);
}

FUN(WriteDword)
{
	ZoneList<Expression*>* args = expr->arguments();
	ASSERT(args->length == 2);

	VisitForAccumulatorValue(args->at(1));
	__ mov(edx, eax);
	__ ReadInteger(edx);
	VisitForAccumulatorValue(args->at(0));
	__ LoadPointer(eax);
	Operand self(eax, 0, RelocInfo::NONE);
	__ mov (self, edx);
}

FUN(LoadPointer)
{
	ZoneList<Expression*>* args = expr->arguments();
	ASSERT(args->length == 1);

	VisitForAccumulatorValue(args->at(0));
	__ LoadPointer(eax);

	__ mov(ecx, eax);
	__ CreateInteger(ecx, eax, edi, ebx);
	context()->Plug(eax);
}

//arg0: HeapNumber obj/SMI, arg1: pointer addr
FUN(StorePointer)
{
	ZoneList<Expression*>* args = expr->arguments();
	ASSERT(args->length == 2);

	VisitForAccumulatorValue(args->at(1));
	__ mov(edx, eax);
	__ ReadInteger(edx);
	VisitForAccumulatorValue(args->at(0));
	__ StorePointer(eax, edx, ecx, edi);
}

//arg0: c++ string address
FUN(JSStringFromCString)
{
	ZoneList<Expression*>* args = expr->arguments();
	ASSERT(args->length == 1);

	VisitForAccumulatorValue(args->at(0));
	__ LoadPointer(eax);
	__ mov(edi, eax);

	__ AllocateStringFromCString(eax, edi, esi, ecx, edx);
	__ mov(esi, Operand(ebp, StandardFrameConstants::kContextOffset));
	context()->Plug(eax);
}


// Pin functions
FUN(PIN_GetPid)
{
	const int argument_count = 0;
	__ PrepareCallCFunction(argument_count, ebx);
	__ CallCFunction(ExternalReference::PIN_GetPid_function(isolate()), argument_count);
	__ SmiTag(eax);
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

	__ Int64ToFP(eax, edx, xmm0, xmm1);
	__ CreateInteger(xmm0, eax, edi, ebx);
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
	__ ReadInteger(eax);

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
	__ ReadInteger(eax);

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
	__ ReadInteger(eax);

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
	__ LoadPointer(eax);

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
	__ LoadPointer(eax);

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
	__ LoadPointer(eax);

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
	__ LoadPointer(eax);

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
	__ LoadPointer(eax);

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

	VisitForAccumulatorValue(args->at(0));
	__ mov(edx,eax);
	__ LoadPointer(edx);
	VisitForAccumulatorValue(args->at(1));
	__ ReadInteger(eax);

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
	__ LoadPointer(eax);

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
	__ SmiTag(eax);
	context()->Plug(eax);
}

FUN(APP_ImgHead)
{
	const int argument_count = 0;
	__ PrepareCallCFunction(argument_count, ebx);
	__ CallCFunction(ExternalReference::APP_ImgHead_function(isolate()), argument_count);
	__ SmiTag(eax);
	context()->Plug(eax);
}

FUN(APP_ImgTail)
{
	const int argument_count = 0;
	__ PrepareCallCFunction(argument_count, ebx);
	__ CallCFunction(ExternalReference::APP_ImgTail_function(isolate()), argument_count);
	__ SmiTag(eax);
	context()->Plug(eax);
}

FUN(IMG_Next)
{
	ZoneList<Expression*>* args = expr->arguments();
	ASSERT(args->length == 1);

	VisitForAccumulatorValue(args->at(0));  //IMG_Next argument
	__ ReadInteger(eax);

	const int argument_count = 1;

	__ PrepareCallCFunction(argument_count, ebx);
	__ mov (Operand(esp, kPointerSize * 0), eax);
	__ CallCFunction(ExternalReference::IMG_Next_function(isolate()), argument_count);
	__ SmiTag(eax);
	context()->Plug(eax);
}

FUN(IMG_Prev)
{
	ZoneList<Expression*>* args = expr->arguments();
	ASSERT(args->length == 1);

	VisitForAccumulatorValue(args->at(0));  //IMG_Prev argument
	__ ReadInteger(eax);

	const int argument_count = 1;

	__ PrepareCallCFunction(argument_count, ebx);
	__ mov (Operand(esp, kPointerSize * 0), eax);
	__ CallCFunction(ExternalReference::IMG_Prev_function(isolate()), argument_count);
	__ SmiTag(eax);
	context()->Plug(eax);
}

FUN(IMG_Name)
{
	ZoneList<Expression*>* args = expr->arguments();
	ASSERT(args->length == 1);

	VisitForAccumulatorValue(args->at(0));
	__ ReadInteger(eax);

	const int argument_count = 1;

	__ PrepareCallCFunction(argument_count, ebx);
	__ mov (Operand(esp, 0), eax);
	__ CallCFunction(ExternalReference::IMG_Name_function(isolate()), argument_count);
	__ mov (edx, eax);
	__ StorePointer(eax, edx, ecx, ebx);
	context()->Plug(eax);
}

FUN(IMG_Entry)
{
	ZoneList<Expression*>* args = expr->arguments();
	ASSERT(args->length == 1);

	VisitForAccumulatorValue(args->at(0));
	__ ReadInteger(eax);

	const int argument_count = 1;

	__ PrepareCallCFunction(argument_count, ebx);
	__ mov (Operand(esp, 0), eax);
	__ CallCFunction(ExternalReference::IMG_Entry_function(isolate()), argument_count);
	__ mov (edx, eax);
	__ StorePointer(eax, edx, ecx, ebx);
	context()->Plug(eax);
}

FUN(IMG_LoadOffset)
{
	ZoneList<Expression*>* args = expr->arguments();
	ASSERT(args->length == 1);

	VisitForAccumulatorValue(args->at(0));
	__ ReadInteger(eax);

	const int argument_count = 1;

	__ PrepareCallCFunction(argument_count, ebx);
	__ mov (Operand(esp, 0), eax);
	__ CallCFunction(ExternalReference::IMG_LoadOffset_function(isolate()), argument_count);
	__ mov (edx, eax);
	__ StorePointer(eax, edx, ecx, ebx);
	context()->Plug(eax);
}

FUN(IMG_FindByAddress)
{
	ZoneList<Expression*>* args = expr->arguments();
	ASSERT(args->length == 1);

	VisitForAccumulatorValue(args->at(0));
	__ LoadPointer(eax);

	const int argument_count = 1;

	__ PrepareCallCFunction(argument_count, ebx);
	__ mov (Operand(esp, kPointerSize * 0), eax);
	__ CallCFunction(ExternalReference::IMG_FindByAddress_function(isolate()), argument_count);
	__ SmiTag(eax);
	context()->Plug(eax);
}

FUN(IMG_FindImgById)
{
	ZoneList<Expression*>* args = expr->arguments();
	ASSERT(args->length == 1);

	VisitForAccumulatorValue(args->at(0));
	__ ReadInteger(eax);

	const int argument_count = 1;

	__ PrepareCallCFunction(argument_count, ebx);
	__ mov (Operand(esp, kPointerSize * 0), eax);
	__ CallCFunction(ExternalReference::IMG_FindImgById_function(isolate()), argument_count);
	__ SmiTag(eax);
	context()->Plug(eax);
}

FUN(IMG_Open)
{
	ZoneList<Expression*>* args = expr->arguments();
	ASSERT(args->length == 1);

	VisitForAccumulatorValue(args->at(0));
	__ LoadPointer(eax);

	const int argument_count = 1;

	__ PrepareCallCFunction(argument_count, ebx);
	__ mov (Operand(esp, kPointerSize * 0), eax);
	__ CallCFunction(ExternalReference::IMG_Open_function(isolate()), argument_count);
	__ SmiTag(eax);
	context()->Plug(eax);
}

FUN(IMG_Close)
{
	ZoneList<Expression*>* args = expr->arguments();
	ASSERT(args->length == 1);

	VisitForAccumulatorValue(args->at(0));
	__ ReadInteger(eax);

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
	__ SmiTag(eax);
	context()->Plug(eax);
}

FUN(SYM_Invalid)
{
	const int argument_count = 0;
	__ PrepareCallCFunction(argument_count, ebx);
	__ CallCFunction(ExternalReference::SYM_Invalid_function(isolate()), argument_count);
	__ SmiTag(eax);
	context()->Plug(eax);
}

FUN(IMG_SecHead)
{
	ZoneList<Expression*>* args = expr->arguments();
	ASSERT(args->length == 1);

	VisitForAccumulatorValue(args->at(0));
	__ ReadInteger(eax);

	const int argument_count = 1;

	__ PrepareCallCFunction(argument_count, ebx);
	__ mov (Operand(esp, kPointerSize * 0), eax);
	__ CallCFunction(ExternalReference::IMG_SecHead_function(isolate()), argument_count);
	__ SmiTag(eax);
	context()->Plug(eax);
}

FUN(IMG_SecTail)
{
	ZoneList<Expression*>* args = expr->arguments();
	ASSERT(args->length == 1);

	VisitForAccumulatorValue(args->at(0));
	__ ReadInteger(eax);

	const int argument_count = 1;

	__ PrepareCallCFunction(argument_count, ebx);
	__ mov (Operand(esp, kPointerSize * 0), eax);
	__ CallCFunction(ExternalReference::IMG_SecTail_function(isolate()), argument_count);
	__ SmiTag(eax);
	context()->Plug(eax);
}

FUN(SEC_Next)
{
	ZoneList<Expression*>* args = expr->arguments();
	ASSERT(args->length == 1);

	VisitForAccumulatorValue(args->at(0));
	__ ReadInteger(eax);

	const int argument_count = 1;

	__ PrepareCallCFunction(argument_count, ebx);
	__ mov (Operand(esp, kPointerSize * 0), eax);
	__ CallCFunction(ExternalReference::SEC_Next_function(isolate()), argument_count);
	__ SmiTag(eax);
	context()->Plug(eax);
}

FUN(SEC_Prev)
{
	ZoneList<Expression*>* args = expr->arguments();
	ASSERT(args->length == 1);

	VisitForAccumulatorValue(args->at(0));
	__ ReadInteger(eax);

	const int argument_count = 1;

	__ PrepareCallCFunction(argument_count, ebx);
	__ mov (Operand(esp, kPointerSize * 0), eax);
	__ CallCFunction(ExternalReference::SEC_Prev_function(isolate()), argument_count);
	__ SmiTag(eax);
	context()->Plug(eax);
}

FUN(SEC_Name)
{
	ZoneList<Expression*>* args = expr->arguments();
	ASSERT(args->length == 1);

	VisitForAccumulatorValue(args->at(0));
	__ ReadInteger(eax);

	const int argument_count = 1;

	__ PrepareCallCFunction(argument_count, ebx);
	__ mov (Operand(esp, 0), eax);
	__ CallCFunction(ExternalReference::SEC_Name_function(isolate()), argument_count);
	__ mov (edx, eax);
	__ StorePointer(eax, edx, ecx, ebx);
	context()->Plug(eax);
}

FUN(IMG_RegsymHead)
{
	ZoneList<Expression*>* args = expr->arguments();
	ASSERT(args->length == 1);

	VisitForAccumulatorValue(args->at(0));
	__ ReadInteger(eax);

	const int argument_count = 1;

	__ PrepareCallCFunction(argument_count, ebx);
	__ mov (Operand(esp, kPointerSize * 0), eax);
	__ CallCFunction(ExternalReference::IMG_RegsymHead_function(isolate()), argument_count);
	__ SmiTag(eax);
	context()->Plug(eax);
}

FUN(SYM_Next)
{
	ZoneList<Expression*>* args = expr->arguments();
	ASSERT(args->length == 1);

	VisitForAccumulatorValue(args->at(0));
	__ ReadInteger(eax);

	const int argument_count = 1;

	__ PrepareCallCFunction(argument_count, ebx);
	__ mov (Operand(esp, kPointerSize * 0), eax);
	__ CallCFunction(ExternalReference::SYM_Next_function(isolate()), argument_count);
	__ SmiTag(eax);
	context()->Plug(eax);
}

FUN(SYM_Prev)
{
	ZoneList<Expression*>* args = expr->arguments();
	ASSERT(args->length == 1);

	VisitForAccumulatorValue(args->at(0));
	__ ReadInteger(eax);

	const int argument_count = 1;

	__ PrepareCallCFunction(argument_count, ebx);
	__ mov (Operand(esp, kPointerSize * 0), eax);
	__ CallCFunction(ExternalReference::SYM_Prev_function(isolate()), argument_count);
	__ SmiTag(eax);
	context()->Plug(eax);
}

FUN(SYM_Name)
{
	ZoneList<Expression*>* args = expr->arguments();
	ASSERT(args->length == 1);

	VisitForAccumulatorValue(args->at(0));
	__ ReadInteger(eax);

	const int argument_count = 1;

	__ PrepareCallCFunction(argument_count, ebx);
	__ mov (Operand(esp, 0), eax);
	__ CallCFunction(ExternalReference::SYM_Name_function(isolate()), argument_count);
	__ mov (edx, eax);
	__ StorePointer(eax, edx, ecx, ebx);
	context()->Plug(eax);
}

FUN(RTN_Invalid)
{
	const int argument_count = 0;
	__ PrepareCallCFunction(argument_count, ebx);
	__ CallCFunction(ExternalReference::RTN_Invalid_function(isolate()), argument_count);
	__ SmiTag(eax);
	context()->Plug(eax);
}

FUN(SEC_RtnHead)
{
	ZoneList<Expression*>* args = expr->arguments();
	ASSERT(args->length == 1);

	VisitForAccumulatorValue(args->at(0));
	__ ReadInteger(eax);

	const int argument_count = 1;

	__ PrepareCallCFunction(argument_count, ebx);
	__ mov (Operand(esp, kPointerSize * 0), eax);
	__ CallCFunction(ExternalReference::SEC_RtnHead_function(isolate()), argument_count);
	__ SmiTag(eax);
	context()->Plug(eax);
}

FUN(SEC_RtnTail)
{
	ZoneList<Expression*>* args = expr->arguments();
	ASSERT(args->length == 1);

	VisitForAccumulatorValue(args->at(0));
	__ ReadInteger(eax);

	const int argument_count = 1;

	__ PrepareCallCFunction(argument_count, ebx);
	__ mov (Operand(esp, kPointerSize * 0), eax);
	__ CallCFunction(ExternalReference::SEC_RtnTail_function(isolate()), argument_count);
	__ SmiTag(eax);
	context()->Plug(eax);
}

FUN(RTN_Next)
{
	ZoneList<Expression*>* args = expr->arguments();
	ASSERT(args->length == 1);

	VisitForAccumulatorValue(args->at(0));
	__ ReadInteger(eax);

	const int argument_count = 1;

	__ PrepareCallCFunction(argument_count, ebx);
	__ mov (Operand(esp, kPointerSize * 0), eax);
	__ CallCFunction(ExternalReference::RTN_Next_function(isolate()), argument_count);
	__ SmiTag(eax);
	context()->Plug(eax);
}

FUN(RTN_Prev)
{
	ZoneList<Expression*>* args = expr->arguments();
	ASSERT(args->length == 1);

	VisitForAccumulatorValue(args->at(0));
	__ ReadInteger(eax);

	const int argument_count = 1;

	__ PrepareCallCFunction(argument_count, ebx);
	__ mov (Operand(esp, kPointerSize * 0), eax);
	__ CallCFunction(ExternalReference::RTN_Prev_function(isolate()), argument_count);
	__ SmiTag(eax);
	context()->Plug(eax);
}

FUN(RTN_Name)
{
	ZoneList<Expression*>* args = expr->arguments();
	ASSERT(args->length == 1);

	VisitForAccumulatorValue(args->at(0));
	__ ReadInteger(eax);

	const int argument_count = 1;

	__ PrepareCallCFunction(argument_count, ebx);
	__ mov (Operand(esp, 0), eax);
	__ CallCFunction(ExternalReference::RTN_Name_function(isolate()), argument_count);
	__ mov (edx, eax);
	__ StorePointer(eax, edx, ecx, ebx);
	context()->Plug(eax);
}

FUN(INS_Invalid)
{
	const int argument_count = 0;
	__ PrepareCallCFunction(argument_count, ebx);
	__ CallCFunction(ExternalReference::INS_Invalid_function(isolate()), argument_count);
	__ SmiTag(eax);
	context()->Plug(eax);
}

FUN(RTN_InsHead)
{
	ZoneList<Expression*>* args = expr->arguments();
	ASSERT(args->length == 1);

	VisitForAccumulatorValue(args->at(0));
	__ ReadInteger(eax);

	const int argument_count = 1;

	__ PrepareCallCFunction(argument_count, ebx);
	__ mov (Operand(esp, kPointerSize * 0), eax);
	__ CallCFunction(ExternalReference::RTN_InsHead_function(isolate()), argument_count);
	__ SmiTag(eax);
	context()->Plug(eax);
}

FUN(RTN_InsHeadOnly)
{
	ZoneList<Expression*>* args = expr->arguments();
	ASSERT(args->length == 1);

	VisitForAccumulatorValue(args->at(0));
	__ ReadInteger(eax);

	const int argument_count = 1;

	__ PrepareCallCFunction(argument_count, ebx);
	__ mov (Operand(esp, kPointerSize * 0), eax);
	__ CallCFunction(ExternalReference::RTN_InsHeadOnly_function(isolate()), argument_count);
	__ SmiTag(eax);
	context()->Plug(eax);
}

FUN(RTN_InsTail)
{
	ZoneList<Expression*>* args = expr->arguments();
	ASSERT(args->length == 1);

	VisitForAccumulatorValue(args->at(0));
	__ ReadInteger(eax);

	const int argument_count = 1;

	__ PrepareCallCFunction(argument_count, ebx);
	__ mov (Operand(esp, kPointerSize * 0), eax);
	__ CallCFunction(ExternalReference::RTN_InsTail_function(isolate()), argument_count);
	__ SmiTag(eax);
	context()->Plug(eax);
}

FUN(INS_Next)
{
	ZoneList<Expression*>* args = expr->arguments();
	ASSERT(args->length == 1);

	VisitForAccumulatorValue(args->at(0));
	__ ReadInteger(eax);

	const int argument_count = 1;

	__ PrepareCallCFunction(argument_count, ebx);
	__ mov (Operand(esp, kPointerSize * 0), eax);
	__ CallCFunction(ExternalReference::INS_Next_function(isolate()), argument_count);
	__ SmiTag(eax);
	context()->Plug(eax);
}

FUN(INS_Prev)
{
	ZoneList<Expression*>* args = expr->arguments();
	ASSERT(args->length == 1);

	VisitForAccumulatorValue(args->at(0));
	__ ReadInteger(eax);

	const int argument_count = 1;

	__ PrepareCallCFunction(argument_count, ebx);
	__ mov (Operand(esp, kPointerSize * 0), eax);
	__ CallCFunction(ExternalReference::INS_Prev_function(isolate()), argument_count);
	__ SmiTag(eax);
	context()->Plug(eax);
}

FUN(INS_Disassemble)
{
	ZoneList<Expression*>* args = expr->arguments();
	ASSERT(args->length == 2);

	VisitForAccumulatorValue(args->at(0));
	VisitForStackValue(args->at(1));        //wrapped empty string buffer
	__ ReadInteger(eax);
	__ pop(edx);
	__ LoadPointer(edx);

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
	__ ReadInteger(eax);

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
	__ ReadInteger(eax);

	const int argument_count = 1;

	__ PrepareCallCFunction(argument_count, ebx);
	__ mov (Operand(esp, 0), eax);
	__ CallCFunction(ExternalReference::RTN_Close_function(isolate()), argument_count);
}

FUN(RTN_FindNameByAddress)
{
	ZoneList<Expression*>* args = expr->arguments();
	ASSERT(args->length == 2);

	VisitForAccumulatorValue(args->at(1));        //wrapped empty string buffer
	__ mov(edx, eax);
	__ LoadPointer(edx);
	VisitForAccumulatorValue(args->at(0));
	__ LoadPointer(eax);

	const int argument_count = 2;

	__ PrepareCallCFunction(argument_count, ebx);
	__ mov (Operand(esp, kPointerSize * 0), edx);
	__ mov (Operand(esp, kPointerSize * 1), eax);
	__ CallCFunction(ExternalReference::RTN_FindNameByAddress_function(isolate()), argument_count);
}

FUN(RTN_FindByAddress)
{
	ZoneList<Expression*>* args = expr->arguments();
	ASSERT(args->length == 1);

	VisitForAccumulatorValue(args->at(0));
	__ LoadPointer(eax);

	const int argument_count = 1;

	__ PrepareCallCFunction(argument_count, ebx);
	__ mov (Operand(esp, 0), eax);
	__ CallCFunction(ExternalReference::RTN_FindByAddress_function(isolate()), argument_count);
	__ SmiTag(eax);
	context()->Plug(eax);
}

FUN(INS_Address)
{
	ZoneList<Expression*>* args = expr->arguments();
	ASSERT(args->length == 1);

	VisitForAccumulatorValue(args->at(0));
	__ ReadInteger(eax);

	const int argument_count = 1;

	__ PrepareCallCFunction(argument_count, ebx);
	__ mov (Operand(esp, 0), eax);
	__ CallCFunction(ExternalReference::INS_Address_function(isolate()), argument_count);
	__ mov (edx, eax);
	__ StorePointer(eax, edx, ecx, ebx);
	context()->Plug(eax);
}

FUN(INS_Size)
{
	ZoneList<Expression*>* args = expr->arguments();
	ASSERT(args->length == 1);

	VisitForAccumulatorValue(args->at(0));
	__ ReadInteger(eax);

	const int argument_count = 1;

	__ PrepareCallCFunction(argument_count, ebx);
	__ mov (Operand(esp, 0), eax);
	__ CallCFunction(ExternalReference::INS_Size_function(isolate()), argument_count);
	__ SmiTag(eax);
	context()->Plug(eax);
}

//[addr, name, output]
//XXX: seems broken on Pin 2.12, it lacks a string ptr deref
FUN(RTN_CreateAt)
{
	ZoneList<Expression*>* args = expr->arguments();
	ASSERT(args->length == 2);

	VisitForAccumulatorValue(args->at(0));
	VisitForStackValue(args->at(1));
	__ ReadInteger(eax);
	__ pop(edx);
	__ LoadPointer(edx);

	const int argument_count = 2;

	__ PrepareCallCFunction(argument_count, ebx);
	__ mov (Operand(esp, kPointerSize * 0), eax);
	__ mov (Operand(esp, kPointerSize * 1), edx);
	__ CallCFunction(ExternalReference::RTN_CreateAt_function(isolate()), argument_count);
	__ mov(ecx, eax);
	__ CreateInteger(ecx, eax, edi, ebx);
	context()->Plug(eax);
}

FUN(PIN_SaveContext)
{
	ZoneList<Expression*>* args = expr->arguments();
	ASSERT(args->length == 2);

	VisitForAccumulatorValue(args->at(1));
	__ mov(edx, eax);
	__ LoadPointer(edx);
	VisitForAccumulatorValue(args->at(0));
	__ LoadPointer(eax);

	const int argument_count = 2;

	__ PrepareCallCFunction(argument_count, ebx);
	__ mov (Operand(esp, kPointerSize * 0), eax);
	__ mov (Operand(esp, kPointerSize * 1), edx);
	__ CallCFunction(ExternalReference::PIN_SaveContext_function(isolate()), argument_count);
}

FUN(PIN_ContextContainsState)
{
	ZoneList<Expression*>* args = expr->arguments();
	ASSERT(args->length == 2);

	Label materialize_true, materialize_false;
	Label *if_true = NULL;
	Label *if_false = NULL;
	Label *fall_through = NULL;

	VisitForAccumulatorValue(args->at(1));
	__ mov(edx,eax);
	__ ReadInteger(edx);
	VisitForAccumulatorValue(args->at(0));
	__ LoadPointer(eax);

	const int argument_count = 2;

	__ PrepareCallCFunction(argument_count, ebx);
	__ mov (Operand(esp, kPointerSize * 0), eax);
	__ mov (Operand(esp, kPointerSize * 1), edx);
	__ CallCFunction(ExternalReference::PIN_ContextContainsState_function(isolate()), argument_count);

	context()->PrepareTest(&materialize_true, &materialize_false, &if_true, &if_false, &fall_through);
	PrepareForBailoutBeforeSplit(expr, true, if_true, if_false);
	__ test(eax, eax);
	Split(not_zero, if_true, if_false, fall_through);
	context()->Plug(if_true, if_false);
}

FUN(PIN_GetContextReg)
{
	ZoneList<Expression*>* args = expr->arguments();
	ASSERT(args->length == 2);

	VisitForAccumulatorValue(args->at(1));
	__ mov(edx, eax);
	__ ReadInteger(edx);  //REG is an enum
	VisitForAccumulatorValue(args->at(0));
	__ LoadPointer(eax);

	const int argument_count = 2;

	__ PrepareCallCFunction(argument_count, ebx);
	__ mov (Operand(esp, kPointerSize * 0), eax);
	__ mov (Operand(esp, kPointerSize * 1), edx);
	__ CallCFunction(ExternalReference::PIN_GetContextReg_function(isolate()), argument_count);
	__ mov (edx, eax);
	__ StorePointer(eax, edx, ecx, ebx);
	context()->Plug(eax);
}

FUN(PIN_SetContextReg)
{
	ZoneList<Expression*>* args = expr->arguments();
	ASSERT(args->length == 3);

	VisitForAccumulatorValue(args->at(2));
	__ mov(ecx,eax);  //arg2
	__ ReadInteger(ecx);
	VisitForAccumulatorValue(args->at(1));
	__ mov(edx,eax);  //arg1
	__ ReadInteger(edx);
	VisitForAccumulatorValue(args->at(0));
	__ LoadPointer(eax);

	const int argument_count = 3;

	__ PrepareCallCFunction(argument_count, ebx);
	__ mov (Operand(esp, kPointerSize * 0), eax);
	__ mov (Operand(esp, kPointerSize * 1), edx);
	__ mov (Operand(esp, kPointerSize * 2), ecx);
	__ CallCFunction(ExternalReference::PIN_SetContextReg_function(isolate()), argument_count);
}

FUN(PIN_ClaimToolRegister)
{
	const int argument_count = 0;
	__ PrepareCallCFunction(argument_count, ebx);
	__ CallCFunction(ExternalReference::PIN_ClaimToolRegister_function(isolate()), argument_count);
	__ SmiTag(eax);
	context()->Plug(eax);
}

FUN(REG_FullRegName)
{
	ZoneList<Expression*>* args = expr->arguments();
	ASSERT(args->length == 1);

	VisitForAccumulatorValue(args->at(0));
	__ ReadInteger(eax);

	const int argument_count = 1;

	__ PrepareCallCFunction(argument_count, ebx);
	__ mov (Operand(esp, 0), eax);
	__ CallCFunction(ExternalReference::REG_FullRegName_function(isolate()), argument_count);
	__ mov (edx, eax);
	__ StorePointer(eax, edx, ecx, ebx);
	context()->Plug(eax);
}

FUN(REG_StringShort)
{
	ZoneList<Expression*>* args = expr->arguments();
	ASSERT(args->length == 2);

	VisitForAccumulatorValue(args->at(1));        //wrapped empty string buffer
	__ mov(edx, eax);
	__ LoadPointer(edx);
	VisitForAccumulatorValue(args->at(0));
	__ ReadInteger(eax);

	const int argument_count = 2;

	__ PrepareCallCFunction(argument_count, ebx);
	__ mov (Operand(esp, kPointerSize * 0), edx);
	__ mov (Operand(esp, kPointerSize * 1), eax);
	__ CallCFunction(ExternalReference::REG_StringShort_function(isolate()), argument_count);
}

FUN(TRACE_Rtn)
{
	ZoneList<Expression*>* args = expr->arguments();
	ASSERT(args->length == 1);

	VisitForAccumulatorValue(args->at(0));
	__ LoadPointer(eax);

	const int argument_count = 1;

	__ PrepareCallCFunction(argument_count, ebx);
	__ mov (Operand(esp, 0), eax);
	__ CallCFunction(ExternalReference::TRACE_Rtn_function(isolate()), argument_count);
	__ SmiTag(eax);
	context()->Plug(eax);
}

FUN(TRACE_Address)
{
	ZoneList<Expression*>* args = expr->arguments();
	ASSERT(args->length == 1);

	VisitForAccumulatorValue(args->at(0));
	__ LoadPointer(eax);

	const int argument_count = 1;

	__ PrepareCallCFunction(argument_count, ebx);
	__ mov (Operand(esp, 0), eax);
	__ CallCFunction(ExternalReference::TRACE_Address_function(isolate()), argument_count);
	__ mov (edx, eax);
	__ StorePointer(eax, edx, ecx, ebx);
	context()->Plug(eax);
}

FUN(TRACE_Size)
{
	ZoneList<Expression*>* args = expr->arguments();
	ASSERT(args->length == 1);

	VisitForAccumulatorValue(args->at(0));
	__ LoadPointer(eax);

	const int argument_count = 1;

	__ PrepareCallCFunction(argument_count, ebx);
	__ mov (Operand(esp, 0), eax);
	__ CallCFunction(ExternalReference::TRACE_Size_function(isolate()), argument_count);
	__ SmiTag(eax);
	context()->Plug(eax);
}

FUN(PIN_RemoveInstrumentation)
{
	const int argument_count = 0;
	__ PrepareCallCFunction(argument_count, ebx);
	__ CallCFunction(ExternalReference::PIN_RemoveInstrumentation_function(isolate()), argument_count);
}

FUN(PIN_WaitForThreadTermination)
{
	ZoneList<Expression*>* args = expr->arguments();
	ASSERT(args->length == 2);

	Label materialize_true, materialize_false;
	Label *if_true = NULL;
	Label *if_false = NULL;
	Label *fall_through = NULL;

	VisitForAccumulatorValue(args->at(1));
	__ mov(edx,eax);
	__ ReadInteger(edx);
	VisitForAccumulatorValue(args->at(0));
	__ LoadPointer(eax);

	const int argument_count = 3;

	__ PrepareCallCFunction(argument_count, ebx);
	__ mov (Operand(esp, kPointerSize * 0), eax);
	__ mov (Operand(esp, kPointerSize * 1), edx);
	__ mov (Operand(esp, kPointerSize * 2), Immediate(0));
	__ CallCFunction(ExternalReference::PIN_WaitForThreadTermination_function(isolate()), argument_count);

	context()->PrepareTest(&materialize_true, &materialize_false, &if_true, &if_false, &fall_through);
	PrepareForBailoutBeforeSplit(expr, true, if_true, if_false);
	__ test(eax, eax);
	Split(not_zero, if_true, if_false, fall_through);
	context()->Plug(if_true, if_false);
}

FUN(TRACE_BblHead)
{
	ZoneList<Expression*>* args = expr->arguments();
	ASSERT(args->length == 1);

	VisitForAccumulatorValue(args->at(0));
	__ LoadPointer(eax);

	const int argument_count = 1;

	__ PrepareCallCFunction(argument_count, ebx);
	__ mov (Operand(esp, kPointerSize * 0), eax);
	__ CallCFunction(ExternalReference::TRACE_BblHead_function(isolate()), argument_count);
	__ SmiTag(eax);
	context()->Plug(eax);
}

FUN(TRACE_BblTail)
{
	ZoneList<Expression*>* args = expr->arguments();
	ASSERT(args->length == 1);

	VisitForAccumulatorValue(args->at(0));
	__ LoadPointer(eax);

	const int argument_count = 1;

	__ PrepareCallCFunction(argument_count, ebx);
	__ mov (Operand(esp, kPointerSize * 0), eax);
	__ CallCFunction(ExternalReference::TRACE_BblTail_function(isolate()), argument_count);
	__ SmiTag(eax);
	context()->Plug(eax);
}

FUN(BBL_Next)
{
	ZoneList<Expression*>* args = expr->arguments();
	ASSERT(args->length == 1);

	VisitForAccumulatorValue(args->at(0));
	__ ReadInteger(eax);

	const int argument_count = 1;

	__ PrepareCallCFunction(argument_count, ebx);
	__ mov (Operand(esp, kPointerSize * 0), eax);
	__ CallCFunction(ExternalReference::BBL_Next_function(isolate()), argument_count);
	__ SmiTag(eax);
	context()->Plug(eax);
}

FUN(BBL_Prev)
{
	ZoneList<Expression*>* args = expr->arguments();
	ASSERT(args->length == 1);

	VisitForAccumulatorValue(args->at(0));
	__ ReadInteger(eax);

	const int argument_count = 1;

	__ PrepareCallCFunction(argument_count, ebx);
	__ mov (Operand(esp, kPointerSize * 0), eax);
	__ CallCFunction(ExternalReference::BBL_Prev_function(isolate()), argument_count);
	__ SmiTag(eax);
	context()->Plug(eax);
}

FUN(BBL_Valid)
{
	ZoneList<Expression*>* args = expr->arguments();
	ASSERT(args->length == 1);

	Label materialize_true, materialize_false;
	Label *if_true = NULL;
	Label *if_false = NULL;
	Label *fall_through = NULL;

	VisitForAccumulatorValue(args->at(0));
	__ ReadInteger(eax);

	const int argument_count = 1;

	__ PrepareCallCFunction(argument_count, ebx);
	__ mov (Operand(esp, 0), eax);
	__ CallCFunction(ExternalReference::BBL_Valid_function(isolate()), argument_count);

	context()->PrepareTest(&materialize_true, &materialize_false, &if_true, &if_false, &fall_through);
	PrepareForBailoutBeforeSplit(expr, true, if_true, if_false);
	__ test(eax, eax);
	Split(not_zero, if_true, if_false, fall_through);
	context()->Plug(if_true, if_false);
}

FUN(BBL_InsHead)
{
	ZoneList<Expression*>* args = expr->arguments();
	ASSERT(args->length == 1);

	VisitForAccumulatorValue(args->at(0));
	__ ReadInteger(eax);

	const int argument_count = 1;

	__ PrepareCallCFunction(argument_count, ebx);
	__ mov (Operand(esp, kPointerSize * 0), eax);
	__ CallCFunction(ExternalReference::BBL_InsHead_function(isolate()), argument_count);
	__ SmiTag(eax);
	context()->Plug(eax);
}

FUN(BBL_InsTail)
{
	ZoneList<Expression*>* args = expr->arguments();
	ASSERT(args->length == 1);

	VisitForAccumulatorValue(args->at(0));
	__ ReadInteger(eax);

	const int argument_count = 1;

	__ PrepareCallCFunction(argument_count, ebx);
	__ mov (Operand(esp, kPointerSize * 0), eax);
	__ CallCFunction(ExternalReference::BBL_InsTail_function(isolate()), argument_count);
	__ SmiTag(eax);
	context()->Plug(eax);
}

FUN(BBL_Address)
{
	ZoneList<Expression*>* args = expr->arguments();
	ASSERT(args->length == 1);

	VisitForAccumulatorValue(args->at(0));
	__ ReadInteger(eax);

	const int argument_count = 1;

	__ PrepareCallCFunction(argument_count, ebx);
	__ mov (Operand(esp, kPointerSize * 0), eax);
	__ CallCFunction(ExternalReference::BBL_Address_function(isolate()), argument_count);
	__ mov (edx, eax);
	__ StorePointer(eax, edx, ecx, ebx);
	context()->Plug(eax);
}

FUN(BBL_Size)
{
	ZoneList<Expression*>* args = expr->arguments();
	ASSERT(args->length == 1);

	VisitForAccumulatorValue(args->at(0));
	__ ReadInteger(eax);

	const int argument_count = 1;

	__ PrepareCallCFunction(argument_count, ebx);
	__ mov (Operand(esp, kPointerSize * 0), eax);
	__ CallCFunction(ExternalReference::BBL_Size_function(isolate()), argument_count);
	__ SmiTag(eax);
	context()->Plug(eax);
}

#undef FUN
#endif
