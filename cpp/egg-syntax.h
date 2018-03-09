#define EGG_SYNTAX_NODES(macro) \
  macro(Module) \
  macro(Block) \
  macro(Type) \
  macro(VariableDeclaration) \
  macro(VariableInitialization) \
  macro(Assignment) \
  macro(Mutate) \
  macro(Break) \
  macro(Case) \
  macro(Catch) \
  macro(Continue) \
  macro(Default) \
  macro(Do) \
  macro(Finally) \
  macro(For) \
  macro(ForEach) \
  macro(If) \
  macro(Return) \
  macro(Switch) \
  macro(Try) \
  macro(While) \
  macro(Yield) \
  macro(UnaryOperator) \
  macro(BinaryOperator) \
  macro(TernaryOperator) \
  macro(Call) \
  macro(Named) \
  macro(Identifier) \
  macro(Literal)
#define EGG_SYNTAX_NODE_DECLARE(value) value,

namespace egg::yolk {
  class IEggSyntaxNode;

  enum class EggSyntaxNodeKind {
    EGG_SYNTAX_NODES(EGG_SYNTAX_NODE_DECLARE)
  };

  class IEggSyntaxNodeVisitor {
  public:
    virtual void node(const std::string& prefix, IEggSyntaxNode* children[], size_t count) = 0;

    // Helpers
    template<size_t N>
    void node(const std::string& prefix, const std::unique_ptr<IEggSyntaxNode> (&children)[N]) {
      IEggSyntaxNode* pointers[N];
      for (size_t i = 0; i < N; ++i) {
        pointers[i] = children[i].get();
      }
      this->node(prefix, pointers, N);
    }
    void node(const std::string& prefix, const std::vector<std::unique_ptr<IEggSyntaxNode>>& children);
    void node(const std::string& prefix, const std::unique_ptr<IEggSyntaxNode>& child);
    void node(const std::string& prefix);
  };

  class IEggSyntaxNode {
  public:
    virtual EggSyntaxNodeKind getKind() const = 0;
    virtual size_t getChildren() const = 0;
    virtual IEggSyntaxNode* tryGetChild(size_t index) const = 0;
    virtual const EggTokenizerOperator* tryGetOperator() const = 0;
    virtual void visit(IEggSyntaxNodeVisitor& visitor) = 0;

    // Helpers
    IEggSyntaxNode& getChild(size_t index) const;
  };

  template<EggSyntaxNodeKind KIND>
  class EggSyntaxNodeChildren0 : public IEggSyntaxNode {
  public:
    EggSyntaxNodeChildren0() {
    }
    virtual ~EggSyntaxNodeChildren0() {
    }
    virtual EggSyntaxNodeKind getKind() const override {
      return KIND;
    }
    virtual size_t getChildren() const override {
      return 0;
    }
    virtual IEggSyntaxNode* tryGetChild(size_t) const override {
      return nullptr;
    }
    virtual const EggTokenizerOperator* tryGetOperator() const override {
      return nullptr;
    }
  };

  template<EggSyntaxNodeKind KIND, typename TYPE = IEggSyntaxNode>
  class EggSyntaxNodeChildren1 : public IEggSyntaxNode {
    EGG_NO_COPY(EggSyntaxNodeChildren1);
  protected:
    std::unique_ptr<TYPE> child;
  public:
    explicit EggSyntaxNodeChildren1(std::unique_ptr<TYPE>&& child)
      : child(std::move(child)) {
      assert(this->child != nullptr);
    }
    virtual ~EggSyntaxNodeChildren1() {
    }
    virtual EggSyntaxNodeKind getKind() const override {
      return KIND;
    }
    virtual size_t getChildren() const override {
      return 1;
    }
    virtual IEggSyntaxNode* tryGetChild(size_t index) const override {
      return (index == 0) ? this->child.get() : nullptr;
    }
    virtual const EggTokenizerOperator* tryGetOperator() const override {
      return nullptr;
    }
  };

  template<EggSyntaxNodeKind KIND, size_t N, typename TYPE = IEggSyntaxNode>
  class EggSyntaxNodeChildrenN : public IEggSyntaxNode {
    EGG_NO_COPY(EggSyntaxNodeChildrenN);
  protected:
    std::unique_ptr<TYPE> child[N];
    EggSyntaxNodeChildrenN() {
    }
    EggSyntaxNodeChildrenN(std::unique_ptr<TYPE>&& child0, std::unique_ptr<TYPE>&& child1) {
      // It's quite common to have exactly two children, so this construct makes derived classes more concise
      assert(child0 != nullptr);
      assert(child1 != nullptr);
      this->child[0] = std::move(child0);
      this->child[1] = std::move(child1);
    }
  public:
    virtual ~EggSyntaxNodeChildrenN() {
    }
    virtual EggSyntaxNodeKind getKind() const override {
      return KIND;
    }
    virtual size_t getChildren() const override {
      return N;
    }
    virtual IEggSyntaxNode* tryGetChild(size_t index) const override {
      return (index < N) ? this->child[index].get() : nullptr;
    }
    virtual const EggTokenizerOperator* tryGetOperator() const override {
      return nullptr;
    }
  };

  template<EggSyntaxNodeKind KIND, typename TYPE = IEggSyntaxNode>
  class EggSyntaxNodeChildrenV : public IEggSyntaxNode {
    EGG_NO_COPY(EggSyntaxNodeChildrenV);
  protected:
    std::vector<std::unique_ptr<TYPE>> child;
  public:
    EggSyntaxNodeChildrenV() {
    }
    virtual ~EggSyntaxNodeChildrenV() {
    }
    virtual EggSyntaxNodeKind getKind() const override {
      return KIND;
    }
    virtual size_t getChildren() const override {
      return this->child.size();
    }
    virtual IEggSyntaxNode* tryGetChild(size_t index) const override {
      return (index < this->child.size()) ? this->child[index].get() : nullptr;
    }
    virtual const EggTokenizerOperator* tryGetOperator() const override {
      return nullptr;
    }
    void addChild(std::unique_ptr<TYPE>&& node) {
      this->child.emplace_back(std::move(node));
    }
  };

  class EggSyntaxNode_Module : public EggSyntaxNodeChildrenV<EggSyntaxNodeKind::Module> {
    EGG_NO_COPY(EggSyntaxNode_Module);
  public:
    EggSyntaxNode_Module() {
    }
    virtual void visit(IEggSyntaxNodeVisitor& visitor) override;
  };

  class EggSyntaxNode_Block : public EggSyntaxNodeChildrenV<EggSyntaxNodeKind::Block> {
    EGG_NO_COPY(EggSyntaxNode_Block);
  public:
    EggSyntaxNode_Block() {
    }
    virtual void visit(IEggSyntaxNodeVisitor& visitor) override;
  };

  class EggSyntaxNode_Type : public EggSyntaxNodeChildren0<EggSyntaxNodeKind::Type> {
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
    virtual void visit(IEggSyntaxNodeVisitor& visitor) override;
    bool isInferred() const {
      return (this->allowed & ~Void) == 0;
    }
    static std::string to_string(Allowed allowed);
  };

  class EggSyntaxNode_VariableDeclaration : public EggSyntaxNodeChildren1<EggSyntaxNodeKind::VariableDeclaration> {
    EGG_NO_COPY(EggSyntaxNode_VariableDeclaration);
  private:
    std::string name;
  public:
    EggSyntaxNode_VariableDeclaration(const std::string& name, std::unique_ptr<IEggSyntaxNode>&& type)
      : EggSyntaxNodeChildren1(std::move(type)), name(name) {
    }
    virtual void visit(IEggSyntaxNodeVisitor& visitor) override;
  };

  class EggSyntaxNode_VariableInitialization : public EggSyntaxNodeChildrenN<EggSyntaxNodeKind::VariableInitialization, 2> {
    EGG_NO_COPY(EggSyntaxNode_VariableInitialization);
  private:
    std::string name;
  public:
    EggSyntaxNode_VariableInitialization(const std::string& name, std::unique_ptr<IEggSyntaxNode>&& type, std::unique_ptr<IEggSyntaxNode>&& expr)
      : EggSyntaxNodeChildrenN(std::move(type), std::move(expr)), name(name) {
    }
    virtual void visit(IEggSyntaxNodeVisitor& visitor) override;
  };

  class EggSyntaxNode_Assignment : public EggSyntaxNodeChildrenN<EggSyntaxNodeKind::Assignment, 2> {
    EGG_NO_COPY(EggSyntaxNode_Assignment);
  private:
    EggTokenizerOperator op;
  public:
    EggSyntaxNode_Assignment(EggTokenizerOperator op, std::unique_ptr<IEggSyntaxNode>&& lhs, std::unique_ptr<IEggSyntaxNode>&& rhs)
      : EggSyntaxNodeChildrenN(std::move(lhs), std::move(rhs)), op(op) {
    }
    virtual const EggTokenizerOperator* tryGetOperator() const override {
      return &this->op;
    }
    virtual void visit(IEggSyntaxNodeVisitor& visitor) override;
  };

  class EggSyntaxNode_Mutate : public EggSyntaxNodeChildren1<EggSyntaxNodeKind::Mutate> {
    EGG_NO_COPY(EggSyntaxNode_Mutate);
  private:
    EggTokenizerOperator op;
  public:
    EggSyntaxNode_Mutate(EggTokenizerOperator op, std::unique_ptr<IEggSyntaxNode>&& expr)
      : EggSyntaxNodeChildren1(std::move(expr)), op(op) {
    }
    virtual const EggTokenizerOperator* tryGetOperator() const override {
      return &this->op;
    }
    virtual void visit(IEggSyntaxNodeVisitor& visitor) override;
  };

  class EggSyntaxNode_Break : public EggSyntaxNodeChildren0<EggSyntaxNodeKind::Break> {
    EGG_NO_COPY(EggSyntaxNode_Break);
  public:
    EggSyntaxNode_Break() {
    }
    virtual void visit(IEggSyntaxNodeVisitor& visitor) override;
  };

  class EggSyntaxNode_Case : public EggSyntaxNodeChildren1<EggSyntaxNodeKind::Case> {
    EGG_NO_COPY(EggSyntaxNode_Case);
  public:
    explicit EggSyntaxNode_Case(std::unique_ptr<IEggSyntaxNode>&& expr)
      : EggSyntaxNodeChildren1(std::move(expr)) {
    }
    virtual void visit(IEggSyntaxNodeVisitor& visitor) override;
  };

  class EggSyntaxNode_Catch : public EggSyntaxNodeChildrenN<EggSyntaxNodeKind::Catch, 2> {
    EGG_NO_COPY(EggSyntaxNode_Catch);
  private:
    std::string name;
  public:
    EggSyntaxNode_Catch(const std::string& name, std::unique_ptr<IEggSyntaxNode>&& type, std::unique_ptr<IEggSyntaxNode>&& block)
      : EggSyntaxNodeChildrenN(std::move(type), std::move(block)), name(name) {
    }
    virtual void visit(IEggSyntaxNodeVisitor& visitor) override;
  };

  class EggSyntaxNode_Continue : public EggSyntaxNodeChildren0<EggSyntaxNodeKind::Continue> {
    EGG_NO_COPY(EggSyntaxNode_Continue);
  public:
    EggSyntaxNode_Continue() {
    }
    virtual void visit(IEggSyntaxNodeVisitor& visitor) override;
  };

  class EggSyntaxNode_Default : public EggSyntaxNodeChildren0<EggSyntaxNodeKind::Default> {
    EGG_NO_COPY(EggSyntaxNode_Default);
  public:
    EggSyntaxNode_Default() {
    }
    virtual void visit(IEggSyntaxNodeVisitor& visitor) override;
  };

  class EggSyntaxNode_Do : public EggSyntaxNodeChildrenN<EggSyntaxNodeKind::Do, 2> {
    EGG_NO_COPY(EggSyntaxNode_Do);
  public:
    EggSyntaxNode_Do(std::unique_ptr<IEggSyntaxNode>&& cond, std::unique_ptr<IEggSyntaxNode>&& block)
      : EggSyntaxNodeChildrenN(std::move(cond), std::move(block)) {
    }
    virtual void visit(IEggSyntaxNodeVisitor& visitor) override;
  };

  class EggSyntaxNode_If : public EggSyntaxNodeChildrenV<EggSyntaxNodeKind::If> {
    EGG_NO_COPY(EggSyntaxNode_If);
  public:
    EggSyntaxNode_If(std::unique_ptr<IEggSyntaxNode>&& cond, std::unique_ptr<IEggSyntaxNode>&& block) {
      this->addChild(std::move(cond));
      this->addChild(std::move(block));
    }
    virtual void visit(IEggSyntaxNodeVisitor& visitor) override;
  };

  class EggSyntaxNode_Finally : public EggSyntaxNodeChildren1<EggSyntaxNodeKind::Finally> {
    EGG_NO_COPY(EggSyntaxNode_Finally);
  private:
    std::string name;
  public:
    EggSyntaxNode_Finally(std::unique_ptr<IEggSyntaxNode>&& block)
      : EggSyntaxNodeChildren1(std::move(block)) {
    }
    virtual void visit(IEggSyntaxNodeVisitor& visitor) override;
  };

  class EggSyntaxNode_Return : public EggSyntaxNodeChildrenV<EggSyntaxNodeKind::Return> {
    EGG_NO_COPY(EggSyntaxNode_Return);
  public:
    EggSyntaxNode_Return() {
    }
    virtual void visit(IEggSyntaxNodeVisitor& visitor) override;
  };

  class EggSyntaxNode_Switch : public EggSyntaxNodeChildrenN<EggSyntaxNodeKind::Switch, 2> {
    EGG_NO_COPY(EggSyntaxNode_Switch);
  public:
    EggSyntaxNode_Switch(std::unique_ptr<IEggSyntaxNode>&& expr, std::unique_ptr<IEggSyntaxNode>&& block)
      : EggSyntaxNodeChildrenN(std::move(expr), std::move(block)) {
    }
    virtual void visit(IEggSyntaxNodeVisitor& visitor) override;
  };

  class EggSyntaxNode_Try : public EggSyntaxNodeChildrenV<EggSyntaxNodeKind::Try> {
    EGG_NO_COPY(EggSyntaxNode_Try);
  public:
    EggSyntaxNode_Try(std::unique_ptr<IEggSyntaxNode>&& block) {
      this->addChild(std::move(block));
    }
    virtual void visit(IEggSyntaxNodeVisitor& visitor) override;
  };

  class EggSyntaxNode_While : public EggSyntaxNodeChildrenN<EggSyntaxNodeKind::While, 2> {
    EGG_NO_COPY(EggSyntaxNode_While);
  public:
    EggSyntaxNode_While(std::unique_ptr<IEggSyntaxNode>&& cond, std::unique_ptr<IEggSyntaxNode>&& block)
      : EggSyntaxNodeChildrenN(std::move(cond), std::move(block)) {
    }
    virtual void visit(IEggSyntaxNodeVisitor& visitor) override;
  };

  class EggSyntaxNode_Yield : public EggSyntaxNodeChildren1<EggSyntaxNodeKind::Yield> {
    EGG_NO_COPY(EggSyntaxNode_Yield);
  public:
    EggSyntaxNode_Yield(std::unique_ptr<IEggSyntaxNode>&& expr)
      : EggSyntaxNodeChildren1(std::move(expr)) {
    }
    virtual void visit(IEggSyntaxNodeVisitor& visitor) override;
  };

  class EggSyntaxNode_UnaryOperator : public EggSyntaxNodeChildren1<EggSyntaxNodeKind::UnaryOperator> {
    EGG_NO_COPY(EggSyntaxNode_UnaryOperator);
  private:
    EggTokenizerOperator op;
  public:
    EggSyntaxNode_UnaryOperator(EggTokenizerOperator op, std::unique_ptr<IEggSyntaxNode>&& expr)
      : EggSyntaxNodeChildren1(std::move(expr)), op(op) {
    }
    virtual const EggTokenizerOperator* tryGetOperator() const override {
      return &this->op;
    }
    virtual void visit(IEggSyntaxNodeVisitor& visitor) override;
  };

  class EggSyntaxNode_BinaryOperator : public EggSyntaxNodeChildrenN<EggSyntaxNodeKind::BinaryOperator, 2> {
    EGG_NO_COPY(EggSyntaxNode_BinaryOperator);
  private:
    EggTokenizerOperator op;
  public:
    EggSyntaxNode_BinaryOperator(EggTokenizerOperator op, std::unique_ptr<IEggSyntaxNode>&& lhs, std::unique_ptr<IEggSyntaxNode>&& rhs)
      : EggSyntaxNodeChildrenN(std::move(lhs), std::move(rhs)), op(op) {
    }
    virtual const EggTokenizerOperator* tryGetOperator() const override {
      return &this->op;
    }
    virtual void visit(IEggSyntaxNodeVisitor& visitor) override;
  };

  class EggSyntaxNode_TernaryOperator : public EggSyntaxNodeChildrenN<EggSyntaxNodeKind::TernaryOperator, 3> {
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
    virtual void visit(IEggSyntaxNodeVisitor& visitor) override;
  };

  class EggSyntaxNode_Call : public EggSyntaxNodeChildrenV<EggSyntaxNodeKind::Call> {
    EGG_NO_COPY(EggSyntaxNode_Call);
  public:
    explicit EggSyntaxNode_Call(std::unique_ptr<IEggSyntaxNode>&& callee) {
      this->addChild(std::move(callee));
    }
    virtual void visit(IEggSyntaxNodeVisitor& visitor) override;
  };

  class EggSyntaxNode_Named : public EggSyntaxNodeChildren1<EggSyntaxNodeKind::Named> {
    EGG_NO_COPY(EggSyntaxNode_Named);
  private:
    std::string name;
  public:
    EggSyntaxNode_Named(const std::string& name, std::unique_ptr<IEggSyntaxNode>&& expr)
      : EggSyntaxNodeChildren1(std::move(expr)), name(name) {
    }
    virtual void visit(IEggSyntaxNodeVisitor& visitor) override;
  };

  class EggSyntaxNode_Identifier : public EggSyntaxNodeChildren0<EggSyntaxNodeKind::Identifier> {
    EGG_NO_COPY(EggSyntaxNode_Identifier);
  private:
    std::string name;
  public:
    explicit EggSyntaxNode_Identifier(const std::string& name)
      : name(name) {
    }
    virtual void visit(IEggSyntaxNodeVisitor& visitor) override;
  };

  class EggSyntaxNode_Literal : public EggSyntaxNodeChildren0<EggSyntaxNodeKind::Literal> {
    EGG_NO_COPY(EggSyntaxNode_Literal);
  private:
    EggTokenizerKind kind;
    EggTokenizerValue value;
  public:
    EggSyntaxNode_Literal(EggTokenizerKind kind, const EggTokenizerValue& value)
      : kind(kind), value(value) {
    }
    virtual void visit(IEggSyntaxNodeVisitor& visitor) override;
    std::string to_string() const;
  };

  class EggSyntaxNode {
  public:
    static std::ostream& printToStream(std::ostream& os, IEggSyntaxNode& tree, bool concise);
  };
}
