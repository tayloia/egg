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

  class IVMProgram : public IVMBase {
  public:
  };

  class IVMProgramBuilder : public IVMBase {
  public:
    virtual HardPtr<IVMProgram> build() = 0;
  };

  class IVM : public IVMBase {
  public:
    virtual IAllocator& getAllocator() const = 0;
    virtual IBasket& getBasket() const = 0;
    // Value factories
    virtual HardPtr<IValue> createValueVoid() = 0;
    virtual HardPtr<IValue> createValueNull() = 0;
    virtual HardPtr<IValue> createValueBool(Bool value) = 0;
    virtual HardPtr<IValue> createValueInt(Int value) = 0;
    virtual HardPtr<IValue> createValueFloat(Float value) = 0;
    virtual HardPtr<IValue> createValueString(const String& value) = 0;
    // Builder factories
    virtual HardPtr<IVMProgramBuilder> createProgramBuilder() = 0;
    // Helpers
    inline HardPtr<IValue> createValueString(const std::string& value) {
      return this->createValueString(this->createStringUTF8(value.data(), value.size()));
    }
    inline HardPtr<IValue> createValueString(const std::u8string& value) {
      return this->createValueString(this->createStringUTF8(value.data(), value.size()));
    }
    inline HardPtr<IValue> createValueString(const std::u32string& value) {
      return this->createValueString(this->createStringUTF32(value.data(), value.size()));
    }
  };

  class VMFactory {
  public:
    static HardPtr<IVM> createDefault(IAllocator& allocator);
    static HardPtr<IVM> createTest(IAllocator& allocator);
  };
}
