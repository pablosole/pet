// Copyright 2012 the V8 project authors. All rights reserved.

// All substantial code in this file has been taken from d8.cc in v8,
// so the copyright is kept as v8 with the original license.

#include "sorrow.h"
#include "sorrow_bytes.h"
#include <pintool.h>

namespace sorrow {
	using namespace v8;

	char kArrayBufferMarkerPropName[] = "_is_array_buffer_";
	const char kRefByteArrayPropName[] = "_ref_byte_array_";
    
    void ExternalArrayWeakCallback(Persistent<Value> object, void* data) {
		HandleScope scope;
		int32_t length =
			object->ToObject()->Get(String::New("byteLength"))->Int32Value();
		bool external_buffer  =
			object->ToObject()->Get(String::New("externalBuffer"))->BooleanValue();

		if (!external_buffer) {
			V8::AdjustAmountOfExternalAllocatedMemory(-length);
			delete[] static_cast<uint8_t*>(data);
		}
		object.Dispose();
    }
    
    static size_t convertToUint(Local<Value> value_in, TryCatch* try_catch) {
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

	Handle<Value> CreateExternalArrayBuffer(int32_t length, uint8_t *data) {
	  Handle<Object> buffer = Object::New();
	  buffer->SetHiddenValue(String::New(kArrayBufferMarkerPropName), True());
	  Persistent<Object> persistent_array = Persistent<Object>::New(buffer);
	  persistent_array.MakeWeak(data, ExternalArrayWeakCallback);
	  persistent_array.MarkIndependent();

	  buffer->SetIndexedPropertiesToExternalArrayData(
		  data, v8::kExternalByteArray, length);
	  buffer->Set(String::New("byteLength"), Int32::New(length), ReadOnly);
	  buffer->Set(String::New("address"), Int32::New(reinterpret_cast<uint32_t>(data)), ReadOnly);

	  return buffer;
	}

	Handle<Value> CreateExternalArrayBuffer(int32_t length) {
	  static const int32_t kMaxSize = 0x7fffffff;
	  // Make sure the total size fits into a (signed) int.
	  if (length < 0 || length > kMaxSize) {
		return ThrowException(String::New("ArrayBuffer exceeds maximum size (2G)"));
	  }
	  uint8_t* data = new uint8_t[length];
	  if (data == NULL) {
		return ThrowException(String::New("Memory allocation failed."));
	  }
	  memset(data, 0, length);
	  V8::AdjustAmountOfExternalAllocatedMemory(length);

	  return CreateExternalArrayBuffer(length, data);
	}

	Handle<Value> CreateExternalArrayBuffer(Local<Value> ba) {
		Handle<Value> buffer;
		Bytes *bytes = BYTES_FROM_BIN(ba->ToObject());

		ASSERT_PIN(bytes, "Cannot create an external buffer from an invalid Bytes object");

		bytes->setResizable(false);
		buffer = CreateExternalArrayBuffer(bytes->getLength(), bytes->getBytes());
		
		buffer->ToObject()->SetHiddenValue(String::New(kRefByteArrayPropName), ba);
		buffer->ToObject()->Set(String::New("externalBuffer"), True(), ReadOnly);

		return buffer;
	}

	Handle<Value> CreateExternalArrayBuffer(const Arguments& args) {
	  if (args.Length() == 0) {
		return ThrowException(
			String::New("ArrayBuffer constructor must have one parameter."));
	  }

	  if (args[0]->IsObject() && IS_BINARY(args[0])) {
		  return CreateExternalArrayBuffer(args[0]);
	  }

	  TryCatch try_catch;
	  int32_t length = convertToUint(args[0], &try_catch);
	  if (try_catch.HasCaught()) return try_catch.Exception();

	  return CreateExternalArrayBuffer(length);
	}
    
    Handle<Value> CreateExternalArray(const Arguments& args,
                                             ExternalArrayType type,
                                             size_t element_size) {
		TryCatch try_catch;
		ASSERT_PIN(element_size == 1 || element_size == 2 ||
			 element_size == 4 || element_size == 8, "CreateExternalArray");

		// Currently, only the following constructors are supported:
		//   TypedArray(unsigned long length)
		//   TypedArray(ArrayBuffer buffer,
		//              optional unsigned long byteOffset,
		//              optional unsigned long length)
		Handle<Object> buffer;
		int32_t length;
		int32_t byteLength;
		int32_t byteOffset;
		int32_t bufferLength;

		if (args.Length() == 0) {
		return ThrowException(
			String::New("Array constructor must have at least one parameter."));
		}

		if (args[0]->IsObject() &&
			(!args[0]->ToObject()->GetHiddenValue(String::New(kArrayBufferMarkerPropName)).IsEmpty()) ||
			(IS_BINARY(args[0]))) {
				if (!args[0]->ToObject()->GetHiddenValue(String::New(kArrayBufferMarkerPropName)).IsEmpty()) {
					buffer = args[0]->ToObject();
					bufferLength = convertToUint(buffer->Get(String::New("byteLength")), &try_catch);
					if (try_catch.HasCaught())
						return try_catch.Exception();
				} else {
					buffer = CreateExternalArrayBuffer(args[0])->ToObject();
					bufferLength = convertToUint(buffer->Get(String::New("byteLength")), &try_catch);
					if (try_catch.HasCaught())
						return try_catch.Exception();
				}

		if (args.Length() < 2 || args[1]->IsUndefined()) {
		  byteOffset = 0;
		} else {
		  byteOffset = convertToUint(args[1], &try_catch);
		  if (try_catch.HasCaught()) return try_catch.Exception();
		  if (byteOffset > bufferLength) {
			return ThrowException(String::New("byteOffset out of bounds"));
		  }
		  if (byteOffset % element_size != 0) {
			return ThrowException(
				String::New("byteOffset must be multiple of element_size"));
		  }
		}

		if (args.Length() < 3 || args[2]->IsUndefined()) {
		  byteLength = bufferLength - byteOffset;
		  length = byteLength / element_size;
		  if (byteLength % element_size != 0) {
			return ThrowException(
				String::New("buffer size must be multiple of element_size"));
		  }
		} else {
			  length = convertToUint(args[2], &try_catch);
			  if (try_catch.HasCaught())
				  return try_catch.Exception();
			  byteLength = length * element_size;
			  if (byteOffset + byteLength > bufferLength) {
				  return ThrowException(String::New("length out of bounds"));
			  }
		}
		} else {
			length = convertToUint(args[0], &try_catch);
			byteLength = length * element_size;
			byteOffset = 0;
			Handle<Value> result = CreateExternalArrayBuffer(byteLength);
			if (!result->IsObject())
				return result;
			buffer = result->ToObject();
		}

		void* data = buffer->GetIndexedPropertiesExternalArrayData();
		ASSERT_PIN(data != NULL, "CreateExternalArray data != NULL");

		Handle<Object> array = Object::New();
		array->SetIndexedPropertiesToExternalArrayData(
		  static_cast<uint8_t*>(data) + byteOffset, type, length);
		array->Set(String::New("byteLength"), Int32::New(byteLength), ReadOnly);
		array->Set(String::New("byteOffset"), Int32::New(byteOffset), ReadOnly);
		array->Set(String::New("length"), Int32::New(length), ReadOnly);
		array->Set(String::New("BYTES_PER_ELEMENT"), Int32::New(element_size));
		array->Set(String::New("buffer"), buffer, ReadOnly);

		return array;
    }
    
    
    Handle<Value> ArrayBuffer(const Arguments& args) {
		return CreateExternalArrayBuffer(args);
    }

	//by default memory buffers have direct access
    Handle<Value> MemoryBuffer(const Arguments& args) {
	  HandleScope scope;

	  if (args.Length() < 2) {
		return ThrowException(
			String::New("MemoryBuffer constructor must have two parameters (address and size)."));
	  }

	  TryCatch try_catch;
	  uint32_t address = convertToUint(args[0], &try_catch);
	  uint32_t length = convertToUint(args[1], &try_catch);
	  uint8_t* data;
	  bool is_external = false;

	  if (try_catch.HasCaught()) return try_catch.Exception();

	  static const int32_t kMaxSize = 0x7fffffff;
	  // Make sure the total size fits into a (signed) int.
	  if (length < 0 || length > kMaxSize) {
		return ThrowException(String::New("ArrayBuffer exceeds maximum size (2G)"));
	  }

	  //check if we should snapshot the value
	  if (args.Length() > 2 && args[2]->IsTrue()) {
		  data = new uint8_t[length];
		  if (data == NULL) {
			return ThrowException(String::New("Memory allocation failed."));
		  }
		  V8::AdjustAmountOfExternalAllocatedMemory(length);

		  PIN_SafeCopy(data, reinterpret_cast<uint8_t *>(address), length);
	  } else {
		  data = reinterpret_cast<uint8_t *>(address);
		  is_external = true;
	  }

	  Handle<Value> buffer = CreateExternalArrayBuffer(length, data);
	  buffer->ToObject()->Set(String::New("snapshot"), (is_external ? False() : True()), ReadOnly);

	  if (is_external)
		buffer->ToObject()->Set(String::New("externalBuffer"), True(), ReadOnly);

	  return scope.Close(buffer);
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
        SET_METHOD(target, "MemoryBuffer",  MemoryBuffer)
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