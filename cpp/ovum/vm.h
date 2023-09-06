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
    virtual Value createValueVoid() = 0;
    virtual Value createValueNull() = 0;
    virtual Value createValueBool(Bool value) = 0;
    virtual Value createValueInt(Int value) = 0;
    virtual Value createValueFloat(Float value) = 0;
    virtual Value createValueString(const String& value) = 0;
    virtual Value createValueObject(const Object& value) = 0;
    // Value factory helpers
    inline Value createValue(std::nullptr_t) {
      return this->createValueNull();
    }
    inline Value createValue(Bool value) {
      return this->createValueBool(value);
    }
    inline Value createValue(Int value) {
      return this->createValueInt(value);
    }
    inline Value createValue(Float value) {
      return this->createValueFloat(value);
    }
    inline Value createValue(const char* value) {
      return value ? this->createValueString(this->createString(value)) : this->createValueNull();
    }
    inline Value createValue(const char8_t* value) {
      return value ? this->createValueString(this->createString(value)) : this->createValueNull();
    }
    inline Value createValue(const char32_t* value) {
      return value ? this->createValueString(this->createString(value)) : this->createValueNull();
    }
    inline Value createValue(const std::u8string& value) {
      return this->createValueString(this->createString(value));
    }
    inline Value createValue(const std::u32string& value) {
      return this->createValueString(this->createString(value));
    }
    inline Value createValue(const String& value) {
      return this->createValueString(value);
    }
    inline Value createValue(const Object& value) {
      return this->createValueObject(value);
    }
  };

  class IVMBase : public IHardAcquireRelease, public IVMCommon {
  public:
    virtual IAllocator& getAllocator() const = 0;
  };

  class IVMExecution : public ILogger, public IVMCommon {
  public:
  };

  class IVMProgramRunner : public IVMBase {
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
    virtual void addBuiltin(const String& name, const Value& value) = 0;
    virtual RunOutcome run(Value& retval, RunFlags flags = RunFlags::Default) = 0;
  };

  class IVMProgram : public IVMBase {
  public:
    class Node;
    virtual HardPtr<IVMProgramRunner> createRunner() = 0;
  };

  class IVMProgramBuilder : public IVMBase {
  public:
    virtual void addStatement(IVMProgram::Node& statement) = 0;
    virtual HardPtr<IVMProgram> build() = 0;
    // Expression factories
    virtual IVMProgram::Node& exprVariable(const String& name) = 0;
    virtual IVMProgram::Node& exprLiteral(const Value& literal) = 0;
    // Statement factories
    virtual IVMProgram::Node& stmtFunctionCall(IVMProgram::Node& function) = 0;
    // Helpers
    template<typename... ARGS>
    void addStatement(IVMProgram::Node& statement, IVMProgram::Node& head, ARGS&&... tail) {
      this->appendChild(statement, head);
      this->addStatement(statement, std::forward<ARGS>(tail)...);
    }
  private:
    virtual void appendChild(IVMProgram::Node& parent, IVMProgram::Node& child) = 0;
  };

  class IVM : public IVMBase {
  public:
    // Service access
    virtual ILogger& getLogger() const = 0;
    // Builder factories
    virtual HardPtr<IVMProgramBuilder> createProgramBuilder() = 0;
    // Builtin factories
    virtual Object createBuiltinPrint() = 0;
  };

  class VMFactory {
  public:
    // VM factories
    static HardPtr<IVM> createDefault(IAllocator& allocator, ILogger& logger);
    static HardPtr<IVM> createTest(IAllocator& allocator, ILogger& logger);
  };
}
