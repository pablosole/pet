/*
 copyright 2012 sam l'ecuyer
 */

#include "sorrow.h"
#include "sorrow_bytes.h"

namespace sorrow {
	using namespace v8;
    
    Persistent<Function> rawStream_f;
    Persistent<Function> textStream_f;
    
	void ExternalWeakIOCallback(Persistent<Value> object, void* file) {
        fclose((FILE*)file);
		object.Dispose();
	}
    
    const char isRawStreamPropName[] = "_is_raw_stream_";
    const char isTextStreamPropName[] = "_is_text_stream_";

    // args[0] - file name
    // args[1] - {app, ate, binary, read, write, trunc}
	V8_FUNCTN(RawIOStream) {
        HandleScope scope;
		Handle<Object> rawStream = args.This();
        
        FILE *file;
        if (args.Length() == 2 && args[0]->IsString() && args[1]->IsString()) {
            String::Utf8Value name(args[0]);
            String::Utf8Value mode(args[1]);
            
            file = fopen(*name, *mode);
            NULL_STREAM_EXCEPTION(file, "Could not open stream")
        } else if (args[0]->IsExternal()) {
			// ByteString([buffer], size)
            file = (FILE*)External::Unwrap(args[0]);
        } else {
            return THROW(TYPE_ERR(V8_STR("Invalid parameters")))
        }
		Persistent<Object> persistent_stream = Persistent<Object>::New(rawStream);
		persistent_stream.MakeWeak(file, ExternalWeakIOCallback);
		persistent_stream.MarkIndependent();
		
		rawStream->SetPointerInInternalField(0, file);
		rawStream->Set(V8_STR(isRawStreamPropName), True(), ReadOnly);
		return rawStream;
	}
    
    V8_GETTER(RawStreamPositionGetter) {
        FILE *file = (FILE*)info.This()->ToObject()->GetPointerFromInternalField(0);
        NULL_STREAM_EXCEPTION(file, "Could not get position")
        int pos = ftell(file);
        return Integer::New(pos);
    }
    
    V8_SETTER(RawStreamPositionSetter) {
        FILE *file = (FILE*)info.This()->ToObject()->GetPointerFromInternalField(0);
        uint32_t pos = value->Uint32Value();
        fseek(file, pos, SEEK_SET);
        if (ferror(file)) {
            clearerr(file);
            THROW(ERR(V8_STR("Could not set stream position")))
            return;
        }
    }
	
    V8_FUNCTN(RawIOStreamClose) {
        FILE *file = (FILE*)args.This()->ToObject()->GetPointerFromInternalField(0);
        NULL_STREAM_EXCEPTION(file, "Could not close stream")
        fclose(file);
        if (ferror(file)) {
            clearerr(file);
            return THROW(ERR(V8_STR("Could not close stream")))
        }
        return Undefined();
	}	
	
    V8_FUNCTN(RawIOStreamFlush) {
        FILE *file = (FILE*)args.This()->ToObject()->GetPointerFromInternalField(0);
        NULL_STREAM_EXCEPTION(file, "Could not flush stream")
        fflush(file);
        if (ferror(file)) {
            clearerr(file);
            return THROW(ERR(V8_STR("Could not flush stream")))
        }
        return Undefined();
	}	
    
    V8_FUNCTN(RawIOStreamSkip) {
        FILE *file = (FILE*)args.This()->ToObject()->GetPointerFromInternalField(0);
        uint32_t dist = args[0]->Uint32Value();
        NULL_STREAM_EXCEPTION(file, "Could not skip on stream")
        fseek(file, dist, SEEK_CUR);
        if (ferror(file)) {
            clearerr(file);
            return THROW(ERR(V8_STR("Could not skip on stream")))
        }
        return Undefined();
	}	
    
    V8_FUNCTN(RawIOStreamRead) {
        HandleScope scope;
        FILE *file = (FILE*)args.This()->ToObject()->GetPointerFromInternalField(0);
        NULL_STREAM_EXCEPTION(file, "Could not read from stream")
        uint32_t n;
        uint8_t *buffer;
        if (feof(file)) {
            Local<Value> bs = byteString->NewInstance();
            return scope.Close(bs);
        }
        if (args.Length() == 0) {
            int start = ftell(file);
            fseek(file, 0, SEEK_END);
            n = ftell(file);
            fseek(file, start, SEEK_SET);
            buffer = new uint8_t[n-start];
        } else if (args.Length() == 1) {
            if (args[0]->IsNull()) {
                n = 1024;
            } else if (args[0]->IsNumber()) {
                n = args[0]->Uint32Value();
            } else {
                return THROW(TYPE_ERR(V8_STR("Not valid parameters")))
            }
            buffer = new uint8_t[n];
        } else {
            return THROW(TYPE_ERR(V8_STR("Not valid parameters")))
        }
        size_t readBytes = fread(buffer, sizeof(uint8_t), size_t(n), file);
        if (ferror(file)) {
            clearerr(file);
            delete[] buffer;
            return THROW(ERR(V8_STR("Stream is in a bad state")))
        }
        Bytes *bytes = new Bytes(readBytes, buffer);
		Local<Value> bsArgs[1] = { External::New((void*)bytes) };
		Local<Value> bs = byteString->NewInstance(1, bsArgs);
        delete[] buffer;
        return scope.Close(bs);
	}
	
    V8_FUNCTN(RawIOStreamWrite) {
        HandleScope scope;
        FILE *file = (FILE*)args.This()->ToObject()->GetPointerFromInternalField(0);
        NULL_STREAM_EXCEPTION(file, "Could not write to stream")
        const void *data;
        int size;
        
        if (args.Length() == 0) {
            return THROW(ERR(V8_STR("This method takes at least one parameter")))
        } else if (args.Length() == 1) {
            Bytes *bytes = BYTES_FROM_BIN(args[0]->ToObject());
            data = bytes->getBytes();
            size = bytes->getLength();
        } else {
            return THROW(ERR(V8_STR("Not currently supported")))
        }
        int wrote = fwrite(data, sizeof(uint8_t), size, file);
        if (ferror(file) || wrote != size) {
            clearerr(file);
            return THROW(ERR(V8_STR("Could not write to stream")))
        }
        return Integer::New(wrote);
	}
    
    // args[0] - raw stream
    // args[1] - {app, ate, binary, read, write, trunc}
	V8_FUNCTN(TextIOStream) {
        HandleScope scope;
		Handle<Object> textStream = args.This();
        
        if (args.Length() == 2 && args[0]->IsObject() && args[1]->IsObject()) {
            textStream->Set(V8_STR(isRawStreamPropName), True(), ReadOnly);
            textStream->Set(V8_STR("raw"), args[0]->ToObject(), ReadOnly);
        } else {
            return THROW(TYPE_ERR(V8_STR("Not valid parameters")))
        }
		return textStream;
	}
    
    V8_FUNCTN(TextIOStreamReadLine) {
        HandleScope scope;
        FILE *file = (FILE*)args.This()->Get(V8_STR("raw"))
                        ->ToObject()->GetPointerFromInternalField(0);
        NULL_STREAM_EXCEPTION(file, "Could not read from stream")
        int n = 4096;
        char *buffer;
        if (feof(file)) {
            return THROW(ERR(V8_STR("EOF")))
        }
        buffer = new char[n+1];
        buffer[n] = '\0';
        fgets(buffer, n, file);
        if (ferror(file)) {
            clearerr(file);
            delete[] buffer;
            return THROW(ERR(V8_STR("Error reading line")))
        }
        int length = strlen(buffer);
		Handle<String> stringVal = String::New(buffer, length);
        delete[] buffer;
        return stringVal;
    }
    
    V8_FUNCTN(TextIOStreamWriteLine) {
        HandleScope scope;
        FILE *file = (FILE*)args.This()->Get(V8_STR("raw"))
                ->ToObject()->GetPointerFromInternalField(0);
        NULL_STREAM_EXCEPTION(file, "Could not read from stream")
        if (args.Length() >= 1) {
            String::Utf8Value val(args[0]->ToString());
            fwrite(*val, sizeof(char), val.length(), file);
        }
        if (!args[1]->IsTrue()) {
            fwrite("\n", 1, 1, file);
        }
        if (ferror(file)) {
            clearerr(file);
            return THROW(ERR(V8_STR("Error writing line")))
        }
        return Undefined();
    }
	
    namespace IOStreams {
	void Initialize(Handle<Object> internals) {
		HandleScope scope;
		
        // Create RawStream type
		Local<FunctionTemplate> rawStream_t = FunctionTemplate::New(RawIOStream);
        Local<ObjectTemplate> rawStream_ot = rawStream_t->InstanceTemplate();
        
        rawStream_ot->SetAccessor(V8_STR("position"), 
                                  RawStreamPositionGetter, 
                                  RawStreamPositionSetter);
        SET_METHOD(rawStream_ot, "close", RawIOStreamClose)
        SET_METHOD(rawStream_ot, "flush", RawIOStreamFlush)
        SET_METHOD(rawStream_ot, "skip",  RawIOStreamSkip)
        SET_METHOD(rawStream_ot, "read",  RawIOStreamRead)
        SET_METHOD(rawStream_ot, "write", RawIOStreamWrite)
        rawStream_ot->SetInternalFieldCount(1);
		
        rawStream_f = Persistent<Function>::New(rawStream_t->GetFunction());
		internals->Set(V8_STR("RawStream"), rawStream_f);
        
        // Create TextStreamType
        Local<FunctionTemplate> textStream_t = FunctionTemplate::New(TextIOStream);
        Local<ObjectTemplate> textStream_ot = textStream_t->InstanceTemplate();
		
        SET_METHOD(textStream_ot, "readLine",  TextIOStreamReadLine)
        SET_METHOD(textStream_ot, "writeLine", TextIOStreamWriteLine)
        
        textStream_f = Persistent<Function>::New(textStream_t->GetFunction());
		internals->Set(V8_STR("TextStream"), textStream_f);
        
        // seup standard streams
        Local<Value> rawInArgs[1] = { External::New(stdin) };
        Local<Object> rawIn = rawStream_f->NewInstance(1, rawInArgs);
        Local<Value> stdinArgs[2] = { rawIn, Object::New() };
		internals->Set(V8_STR("stdin"), textStream_f->NewInstance(2, stdinArgs));
        
        Local<Value> rawOutArgs[1] = { External::New(stdout) };
        Local<Object> rawOut = rawStream_f->NewInstance(1, rawOutArgs);
        Local<Value> stdoutArgs[2] = { rawOut, Object::New() };
		internals->Set(V8_STR("stdout"), textStream_f->NewInstance(2, stdoutArgs));
        
        Local<Value> rawErrArgs[1] = { External::New(stderr) };
        Local<Object> rawErr = rawStream_f->NewInstance(1, rawErrArgs);
        Local<Value> stderrArgs[2] = { rawErr, Object::New() };
		internals->Set(V8_STR("stderr"), textStream_f->NewInstance(2, stderrArgs));
	}
    }
	
}
