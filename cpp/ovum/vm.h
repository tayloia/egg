namespace egg::ovum {
  class IVMBase : public IHardAcquireRelease {
  public:
    virtual IAllocator& getAllocator() const = 0;
    virtual IBasket& getBasket() const = 0;
    // String factory helpers
    inline String createStringUTF8(const void* utf8, size_t bytes = SIZE_MAX, size_t codepoints = SIZE_MAX) {
      return String::fromUTF8(&this->getAllocator(), utf8, bytes, codepoints);
    }
    inline String createStringUTF32(const void* utf32, size_t codepoints = SIZE_MAX) {
      return String::fromUTF32(&this->getAllocator(), utf32, codepoints);
    }
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
    virtual IVMProgram::Node& exprLiteralString(const String& literal) = 0;
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
    virtual IAllocator& getAllocator() const = 0;
    virtual IBasket& getBasket() const = 0;
    // Value factories
    virtual Value createValueVoid() = 0;
    virtual Value createValueNull() = 0;
    virtual Value createValueBool(Bool value) = 0;
    virtual Value createValueInt(Int value) = 0;
    virtual Value createValueFloat(Float value) = 0;
    virtual Value createValueString(const String& value) = 0;
    // Builder factories
    virtual HardPtr<IVMProgramBuilder> createProgramBuilder() = 0;
    // Helpers
    inline Value createValueString(const std::string& value) {
      return this->createValueString(this->createStringUTF8(value.data(), value.size()));
    }
    inline Value createValueString(const std::u8string& value) {
      return this->createValueString(this->createStringUTF8(value.data(), value.size()));
    }
    inline Value createValueString(const std::u32string& value) {
      return this->createValueString(this->createStringUTF32(value.data(), value.size()));
    }
  };

  class VMFactory {
  public:
    static HardPtr<IVM> createDefault(IAllocator& allocator);
    static HardPtr<IVM> createTest(IAllocator& allocator);
  };
}
