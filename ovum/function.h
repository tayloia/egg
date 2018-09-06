namespace egg::ovum {
  // WIBBLE retire
  class Function {
  public:
    // Helpers
    enum class Parts {
      ReturnType = 0x01,
      FunctionName = 0x02,
      ParameterList = 0x04,
      ParameterNames = 0x08,
      NoNames = ReturnType | ParameterList,
      All = ~0
    };
    static String signatureToString(const IFunctionSignature& signature, Parts parts = Parts::All);
    static const IParameters& NoParameters;
  };

  class FunctionSignatureParameter : public IFunctionSignatureParameter {
  private:
    String name; // may be empty
    Type type;
    size_t position; // may be SIZE_MAX
    Flags flags;
  public:
    FunctionSignatureParameter(const String& name, const Type& type, size_t position, Flags flags)
      : name(name), type(type), position(position), flags(flags) {
    }
    virtual String getName() const override {
      return this->name;
    }
    virtual Type getType() const override {
      return this->type;
    }
    virtual size_t getPosition() const override {
      return this->position;
    }
    virtual Flags getFlags() const override {
      return this->flags;
    }
  };

  class FunctionSignature : public IFunctionSignature {
  private:
    String name;
    Type rettype;
    std::vector<FunctionSignatureParameter> parameters;
  public:
    FunctionSignature(const String& name, const Type& rettype)
      : name(name), rettype(rettype) {
    }
    void addSignatureParameter(const String& parameterName, const Type& parameterType, size_t position, IFunctionSignatureParameter::Flags flags) {
      this->parameters.emplace_back(parameterName, parameterType, position, flags);
    }
    virtual String getFunctionName() const override {
      return this->name;
    }
    virtual Type getReturnType() const override {
      return this->rettype;
    }
    virtual size_t getParameterCount() const override {
      return this->parameters.size();
    }
    virtual const IFunctionSignatureParameter& getParameter(size_t index) const override {
      assert(index < this->parameters.size());
      return this->parameters[index];
    }
  };

  class FunctionType : public HardReferenceCounted<TypeBase> {
    FunctionType(const FunctionType&) = delete;
    FunctionType& operator=(const FunctionType&) = delete;
  protected:
    FunctionSignature signature;
  public:
    FunctionType(IAllocator& allocator, const String& name, const Type& rettype);
    // Interface
    virtual AssignmentSuccess canBeAssignedFrom(const IType& rhs) const override;
    virtual const IFunctionSignature* callable() const override;
    virtual std::pair<std::string, int> toStringPrecedence() const override;
    virtual Node compile(IAllocator& allocator, const NodeLocation& location) const override;
    // Implementation
    void addParameter(const String& name, const Type& type, IFunctionSignatureParameter::Flags flags, size_t index);
    void addParameter(const String& name, const Type& type, IFunctionSignatureParameter::Flags flags);
    // Helpers
    static FunctionType* createFunctionType(IAllocator& allocator, const String& name, const Type& rettype);
    static FunctionType* createGeneratorType(IAllocator& allocator, const String& name, const Type& rettype);
  };
}
