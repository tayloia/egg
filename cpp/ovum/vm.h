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
    virtual HardValue createValueVoid() = 0;
    virtual HardValue createValueNull() = 0;
    virtual HardValue createValueBool(Bool value) = 0;
    virtual HardValue createValueInt(Int value) = 0;
    virtual HardValue createValueFloat(Float value) = 0;
    virtual HardValue createValueString(const String& value) = 0;
    virtual HardValue createValueObjectHard(const HardObject& value) = 0;
    inline HardValue createValue(std::nullptr_t) {
      return this->createValueNull();
    }
    inline HardValue createValue(Bool value) {
      return this->createValueBool(value);
    }
    inline HardValue createValue(Int value) {
      return this->createValueInt(value);
    }
    inline HardValue createValue(Float value) {
      return this->createValueFloat(value);
    }
    inline HardValue createValue(const char* value) {
      return value ? this->createValueString(this->createString(value)) : this->createValueNull();
    }
    inline HardValue createValue(const char8_t* value) {
      return value ? this->createValueString(this->createString(value)) : this->createValueNull();
    }
    inline HardValue createValue(const char32_t* value) {
      return value ? this->createValueString(this->createString(value)) : this->createValueNull();
    }
    inline HardValue createValue(const std::u8string& value) {
      return this->createValueString(this->createString(value));
    }
    inline HardValue createValue(const std::u32string& value) {
      return this->createValueString(this->createString(value));
    }
    inline HardValue createValue(const String& value) {
      return this->createValueString(value);
    }
  };

  class IVMExecution : public ILogger, public IVMCommon {
  public:
    virtual HardValue raiseException(const String& message) = 0;
    template<typename... ARGS>
    inline HardValue raise(ARGS&&... args) {
      // TODO deprecate StringBuilder at the public interface
      egg::ovum::StringBuilder sb{ nullptr };
      auto message = sb.add(std::forward<ARGS>(args)...).toUTF8();
      return this->raiseException(this->createString(message.data(), message.size()));
    }
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
    virtual Node& exprVariable(const String& name) = 0;
    virtual Node& exprLiteral(const HardValue& literal) = 0;
    virtual Node& exprFunctionCall(Node& function) = 0;
    // Statement factories
    virtual Node& stmtVariableDeclare(const String& name) = 0;
    virtual Node& stmtVariableDefine(const String& name) = 0;
    virtual Node& stmtVariableSet(const String& name) = 0;
    virtual Node& stmtVariableUndeclare(const String& name) = 0;
    virtual Node& stmtPropertySet(Node& instance, const HardValue& property) = 0;
    virtual Node& stmtFunctionCall(Node& function) = 0;
    // Helpers
    template<typename... ARGS>
    void addStatement(Node& statement, Node& head, ARGS&&... tail) {
      this->appendChild(statement, head);
      this->addStatement(statement, std::forward<ARGS>(tail)...);
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
    // Helpers
    virtual void softAcquire(ICollectable*& target, const ICollectable* value) = 0;
    template<typename T>
    inline void setSoftPtr(SoftPtr<T>& target, const T* value) {
      this->softAcquire(target.ptr, value);
    }
    template<typename T>
    inline void setSoftPtr(SoftPtr<T>& target, const HardPtr<T>& value) {
      this->softAcquire(target.ptr, value.get());
    }
  };

  class VMFactory {
  public:
    // VM factories
    static HardPtr<IVM> createDefault(IAllocator& allocator, ILogger& logger);
  };
}
