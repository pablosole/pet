#include "pintool.h"

VOID ConstructJSProxy(PinContextSwitch *buffer, PinContext *pincontext)
{
	Isolate *isolate = Isolate::New();
	Persistent<Context> context;

	if (isolate)
	{
		//Enter and lock the new isolate
		Isolate::Scope iscope(isolate);
		Locker lock(isolate);
		HandleScope hscope;

		context = Context::New();

		if (!context.IsEmpty()) {
			Context::Scope cscope(context);

			Local<ObjectTemplate> objt = ObjectTemplate::New();
			objt->SetInternalFieldCount(1);
			Local<Object> obj = objt->NewInstance();
			context->Global()->SetHiddenValue(String::New("SorrowInstance"), obj);

			SorrowContext *sorrowctx = new SorrowContext(0, NULL);

			pincontext->SetIsolate(isolate);
			pincontext->SetContext(context);
			pincontext->SetSorrowContext(sorrowctx);

			string source = "AfSetup(";
			char buf[30] = {0};
			sprintf(buf, "%u", (uint32_t)buffer);
			source += buf;
			source += ", require);";

			sorrowctx->LoadScript(source.c_str(), source.size());

			//we get here after a FINISH_CTX action is received
			V8::TerminateExecution(isolate);
			delete sorrowctx;
		} else {
			DEBUG("Failed to create Context for TID " << PIN_ThreadId());
			KillPinTool();
		}
		//Scope and Locker are destroyed here.
	} else {
		DEBUG("Failed to create Isolate for TID " << PIN_ThreadId());
		KillPinTool();
	}

	context.Dispose();
	V8::ContextDisposedNotification();

	ReturnContext(buffer);
	//never reaches here
}

PinContext::PinContext(THREADID _tid) :
ctxswitch((uintptr_t)&ConstructJSProxy, 1)
{
	DEBUG("Create JS Context for " << tid);

	tid = _tid;
	context.Clear();
	isolate = 0;
	sorrowctx = 0;
	afctorscalled = false;

	//args[0] is set automatically to point to the PinContextSwitch instance
	ctxswitch.args[1] = (uintptr_t)this;
}

PinContext::~PinContext() {
	DEBUG("Destroy JS Context for " << tid);

	if (isolate != 0)
	{
		//Get ownership of the isolate to call the destructors
		{
			Isolate::Scope iscope(isolate);
			Unlocker unlock(isolate);
			{
				Locker lock(isolate);
				DEBUG("Calling AF destructors for TID=" << GetTid() << " on TID=" << PIN_ThreadId());
				struct PinContextAction *action = ctxswitch.AllocateAction();
				action->type = PinContextAction::FINISH_CTX;
				EnterContext(&ctxswitch);
			}
		}

		isolate->Dispose();
	}
}


VOID PinContext::ThreadInfoDestructor(VOID *v)
{
	PinContext *context = static_cast<PinContext *>(v);

	if (!PinContext::IsValid(context))
		return;

	ctxmgr->DestroyContext(context->GetTid());
}

void __declspec(naked) __stdcall ReturnContext(PinContextSwitch *buffer) {
	__asm {
		MOV eax, [esp+4]
		MOV [eax]PinContextSwitch.newstate[0], ebx
		MOV [eax]PinContextSwitch.newstate[4], esi
		MOV [eax]PinContextSwitch.newstate[8], edi
		MOV [eax]PinContextSwitch.newstate[12], ebp

		//ret address
		MOV ebx, [esp] 
		MOV [eax]PinContextSwitch.neweip, ebx

		//simulate a ret 4
		add esp, 8
		MOV [eax]PinContextSwitch.newstate[16], esp

		//switch to the saved stack and registers
		MOV ebx, [eax]PinContextSwitch.savedstate[0]
		MOV esi, [eax]PinContextSwitch.savedstate[4]
		MOV edi, [eax]PinContextSwitch.savedstate[8]
		MOV ebp, [eax]PinContextSwitch.savedstate[12]
		MOV esp, [eax]PinContextSwitch.savedstate[16]

		ret 4
	}
}

void __declspec(naked) __stdcall EnterContext(PinContextSwitch *buffer) {
	__asm {
		MOV eax, [esp+4]
		MOV [eax]PinContextSwitch.savedstate[0], ebx
		MOV [eax]PinContextSwitch.savedstate[4], esi
		MOV [eax]PinContextSwitch.savedstate[8], edi
		MOV [eax]PinContextSwitch.savedstate[12], ebp
		MOV [eax]PinContextSwitch.savedstate[16], esp

		//switch to the new stack and registers
		MOV ebx, [eax]PinContextSwitch.newstate[0]
		MOV esi, [eax]PinContextSwitch.newstate[4]
		MOV edi, [eax]PinContextSwitch.newstate[8]
		MOV ebp, [eax]PinContextSwitch.newstate[12]
		MOV esp, [eax]PinContextSwitch.newstate[16]

		JMP [eax]PinContextSwitch.neweip
	}
}

PinContextSwitch::PinContextSwitch(uintptr_t fptr, uint32_t num) {
	memset(stack, 0, sizeof(stack));
	memset(actions, 0, sizeof(actions));
	current_action = 0;

	//ESP points to the end of our stack
	neweip = fptr;
	num_args = num+1;
	args = &stack[0xFFFF-num_args];
	args[0] = (uintptr_t)this;
	newstate[4] = (uintptr_t)&stack[0xFFFF-num_args-1];
}

struct PinContextAction *PinContextSwitch::AllocateAction() {
	long action = WINDOWS::InterlockedIncrement(&current_action);

	if (action == kMaxContextActions) {
		DEBUG("No more space for PinContextActions available");
		KillPinTool();
	}

	return &actions[action-1];
}
