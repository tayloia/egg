namespace egg::ovum {
  class IVMProgram;
  class IVMRunner;

  enum class VMSymbolKind {
    Builtin,
    Variable,
    Type
  };

  class IVMCommon {
  public:
    // Interface
    virtual ~IVMCommon() {}
    // String factories
    virtual String createStringUTF8(const void* utf8, size_t bytes = SIZE_MAX, size_t codepoints = SIZE_MAX) = 0;
    virtual String createStringUTF32(const void* utf32, size_t codepoints = SIZE_MAX) = 0;
    // String factory helpers
    inline String createString(const std::u8string& utf8) {
      return this->createStringUTF8(utf8.data(), utf8.size());
    }
    inline String createString(const std::u32string& utf32) {
      return this->createStringUTF32(utf32.data(), utf32.size());
    }
    inline String createString(const char* utf8, size_t bytes = SIZE_MAX, size_t codepoints = SIZE_MAX) {
      return this->createStringUTF8(utf8, bytes, codepoints);
    }
    inline String createString(const char8_t* utf8, size_t bytes = SIZE_MAX, size_t codepoints = SIZE_MAX) {
      return this->createStringUTF8(utf8, bytes, codepoints);
    }
    inline String createString(const char32_t* utf32, size_t codepoints = SIZE_MAX) {
      return this->createStringUTF32(utf32, codepoints);
    }
    // Value factories
    virtual HardValue createHardValueVoid() = 0;
    virtual HardValue createHardValueNull() = 0;
    virtual HardValue createHardValueBool(Bool value) = 0;
    virtual HardValue createHardValueInt(Int value) = 0;
    virtual HardValue createHardValueFloat(Float value) = 0;
    virtual HardValue createHardValueString(const String& value) = 0;
    virtual HardValue createHardValueObject(const HardObject& value) = 0;
    virtual HardValue createHardValueType(const Type& value) = 0;
    inline HardValue createHardValue(std::nullptr_t) {
      return this->createHardValueNull();
    }
    inline HardValue createHardValue(bool value) {
      return this->createHardValueBool(value);
    }
    inline HardValue createHardValue(int value) {
      return this->createHardValueInt(value);
    }
    inline HardValue createHardValue(float value) {
      return this->createHardValueFloat(value);
    }
    inline HardValue createHardValue(double value) {
      return this->createHardValueFloat(value);
    }
    inline HardValue createHardValue(const char* value) {
      return value ? this->createHardValueString(this->createString(value)) : this->createHardValueNull();
    }
    inline HardValue createHardValue(const char8_t* value) {
      return value ? this->createHardValueString(this->createString(value)) : this->createHardValueNull();
    }
    inline HardValue createHardValue(const char32_t* value) {
      return value ? this->createHardValueString(this->createString(value)) : this->createHardValueNull();
    }
    inline HardValue createHardValue(const std::u8string& value) {
      return this->createHardValueString(this->createString(value));
    }
    inline HardValue createHardValue(const std::u32string& value) {
      return this->createHardValueString(this->createString(value));
    }
    inline HardValue createHardValue(const String& value) {
      return this->createHardValueString(value);
    }
    inline HardValue createHardValue(const Type& value) {
      return this->createHardValueType(value);
    }
  };

  class IVMCollectable : public ICollectable, public IVMCommon {
  public:
    virtual IAllocator& getAllocator() const = 0;
  };

  class IVMUncollectable : public IHardAcquireRelease, public IVMCommon {
  public:
    virtual IAllocator& getAllocator() const = 0;
  };

  struct VMCallCapture {
    VMSymbolKind kind;
    Type type;
    String name;
    IValue* soft;
  };

  class IVMCallCaptures {
  public:
    // Interface
    virtual ~IVMCallCaptures() {}
    virtual size_t getCaptureCount() const = 0;
    virtual const VMCallCapture* getCaptureByIndex(size_t index) const = 0;
  };

  class IVMCallStack : public IHardAcquireRelease {
  public:
    virtual const IVMCallStack* getCaller() const = 0;
    virtual String getResource() const = 0;
    virtual const SourceRange* getSourceRange() const = 0;
    virtual String getFunction() const = 0;
    virtual void print(Printer& printer) const = 0;
  };

  class IVMModule : public IVMUncollectable {
  public:
    class Node;
    virtual HardPtr<IVMRunner> createRunner(IVMProgram& program) = 0;
  };

  class IVMRunner : public IVMCollectable {
  public:
    virtual void addBuiltin(const String& symbol, const HardValue& value) = 0;
    virtual HardValue step() = 0;
    virtual HardValue run() = 0;
    virtual HardValue yield() = 0;
  };

  class IVMTypeSpecification : public IVMUncollectable {
  public:
    using Parameters = std::vector<Type>;
    virtual Type instantiateType(const Parameters& parameters) = 0;
    virtual HardValue instantiateManifestation(IVMExecution& execution, const Parameters& parameters) = 0;
    virtual void finalizeManifestation(const Type& infratype, const HardValue& manifestation) = 0;
  };

  class IVMTypeSpecificationBuilder : public IVMUncollectable {
  public:
    virtual void setDescription(const String& description, int precedence) = 0;
    virtual void addStaticMember(const String& name, const Type& type) = 0;
    virtual void addInstanceMember(const String& name, const Type& type, Accessability accessability) = 0;
    virtual IVMTypeSpecification& build() = 0;
  };

  class IVMExecution : public ILogger, public IVMCommon {
  public:
    // Exceptions
    virtual HardValue raiseException(const HardValue& inner) = 0;
    virtual HardValue raiseRuntimeError(const String& message, const SourceRange* source) = 0;
    // Assignment
    virtual bool assignValue(IValue& lhs, const Type& ltype, const IValue& rhs) = 0;
    // Function calls
    virtual HardValue initiateFunctionCall(const IFunctionSignature& signature, const IVMModule::Node& definition, const ICallArguments& arguments, const IVMCallCaptures* captures) = 0;
    virtual HardValue initiateManifestationCall(const Type& infratype, const IVMModule::Node& specification, const IVMTypeSpecification::Parameters& parameters, const IVMCallCaptures* captures) = 0;
    // Soft values
    virtual HardValue getSoftValue(const SoftValue& soft) = 0;
    virtual bool setSoftValue(SoftValue& lhs, const HardValue& rhs) = 0;
    virtual HardValue mutSoftValue(SoftValue& lhs, ValueMutationOp mutation, const HardValue& rhs) = 0;
    virtual HardValue refSoftValue(const SoftValue& soft, Modifiability modifiability) = 0;
    // References
    virtual HardValue refIndex(const HardObject& instance, const HardValue& index, Modifiability modifiability, const Type& pointeeType) = 0;
    virtual HardValue refProperty(const HardObject& instance, const HardValue& property, Modifiability modifiability, const Type& pointeeType) = 0;
    // Operations
    virtual HardValue evaluateValueUnaryOp(ValueUnaryOp op, const HardValue& arg) = 0;
    virtual HardValue evaluateValueBinaryOp(ValueBinaryOp op, const HardValue& lhs, const HardValue& rhs) = 0;
    virtual HardValue evaluateValueTernaryOp(ValueTernaryOp op, const HardValue& lhs, const HardValue& mid, const HardValue& rhs) = 0;
    virtual HardValue precheckValueMutationOp(ValueMutationOp op, HardValue& lhs, ValueFlags rhs) = 0;
    virtual HardValue evaluateValueMutationOp(ValueMutationOp op, HardValue& lhs, const HardValue& rhs) = 0;
    virtual HardValue evaluateValuePredicateOp(ValuePredicateOp op, const HardValue& lhs, const HardValue& rhs) = 0;
    // Debugging TODO remove
    virtual HardValue debugSymtable() = 0;
  };

  class IVMProgram : public IVMUncollectable {
  public:
    virtual size_t getModuleCount() const = 0;
    virtual HardPtr<IVMModule> getModule(size_t index) const = 0;
    virtual HardPtr<IVMRunner> createRunner() = 0;
  };

  class IVMTypeResolver {
  public:
    enum class Kind {
      Value,
      Type
    };
    virtual ~IVMTypeResolver() {}
    virtual Type resolveSymbol(const String& symbol, Kind& kind) = 0;
    virtual IVMTypeSpecification* resolveTypeSpecification(const IVMModule::Node& spec) = 0;
  };

  class IVMModuleBuilder : public IVMUncollectable {
  public:
    using Resolver = IVMTypeResolver;
    class Reporter {
    public:
      virtual ~Reporter() {}
      virtual void report(const SourceRange& range, const String& problem) = 0;
    };
    using Node = IVMModule::Node;
    virtual Node& getRoot() = 0;
    virtual HardPtr<IVMModule> build() = 0;
    // Value expression factories
    virtual Node& exprValueUnaryOp(ValueUnaryOp op, Node& arg, const SourceRange& range) = 0;
    virtual Node& exprValueBinaryOp(ValueBinaryOp op, Node& lhs, Node& rhs, const SourceRange& range) = 0;
    virtual Node& exprValueTernaryOp(ValueTernaryOp op, Node& lhs, Node& mid, Node& rhs, const SourceRange& range) = 0;
    virtual Node& exprValuePredicateOp(ValuePredicateOp op, const SourceRange& range) = 0;
    virtual Node& exprLiteral(const HardValue& literal, const SourceRange& range) = 0;
    virtual Node& exprFunctionCall(Node& function, const SourceRange& range) = 0;
    virtual Node& exprVariableGet(const String& symbol, const SourceRange& range) = 0;
    virtual Node& exprPropertyGet(Node& instance, Node& property, const SourceRange& range) = 0;
    virtual Node& exprIndexGet(Node& instance, Node& index, const SourceRange& range) = 0;
    virtual Node& exprPointeeGet(Node& pointer, const SourceRange& range) = 0;
    virtual Node& exprVariableRef(const String& symbol, const SourceRange& range) = 0;
    virtual Node& exprPropertyRef(Node& instance, Node& property, const SourceRange& range) = 0;
    virtual Node& exprIndexRef(Node& instance, Node& index, const SourceRange& range) = 0;
    virtual Node& exprArrayConstruct(const Type& elementType, const SourceRange& range) = 0;
    virtual Node& exprEonConstruct(const SourceRange& range) = 0;
    virtual Node& exprObjectConstruct(Node& objectType, const SourceRange& range) = 0;
    virtual Node& exprObjectConstructProperty(const String& property, Node& type, Node& value, Accessability accessability, const SourceRange& range) = 0;
    virtual Node& exprFunctionConstruct(Node& functionType, Node& invoke, const SourceRange& range) = 0;
    virtual Node& exprFunctionCapture(const String& symbol, const SourceRange& range) = 0;
    virtual Node& exprGuard(const String& symbol, Node& value, const SourceRange& range) = 0;
    virtual Node& exprNamed(const HardValue& name, Node& value, const SourceRange& range) = 0;
    // Type expression factories
    virtual Node& typeLiteral(const Type& type, const SourceRange& range) = 0;
    virtual Node& typeManifestation(Node& type, const SourceRange& range) = 0;
    virtual Node& typeVariableGet(const String& symbol, const SourceRange& range) = 0;
    virtual Node& typePropertyGet(Node& instance, Node& property, const SourceRange& range) = 0;
    virtual Node& typeUnaryOp(TypeUnaryOp op, Node& arg, const SourceRange& range) = 0;
    virtual Node& typeBinaryOp(TypeBinaryOp op, Node& lhs, Node& rhs, const SourceRange& range) = 0;
    virtual Node& typeFunctionSignature(const String& fname, Node& rtype, const SourceRange& range) = 0;
    virtual Node& typeFunctionSignatureParameter(const String& pname, IFunctionSignatureParameter::Flags pflags, Node& ptype, const SourceRange& range) = 0;
    virtual Node& typeSpecification(const SourceRange& range) = 0;
    virtual Node& typeSpecificationDescription(const String& description, const SourceRange& range) = 0;
    virtual Node& typeSpecificationStaticMember(const String& symbol, Node& type, const SourceRange& range) = 0;
    virtual Node& typeSpecificationInstanceMember(const String& symbol, Node& type, Accessability accessability, const SourceRange& range) = 0;
    // Statement factories
    virtual Node& stmtBlock(const SourceRange& range) = 0;
    virtual Node& stmtIf(Node& condition, const SourceRange& range) = 0;
    virtual Node& stmtWhile(Node& condition, Node& block, const SourceRange& range) = 0;
    virtual Node& stmtDo(Node& block, Node& condition, const SourceRange& range) = 0;
    virtual Node& stmtForEach(const String& symbol, Node& type, Node& iteration, Node& block, const SourceRange& range) = 0;
    virtual Node& stmtForLoop(Node& initial, Node& condition, Node& advance, Node& block, const SourceRange& range) = 0;
    virtual Node& stmtSwitch(Node& expression, size_t defaultIndex, const SourceRange& range) = 0;
    virtual Node& stmtCase(Node& block, const SourceRange& range) = 0;
    virtual Node& stmtBreak(const SourceRange& range) = 0;
    virtual Node& stmtContinue(const SourceRange& range) = 0;
    virtual Node& stmtFunctionInvoke(const SourceRange& range) = 0;
    virtual Node& stmtGeneratorInvoke(Node& function, const SourceRange& range) = 0;
    virtual Node& stmtManifestationInvoke(const SourceRange& range) = 0;
    virtual Node& stmtManifestationProperty(const String& property, Node& type, Node& value, Accessability accessability, const SourceRange& range) = 0;
    virtual Node& stmtVariableDeclare(const String& symbol, Node& type, const SourceRange& range) = 0;
    virtual Node& stmtVariableDefine(const String& symbol, Node& type, Node& value, const SourceRange& range) = 0;
    virtual Node& stmtVariableMutate(const String& symbol, ValueMutationOp op, Node& value, const SourceRange& range) = 0;
    virtual Node& stmtVariableUndeclare(const String& symbol, const SourceRange& range) = 0;
    virtual Node& stmtTypeDefine(const String& symbol, Node& type, const SourceRange& range) = 0;
    virtual Node& stmtPropertyMutate(Node& instance, Node& property, ValueMutationOp op, Node& value, const SourceRange& range) = 0;
    virtual Node& stmtPointeeMutate(Node& instance, ValueMutationOp op, Node& value, const SourceRange& range) = 0;
    virtual Node& stmtIndexMutate(Node& instance, Node& index, ValueMutationOp op, Node& value, const SourceRange& range) = 0;
    virtual Node& stmtThrow(Node& exception, const SourceRange& range) = 0;
    virtual Node& stmtTry(Node& block, const SourceRange& range) = 0;
    virtual Node& stmtCatch(const String& symbol, Node& type, const SourceRange& range) = 0;
    virtual Node& stmtRethrow(const SourceRange& range) = 0;
    virtual Node& stmtReturn(const SourceRange& range) = 0;
    virtual Node& stmtYield(Node& value, const SourceRange& range) = 0;
    virtual Node& stmtYieldAll(Node& value, const SourceRange& range) = 0;
    virtual Node& stmtYieldBreak(const SourceRange& range) = 0;
    virtual Node& stmtYieldContinue(const SourceRange& range) = 0;
    // Type operations
    virtual ITypeForge& getTypeForge() const = 0;
    virtual IVMTypeSpecification* registerTypeSpecification(const Node& spec, Resolver& resolver, Reporter& reporter) = 0;
    virtual Type deduceType(Node& node, Resolver& resolver, Reporter* reporter, egg::ovum::IVMTypeResolver::Kind& deduced) = 0;
    // Constant operations
    virtual HardValue deduceConstant(Node& node) = 0;
    // Modifiers
    virtual void appendChild(Node& parent, Node& child) = 0;
    // Helpers
    Node& glue(Node& parent) {
      return parent;
    }
    template<typename... ARGS>
    Node& glue(Node& parent, Node& head, ARGS&&... tail) {
      this->appendChild(parent, head);
      return this->glue(parent, std::forward<ARGS>(tail)...);
    }
  };

  class IVMProgramBuilder : public IVMUncollectable {
  public:
    virtual IVM& getVM() const = 0;
    virtual bool addBuiltin(const String& symbol, const Type& type) = 0;
    virtual void visitBuiltins(const std::function<void(const String& symbol, const Type& type)>& visitor) const = 0;
    virtual HardPtr<IVMModuleBuilder> createModuleBuilder(const String& resource) = 0;
    virtual HardPtr<IVMProgram> build() = 0;
  };

  class IVM : public IVMUncollectable {
  public:
    // Service access
    virtual IAllocator& getAllocator() const = 0;
    virtual IBasket& getBasket() const = 0;
    virtual ILogger& getLogger() const = 0;
    virtual ITypeForge& getTypeForge() const = 0;
    virtual IBasket& shutdown() = 0;
    // Specification cache
    virtual void addTypeSpecification(IVMTypeSpecification& specification, const IVMModule::Node* node) = 0;
    virtual IVMTypeSpecification* findTypeSpecification(const IVMModule::Node& node) const = 0;
    // Manifestation cache
    virtual void addManifestation(const Type& infratype, const HardObject& manifestation) = 0;
    virtual HardObject findManifestation(const Type& infratype) = 0;
    virtual void finalizeManifestation(const Type& infratype, const HardObject& manifestation) = 0;
    // Builder factories
    virtual HardPtr<IVMProgramBuilder> createProgramBuilder() = 0;
    virtual HardPtr<IVMTypeSpecificationBuilder> createTypeSpecificationBuilder(const IVMModule::Node* spec) = 0;
    // Builtin factories
    virtual HardObject createBuiltinAssert() = 0;
    virtual HardObject createBuiltinPrint() = 0;
    virtual HardObject createBuiltinExpando() = 0; // TODO deprecate
    virtual HardObject createBuiltinCollector() = 0; // TODO testing only
    virtual HardObject createBuiltinSymtable() = 0; // TODO testing only
    // Collectable helpers
    IValue& createSoftValue() {
      auto* soft = this->softCreateValue(nullptr);
      assert(soft != nullptr);
      return *soft;
    }
    IValue& createSoftValue(const HardValue& init) {
      auto* soft = this->softCreateValue(&init.get());
      assert(soft != nullptr);
      return *soft;
    }
    IValue& createSoftAlias(const HardValue& value) {
      auto* soft = this->softCreateAlias(value.get());
      assert(soft != nullptr);
      return *soft;
    }
    IValue& createSoftOwned(const HardValue& value) {
      auto* soft = this->softCreateOwned(value.get());
      assert(soft != nullptr);
      return *soft;
    }
    HardValue getSoftKey(const SoftKey& key) {
      assert(key.ptr != nullptr);
      return this->softHarden(*key.ptr);
    }
    HardValue getSoftValue(const SoftValue& value) {
      assert(value.ptr.ptr != nullptr);
      return this->softHarden(*value.ptr.ptr);
    }
    bool setSoftValue(SoftValue& instance, const HardValue& value) {
      return this->softSetValue(instance.ptr.ptr, value.get());
    }
    HardValue mutSoftValue(SoftValue& instance, ValueMutationOp mutation, const HardValue& value) {
      return this->softMutValue(instance.ptr.ptr, mutation, value.get());
    }
    HardValue refSoftValue(const SoftValue& instance, Modifiability modifiability) {
      return this->softRefValue(*instance.ptr.ptr, modifiability);
    }
    template<typename T>
    T* acquireSoftObject(const T* hard) {
      // TODO simplify?
      if (hard == nullptr) {
        return nullptr;
      }
      ICollectable* soft = nullptr;
      this->softAcquire(soft, hard);
      assert(soft != nullptr);
      return static_cast<T*>(soft);
    }
  private:
    virtual IValue* softCreateValue(const IValue* init) = 0;
    virtual IValue* softCreateAlias(const IValue& value) = 0;
    virtual IValue* softCreateOwned(const IValue& value) = 0;
    virtual void softAcquire(ICollectable*& target, const ICollectable* value) = 0;
    virtual HardValue softHarden(const IValue& soft) = 0;
    virtual bool softSetValue(IValue*& soft, const IValue& value) = 0;
    virtual HardValue softMutValue(IValue*& soft, ValueMutationOp mutation, const IValue& value) = 0;
    virtual HardValue softRefValue(const IValue& pointee, Modifiability modifiability) = 0;
  };

  class VMFactory {
  public:
    // VM factories
    static HardPtr<IVM> createDefault(IAllocator& allocator, ILogger& logger);
    // Function/generator factories
    static HardObject createFunction(IVM& vm, const Type& ftype, const IFunctionSignature& signature, const IVMModule::Node& definition, std::vector<VMCallCapture>&& captures);
    static HardObject createGeneratorIterator(IVM& vm, const Type& ftype, IVMRunner& runner);
  };
}
