// copyright 2012 sam l'ecuyer
#include <v8.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "sorrow.h"
#include "sorrow_natives.h"
#include <pintool.h>

namespace sorrow {
	using namespace v8;

	void LoadNativeLibraries(Handle<Object> libs) {
		HandleScope scope;
		for (int i = 1; natives[i].name; i++) {
			Local<String> name = V8_STR(natives[i].name);
			Handle<String> source = String::New(natives[i].source, natives[i].source_len);
			libs->Set(name, source);
		}
	} // LoadNativeLibraries

	void SorrowContext::FireExit() {
		HandleScope scope;
		TryCatch tryCatch;
		Local<Value> func = internals->Get(V8_STR("fire"));
		
		if (tryCatch.HasCaught() || func == Undefined()) {
			return;
		}
		ASSERT_PIN(func->IsFunction(), "FireExit");
		Local<Function> f = Local<Function>::Cast(func);
        
		Local<Object> global = Context::GetCurrent()->Global();
		Local<Value> args[1] = { V8_STR("exit") };
		f->Call(global, 1, args);
	}

	V8_FUNCTN(Quit) {
		ctxmgr->Abort();
		return Undefined();
	} // Quit

	V8_FUNCTN(Version) {
		return V8_STR(V8::GetVersion());
	} // Version	

	Local<Value> ExecuteString(Handle<String> source, Handle<Value> filename) {
		HandleScope scope;
		TryCatch tryCatch;
		
		Local<Script> script = Script::Compile(source, filename);
		if (script.IsEmpty()) {
			ReportException(&tryCatch);
			exit(1);
		}
		
		Local<Value> result = script->Run();
		if (result.IsEmpty()) {
			ReportException(&tryCatch);
			exit(1);
		}
		
		return scope.Close(result);
	} // ExecuteString

	V8_FUNCTN(CompileScript) {
		HandleScope scope;
		Local<String> script = args[0]->ToString();
		Local<String> source = args[1]->ToString();
		Local<Value> result = ExecuteString(script, source);
		return scope.Close(result);
	} // CompileScript
    
	Handle<Value> EnvGetter(Local<String> name, const AccessorInfo &info) {
		HandleScope scope;
		String::Utf8Value nameVal(name);
		char *env = getenv(*nameVal);
		if (env == NULL) {
			return Undefined();
		}
		Local<String> ret = V8_STR(env);
		return scope.Close(ret);
	}

	void SorrowContext::LoadMain() {
		HandleScope handle_scope;
		TryCatch tryCatch;

		Local<Value> func = internals->Get(String::New("loadMain"));
		
		ASSERT_PIN(func->IsFunction(), "loadMain not found");
		Local<Function> f = Local<Function>::Cast(func);
		
		Local<Object> global = Context::GetCurrent()->Global();
		
		f->Call(global, 0, NULL);
		
		if (tryCatch.HasCaught())  {
			ReportException(&tryCatch);
			exit(11);
		}
	}

	Handle<Object> SorrowContext::SetupInternals(int argc, const char *argv[]) {
		HandleScope scope;
		Local<FunctionTemplate> internals_template = FunctionTemplate::New();
		internals = Persistent<Object>::New(internals_template->GetFunction()->NewInstance());
		
		Local<Object> global = Context::GetCurrent()->Global();
		SET_METHOD(global, "quit",    Quit)
		SET_METHOD(global, "version", Version)
		SET_METHOD(internals, "compile", CompileScript)
        
		if (argc) {
			Local<Array> lineArgs = Array::New(argc-1);
			for (int i = 0; i +1 < argc; i++) {
				lineArgs->Set(Integer::New(i), V8_STR(argv[i+1]));
			}
			internals->Set(V8_STR("args"), lineArgs);
		} else {
			internals->Set(V8_STR("args"), Array::New());
		}

		Handle<Object> libsObject = Object::New();
		LoadNativeLibraries(libsObject);
		internals->Set(V8_STR("stdlib"), libsObject);
        
		Handle<ObjectTemplate> env = ObjectTemplate::New();
		env->SetNamedPropertyHandler(EnvGetter);
		internals->Set(V8_STR("env"), env->NewInstance());
        
		internals->Set(V8_STR("global"), global);
		
		binarytypes = new BinaryTypes(internals);
		iostreams = new IOStreams(internals);

		Filesystem::Initialize(internals);
		Extensions::Initialize(internals);
		

		TryCatch tryCatch;
		Local<Value> func = ExecuteString(V8_STR(sorrow_native), V8_STR("sorrow.js"));
		
		if (tryCatch.HasCaught()) {
			ReportException(&tryCatch);
			exit(10);
		}

		ASSERT_PIN(func->IsFunction(), "sorrow.js main function not found");
		Local<Function> f = Local<Function>::Cast(func);
		
		Local<Value> args[1] = { Local<Value>::New(internals) };
		
		f->Call(global, 1, args);
		
		if (tryCatch.HasCaught())  {
			ReportException(&tryCatch);
			exit(11);
		}

		return internals;
	} // SetupInternals

	SorrowContext::SorrowContext(int argc, const char *argv[]) {
		HandleScope handle_scope;

		//pointer to ourself inside the context
		Context::GetCurrent()->Global()->SetPointerInInternalField(0, this);

		//global array methods
		InitV8Arrays(Context::GetCurrent()->Global());

		//PointerTypes global methods
		pointertypes = new PointerTypes(Context::GetCurrent()->Global());

		//sorrow methods
		SetupInternals(argc, argv);
	}

	SorrowContext::~SorrowContext() {
		delete iostreams;
		delete binarytypes;
		delete pointertypes;
		internals.Dispose();
		context.Dispose();
	}

} // namespace sorrow
