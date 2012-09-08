RUNTIME_FUNCTION(MaybeObject*, Runtime_WrapPtr) {
	HandleScope scope(isolate);
	ASSERT(args.length() == 1);

	CONVERT_NUMBER_CHECKED(uintptr_t, ptr, Uint32, args[0]);

	Handle<Object> iglobal(isolate->context()->global_proxy());
	v8::Local<v8::Object> global = Utils::ToLocal(Handle<JSObject>::cast(iglobal));
	v8::Handle<v8::ObjectTemplate> tmpl = v8::Handle<v8::ObjectTemplate>::Handle((v8::ObjectTemplate *) global->GetPointerFromInternalField(1));
	v8::Handle<v8::Object> obj = tmpl->NewInstance();

	obj->SetPointerInInternalField(0, reinterpret_cast<void *>(ptr));

	return *Utils::OpenHandle(*obj);
}
