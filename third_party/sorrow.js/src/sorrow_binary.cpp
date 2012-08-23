/*
 copyright 2012 sam l'ecuyer
 */

#include "sorrow.h"
#include "sorrow_bytes.h"

namespace sorrow {
	using namespace v8;
	
    Persistent<Function> byteString;
    Persistent<Function> byteArray;
    Persistent<FunctionTemplate> byteString_t;
    Persistent<FunctionTemplate> byteArray_t;
    
    /** 
     * Binary functions to be shared between all Binary subclasses
     */
    
	V8_FUNCTN(BinaryFunction) {
		return THROW(V8_STR("Binary is non-instantiable"));
	}
    
    V8_GETTER(BinaryLengthGetter) {
        return Integer::New(BYTES_FROM_BIN(info.This())->getLength());
    }
    
	void ExternalWeakCallback(Persistent<Value> object, void* data) {
        delete reinterpret_cast<Bytes*>(data);
		object.Dispose();
	}
    
    V8_FUNCTN(BinaryToArray) {
        return BYTES_FROM_BIN(args.This())->toArray();
    }
    
    
    V8_FUNCTN(BinaryCodeAt) {
        uint32_t index = args[0]->Uint32Value();
        uint8_t code = BYTES_FROM_BIN(args.This())->getByteAt(index);
        return Number::New(code);
    }
    
    V8_FUNCTN(DecodeToString) {
        HandleScope scope;
        String::Utf8Value charset(args[0]->ToString());
        Bytes *bytes = BYTES_FROM_BIN(args.This());
        //Bytes *transcodedBytes = bytes->transcode(*charset, "utf-8");
        return String::New((const char*)bytes->getBytes(), bytes->getLength());
    }
    
    
    V8_FUNCTN(ToByteString) {
        HandleScope scope;
        Bytes *bytes = BYTES_FROM_BIN(args.This());
        Bytes *bytesToUse = bytes;

		/*
        if (args.Length()>=2) {
            String::Utf8Value src(args[0]->ToString());
            String::Utf8Value tgt(args[1]->ToString());
            bytesToUse = bytes->transcode(*src, *tgt);
        }
		*/

		Local<Value> bsArgs[1] = { External::New((void*)bytesToUse) };
		Local<Value> bs = byteString->NewInstance(1, bsArgs);
        return scope.Close(bs);
    }
    
    V8_FUNCTN(ToByteArray) {
        HandleScope scope;
        Bytes *bytes = BYTES_FROM_BIN(args.This());
        Bytes *bytesToUse = bytes;

		/*
		No transcoding.
        if (args.Length()>=2) {
			return THROW(ERR(V8_STR("Unimplemented")))
        }
		*/

		Local<Value> baArgs[1] = { External::New((void*)bytesToUse) };
		Local<Value> ba = byteArray->NewInstance(1, baArgs);
        return scope.Close(ba);
    }
    
    /** 
     * ByteString functions
     */
    
    V8_FUNCTN(ByteString) {
        HandleScope scope;
        Handle<Object> string = args.This();
		Bytes *bytes;
		int argsLength = args.Length();
        switch (argsLength) {
            case 0: 
                // ByteString()
                bytes = new Bytes();
                break;
            case 1:
                if (args[0]->IsArray()) {
                    // ByteString(arrayOfNumbers)
                    Local<Array> array =  Array::Cast(*args[0]);
                    bytes = new Bytes(array);
                } else if (IS_BINARY(args[0])) {
                    // ByteString(byteString|byteArray)
                    Local<Object> obj = Object::Cast(*args[0]);
                    bytes = new Bytes(BYTES_FROM_BIN(obj));
                } else if (args[0]->IsExternal()) {
                    Bytes *otherBytes = reinterpret_cast<Bytes*>(External::Unwrap(args[0]));
                    bytes = new Bytes(otherBytes);
                } else{
                    return THROW(TYPE_ERR(V8_STR("Not valid parameters")))
                }
                break;
        }
		Persistent<Object> persistent_string = Persistent<Object>::New(string);
		persistent_string.MakeWeak(bytes, ExternalWeakCallback);
		persistent_string.MarkIndependent();
		
		string->SetPointerInInternalField(0, bytes);
		return string;
    }
    
    Handle<Value> ByteStringIndexedGetter(uint32_t index, const AccessorInfo &info) {
        HandleScope scope;
        uint8_t byte[1] = { BYTES_FROM_BIN(info.This())->getByteAt(index) };
        Bytes *bytes = new Bytes(1, byte);
		Local<Value> bsArgs[1] = { External::New((void*)bytes) };
		Local<Value> bs = byteString->NewInstance(1, bsArgs);
        
        return scope.Close(bs);
    }

    
    V8_FUNCTN(ByteStringConcat) {
        HandleScope scope;
        Bytes *bytes = BYTES_FROM_BIN(args.This())->concat(args);
		Local<Value> bsArgs[1] = { External::New((void*)bytes) };
		Local<Value> bs = byteString->NewInstance(1, bsArgs);

		return scope.Close(bs);
    }
    
    
    /** 
     * ByteArray functions
     */
    
	V8_FUNCTN(ByteArray) {
        HandleScope scope;
        Handle<Object> string = args.This();
		Bytes *bytes;
		int argsLength = args.Length();
        switch (argsLength) {
            case 0: 
                // ByteString()
                bytes = new Bytes();
                break;
            case 1:
                if (args[0]->IsNumber()) {
                    // ByteString(arrayOfNumbers)
                    bytes = new Bytes(args[0]->Uint32Value());
                } else if (args[0]->IsArray()) {
                    // ByteString(arrayOfNumbers)
                    Local<Array> array =  Array::Cast(*args[0]);
                    bytes = new Bytes(array);
                } else if (IS_BINARY(args[0])) {
                    // ByteString(byteString|byteArray)
                    Local<Object> obj = Object::Cast(*args[0]);
                    bytes = new Bytes(BYTES_FROM_BIN(obj));
                } else if (args[0]->IsExternal()) {
                    Bytes *otherBytes = reinterpret_cast<Bytes*>(External::Unwrap(args[0]));
                    bytes = new Bytes(otherBytes);
                } else{
                    return THROW(TYPE_ERR(V8_STR("Not valid parameters")))
                }
                break;
        }
		Persistent<Object> persistent_string = Persistent<Object>::New(string);
		persistent_string.MakeWeak(bytes, ExternalWeakCallback);
		persistent_string.MarkIndependent();
		
		string->SetPointerInInternalField(0, bytes);
		return string;
	}	
    
    Handle<Value> ByteArrayIndexedGetter(uint32_t index, const AccessorInfo &info) {
        assert(index >= 0 && index < BYTES_FROM_BIN(info.This())->getLength());
        return Integer::New(BYTES_FROM_BIN(info.This())->getByteAt(index));
    }
    Handle<Value> ByteArrayIndexedSetter(uint32_t index, Local< Value > value, const AccessorInfo &info) {
        uint32_t val = value->Uint32Value();
        if (!IS_BYTE(val)) {
            return THROW(ERR(V8_STR("must be a byte value")))
        }
        BYTES_FROM_BIN(info.This())->setByteAt(index, val);
        return Undefined();
    }
    
    V8_SETTER(ByteArrayLengthSetter) {
        BYTES_FROM_BIN(info.This())->resize(value->Uint32Value(), true);
    }
    
    V8_FUNCTN(ByteArrayConcat) {
        HandleScope scope;
        Bytes *bytes = BYTES_FROM_BIN(args.This())->concat(args);
		Local<Value> bsArgs[1] = { External::New((void*)bytes) };
		return byteArray->NewInstance(1, bsArgs);
    }

	
    
    /** 
     * Initialization function to assemble the binary types
     */
    
    namespace BinaryTypes {
	void Initialize(Handle<Object> internals) {
		HandleScope scope;
        
		// function Binary
		Local<FunctionTemplate> binary_t = FunctionTemplate::New(BinaryFunction);
        Local<ObjectTemplate> binary_ot = binary_t->PrototypeTemplate();
        SET_METHOD(binary_ot, "toArray", BinaryToArray);
        SET_METHOD(binary_ot, "codeAt", BinaryCodeAt)
        SET_METHOD(binary_ot, "decodeToString", DecodeToString)
        SET_METHOD(binary_ot, "toByteString", ToByteString)
        SET_METHOD(binary_ot, "toByteArray", ToByteArray)
		internals->Set(V8_STR("Binary"), binary_t->GetFunction());
		
        // function ByteArray
		byteArray_t = Persistent<FunctionTemplate>::New(FunctionTemplate::New(ByteArray));
        Local<ObjectTemplate> byteArray_ot = byteArray_t->InstanceTemplate();
		byteArray_t->Inherit(binary_t);
        
        SET_METHOD(byteArray_ot, "concat", ByteArrayConcat)
        byteArray_ot->SetAccessor(V8_STR("length"), BinaryLengthGetter, ByteArrayLengthSetter);
        byteArray_ot->SetIndexedPropertyHandler(ByteArrayIndexedGetter, ByteArrayIndexedSetter);
        byteArray_ot->SetInternalFieldCount(1);
		
        byteArray = Persistent<Function>::New(byteArray_t->GetFunction());
        byteArray_t->SetClassName(V8_STR("ByteArray"));
		internals->Set(V8_STR("ByteArray"), byteArray);
        
        // function ByteString
        byteString_t = Persistent<FunctionTemplate>::New(FunctionTemplate::New(ByteString));
        Local<ObjectTemplate> byteString_ot = byteString_t->InstanceTemplate();
		byteString_t->Inherit(binary_t);
        
        
        byteString_ot->SetAccessor(V8_STR("length"), BinaryLengthGetter);
        byteString_ot->SetIndexedPropertyHandler(ByteStringIndexedGetter);
        SET_METHOD(byteString_ot, "concat", ByteStringConcat)
        byteString_ot->SetInternalFieldCount(1);
		
        byteString = Persistent<Function>::New(byteString_t->GetFunction());
        byteArray_t->SetClassName(V8_STR("ByteString"));
		internals->Set(V8_STR("ByteString"), byteString);
		
	}
    }
	
}
