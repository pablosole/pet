// copyright 2012 sam l'ecuyer

#ifndef _SORROW_H_
#define _SORROW_H_

#include <v8.h>
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

#define IS_BINARY(obj) (reinterpret_cast<SorrowContext *>(Context::GetCurrent()->Global()->GetPointerFromInternalField(0))->GetBinaryTypes()->GetByteStringTempl()->HasInstance(obj) || reinterpret_cast<SorrowContext *>(Context::GetCurrent()->Global()->GetPointerFromInternalField(0))->GetBinaryTypes()->GetByteArrayTempl()->HasInstance(obj))
#define BYTES_FROM_BIN(obj) reinterpret_cast<Bytes*>(obj->GetPointerFromInternalField(0))

namespace sorrow {
	using namespace v8;
	
	/**
	 * v8_arrays.cpp
	 */
	void InitV8Arrays(Handle<Object> target);

    /**
     * sorrow_io.cpp
     */
    class IOStreams {
	public:
        IOStreams(Handle<Object> internals);
		~IOStreams();

	private:
		// Can only be used after streams are setup
		Persistent<Function> rawStream_f;
		Persistent<Function> textStream_f;
    };
    
    
    /**
     * sorrow_binary.cpp
     */
    class BinaryTypes {
	public:
        BinaryTypes(Handle<Object> internals);
		~BinaryTypes();
		inline Persistent<Function> &GetByteString() { return byteString; }
		inline Persistent<Function> &GetByteArray() { return byteArray; }
		inline Persistent<FunctionTemplate> &GetByteStringTempl() { return byteString_t; }
		inline Persistent<FunctionTemplate> &GetByteArrayTempl() { return byteArray_t; }
	private:
		// Can only be used after types are setup
		Persistent<Function> byteString;
		Persistent<Function> byteArray;
		Persistent<FunctionTemplate> byteString_t;
		Persistent<FunctionTemplate> byteArray_t;
    };
    
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

	/**
     * sorrow_pin.cpp
     */
	class PointerTypes {
	public:
		PointerTypes(Handle<Object> target);
		inline Persistent<Function> &GetExternalPointerFunct() { return extptr; }
		inline Persistent<Function> &GetOwnPointerFunct() { return ownptr; }
		inline Persistent<Function> &GetOwnStringFunct() { return ownstr; }
		~PointerTypes();
	private:
		Persistent<FunctionTemplate> extptr_t;
		Persistent<Function> extptr;
		Persistent<FunctionTemplate> ownptr_t;
		Persistent<Function> ownptr;
		Persistent<FunctionTemplate> ownstr_t;
		Persistent<Function> ownstr;
	};

	//keep in sync with pin.js
	enum EventTypes { NEWROUTINE, NEWTRACE, NEWINSTRUCTION, 
		STARTTHREAD, FINITHREAD, LOADIMAGE, UNLOADIMAGE, STARTAPP, FINIAPP, 
		FOLLOWCHILD, DETACH, FETCH, MEMORYADDRESSTRANSLATION, CONTEXTCHANGE,
		SYSCALLENTER, SYSCALLEXIT };

	/**
     * sorrow.cpp
     */
    
	class SorrowContext {
	public:
		SorrowContext(int argc, const char *argv[]);
		~SorrowContext();
		void FireExit();
		void LoadScript(const char *text, uint32_t size);
		void LoadMain();
		Handle<Object> SetupInternals(int argc, const char *argv[]);
		void Load();
		inline IOStreams *GetIOStreams() { return iostreams; }
		inline BinaryTypes *GetBinaryTypes() { return binarytypes; }
		inline PointerTypes *GetPointerTypes() { return pointertypes; }
	private:
		Persistent<Object> internals;
		Persistent<Context> context;
		Isolate *isolate;
		IOStreams *iostreams;
		BinaryTypes *binarytypes;
		PointerTypes *pointertypes;
	};
}

#endif // _SORROW_H_
