// copyright 2012 sam l'ecuyer
#include <v8.h>
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "sorrow.h"
#include "sorrow_natives.h"

namespace sorrow {
	using namespace v8;

	static Persistent<Object> internals;
	
	const char* ToCString(const String::Utf8Value& value) {
		return *value ? *value : "<string conversion failed>";
	} // ToCString
    
    void FireExit() {
        HandleScope scope;
        TryCatch tryCatch;
		Local<Value> func = internals->Get(V8_STR("fire"));
		
		if (tryCatch.HasCaught() || func == Undefined()) {
			return;
		}
		assert(func->IsFunction());
		Local<Function> f = Local<Function>::Cast(func);
        
		Local<Object> global = Context::GetCurrent()->Global();
		Local<Value> args[1] = { V8_STR("exit") };
		f->Call(global, 1, args);
    }
	
	V8_FUNCTN(Quit) {
		int exit_code = args[0]->Int32Value();
		exit(exit_code);
		return Undefined();
	} // Quit
	
	V8_FUNCTN(Version) {
		return V8_STR(V8::GetVersion());
	} // Version
	
	void ReportException(TryCatch* try_catch) {
		HandleScope handle_scope;
		String::Utf8Value exception(try_catch->Exception());
		const char* exception_string = ToCString(exception);
		Handle<Message> message = try_catch->Message();
		if (message.IsEmpty()) {
			// V8 didn't provide any extra information about this error; just
			// print the exception.
			printf("%s\n", exception_string);
		} else {
			// Print (filename):(line number): (message).
			String::Utf8Value filename(message->GetScriptResourceName());
			const char* filename_string = ToCString(filename);
			int linenum = message->GetLineNumber();
			printf("%s:%i: %s\n", filename_string, linenum, exception_string);
			// Print line of source code.
			String::Utf8Value sourceline(message->GetSourceLine());
			const char* sourceline_string = ToCString(sourceline);
			printf("%s\n", sourceline_string);
			// Print wavy underline (GetUnderline is deprecated).
			int start = message->GetStartColumn();
			for (int i = 0; i < start; i++) {
				printf(" ");
			}
			int end = message->GetEndColumn();
			for (int i = start; i < end; i++) {
				printf("^");
			}
			printf("\n");
			String::Utf8Value stack_trace(try_catch->StackTrace());
			if (stack_trace.length() > 0) {
				const char* stack_trace_string = ToCString(stack_trace);
				printf("%s\n", stack_trace_string);
			}
		}
	} // ReportException
	
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
	
	void Load(Handle<Object> internals) {
		TryCatch tryCatch;
		
		Local<Value> func = ExecuteString(V8_STR(sorrow_native), V8_STR("sorrow.js"));
		
		if (tryCatch.HasCaught()) {
			ReportException(&tryCatch);
			exit(10);
		}
		assert(func->IsFunction());
		Local<Function> f = Local<Function>::Cast(func);
		
		Local<Object> global = Context::GetCurrent()->Global();
		Local<Value> args[1] = { Local<Value>::New(internals) };
		
		f->Call(global, 1, args);
		
		if (tryCatch.HasCaught())  {
			ReportException(&tryCatch);
			exit(11);
		}
	} // Load
	
	Handle<Object> SetupInternals(int argc, const char *argv[]) {
		HandleScope scope;
		Local<FunctionTemplate> internals_template = FunctionTemplate::New();
		internals = Persistent<Object>::New(internals_template->GetFunction()->NewInstance());
		
        Local<Object> global = Context::GetCurrent()->Global();
		SET_METHOD(global, "quit",    Quit)
		SET_METHOD(global, "version", Version)
        SET_METHOD(internals, "compile", CompileScript)
        
		Local<Array> lineArgs = Array::New(argc-1);
		for (int i = 0; i +1 < argc; i++) {
			lineArgs->Set(Integer::New(i), V8_STR(argv[i+1]));
		}
		internals->Set(V8_STR("args"), lineArgs);
		
		Handle<Object> libsObject = Object::New();
		LoadNativeLibraries(libsObject);
		internals->Set(V8_STR("stdlib"), libsObject);
        
        Handle<ObjectTemplate> env = ObjectTemplate::New();
        env->SetNamedPropertyHandler(EnvGetter);
        internals->Set(V8_STR("env"), env->NewInstance());
        
        internals->Set(V8_STR("global"), global);
		
        BinaryTypes::Initialize(internals);
		IOStreams::Initialize(internals);
        Filesystem::Initialize(internals);
		Extensions::Initialize(internals);
		
        return internals;
	} // SetupInternals

	void MainCommonTasks(int argc, const char *argv[], Handle<Context> highlander) {
		Locker locker;
		HandleScope handle_scope;
		Context::Scope context_scope(highlander);

        InitV8Arrays(highlander->Global());
		Handle<Object> internals = SetupInternals(argc, argv);
        
		Load(internals);
	}
	
	int Main(int argc, const char *argv[]) {
		V8::Initialize();
		{
			Locker locker;
			HandleScope handle_scope;
			
			Persistent<Context> highlander = Context::New(); // there can only be one!
			Context::Scope context_scope(highlander);

			MainCommonTasks(argc, argv, highlander);
			FireExit();

			highlander.Dispose();
		}
		V8::Dispose();

		return 0;
	} // Main
	
	void LoadNativeLibraries(Handle<Object> libs) {
		HandleScope scope;
		for (int i = 1; natives[i].name; i++) {
			Local<String> name = V8_STR(natives[i].name);
			Handle<String> source = String::New(natives[i].source, natives[i].source_len);
			libs->Set(name, source);
		}
	} // LoadNativeLibraries
	
} // namespce sorrow
