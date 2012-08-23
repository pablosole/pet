// Copyright 2012 the V8 project authors. All rights reserved.

// All substantial code in this file has been taken from d8.cc in v8,
// so the copyright is kept as v8 with the original license.

#include "sorrow.h"

namespace sorrow {
	using namespace v8;

    const char kArrayBufferReferencePropName[] = "_is_array_buffer_";
    const char kArrayBufferMarkerPropName[] = "_array_buffer_ref_";
    
    void ExternalArrayWeakCallback(Persistent<Value> object, void* data) {
        HandleScope scope;
        Handle<String> prop_name = String::New(kArrayBufferReferencePropName);
        Handle<Object> converted_object = object->ToObject();
        Local<Value> prop_value = converted_object->Get(prop_name);
        if (data != NULL && !prop_value->IsObject()) {
            free(data);
        }
        object.Dispose();
    }
    
    static size_t convertToUint(Local<Value> value_in, TryCatch* try_catch) {
        if (value_in->IsUint32()) {
            return value_in->Uint32Value();
        }
        
        Local<Value> number = value_in->ToNumber();
        if (try_catch->HasCaught()) return 0;
        
        assert(number->IsNumber());
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
        ASSERT(kMaxLength == i::ExternalArray::kMaxLength);
#endif  // V8_SHARED
        if (raw_value > static_cast<int32_t>(kMaxLength)) {
            ThrowException(
                           String::New("Array length exceeds maximum length."));
        }
        return static_cast<size_t>(raw_value);
    }

    
    Handle<Value> CreateExternalArray(const Arguments& args,
                                             ExternalArrayType type,
                                             size_t element_size) {
        TryCatch try_catch;
        bool is_array_buffer_construct = element_size == 0;
        if (is_array_buffer_construct) {
            type = v8::kExternalByteArray;
            element_size = 1;
        }
        assert(element_size == 1 || element_size == 2 || element_size == 4 ||
               element_size == 8);
        if (args.Length() == 0) {
            return ThrowException(
                                  String::New("Array constructor must have at least one "
                                              "parameter."));
        }
        bool first_arg_is_array_buffer =
        args[0]->IsObject() &&
        args[0]->ToObject()->Get(
                                 String::New(kArrayBufferMarkerPropName))->IsTrue();
        // Currently, only the following constructors are supported:
        //   TypedArray(unsigned long length)
        //   TypedArray(ArrayBuffer buffer,
        //              optional unsigned long byteOffset,
        //              optional unsigned long length)
        if (args.Length() > 3) {
            return ThrowException(
                                  String::New("Array constructor from ArrayBuffer must "
                                              "have 1-3 parameters."));
        }
        
        Local<Value> length_value = (args.Length() < 3)
        ? (first_arg_is_array_buffer
           ? args[0]->ToObject()->Get(String::New("length"))
           : args[0])
        : args[2];
        size_t length = convertToUint(length_value, &try_catch);
        if (try_catch.HasCaught()) return try_catch.Exception();
        
        void* data = NULL;
        size_t offset = 0;
        
        Handle<Object> array = Object::New();
        if (first_arg_is_array_buffer) {
            Handle<Object> derived_from = args[0]->ToObject();
            data = derived_from->GetIndexedPropertiesExternalArrayData();
            
            size_t array_buffer_length = convertToUint(
                                                       derived_from->Get(String::New("length")),
                                                       &try_catch);
            if (try_catch.HasCaught()) return try_catch.Exception();
            
            if (data == NULL && array_buffer_length != 0) {
                return ThrowException(
                                      String::New("ArrayBuffer doesn't have data"));
            }
            
            if (args.Length() > 1) {
                offset = convertToUint(args[1], &try_catch);
                if (try_catch.HasCaught()) return try_catch.Exception();
                
                // The given byteOffset must be a multiple of the element size of the
                // specific type, otherwise an exception is raised.
                if (offset % element_size != 0) {
                    return ThrowException(
                                          String::New("offset must be multiple of element_size"));
                }
            }
            
            if (offset > array_buffer_length) {
                return ThrowException(
                                      String::New("byteOffset must be less than ArrayBuffer length."));
            }
            
            if (args.Length() == 2) {
                // If length is not explicitly specified, the length of the ArrayBuffer
                // minus the byteOffset must be a multiple of the element size of the
                // specific type, or an exception is raised.
                length = array_buffer_length - offset;
            }
            
            if (args.Length() != 3) {
                if (length % element_size != 0) {
                    return ThrowException(
                                          String::New("ArrayBuffer length minus the byteOffset must be a "
                                                      "multiple of the element size"));
                }
                length /= element_size;
            }
            
            // If a given byteOffset and length references an area beyond the end of
            // the ArrayBuffer an exception is raised.
            if (offset + (length * element_size) > array_buffer_length) {
                return ThrowException(
                                      String::New("length references an area beyond the end of the "
                                                  "ArrayBuffer"));
            }
            
            // Hold a reference to the ArrayBuffer so its buffer doesn't get collected.
            array->Set(String::New(kArrayBufferReferencePropName), args[0], ReadOnly);
        }
        
        if (is_array_buffer_construct) {
            array->Set(String::New(kArrayBufferMarkerPropName), True(), ReadOnly);
        }
        
        Persistent<Object> persistent_array = Persistent<Object>::New(array);
        persistent_array.MakeWeak(data, ExternalArrayWeakCallback);
        persistent_array.MarkIndependent();
        if (data == NULL && length != 0) {
            data = calloc(length, element_size);
            if (data == NULL) {
                return ThrowException(String::New("Memory allocation failed."));
            }
        }
        
        array->SetIndexedPropertiesToExternalArrayData(
                                                       reinterpret_cast<uint8_t*>(data) + offset, type,
                                                       static_cast<int>(length));
        array->Set(String::New("length"),
                   Int32::New(static_cast<int32_t>(length)), ReadOnly);
        array->Set(String::New("BYTES_PER_ELEMENT"),
                   Int32::New(static_cast<int32_t>(element_size)));
        return array;
    }
    
    
    Handle<Value> ArrayBuffer(const Arguments& args) {
        return CreateExternalArray(args, v8::kExternalByteArray, 0);
    }
    
    
    Handle<Value> Int8Array(const Arguments& args) {
        return CreateExternalArray(args, v8::kExternalByteArray, sizeof(int8_t));
    }
    
    
    Handle<Value> Uint8Array(const Arguments& args) {
        return CreateExternalArray(args, kExternalUnsignedByteArray, sizeof(uint8_t));
    }
    
    
    Handle<Value> Int16Array(const Arguments& args) {
        return CreateExternalArray(args, kExternalShortArray, sizeof(int16_t));
    }
    
    
    Handle<Value> Uint16Array(const Arguments& args) {
        return CreateExternalArray(args, kExternalUnsignedShortArray,
                                   sizeof(uint16_t));
    }
    
    
    Handle<Value> Int32Array(const Arguments& args) {
        return CreateExternalArray(args, kExternalIntArray, sizeof(int32_t));
    }
    
    
    Handle<Value> Uint32Array(const Arguments& args) {
        return CreateExternalArray(args, kExternalUnsignedIntArray, sizeof(uint32_t));
    }
    
    
    Handle<Value> Float32Array(const Arguments& args) {
        return CreateExternalArray(args, kExternalFloatArray,
                                   sizeof(float));  // NOLINT
    }
    
    
    Handle<Value> Float64Array(const Arguments& args) {
        return CreateExternalArray(args, kExternalDoubleArray,
                                   sizeof(double));  // NOLINT
    }
    
    
    Handle<Value> PixelArray(const Arguments& args) {
        return CreateExternalArray(args, kExternalPixelArray, sizeof(uint8_t));
    }
    
    void InitV8Arrays(Handle<Object> target) {
        SET_METHOD(target, "ArrayBuffer",   ArrayBuffer)
        SET_METHOD(target, "Int8Array",     Int8Array)
        SET_METHOD(target, "Uint8Array",    Uint8Array)
        SET_METHOD(target, "Int16Array",    Int16Array)
        SET_METHOD(target, "Uint16Array",   Uint16Array)
        SET_METHOD(target, "Int32Array",    Int32Array)
        SET_METHOD(target, "Uint32Array",   Uint32Array)
        SET_METHOD(target, "Float32Array",  Float32Array)
        SET_METHOD(target, "Float64Array",  Float64Array)
        SET_METHOD(target, "PixelArray",    PixelArray)
    }

    
} // namespce sorrow