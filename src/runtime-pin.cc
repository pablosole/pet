RUNTIME_FUNCTION(MaybeObject*, Runtime_PIN_Test) {
  NoHandleAllocation ha;
  ASSERT(args.length() == 1);

  return args[0];
}
