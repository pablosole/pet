#include "pintool.h"

bool PinContext::CreateJSContext()
{
	DEBUG("Create JS Context for " << tid);

	Lock();

	ctxswitch = new PinContextSwitch((uintptr_t)&ConstructInsertCallProxy, 1);
	ctxswitch->args[1] = (uintptr_t)this;
	SetState(INITIALIZED_CONTEXT);

	Unlock();

	return true;
}

void PinContext::DestroyInsertCallProxy()
{
	{
		Isolate::Scope iscope(isolate);
		Unlocker unlock(isolate);
		{
			Locker lock(isolate);
			DEBUG("Calling AF destructors for TID=" << GetTid() << " on TID=" << PIN_ThreadId());
			PinContextSwitch *buffer = GetContextSwitchBuffer();
			buffer->passbuffer[0] = PinContextSwitch::FINISH_CTX;
			EnterContext(buffer);
		}
	}

	isolate->Dispose();
}

void PinContext::DestroyJSContext()
{
	DEBUG("Destroy context for tid:" << tid);

	Lock();
	if (GetState() == DEAD_CONTEXT) {
		Unlock();
		return;
	}

	SetState(DEAD_CONTEXT);

	if (isolate != 0)
	{
		DestroyInsertCallProxy();

		delete ctxswitch;
	}

	Unlock();
	Notify();
}

VOID PinContext::ThreadInfoDestructor(VOID *v)
{
	PinContext *context = static_cast<PinContext *>(v);

	if (!PinContext::IsValid(context))
		return;

	DEBUG("Queued context kill for TID " << context->GetTid());
	context->Lock();
	context->SetState(PinContext::KILLING_CONTEXT);
	context->Unlock();

	context->DestroyJSContext();
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
