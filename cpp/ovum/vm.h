namespace egg::ovum {
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

  class IVMExecution : public ILogger, public IVMCommon {
  public:
    // Exceptions
    virtual HardValue raiseException(const String& message) = 0;
    template<typename... ARGS>
    inline HardValue raise(ARGS&&... args) {
      // TODO deprecate StringBuilder at the public interface
      egg::ovum::StringBuilder sb{ nullptr };
      auto message = sb.add(std::forward<ARGS>(args)...).toUTF8();
      return this->raiseException(this->createString(message.data(), message.size()));
    }
    // Binary operations
    enum class BinaryOp {
      Add,
      Sub,
      Mul,
      Div,
      Rem,
      LT,
      LE,
      EQ,
      NE,
      GE,
      GT,
      BAnd,
      BOr,
      BXor,
      ShiftL,
      ShiftR,
      ShiftU,
      LAnd,
      LOr,
      LNull
    };
    virtual HardValue evaluateBinaryOp(BinaryOp op, const HardValue& lhs, const HardValue& rhs) = 0;
  };

  class IVMCollectable : public ICollectable, public IVMCommon {
  public:
    virtual IAllocator& getAllocator() const = 0;
  };

  class IVMProgramRunner : public IVMCollectable {
  public:
    enum class RunFlags {
      None = 0x0000,
      Step = 0x0001,
      Default = None
    };
    enum class RunOutcome {
      Stepped = 1,
      Completed = 2,
      Faulted = 3
    };
    virtual void addBuiltin(const String& name, const HardValue& value) = 0;
    virtual RunOutcome run(HardValue& retval, RunFlags flags = RunFlags::Default) = 0;
  };

  class IVMProgram : public IVMCollectable {
  public:
    class Node;
    virtual HardPtr<IVMProgramRunner> createRunner() = 0;
  };

  class IVMProgramBuilder : public IVMCollectable {
  public:
    using Node = IVMProgram::Node;
    virtual void addStatement(Node& statement) = 0;
    virtual HardPtr<IVMProgram> build() = 0;
    // Expression factories
    virtual Node& exprBinaryOp(IVMExecution::BinaryOp op, Node& lhs, Node& rhs) = 0;
    virtual Node& exprVariable(const String& name) = 0;
    virtual Node& exprLiteral(const HardValue& literal) = 0;
    virtual Node& exprPropertyGet(Node& instance, Node& property) = 0;
    virtual Node& exprFunctionCall(Node& function) = 0;
    // Statement factories
    virtual Node& stmtVariableDeclare(const String& name) = 0;
    virtual Node& stmtVariableDefine(const String& name, Node& value) = 0;
    virtual Node& stmtVariableSet(const String& name, Node& value) = 0;
    virtual Node& stmtPropertySet(Node& instance, Node& property, Node& value) = 0;
    virtual Node& stmtFunctionCall(Node& function) = 0;
    // Helpers
    Node& glue(Node& parent) {
      return parent;
    }
    template<typename... ARGS>
    Node& glue(Node& parent, Node& head, ARGS&&... tail) {
      this->appendChild(parent, head);
      return this->glue(parent, std::forward<ARGS>(tail)...);
    }
  private:
    virtual void appendChild(Node& parent, Node& child) = 0;
  };

  class IVM : public IHardAcquireRelease, public IVMCommon {
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
    inline IValue* createSoftValue() {
      return this->softCreateValue();
    }
    inline IValue* createSoftAlias(const IValue& value) {
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
