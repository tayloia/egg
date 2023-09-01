namespace egg::ovum {

  class IVMBase : public IHardAcquireRelease {
  public:
    virtual IAllocator& getAllocator() const = 0;
    virtual IBasket& getBasket() const = 0;
    // String factory helpers
    inline String createUTF8(const std::u8string& utf8, size_t codepoints = SIZE_MAX) {
      return this->getAllocator().fromUTF8(utf8, codepoints);
    }
    inline String createUTF32(const std::u32string& utf32) {
      return this->getAllocator().fromUTF32(utf32);
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
    // Build factories
    virtual HardPtr<IVMProgramBuilder> createProgramBuilder() = 0;
  };

  class VMFactory {
  public:
    static HardPtr<IVM> createDefault(IAllocator& allocator);
    static HardPtr<IVM> createTest(IAllocator& allocator);
  };
}
