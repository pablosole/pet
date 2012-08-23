// copyright 2012 sam l'ecuyer

#ifndef _SORROW_H_
#define _SORROW_H_

#include <v8.h>
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define V8_STR(lit)     String::New(lit)
#define V8_SETTER(name) void name(Local<String> property, Local<Value> value, const AccessorInfo& info)
#define V8_GETTER(name) Handle<Value> name(Local<String> property, const AccessorInfo& info)
#define V8_FUNCTN(name) Handle<Value> name(const Arguments& args)
#define FN_OF_TMPLT(name) FunctionTemplate::New(name)->GetFunction()
#define SET_METHOD(obj,name,method) obj->Set(V8_STR(name), FN_OF_TMPLT(method));

#define THROW(excp) ThrowException(excp);
#define ERR(val) Exception::Error(val)
#define RANGE_ERR(val) Exception::RangeError(val)
#define TYPE_ERR(val) Exception::TypeError(val)

#define NULL_STREAM_EXCEPTION(file, message) \
    if (file == NULL) { \
        return THROW(ERR(V8_STR(#message))) \
    }

#define IS_BINARY(obj) (byteString_t->HasInstance(obj) || byteArray_t->HasInstance(obj))
#define BYTES_FROM_BIN(obj) reinterpret_cast<Bytes*>(obj->GetPointerFromInternalField(0))

namespace sorrow {
	using namespace v8;
	
	Local<Value> ExecuteString(Handle<String> source, Handle<Value> filename);
	V8_FUNCTN(Quit);
	V8_FUNCTN(Version);
	void ReportException(TryCatch* handler);
	void LoadNativeLibraries(Handle<Object> natives);
	
    void InitV8Arrays(Handle<Object> target);
    
    /**
     * sorrow_io.cpp
     */
    namespace IOStreams {
        void Initialize(Handle<Object> internals);
    }
    // Can only be used after streams are setup
    extern Persistent<Function> rawStream_f;
    extern Persistent<Function> textStream_f;
    
    
    /**
     * sorrow_binary.cpp
     */
    namespace BinaryTypes {
        void Initialize(Handle<Object> internals);
    }
    // Can only be used after types are setup
    extern Persistent<Function> byteString;
    extern Persistent<Function> byteArray;
    extern Persistent<FunctionTemplate> byteString_t;
    extern Persistent<FunctionTemplate> byteArray_t;
    
    /**
     * sorrow_fs.cpp
     */
    namespace Filesystem {
        void Initialize(Handle<Object> internals);
    }
	
	/**
     * sorrow_ext.cpp
     */
    namespace Extensions {
        void Initialize(Handle<Object> internals);
    }
	
	typedef Handle<Object> sp_init();
	typedef void *sp_destroy();

    
}

#endif // _SORROW_H_
