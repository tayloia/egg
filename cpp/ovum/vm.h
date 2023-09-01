namespace egg::ovum {
  class IVMProgramRunner;

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

  class IVMProgramNode : public IVMBase {
  public:
    virtual void addChild(const HardPtr<IVMProgramNode>& child) = 0;
    virtual HardPtr<IVMProgramNode> build() = 0;
  };

  class IVMProgramRunner : public IVMBase {
  public:
    virtual Value step() = 0;
    virtual Value run() = 0;
  };

  class IVMProgram : public IVMBase {
  public:
    virtual HardPtr<IVMProgramRunner> createRunner() = 0;
  };

  class IVMProgramBuilder : public IVMBase {
  public:
    virtual void addStatement(const HardPtr<IVMProgramNode>& statement) = 0;
    virtual HardPtr<IVMProgram> build() = 0;
    // Expression factories
    virtual HardPtr<IVMProgramNode> exprVariable(const String& name) = 0;
    virtual HardPtr<IVMProgramNode> exprLiteralString(const String& literal) = 0;
    // Statement factories
    virtual HardPtr<IVMProgramNode> stmtFunctionCall(const HardPtr<IVMProgramNode>& function) = 0;
    // Helpers
    template<typename... ARGS>
    IVMProgramBuilder* add(const HardPtr<IVMProgramNode>& head, ARGS&&... tail) {
      assert(head != nullptr);
      this->addChildren(head, std::forward<ARGS>(tail)...);
      this->addStatement(head->build());
      return this;
    }
  private:
    void addChildren(const HardPtr<IVMProgramNode>&) {
      // Do nothing
    }
    template<typename... ARGS>
    void addChildren(const HardPtr<IVMProgramNode>& head, ARGS&&... args, const HardPtr<IVMProgramNode>& tail) {
      assert(head != nullptr);
      assert(tail != nullptr);
      this->add(head, std::forward<ARGS>(args)...);
      head->addChild(tail);
    }
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
