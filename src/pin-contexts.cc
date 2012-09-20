#include "pintool.h"

Persistent<Function> AnalysisFunctionFastCache[kFastCacheSize];

bool PinContext::CreateJSContext()
{
	DEBUG("Create JS Context for " << tid);

	Lock();

	isolate = Isolate::New();
	if (isolate != 0)
	{
		//Enter and lock the new isolate
		Isolate::Scope iscope(isolate);
		Locker lock(isolate);
		HandleScope hscope;

		context = Context::New();

		if (!context.IsEmpty()) {
			//this is the "receiver" argument for Invoke, it's the "this" of the function called
			Context::Scope cscope(context);

			Local<ObjectTemplate> objt = ObjectTemplate::New();
			objt->SetInternalFieldCount(1);
			Local<Object> obj = objt->NewInstance();
			context->Global()->SetHiddenValue(String::New("SorrowInstance"), obj);

			sorrowctx = new SorrowContext(0, NULL);

			i::Object** ctx = reinterpret_cast<i::Object**>(*context);
			i::Handle<i::Context> internal_context = i::Handle<i::Context>::cast(i::Handle<i::Object>(ctx));
			i::GlobalObject *internal_global = internal_context->global();
			i::Handle<i::JSObject> receiver(internal_global->global_receiver());
			Local<Object> receiver_local(Utils::ToLocal(receiver));
			global = Persistent<Object>::New(receiver_local);
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
	DEBUG("Destroy context for tid:" << tid);

	Lock();
	if (GetState() == DEAD_CONTEXT) {
		Unlock();
		return;
	}

	SetState(DEAD_CONTEXT);

	if (isolate != 0)
	{
		{
			Isolate::Scope iscope(isolate);
			Locker lock(isolate);

			if (!context.IsEmpty())
			{
				{
					HandleScope hscope;
					Context::Scope cscope(context);
					AnalysisFunction *af;
					TryCatch trycatch;

					//Call the AF destructors for all Ensured functions
					for (unsigned int idx=0; idx < ctxmgr->GetLastFunctionId(); idx++) {
						af = ctxmgr->GetFunction(idx);

						//check if the function was actually used (cached)
						if (af && funcache.find(af->GetHash()) != funcache.end()) {
							const string& dtor = af->GetDtor();
							if (!dtor.empty()) {
								DEBUG("Calling AF Destructor for AF Hash:" << af->GetHash() << " on TID:" << GetTid());
								sorrowctx->LoadScript(dtor.c_str(), dtor.size());
							}
						}
					}
				}

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

				delete sorrowctx;

				global.Dispose();
				context.Dispose();
				V8::ContextDisposedNotification();
			}
		}

		isolate->Dispose();
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
		DEBUG(ReportExceptionToString(&trycatch));
		KillPinTool();
	}

	Handle<Value> fun_val = script->Run();
	if (!fun_val->IsFunction()) {
		DEBUG("Function body didn't create a function on TID:" << GetTid());
		KillPinTool();
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
		sorrowctx->LoadScript(init.c_str(), init.size());
	}

	funcache[af->GetHash()] = newfun;
	unsigned int index = GetFastCacheIndex(af->GetFuncId());
	if (index != kInvalidFastCacheIndex)
		AnalysisFunctionFastCache[index] = newfun;

	return newfun;
}
