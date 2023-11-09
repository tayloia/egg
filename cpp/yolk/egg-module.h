namespace egg::yolk {
  class IEggModule {
  public:
    virtual ~IEggModule() {}
    virtual void execute() const = 0;
  };
}
