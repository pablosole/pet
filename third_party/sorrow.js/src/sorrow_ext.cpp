/*
 * copyright 2012 sam l'ecuyer
 */

#include <windows.h>

#include "sorrow.h"

namespace sorrow {
	using namespace v8;
	
	void PluginWeakCallback(Persistent<Value> object, void* handle) {
		sp_destroy *destructor = (sp_destroy*)GetProcAddress((HMODULE)handle, "Teardown");
		if (destructor == NULL) {
			CloseHandle((HMODULE)handle);
		}
		destructor();
		CloseHandle((HMODULE)handle);
		object.Dispose();
	}
	
	/** 
     * Plugin functions
     */
    
	V8_FUNCTN(SorrowPlugin) {
        HandleScope scope;
		if (args.Length() != 1) {
                return THROW(ERR(V8_STR("This must take one argument")))
        }
        Handle<Object> plugin = args.This();
		plugin->SetHiddenValue(V8_STR("lib"), args[0]);
		return plugin;
	}	
	
	
	V8_FUNCTN(LoadExtension) {
		HandleScope scope;
		Handle<Object> plugin = args.This();
		String::Utf8Value source(plugin->GetHiddenValue(V8_STR("lib")));
		HMODULE sp_library = LoadLibraryA(*source);
		if (sp_library == NULL) {
            return THROW(ERR(V8_STR("LoadLibrary failed")))
		}
		
		sp_init *initializer = (sp_init*)GetProcAddress(sp_library, "Initialize");
		if (initializer == NULL) {
			CloseHandle(sp_library);
			return THROW(ERR(V8_STR("Initialize function not found")))
		}
		
		Persistent<Object> persistent_plugin = Persistent<Object>::New(plugin);
		persistent_plugin.MakeWeak(sp_library, PluginWeakCallback);
		persistent_plugin.MarkIndependent();
		
		plugin->SetPointerInInternalField(0, (void *)sp_library);
		Handle<Object> ret = initializer();
		return scope.Close(ret);
	}
	
	V8_FUNCTN(CloseExtension) {
		HandleScope scope;
		Handle<Object> plugin = args.This();
		HMODULE sp_library = (HMODULE) plugin->GetPointerFromInternalField(0);
		sp_destroy *destructor = (sp_destroy*)GetProcAddress(sp_library, "Teardown");
		if (destructor == NULL) {
			CloseHandle(sp_library);
		}
		destructor();
        CloseHandle(sp_library);
		return Undefined();
	}
    
    namespace Extensions {    
		void Initialize(Handle<Object> internals) {
			HandleScope scope;
			Local<Object> extObj = Object::New();
			
			Local<FunctionTemplate> plugin_t = FunctionTemplate::New(SorrowPlugin);
			Local<ObjectTemplate> plugin_ot = plugin_t->PrototypeTemplate();
			SET_METHOD(plugin_ot, "load",	LoadExtension)
			SET_METHOD(plugin_ot, "close",	CloseExtension)
			plugin_ot->SetInternalFieldCount(1);
			extObj->Set(V8_STR("Plugin"), plugin_t->GetFunction());
			
			internals->Set(String::New("ext"), extObj);
		}
    }
	
} // namespce sorrow