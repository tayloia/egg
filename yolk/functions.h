namespace egg::yolk {
  class EggProgramContext;
  class FunctionSignature;
  class IEggProgramNode;

  class FunctionType : public egg::ovum::HardReferenceCounted<egg::ovum::IType> {
    EGG_NO_COPY(FunctionType);
  protected:
    std::unique_ptr<FunctionSignature> signature;
  public:
    FunctionType(egg::ovum::IAllocator& allocator, const egg::ovum::String& name, const egg::ovum::ITypeRef& returnType);
    virtual ~FunctionType() override;
    virtual std::pair<std::string, int> toStringPrecedence() const override;
    virtual AssignmentSuccess canBeAssignedFrom(const IType& rtype) const;
    virtual const egg::lang::IFunctionSignature* callable() const override;
    void addParameter(const egg::ovum::String& name, const egg::ovum::ITypeRef& type, egg::lang::IFunctionSignatureParameter::Flags flags);

    // Helpers
    static FunctionType* createFunctionType(egg::ovum::IAllocator& allocator, const egg::ovum::String& name, const egg::ovum::ITypeRef& returnType);
    static FunctionType* createGeneratorType(egg::ovum::IAllocator& allocator, const egg::ovum::String& name, const egg::ovum::ITypeRef& returnType);
    static void buildFunctionSignature(egg::ovum::StringBuilder& sb, const egg::lang::IFunctionSignature& signature, egg::lang::IFunctionSignature::Parts parts);
  };

  class FunctionCoroutine : public egg::ovum::IHardAcquireRelease {
  public:
    virtual egg::lang::ValueLegacy resume(EggProgramContext& context) = 0;

    static egg::ovum::HardPtr<FunctionCoroutine> create(egg::ovum::IAllocator& allocator, const std::shared_ptr<egg::yolk::IEggProgramNode>& block);
  };

  class Builtins {
  public:
    // Built-ins
    static egg::lang::ValueLegacy builtinString(egg::ovum::IAllocator& allocator);
    static egg::lang::ValueLegacy builtinType(egg::ovum::IAllocator& allocator);
    static egg::lang::ValueLegacy builtinAssert(egg::ovum::IAllocator& allocator);
    static egg::lang::ValueLegacy builtinPrint(egg::ovum::IAllocator& allocator);

    // Built-ins
    using StringBuiltinFactory = std::function<egg::lang::ValueLegacy(egg::ovum::IAllocator& allocator, const egg::ovum::String& instance, const egg::ovum::String& property)>;
    static StringBuiltinFactory stringBuiltinFactory(const egg::ovum::String& property);
    static egg::lang::ValueLegacy stringBuiltin(egg::ovum::IExecution& execution, const egg::ovum::String& instance, const egg::ovum::String& property);
  };
}
