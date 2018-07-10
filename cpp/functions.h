namespace egg::yolk {
  class FunctionSignature;

  class FunctionType : public egg::gc::HardReferenceCounted<egg::lang::IType> {
    EGG_NO_COPY(FunctionType);
  private:
    std::unique_ptr<FunctionSignature> signature;
  public:
    FunctionType(const egg::lang::String& name, const egg::lang::ITypeRef& returnType);
    virtual ~FunctionType() override;
    virtual std::pair<std::string, int> toStringPrecedence() const override;
    virtual AssignmentSuccess canBeAssignedFrom(const IType& rtype) const;
    virtual const egg::lang::IFunctionSignature* callable() const override;
    void addParameter(const egg::lang::String& name, const egg::lang::ITypeRef& type, egg::lang::IFunctionSignatureParameter::Flags flags);
  };
}
