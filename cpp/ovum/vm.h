namespace egg::ovum {
  // Value operators
  enum class ValueUnaryOp {
    Negate,             // -a
    BitwiseNot,         // ~a
    LogicalNot          // !a
  };
  enum class ValueBinaryOp {
    Add,                // a + b
    Subtract,           // a - b
    Multiply,           // a * b
    Divide,             // a / b
    Remainder,          // a % b
    LessThan,           // a < b
    LessThanOrEqual,    // a <= b
    Equal,              // a == b
    NotEqual,           // a != b
    GreaterThanOrEqual, // a >= b
    GreaterThan,        // a > b
    BitwiseAnd,         // a & b
    BitwiseOr,          // a | b
    BitwiseXor,         // a ^ b
    ShiftLeft,          // a << b
    ShiftRight,         // a >> b
    ShiftRightUnsigned, // a >>> b
    IfNull,             // a ?? b
    IfFalse,            // a && b
    IfTrue              // a || b
  };
  enum class ValueTernaryOp {
    IfThenElse          // a ? b : c
  };
  enum class ValueMutationOp {
    Assign,             // a = b
    Decrement,          // --a
    Increment,          // ++a
    Add,                // a += b
    Subtract,           // a -= b
    Multiply,           // a *= b
    Divide,             // a /= b
    Remainder,          // a %= b
    BitwiseAnd,         // a &= b
    BitwiseOr,          // a |= b
    BitwiseXor,         // a ^= b
    ShiftLeft,          // a <<= b
    ShiftRight,         // a >>= b
    ShiftRightUnsigned, // a >>>= b
    IfNull,             // a ??= b
    IfFalse,            // a &&= b
    IfTrue,             // a ||= b
    Noop // TODO cancels any pending prechecks on this thread
  };

  // Type operators
  enum class TypeBinaryOp {
    Union               // a | b
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
  };

  class IVMCallStack : public IHardAcquireRelease {
  public:
    virtual void print(Printer& printer) const = 0;
  };

  class IVMExecution : public ILogger, public IVMCommon {
  public:
    // Exceptions
    virtual HardValue raiseException(const HardValue& inner) = 0;
    virtual HardValue raiseRuntimeError(const String& message) = 0;
    // Operations
    virtual HardValue evaluateValueUnaryOp(ValueUnaryOp op, const HardValue& arg) = 0;
    virtual HardValue evaluateValueBinaryOp(ValueBinaryOp op, const HardValue& lhs, const HardValue& rhs) = 0;
    virtual HardValue evaluateValueTernaryOp(ValueTernaryOp op, const HardValue& lhs, const HardValue& mid, const HardValue& rhs) = 0;
    virtual HardValue precheckValueMutationOp(ValueMutationOp op, HardValue& lhs, ValueFlags rhs) = 0;
    virtual HardValue evaluateValueMutationOp(ValueMutationOp op, HardValue& lhs, const HardValue& rhs) = 0;
  };

  class IVMCollectable : public ICollectable, public IVMCommon {
  public:
    virtual IAllocator& getAllocator() const = 0;
  };

  class IVMUncollectable : public IHardAcquireRelease, public IVMCommon {
  public:
    virtual IAllocator& getAllocator() const = 0;
  };

  class IVMRunner : public IVMCollectable {
  public:
    enum class RunFlags {
      None = 0x0000,
      Step = 0x0001,
      Default = None
    };
    enum class RunOutcome {
      Stepped = 1,
      Succeeded = 2,
      Failed = 3
    };
    virtual void addBuiltin(const String& symbol, const HardValue& value) = 0;
    virtual RunOutcome run(HardValue& retval, RunFlags flags = RunFlags::Default) = 0;
  };

  class IVMProgram;

  class IVMModule : public IVMUncollectable {
  public:
    class Node;
    virtual HardPtr<IVMRunner> createRunner(IVMProgram& program) = 0;
  };

  class IVMProgram : public IVMUncollectable {
  public:
    virtual size_t getModuleCount() const = 0;
    virtual HardPtr<IVMModule> getModule(size_t index) const = 0;
    virtual HardPtr<IVMRunner> createRunner() = 0;
  };

  class IVMModuleBuilder : public IVMUncollectable {
  public:
    using Node = IVMModule::Node;
    virtual Node& getRoot() = 0;
    virtual HardPtr<IVMModule> build() = 0;
    // Value expression factories
    virtual Node& exprValueUnaryOp(ValueUnaryOp op, Node& arg, size_t line, size_t column) = 0;
    virtual Node& exprValueBinaryOp(ValueBinaryOp op, Node& lhs, Node& rhs, size_t line, size_t column) = 0;
    virtual Node& exprValueTernaryOp(ValueTernaryOp op, Node& lhs, Node& mid, Node& rhs, size_t line, size_t column) = 0;
    virtual Node& exprVariable(const String& symbol, size_t line, size_t column) = 0;
    virtual Node& exprLiteral(const HardValue& literal, size_t line, size_t column) = 0;
    virtual Node& exprPropertyGet(Node& instance, Node& property, size_t line, size_t column) = 0;
    virtual Node& exprFunctionCall(Node& function, size_t line, size_t column) = 0;
    // Type expression factories
    virtual Node& typePrimitive(ValueFlags primitive, size_t line, size_t column) = 0;
    // Statement factories
    virtual Node& stmtBlock(size_t line, size_t column) = 0;
    virtual Node& stmtIf(Node& condition, size_t line, size_t column) = 0;
    virtual Node& stmtWhile(Node& condition, Node& block, size_t line, size_t column) = 0;
    virtual Node& stmtDo(Node& block, Node& condition, size_t line, size_t column) = 0;
    virtual Node& stmtFor(Node& initial, Node& condition, Node& advance, Node& block, size_t line, size_t column) = 0;
    virtual Node& stmtSwitch(Node& expression, size_t defaultIndex, size_t line, size_t column) = 0;
    virtual Node& stmtCase(Node& block, size_t line, size_t column) = 0;
    virtual Node& stmtBreak(size_t line, size_t column) = 0;
    virtual Node& stmtContinue(size_t line, size_t column) = 0;
    virtual Node& stmtVariableDeclare(const String& symbol, Node& type, size_t line, size_t column) = 0;
    virtual Node& stmtVariableDefine(const String& symbol, Node& type, Node& value, size_t line, size_t column) = 0;
    virtual Node& stmtVariableSet(const String& symbol, Node& value, size_t line, size_t column) = 0;
    virtual Node& stmtVariableMutate(const String& symbol, ValueMutationOp op, Node& value, size_t line, size_t column) = 0;
    virtual Node& stmtPropertySet(Node& instance, Node& property, Node& value, size_t line, size_t column) = 0;
    virtual Node& stmtFunctionCall(Node& function, size_t line, size_t column) = 0;
    virtual Node& stmtThrow(Node& exception, size_t line, size_t column) = 0;
    virtual Node& stmtTry(Node& block, size_t line, size_t column) = 0;
    virtual Node& stmtCatch(const String& symbol, Node& type, size_t line, size_t column) = 0;
    virtual Node& stmtRethrow(size_t line, size_t column) = 0;
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
    virtual HardPtr<IVMModuleBuilder> createModuleBuilder(const String& resource) = 0;
    virtual HardPtr<IVMProgram> build() = 0;
  };

  class IVM : public IVMUncollectable {
  public:
    // Service access
    virtual IAllocator& getAllocator() const = 0;
    virtual IBasket& getBasket() const = 0;
    virtual ILogger& getLogger() const = 0;
    // Builder factories
    virtual HardPtr<IVMProgramBuilder> createProgramBuilder() = 0;
    // Builtin factories
    virtual HardObject createBuiltinAssert() = 0;
    virtual HardObject createBuiltinPrint() = 0;
    virtual HardObject createBuiltinExpando() = 0; // TODO deprecate
    virtual HardObject createBuiltinCollector() = 0; // TODO testing only
    // Collectable helpers
    IValue* createSoftValue() {
      return this->softCreateValue();
    }
    IValue* createSoftAlias(const IValue& value) {
      return this->softCreateAlias(value);
    }
    HardValue getSoftValue(SoftValue& instance) {
      auto* hard = this->softHarden(instance.ptr.ptr);
      assert(hard != nullptr);
      return HardValue(*hard);
    }
    bool setSoftValue(SoftValue& instance, const HardValue& value) {
      return this->softSetValue(instance.ptr.ptr, value.get());
    }
  private:
    virtual IValue* softCreateValue() = 0;
    virtual IValue* softCreateAlias(const IValue& value) = 0;
    virtual void softAcquire(ICollectable*& target, const ICollectable* value) = 0;
    virtual IValue* softHarden(const IValue* soft) = 0;
    virtual bool softSetValue(IValue*& soft, const IValue& value) = 0;
  };

  class VMFactory {
  public:
    // VM factories
    static HardPtr<IVM> createDefault(IAllocator& allocator, ILogger& logger);
  };
}
