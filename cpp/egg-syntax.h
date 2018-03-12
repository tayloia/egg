namespace egg::yolk {
  class IEggSyntaxNode;
  class IEggParserNode;

  class IEggSyntaxNode {
  public:
    virtual void dump(std::ostream& os) const = 0;
    virtual std::unique_ptr<IEggParserNode> promote();
  };

  class EggSyntaxNodeChildren0 : public IEggSyntaxNode {
  public:
    EggSyntaxNodeChildren0() {
    }
    virtual ~EggSyntaxNodeChildren0() {
    }
  };

  class EggSyntaxNodeChildren1 : public IEggSyntaxNode {
    EGG_NO_COPY(EggSyntaxNodeChildren1);
  protected:
    std::unique_ptr<IEggSyntaxNode> child;
  public:
    explicit EggSyntaxNodeChildren1(std::unique_ptr<IEggSyntaxNode>&& child)
      : child(std::move(child)) {
      assert(this->child != nullptr);
    }
    virtual ~EggSyntaxNodeChildren1() {
    }
  };

  template<size_t N>
  class EggSyntaxNodeChildrenN : public IEggSyntaxNode {
    EGG_NO_COPY(EggSyntaxNodeChildrenN);
  protected:
    static const size_t count = N;
    std::unique_ptr<IEggSyntaxNode> child[N];
    EggSyntaxNodeChildrenN() {
    }
    EggSyntaxNodeChildrenN(std::unique_ptr<IEggSyntaxNode>&& child0, std::unique_ptr<IEggSyntaxNode>&& child1) {
      // It's quite common to have exactly two children, so this construct makes derived classes more concise
      assert(child0 != nullptr);
      assert(child1 != nullptr);
      this->child[0] = std::move(child0);
      this->child[1] = std::move(child1);
    }
  public:
    virtual ~EggSyntaxNodeChildrenN() {
    }
  };

  class EggSyntaxNodeChildrenV : public IEggSyntaxNode {
    EGG_NO_COPY(EggSyntaxNodeChildrenV);
  protected:
    std::vector<std::unique_ptr<IEggSyntaxNode>> child;
  public:
    EggSyntaxNodeChildrenV() {
    }
    virtual ~EggSyntaxNodeChildrenV() {
    }
    void addChild(std::unique_ptr<IEggSyntaxNode>&& node) {
      this->child.emplace_back(std::move(node));
    }
  };

  class EggSyntaxNode_Empty : public EggSyntaxNodeChildren0 {
    EGG_NO_COPY(EggSyntaxNode_Empty);
  public:
    EggSyntaxNode_Empty() {
    }
    virtual void dump(std::ostream& os) const override;
  };

  class EggSyntaxNode_Module : public EggSyntaxNodeChildrenV {
    EGG_NO_COPY(EggSyntaxNode_Module);
  public:
    EggSyntaxNode_Module() {
    }
    virtual void dump(std::ostream& os) const override;
  };

  class EggSyntaxNode_Block : public EggSyntaxNodeChildrenV {
    EGG_NO_COPY(EggSyntaxNode_Block);
  public:
    EggSyntaxNode_Block() {
    }
    virtual void dump(std::ostream& os) const override;
  };

  class EggSyntaxNode_Type : public EggSyntaxNodeChildren0 {
    EGG_NO_COPY(EggSyntaxNode_Type);
  public:
    enum Allowed {
      Inferred = 0x00,
      Void = 0x01,
      Bool = 0x02,
      Int = 0x04,
      Float = 0x08,
      String = 0x10,
      Object = 0x20
    };
  private:
    Allowed allowed;
  public:
    explicit EggSyntaxNode_Type(Allowed allowed)
      : allowed(allowed) {
    }
    virtual void dump(std::ostream& os) const override;
    bool isInferred() const {
      return (this->allowed & ~Void) == 0;
    }
    static std::string to_string(Allowed allowed);
  };

  class EggSyntaxNode_VariableDeclaration : public EggSyntaxNodeChildren1 {
    EGG_NO_COPY(EggSyntaxNode_VariableDeclaration);
  private:
    std::string name;
  public:
    EggSyntaxNode_VariableDeclaration(const std::string& name, std::unique_ptr<IEggSyntaxNode>&& type)
      : EggSyntaxNodeChildren1(std::move(type)), name(name) {
    }
    virtual void dump(std::ostream& os) const override;
  };

  class EggSyntaxNode_VariableInitialization : public EggSyntaxNodeChildrenN<2> {
    EGG_NO_COPY(EggSyntaxNode_VariableInitialization);
  private:
    std::string name;
  public:
    EggSyntaxNode_VariableInitialization(const std::string& name, std::unique_ptr<IEggSyntaxNode>&& type, std::unique_ptr<IEggSyntaxNode>&& expr)
      : EggSyntaxNodeChildrenN(std::move(type), std::move(expr)), name(name) {
    }
    virtual void dump(std::ostream& os) const override;
  };

  class EggSyntaxNode_Assignment : public EggSyntaxNodeChildrenN<2> {
    EGG_NO_COPY(EggSyntaxNode_Assignment);
  private:
    EggTokenizerOperator op;
  public:
    EggSyntaxNode_Assignment(EggTokenizerOperator op, std::unique_ptr<IEggSyntaxNode>&& lhs, std::unique_ptr<IEggSyntaxNode>&& rhs)
      : EggSyntaxNodeChildrenN(std::move(lhs), std::move(rhs)), op(op) {
    }
    virtual void dump(std::ostream& os) const override;
  };

  class EggSyntaxNode_Mutate : public EggSyntaxNodeChildren1 {
    EGG_NO_COPY(EggSyntaxNode_Mutate);
  private:
    EggTokenizerOperator op;
  public:
    EggSyntaxNode_Mutate(EggTokenizerOperator op, std::unique_ptr<IEggSyntaxNode>&& expr)
      : EggSyntaxNodeChildren1(std::move(expr)), op(op) {
    }
    virtual void dump(std::ostream& os) const override;
  };

  class EggSyntaxNode_Break : public EggSyntaxNodeChildren0 {
    EGG_NO_COPY(EggSyntaxNode_Break);
  public:
    EggSyntaxNode_Break() {
    }
    virtual void dump(std::ostream& os) const override;
  };

  class EggSyntaxNode_Case : public EggSyntaxNodeChildren1 {
    EGG_NO_COPY(EggSyntaxNode_Case);
  public:
    explicit EggSyntaxNode_Case(std::unique_ptr<IEggSyntaxNode>&& expr)
      : EggSyntaxNodeChildren1(std::move(expr)) {
    }
    virtual void dump(std::ostream& os) const override;
  };

  class EggSyntaxNode_Catch : public EggSyntaxNodeChildrenN<2> {
    EGG_NO_COPY(EggSyntaxNode_Catch);
  private:
    std::string name;
  public:
    EggSyntaxNode_Catch(const std::string& name, std::unique_ptr<IEggSyntaxNode>&& type, std::unique_ptr<IEggSyntaxNode>&& block)
      : EggSyntaxNodeChildrenN(std::move(type), std::move(block)), name(name) {
    }
    virtual void dump(std::ostream& os) const override;
  };

  class EggSyntaxNode_Continue : public EggSyntaxNodeChildren0 {
    EGG_NO_COPY(EggSyntaxNode_Continue);
  public:
    EggSyntaxNode_Continue() {
    }
    virtual void dump(std::ostream& os) const override;
  };

  class EggSyntaxNode_Default : public EggSyntaxNodeChildren0 {
    EGG_NO_COPY(EggSyntaxNode_Default);
  public:
    EggSyntaxNode_Default() {
    }
    virtual void dump(std::ostream& os) const override;
  };

  class EggSyntaxNode_Do : public EggSyntaxNodeChildrenN<2> {
    EGG_NO_COPY(EggSyntaxNode_Do);
  public:
    EggSyntaxNode_Do(std::unique_ptr<IEggSyntaxNode>&& cond, std::unique_ptr<IEggSyntaxNode>&& block)
      : EggSyntaxNodeChildrenN(std::move(cond), std::move(block)) {
    }
    virtual void dump(std::ostream& os) const override;
  };

  class EggSyntaxNode_If : public EggSyntaxNodeChildrenV {
    EGG_NO_COPY(EggSyntaxNode_If);
  public:
    EggSyntaxNode_If(std::unique_ptr<IEggSyntaxNode>&& cond, std::unique_ptr<IEggSyntaxNode>&& block) {
      this->addChild(std::move(cond));
      this->addChild(std::move(block));
    }
    virtual void dump(std::ostream& os) const override;
  };

  class EggSyntaxNode_Finally : public EggSyntaxNodeChildren1 {
    EGG_NO_COPY(EggSyntaxNode_Finally);
  private:
    std::string name;
  public:
    EggSyntaxNode_Finally(std::unique_ptr<IEggSyntaxNode>&& block)
      : EggSyntaxNodeChildren1(std::move(block)) {
    }
    virtual void dump(std::ostream& os) const override;
  };

  class EggSyntaxNode_For : public EggSyntaxNodeChildrenN<4> {
    EGG_NO_COPY(EggSyntaxNode_For);
  public:
    EggSyntaxNode_For(std::unique_ptr<IEggSyntaxNode>&& pre, std::unique_ptr<IEggSyntaxNode>&& cond, std::unique_ptr<IEggSyntaxNode>&& post, std::unique_ptr<IEggSyntaxNode>&& block)
      : EggSyntaxNodeChildrenN(std::move(pre), std::move(cond)) {
      assert(post != nullptr);
      assert(block != nullptr);
      this->child[2] = std::move(post);
      this->child[3] = std::move(block);
    }
    virtual void dump(std::ostream& os) const override;
  };

  class EggSyntaxNode_Foreach : public EggSyntaxNodeChildrenN<3> {
    EGG_NO_COPY(EggSyntaxNode_Foreach);
  public:
    EggSyntaxNode_Foreach(std::unique_ptr<IEggSyntaxNode>&& target, std::unique_ptr<IEggSyntaxNode>&& expr, std::unique_ptr<IEggSyntaxNode>&& block)
      : EggSyntaxNodeChildrenN(std::move(target), std::move(expr)) {
      assert(block != nullptr);
      this->child[2] = std::move(block);
    }
    virtual void dump(std::ostream& os) const override;
  };

  class EggSyntaxNode_Return : public EggSyntaxNodeChildrenV {
    EGG_NO_COPY(EggSyntaxNode_Return);
  public:
    EggSyntaxNode_Return() {
    }
    virtual void dump(std::ostream& os) const override;
  };

  class EggSyntaxNode_Switch : public EggSyntaxNodeChildrenN<2> {
    EGG_NO_COPY(EggSyntaxNode_Switch);
  public:
    EggSyntaxNode_Switch(std::unique_ptr<IEggSyntaxNode>&& expr, std::unique_ptr<IEggSyntaxNode>&& block)
      : EggSyntaxNodeChildrenN(std::move(expr), std::move(block)) {
    }
    virtual void dump(std::ostream& os) const override;
  };

  class EggSyntaxNode_Throw : public EggSyntaxNodeChildrenV {
    EGG_NO_COPY(EggSyntaxNode_Throw);
  public:
    EggSyntaxNode_Throw() {
    }
    virtual void dump(std::ostream& os) const override;
  };

  class EggSyntaxNode_Try : public EggSyntaxNodeChildrenV {
    EGG_NO_COPY(EggSyntaxNode_Try);
  public:
    EggSyntaxNode_Try(std::unique_ptr<IEggSyntaxNode>&& block) {
      this->addChild(std::move(block));
    }
    virtual void dump(std::ostream& os) const override;
  };

  class EggSyntaxNode_Using : public EggSyntaxNodeChildrenN<2> {
    EGG_NO_COPY(EggSyntaxNode_Using);
  public:
    EggSyntaxNode_Using(std::unique_ptr<IEggSyntaxNode>&& expr, std::unique_ptr<IEggSyntaxNode>&& block)
      : EggSyntaxNodeChildrenN(std::move(expr), std::move(block)) {
    }
    virtual void dump(std::ostream& os) const override;
  };

  class EggSyntaxNode_While : public EggSyntaxNodeChildrenN<2> {
    EGG_NO_COPY(EggSyntaxNode_While);
  public:
    EggSyntaxNode_While(std::unique_ptr<IEggSyntaxNode>&& cond, std::unique_ptr<IEggSyntaxNode>&& block)
      : EggSyntaxNodeChildrenN(std::move(cond), std::move(block)) {
    }
    virtual void dump(std::ostream& os) const override;
  };

  class EggSyntaxNode_Yield : public EggSyntaxNodeChildren1 {
    EGG_NO_COPY(EggSyntaxNode_Yield);
  public:
    EggSyntaxNode_Yield(std::unique_ptr<IEggSyntaxNode>&& expr)
      : EggSyntaxNodeChildren1(std::move(expr)) {
    }
    virtual void dump(std::ostream& os) const override;
  };

  class EggSyntaxNode_UnaryOperator : public EggSyntaxNodeChildren1 {
    EGG_NO_COPY(EggSyntaxNode_UnaryOperator);
  private:
    EggTokenizerOperator op;
  public:
    EggSyntaxNode_UnaryOperator(EggTokenizerOperator op, std::unique_ptr<IEggSyntaxNode>&& expr)
      : EggSyntaxNodeChildren1(std::move(expr)), op(op) {
    }
    virtual void dump(std::ostream& os) const override;
  };

  class EggSyntaxNode_BinaryOperator : public EggSyntaxNodeChildrenN<2> {
    EGG_NO_COPY(EggSyntaxNode_BinaryOperator);
  private:
    EggTokenizerOperator op;
  public:
    EggSyntaxNode_BinaryOperator(EggTokenizerOperator op, std::unique_ptr<IEggSyntaxNode>&& lhs, std::unique_ptr<IEggSyntaxNode>&& rhs)
      : EggSyntaxNodeChildrenN(std::move(lhs), std::move(rhs)), op(op) {
    }
    virtual void dump(std::ostream& os) const override;
  };

  class EggSyntaxNode_TernaryOperator : public EggSyntaxNodeChildrenN<3> {
    EGG_NO_COPY(EggSyntaxNode_TernaryOperator);
  public:
    explicit EggSyntaxNode_TernaryOperator(std::unique_ptr<IEggSyntaxNode>&& condition, std::unique_ptr<IEggSyntaxNode>&& whenTrue, std::unique_ptr<IEggSyntaxNode>&& whenFalse) {
      assert(condition != nullptr);
      assert(whenTrue != nullptr);
      assert(whenFalse != nullptr);
      this->child[0] = std::move(condition);
      this->child[1] = std::move(whenTrue);
      this->child[2] = std::move(whenFalse);
    }
    virtual void dump(std::ostream& os) const override;
  };

  class EggSyntaxNode_Call : public EggSyntaxNodeChildrenV {
    EGG_NO_COPY(EggSyntaxNode_Call);
  public:
    explicit EggSyntaxNode_Call(std::unique_ptr<IEggSyntaxNode>&& callee) {
      this->addChild(std::move(callee));
    }
    virtual void dump(std::ostream& os) const override;
  };

  class EggSyntaxNode_Named : public EggSyntaxNodeChildren1 {
    EGG_NO_COPY(EggSyntaxNode_Named);
  private:
    std::string name;
  public:
    EggSyntaxNode_Named(const std::string& name, std::unique_ptr<IEggSyntaxNode>&& expr)
      : EggSyntaxNodeChildren1(std::move(expr)), name(name) {
    }
    virtual void dump(std::ostream& os) const override;
  };

  class EggSyntaxNode_Identifier : public EggSyntaxNodeChildren0 {
    EGG_NO_COPY(EggSyntaxNode_Identifier);
  private:
    std::string name;
  public:
    explicit EggSyntaxNode_Identifier(const std::string& name)
      : name(name) {
    }
    virtual void dump(std::ostream& os) const override;
  };

  class EggSyntaxNode_Literal : public EggSyntaxNodeChildren0 {
    EGG_NO_COPY(EggSyntaxNode_Literal);
  private:
    EggTokenizerKind kind;
    EggTokenizerValue value;
  public:
    EggSyntaxNode_Literal(EggTokenizerKind kind, const EggTokenizerValue& value)
      : kind(kind), value(value) {
    }
    virtual void dump(std::ostream& os) const override;
  };
}
