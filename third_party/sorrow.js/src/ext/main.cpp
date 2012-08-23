// build with:
// g++ -dynamiclib -undefined suppress -flat_namespace \
   -I../../deps/v8/include/ -arch i386  main.cpp -o plugin.dylib

#include "../sorrow_ext.cpp"

namespace my_extension {
	using namespace v8;

	V8_FUNCTN(MyFunction) {
		return Integer::New(42);
	}

	extern "C" Handle<Object> Initialize() {
		HandleScope scope;
		Local<Object> exports = Object::New();
	
		SET_METHOD(exports, "theAnswer", MyFunction)
		
		return exports;
	}

	extern "C" void Teardown(Handle<Object> o) {
		//nothing really to do here
	}

}