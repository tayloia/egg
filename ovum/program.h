namespace egg::ovum {
  class IProgram : public IHardAcquireRelease {
  public:
    virtual Variant run(const IModule& module) = 0;
  };
  using Program = HardPtr<IProgram>;

  class ProgramFactory {
  public:
    static Program create(IAllocator& allocator, ILogger& logger);
  };
}
