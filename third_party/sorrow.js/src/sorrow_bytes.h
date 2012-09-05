#ifndef SORROW_BYTES_H
#define SORROW_BYTES_H

#include "sorrow.h"

#define IS_BYTE(b) (b >= 0 && b < 256)

namespace sorrow {
	class Bytes {
	public:
		Bytes();
		Bytes(size_t);
		Bytes(size_t, uint8_t*, bool _resizable=true, bool _readonly=false);
		Bytes(Bytes *);
		Bytes(Bytes *, size_t, size_t);
		Bytes(Handle<Array>);
        
        virtual ~Bytes();

        uint8_t *getBytes();
        size_t  getLength();
        
        uint8_t getByteAt(size_t);
        void    setByteAt(size_t, uint8_t);
        void    resize(size_t, bool); 
        
        Bytes *concat(const Arguments&);
        Bytes *transcode(const char*, const char*);
		inline bool setReadOnly(bool state) { return (readonly = state); }
		inline bool setResizable(bool state) { return (resizable = state); }
        
        Handle<Array> toArray();

	private:
		uint8_t *bytes;
		size_t   len;
		bool readonly;
		bool resizable;
	};
}
#endif
