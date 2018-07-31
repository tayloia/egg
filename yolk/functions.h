namespace egg::yolk {
  class EggProgramContext;
  class FunctionSignature;
  class IEggProgramNode;

  class FunctionType : public egg::gc::HardReferenceCounted<egg::lang::IType> {
    EGG_NO_COPY(FunctionType);
  protected:
    std::unique_ptr<FunctionSignature> signature;
    FunctionType(const egg::lang::String& name, const egg::lang::ITypeRef& returnType);
  public:
    virtual ~FunctionType() override;
    virtual std::pair<std::string, int> toStringPrecedence() const override;
    virtual AssignmentSuccess canBeAssignedFrom(const IType& rtype) const;
    virtual const egg::lang::IFunctionSignature* callable() const override;
    void addParameter(const egg::lang::String& name, const egg::lang::ITypeRef& type, egg::lang::IFunctionSignatureParameter::Flags flags);

    // Helpers
    static FunctionType* createFunctionType(const egg::lang::String& name, const egg::lang::ITypeRef& returnType);
    static FunctionType* createGeneratorType(const egg::lang::String& name, const egg::lang::ITypeRef& returnType);
    static void buildFunctionSignature(egg::lang::StringBuilder& sb, const egg::lang::IFunctionSignature& signature, egg::lang::IFunctionSignature::Parts parts);
  };

  class FunctionCoroutine {
  public:
    virtual ~FunctionCoroutine() {}
    virtual egg::lang::Value resume(EggProgramContext& context) = 0;

    static FunctionCoroutine* create(const std::shared_ptr<egg::yolk::IEggProgramNode>& block);
  };
}
