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
    virtual const egg::ovum::IFunctionSignature* callable() const override;
    void addParameter(const egg::ovum::String& name, const egg::ovum::ITypeRef& type, egg::ovum::IFunctionSignatureParameter::Flags flags);

    // Helpers
    static FunctionType* createFunctionType(egg::ovum::IAllocator& allocator, const egg::ovum::String& name, const egg::ovum::ITypeRef& returnType);
    static FunctionType* createGeneratorType(egg::ovum::IAllocator& allocator, const egg::ovum::String& name, const egg::ovum::ITypeRef& returnType);
    static void buildFunctionSignature(egg::ovum::StringBuilder& sb, const egg::ovum::IFunctionSignature& signature, egg::ovum::IFunctionSignature::Parts parts);
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
