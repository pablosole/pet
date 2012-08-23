#include "sorrow.h"
#include "sorrow_bytes.h"

#include <iconv.h>
#include <errno.h>

namespace sorrow {
    using namespace v8;

    Bytes::Bytes(): 
        len(0), bytes(NULL), readonly(false), resizable(true) {}

    Bytes::Bytes(size_t l): 
        len(0), bytes(NULL), readonly(false), resizable(true) {
        resize(l, true);
    }
    
    Bytes::Bytes(size_t l, uint8_t *data): 
        len(0), bytes(NULL), readonly(false), resizable(true) {
            //printf("constructor: %s", (char*)data);
            resize(l, false);
            memcpy(this->bytes, data, l);
    }
    
    Bytes::Bytes(Bytes *b): 
        len(0), bytes(NULL), readonly(false), resizable(true) {
            resize(b->getLength(), false);
            memcpy(this->bytes, b->getBytes(), this->len);
    }
    
    Bytes::Bytes(Handle<Array> array): 
        len(0), bytes(NULL), readonly(false), resizable(true) {
            resize(array->Length(), false);
            for (int i = 0; i < array->Length(); i++) {
                uint32_t val = array->Get(i)->IntegerValue();
                if (!IS_BYTE(val)) throw "NonByte";
                this->setByteAt(i, val);
            }
    }

    Bytes::~Bytes() {
        free(this->bytes);
    }
    
    uint8_t *Bytes::getBytes() {
        return this->bytes;
    }

    size_t Bytes::getLength() {
        return this->len;
    }

    uint8_t Bytes::getByteAt(size_t i) {
        return this->bytes[i];
    }

    void Bytes::setByteAt(size_t i, uint8_t b) {
		if (!readonly)
			this->bytes[i] = b;
    }

	void Bytes::resize(size_t nl, bool zero) {
		if (!resizable)
			return;

        if (!nl) {
            free(this->bytes);
            this->bytes = NULL;
        } else {
            this->bytes = (uint8_t*)realloc(this->bytes, nl);
            if (zero && nl > this->len) { 
                memset(this->bytes + this->len, 0, nl-this->len);
            }
        }
        this->len = nl;
    }
    
    
    Bytes *Bytes::concat(const Arguments &args) {
        size_t totalSize = this->len;
        for (int i = 0; i < args.Length(); i++) {
            if (IS_BINARY(args[i])) {
                totalSize += BYTES_FROM_BIN(Object::Cast(*args[i]))->getLength();
            } else if (args[0]->IsArray()){
                totalSize += Array::Cast(*args[i])->Length();
            }
        }
        uint8_t *newBytes = (uint8_t*)malloc(totalSize);
        
        memcpy(newBytes, this->bytes, this->len);
        uint8_t *nca = newBytes + this->len;
        for (int i = 0; i < args.Length(); i++) {
            if (IS_BINARY(args[i])) {
                Bytes *toCopy = BYTES_FROM_BIN(Object::Cast(*args[i]));
                memcpy(nca, toCopy->getBytes(), toCopy->getLength());
                nca += toCopy->getLength();
            } else if (args[i]->IsArray()){
                Local<Array> array = Array::Cast(*args[i]);
                for (int i = 0; i < array->Length(); i++) {
                    uint32_t val = array->Get(i)->Uint32Value();
                    if (!IS_BYTE(val)) throw "NonByte";
                    *(nca++) = val;
                }
            }
        }
        Bytes *ret = new Bytes(totalSize, newBytes);
        free(newBytes);
        return ret;
    }
    
    Bytes *Bytes::transcode(const char* source, const char* target) {
        if (this->len == 0) {
            return new Bytes();
        }
        return new Bytes(this);
    }
    
    Handle<Array>Bytes::toArray() {
        HandleScope scope;
        Handle<Array> array = Array::New(this->len);
        for (int i = 0; i < this->len; i++) {
            uint32_t val = array->Set(i, Integer::New(this->getByteAt(i)));
        }
        return scope.Close(array);
    }
}
