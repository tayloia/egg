namespace egg::yolk {
  class EggEngineProgram {
  private:
    std::shared_ptr<IEggParserNode> root;
  public:
    explicit EggEngineProgram(const std::shared_ptr<IEggParserNode>& root)
      : root(root) {
      assert(root != nullptr);
    }
    void execute(IEggEngineExecutionContext& execution);
  };
}
