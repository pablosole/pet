#include "pintool.h"

Persistent<Function> AnalysisFunctionFastCache[kFastCacheSize];

bool PinContext::CreateJSContext()
{
	DEBUG("create new context for tid:" << tid);

	Lock();

	isolate = Isolate::New();
	if (isolate != 0)
	{
		//Enter and lock the new isolate
		Isolate::Scope iscope(isolate);
		Locker lock(isolate);
		HandleScope hscope;

		Handle<ObjectTemplate> global_templ = ObjectTemplate::New();
		global_templ->SetInternalFieldCount(1);

		context = Context::New(NULL, global_templ);

		if (!context.IsEmpty()) {
			//this is the "receiver" argument for Invoke, it's the "this" of the function called
			Context::Scope cscope(context);
			i::Object** ctx = reinterpret_cast<i::Object**>(*context);
			i::Handle<i::Context> internal_context = i::Handle<i::Context>::cast(i::Handle<i::Object>(ctx));
			i::GlobalObject *internal_global = internal_context->global();
			i::Handle<i::JSObject> receiver(internal_global->global_receiver());
			Local<Object> receiver_local(Utils::ToLocal(receiver));
			global = Persistent<Object>::New(receiver_local);

			sorrowctx = new SorrowContext(0, NULL);
		}
		//Scope and Locker are destroyed here.
	}

	if (context.IsEmpty())
		SetState(ERROR_CONTEXT);
	else
		SetState(INITIALIZED_CONTEXT);

	Unlock();
	Notify();

	return GetState() == INITIALIZED_CONTEXT;
}

void PinContext::DestroyJSContext()
{
	DEBUG("destroy context for tid:" << tid);

	Lock();

	if (isolate != 0)
	{
		{
			Isolate::Scope iscope(isolate);
			Locker lock(isolate);

			if (!context.IsEmpty())
			{
				sorrowctx->FireExit();

				V8::TerminateExecution(isolate);

				if (GetTid() <= kFastCacheMaxTid) {
					for (int x=0; x <= kFastCacheMaxFuncId; x++) {
						Persistent<Function>& tmp = AnalysisFunctionFastCache[GetFastCacheIndex(x)];
						if (!tmp.IsEmpty()) {
							tmp.Dispose();
							tmp.Clear();
						}
					}
				}

				context.Dispose();
				context.Clear();
				V8::ContextDisposedNotification();
			}
		}

		isolate->Dispose();
	}

	SetState(DEAD_CONTEXT);
	Unlock();
	Notify();
}

VOID PinContext::ThreadInfoDestructor(VOID *v)
{
	PinContext *context = static_cast<PinContext *>(v);

	if (!PinContext::IsValid(context))
		return;

	DEBUG("set context state to kill");
	context->Lock();
	context->SetState(PinContext::KILLING_CONTEXT);
	context->Unlock();
	context->Notify();
}

Persistent<Function> PinContext::EnsureFunction(AnalysisFunction *af)
{
	//Encode tid and funcid info and see if it fits in the fast cache, go the slow way if not.
	unsigned int index = GetFastCacheIndex(af->GetFuncId());
	if (index != kInvalidFastCacheIndex) {
		Persistent<Function> tmp = AnalysisFunctionFastCache[index];
		if (!tmp.IsEmpty())
			return tmp;
	}
	return EnsureFunctionSlow(af);
}

Persistent<Function> PinContext::EnsureFunctionSlow(AnalysisFunction *af)
{
	FunctionsCacheMap::const_iterator it;

	it = funcache.find(af->GetHash());
	if (it != funcache.end())
		return it->second;

	DEBUG("EnsureFunction hash:" << af->GetHash());

	HandleScope scope;
	Handle<String> source = String::Concat(String::Concat(String::New("("), String::New(af->GetBody().c_str())), String::New(")"));

	TryCatch trycatch;
	Handle<Script> script = Script::Compile(source);
	if (trycatch.HasCaught()) {
		DEBUG("Exception compiling on TID:" << GetTid());
		af->SetLastException(ReportExceptionToString(&trycatch));
		DEBUG(af->GetLastException());
		//If the function is broken, disable the execution of this AF
		af->Disable();
		return Persistent<Function>();
	}

	Handle<Value> fun_val = script->Run();
	if (!fun_val->IsFunction()) {
		DEBUG("Function body didn't create a function on TID:" << GetTid());
		af->SetLastException(string("Function body didn't create a function."));
		//If the function is broken, disable the execution of this AF
		af->Disable();
		return Persistent<Function>();
	}

	Persistent<Function> newfun = Persistent<Function>::New(Handle<Function>::Cast(fun_val));

	//If the function is set with a name (non-anonymous function) make it available globally.
	Handle<Value> name_val = newfun->GetName();
	if (!name_val.IsEmpty()) {
		Handle<String> name(String::Cast(*name_val));
		GetContext()->Global()->Set(name, fun_val);
	}

	const string& init = af->GetInit();
	if (!init.empty()) {
		source = String::New(init.c_str());
		script = Script::Compile(source);
		if (trycatch.HasCaught()) {
			af->SetLastException(ReportExceptionToString(&trycatch));
			DEBUG(af->GetLastException());
			//If the function is broken, disable the execution of this AF
			af->Disable();
		} else {
			script->Run();
			if (trycatch.HasCaught()) {
				af->SetLastException(ReportExceptionToString(&trycatch));
				DEBUG(af->GetLastException());
				//If the function is broken, disable the execution of this AF
				af->Disable();
			}
		}
	}

	funcache[af->GetHash()] = newfun;
	unsigned int index = GetFastCacheIndex(af->GetFuncId());
	if (index != kInvalidFastCacheIndex)
		AnalysisFunctionFastCache[index] = newfun;

	return newfun;
}
