#include "pintool.h"

//This needs to be exported from the main dll that pin uses. Calling any PIN API is not allowed from a third party DLL.
//We trick the MSVC linker to override CreateThread with our own version of it (only in the pintool), provided in a separate
//project (and a separate dll actually), called pin_threading. All it does is to find this export and call to it.
//Notice that PIN_SpawnInternalThread doesn't support all options that CreateThread does, but for our use (to support _beginthreadex)
//it's enough.
WINDOWS::HANDLE WINAPI MyCreateThread(
			WINDOWS::LPSECURITY_ATTRIBUTES lpThreadAttributes,
			WINDOWS::SIZE_T dwStackSize,
			ROOT_THREAD_FUNC lpStartAddress,
			WINDOWS::LPVOID lpParameter,
			WINDOWS::DWORD dwCreationFlags,
			WINDOWS::LPDWORD lpThreadId
	  )
{
	THREADID ret = (PIN_SpawnInternalThread((lpStartAddress),
					lpParameter,
					dwStackSize,
					reinterpret_cast<PIN_THREAD_UID *>(lpThreadId)));

	return (ret == INVALID_THREADID) ? 0 : reinterpret_cast<WINDOWS::HANDLE>(ret);
}

VOID WINAPI MyExitThread(WINDOWS::DWORD dwExitCode) {
	PIN_ExitThread(dwExitCode);
}

void KillPinTool()
{
	//this is the last thing our pintool will be able to do.
	DebugFile.close();
	PIN_ExitProcess(0);
}

EXCEPT_HANDLING_RESULT InternalExceptionHandler(THREADID tid, EXCEPTION_INFO *pExceptInfo, PHYSICAL_CONTEXT *pPhysCtxt, VOID *v)
{
	DEBUG("Uncaught internal exception on tid " << tid << ":" << PIN_ExceptionToString(pExceptInfo));
	KillPinTool();

	return EHR_UNHANDLED;
}

const char* ToCString(const v8::String::Utf8Value& value) {
    return *value ? *value : "<string conversion failed>";
}

void inline ReportException(TryCatch* try_catch) {
	DEBUG(ReportExceptionToString(try_catch));
}

const string ReportExceptionToString(TryCatch* try_catch) {
	stringstream ret;

	v8::HandleScope handle_scope;
	v8::String::Utf8Value exception(try_catch->Exception());
	const char* exception_string = ToCString(exception);
	v8::Handle<v8::Message> message = try_catch->Message();
	if (message.IsEmpty()) {
		// V8 didn't provide any extra information about this error; just
		// print the exception.
		ret << "JS Exception: " << exception_string << endl;
	}
	else {
		// Print (filename):(line number): (message).
		v8::String::Utf8Value filename(message->GetScriptResourceName());
		const char* filename_string = ToCString(filename);
		int linenum = message->GetLineNumber();
		ret << "JS Exception on " << filename_string << ":" << linenum << ": " << exception_string << endl;
		// Print line of source code.
		v8::String::Utf8Value sourceline(message->GetSourceLine());
		const char* sourceline_string = ToCString(sourceline);
		ret << sourceline_string << endl;
		// Print wavy underline (GetUnderline is deprecated).
		int start = message->GetStartColumn();
		for (int i = 0; i < start; i++) {
			ret << " ";
		}
		int end = message->GetEndColumn();
		for (int i = start; i < end; i++) {
			ret << "^";
		}
		ret << endl;
		v8::String::Utf8Value stack_trace(try_catch->StackTrace());
		if (stack_trace.length() > 0) {
			const char* stack_trace_string = ToCString(stack_trace);
			ret << stack_trace_string << endl;
		}
	}

	return ret.str();
}

//evaluates js on an isolate/context and returns a value handle valid on a diff isolate.
Handle<Value> evalOnContext(Isolate *isolate_src, Handle<Context> context_src, Isolate *isolate_dst, Handle<Context> context_dst, const string& source)
{
	char *result;

	//First we execute the code in the SOURCE context and JSON.stringify the result
	{
		Isolate::Scope iscope(isolate_src);
		Locker lock(isolate_src);
		HandleScope hscope_src;
		Context::Scope cscope(context_src);

		Handle<Value> JSON = context_src->Global()->Get(String::New("JSON"));
		Handle<Object> JSON_obj = Handle<Object>::Cast(JSON);
		Handle<Value> stringify = JSON_obj->Get(String::New("stringify"));
		if (!stringify->IsFunction())
			return Handle<Value>();

		Handle<Function> stringify_fun = Handle<Function>::Cast(stringify);

		TryCatch trycatch;
		Handle<Script> script = Script::Compile(String::New(source.c_str()));
		if (trycatch.HasCaught()) {
			ReportException(&trycatch);
			return Handle<Value>();
		}
		Handle<Value> ret_val = script->Run();
		if (trycatch.HasCaught()) {
			ReportException(&trycatch);
			return Handle<Value>();
		}

		static const int argc = 1;
		Handle<Value> argv[argc] = { ret_val };
		Handle<Value> ret = stringify_fun->Call(context_src->Global(), argc, argv);
		if (trycatch.HasCaught()) {
			ReportException(&trycatch);
			return Handle<Value>();
		}

		if (!ret->IsString())
			return Handle<Value>();
		v8::String::Utf8Value ret_str(ret);
		if (!*ret_str)
			return Handle<Value>();
		result = new char[ret_str.length()];
		if (!result)
			return Handle<Value>();
		memcpy(result, *ret_str, ret_str.length());
	}

	//Now revive the object on the destination isolate/context
	Isolate::Scope iscope(isolate_dst);
	Locker lock(isolate_dst);
	HandleScope hscope_dst;
	Context::Scope cscope(context_dst);

	Handle<Value> JSON = context_dst->Global()->Get(String::New("JSON"));
	Handle<Object> JSON_obj = Handle<Object>::Cast(JSON);
	Handle<Value> parse = JSON_obj->Get(String::New("parse"));
	if (!parse->IsFunction()) {
		delete[] result;
		return Handle<Value>();
	}

	Handle<Function> parse_fun = Handle<Function>::Cast(parse);

	static const int argc = 1;
	Handle<Value> argv[argc] = { String::New(result) };
	delete[] result;
	TryCatch trycatch;
	Handle<Value> ret = parse_fun->Call(context_dst->Global(), argc, argv);
	if (trycatch.HasCaught()) {
		ReportException(&trycatch);
		return Handle<Value>();
	}

	return hscope_dst.Close(ret);
}

Handle<Value> evalOnContext(PinContext *pincontext_src, PinContext *pincontext_dst, const string& source)
{
	Isolate *isolate_src = pincontext_src->GetIsolate();
	Handle<Context> context_src = pincontext_src->GetContext();
	Isolate *isolate_dst = pincontext_dst->GetIsolate();
	Handle<Context> context_dst = pincontext_dst->GetContext();

	return evalOnContext(isolate_src, context_src, isolate_dst, context_dst, source);
}

void forceGarbageCollection()
{
    for (unsigned int i = 0; i < 4096; ++i)
    {
        if (v8::V8::IdleNotification())
            break;
    }
}

uint32_t NumberToUint32(Local<Value> value_in, TryCatch* try_catch) {
	HandleScope hscope;

    if (value_in->IsUint32()) {
        return value_in->Uint32Value();
    }
    
    Local<Value> number = value_in->ToNumber();
    if (try_catch->HasCaught()) return 0;
    
    return number->Uint32Value();
}

size_t convertToUint(Local<Value> value_in, TryCatch* try_catch) {
    if (value_in->IsUint32()) {
        return value_in->Uint32Value();
    }
    
    Local<Value> number = value_in->ToNumber();
    if (try_catch->HasCaught()) return 0;
    
    ASSERT_PIN(number->IsNumber(), "convertToUint");
    Local<Int32> int32 = number->ToInt32();
    if (try_catch->HasCaught() || int32.IsEmpty()) return 0;
    
    int32_t raw_value = int32->Int32Value();
    if (try_catch->HasCaught()) return 0;
    
    if (raw_value < 0) {
        ThrowException(String::New("Array length must not be negative."));
        return 0;
    }
    
    static const int kMaxLength = 0x3fffffff;
#ifndef V8_SHARED
    ASSERT_PIN(kMaxLength == i::ExternalArray::kMaxLength, "convertToUint 2");
#endif  // V8_SHARED
    if (raw_value > static_cast<int32_t>(kMaxLength)) {
        ThrowException(
                       String::New("Array length exceeds maximum length."));
    }
    return static_cast<size_t>(raw_value);
}
