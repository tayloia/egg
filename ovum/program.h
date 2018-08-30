namespace egg::ovum {
  class IProgram : public IHardAcquireRelease {
  public:
    virtual bool builtin(const String& name, const Variant& value) = 0;
    virtual Variant run(const IModule& module) = 0;
  };
  using Program = HardPtr<IProgram>;

  class ProgramFactory {
  public:
    static Program createProgram(IAllocator& allocator, ILogger& logger);
  };
}
