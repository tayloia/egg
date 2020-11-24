namespace egg::ovum {
  class IModule;
  class INode;

  class IProgram : public IHardAcquireRelease {
  public:
    virtual bool builtin(const String& name, const Value& value) = 0;
    virtual Value run(const IModule& module, ILogger::Severity* severity) = 0;
  };
  using Program = HardPtr<IProgram>;

  class ProgramFactory {
  public:
    static Program createProgram(IAllocator& allocator, ILogger& logger);
  };
}
