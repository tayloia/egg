namespace egg::yolk {
  class EggProgramContext;
  class FunctionSignature;
  class IEggProgramNode;

  class FunctionCoroutine : public egg::ovum::IHardAcquireRelease {
  public:
    virtual egg::ovum::Variant resume(EggProgramContext& context) = 0;

    static egg::ovum::HardPtr<FunctionCoroutine> create(egg::ovum::IAllocator& allocator, const std::shared_ptr<egg::yolk::IEggProgramNode>& block);
  };

  class Builtins {
  public:
    // Built-ins
    static egg::ovum::Variant builtinString(egg::ovum::IAllocator& allocator);
    static egg::ovum::Variant builtinType(egg::ovum::IAllocator& allocator);
    static egg::ovum::Variant builtinAssert(egg::ovum::IAllocator& allocator);
    static egg::ovum::Variant builtinPrint(egg::ovum::IAllocator& allocator);

    // Built-ins
    using StringBuiltinFactory = std::function<egg::ovum::Variant(egg::ovum::IAllocator& allocator, const egg::ovum::String& instance, const egg::ovum::String& property)>;
    static StringBuiltinFactory stringBuiltinFactory(const egg::ovum::String& property);
    static egg::ovum::Variant stringBuiltin(egg::ovum::IExecution& execution, const egg::ovum::String& instance, const egg::ovum::String& property);
  };
}
