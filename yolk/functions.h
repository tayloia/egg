namespace egg::yolk {
  class EggProgramContext;
  class FunctionSignature;
  class IEggProgramNode;

  class FunctionType : public egg::ovum::HardReferenceCounted<egg::ovum::TypeBase> {
    EGG_NO_COPY(FunctionType);
  protected:
    std::unique_ptr<FunctionSignature> signature;
  public:
    FunctionType(egg::ovum::IAllocator& allocator, const egg::ovum::String& name, const egg::ovum::Type& returnType);
    // Interface
    virtual ~FunctionType() override;
    virtual AssignmentSuccess canBeAssignedFrom(const IType& rhs) const override;
    virtual const egg::ovum::IFunctionSignature* callable() const override;
    virtual std::pair<std::string, int> toStringPrecedence() const override;
    virtual egg::ovum::Node compile(egg::ovum::IAllocator& allocator, const egg::ovum::NodeLocation& location) const override;
    // Implementation
    void addParameter(const egg::ovum::String& name, const egg::ovum::Type& type, egg::ovum::IFunctionSignatureParameter::Flags flags);
    // Helpers
    static FunctionType* createFunctionType(egg::ovum::IAllocator& allocator, const egg::ovum::String& name, const egg::ovum::Type& returnType);
    static FunctionType* createGeneratorType(egg::ovum::IAllocator& allocator, const egg::ovum::String& name, const egg::ovum::Type& returnType);
  };

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
