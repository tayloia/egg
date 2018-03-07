#define EGG_SYNTAX_NODES(macro) \
  macro(Module) \
  macro(Type) \
  macro(VariableDeclaration) \
  macro(VariableDefinition) \
  macro(Assignment) \
  macro(UnaryOperator) \
  macro(BinaryOperator) \
  macro(TernaryOperator) \
  macro(Identifier) \
  macro(Literal)
#define EGG_SYNTAX_NODE_DECLARE(value) value,

namespace egg::yolk {
  enum class EggSyntaxNodeKind {
    EGG_SYNTAX_NODES(EGG_SYNTAX_NODE_DECLARE)
  };

  class IEggSyntaxNode {
  public:
    virtual EggSyntaxNodeKind getKind() const = 0;
    virtual size_t getChildren() const = 0;
    virtual IEggSyntaxNode* tryGetChild(size_t index) const = 0;
    virtual const EggTokenizerOperator* tryGetOperator() const = 0;

    // Helpers
    IEggSyntaxNode& getChild(size_t index) const;
  };

  template<EggSyntaxNodeKind KIND>
  class EggSyntaxNodeChildren0 : public IEggSyntaxNode {
    EGG_NO_COPY(EggSyntaxNodeChildren0);
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
  public:
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
  public:
    std::unique_ptr<TYPE> child[N];
  public:
    EggSyntaxNodeChildrenN() {
    }
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

  class EggSyntaxNode_Module : public IEggSyntaxNode {
  private:
    std::vector<std::unique_ptr<IEggSyntaxNode>> children;
  public:
    virtual ~EggSyntaxNode_Module() {
    }
    virtual EggSyntaxNodeKind getKind() const override {
      return EggSyntaxNodeKind::Module;
    }
    virtual size_t getChildren() const override {
      return this->children.size();
    }
    virtual IEggSyntaxNode* tryGetChild(size_t index) const override {
      return this->children[index].get();
    }
    virtual const EggTokenizerOperator* tryGetOperator() const override {
      return nullptr;
    }
    void addChild(std::unique_ptr<IEggSyntaxNode>&& child) {
      this->children.emplace_back(std::move(child));
    }
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
      Other = 0x20
    };
  private:
    Allowed allowed;
  public:
    explicit EggSyntaxNode_Type(Allowed allowed)
      : allowed(allowed) {
    }
    bool isInferred() const {
      return (this->allowed & ~Void) == 0;
    }
  };

  class EggSyntaxNode_VariableDeclaration : public EggSyntaxNodeChildren1<EggSyntaxNodeKind::VariableDeclaration> {
    EGG_NO_COPY(EggSyntaxNode_VariableDeclaration);
  public:
    std::string name;
  public:
    EggSyntaxNode_VariableDeclaration(std::string name, std::unique_ptr<IEggSyntaxNode>&& type)
      : EggSyntaxNodeChildren1(std::move(type)), name(name) {
    }
  };

  class EggSyntaxNode_VariableDefinition : public EggSyntaxNodeChildrenN<EggSyntaxNodeKind::VariableDefinition, 2> {
    EGG_NO_COPY(EggSyntaxNode_VariableDefinition);
  public:
    std::string name;
  public:
    EggSyntaxNode_VariableDefinition(std::string name, std::unique_ptr<IEggSyntaxNode>&& type, std::unique_ptr<IEggSyntaxNode>&& expr)
      : name(name) {
      assert(type != nullptr);
      assert(expr != nullptr);
      this->child[0] = std::move(type);
      this->child[1] = std::move(expr);
    }
  };

  class EggSyntaxNode_Assignment : public EggSyntaxNodeChildrenN<EggSyntaxNodeKind::Assignment, 2> {
    EGG_NO_COPY(EggSyntaxNode_Assignment);
  public:
    EggTokenizerOperator op;
  public:
    EggSyntaxNode_Assignment(EggTokenizerOperator op, std::unique_ptr<IEggSyntaxNode>&& lhs, std::unique_ptr<IEggSyntaxNode>&& rhs)
      : op(op) {
      assert(lhs != nullptr);
      assert(rhs != nullptr);
      this->child[0] = std::move(lhs);
      this->child[1] = std::move(rhs);
    }
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
  };

  class EggSyntaxNode_BinaryOperator : public EggSyntaxNodeChildrenN<EggSyntaxNodeKind::BinaryOperator, 2> {
    EGG_NO_COPY(EggSyntaxNode_BinaryOperator);
  private:
    EggTokenizerOperator op;
  public:
    EggSyntaxNode_BinaryOperator(EggTokenizerOperator op, std::unique_ptr<IEggSyntaxNode>&& lhs, std::unique_ptr<IEggSyntaxNode>&& rhs)
      : op(op) {
      assert(lhs != nullptr);
      assert(rhs != nullptr);
      this->child[0] = std::move(lhs);
      this->child[1] = std::move(rhs);
    }
    virtual const EggTokenizerOperator* tryGetOperator() const override {
      return &this->op;
    }
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
  };

  class EggSyntaxNode_Identifier : public EggSyntaxNodeChildren0<EggSyntaxNodeKind::Identifier> {
    EGG_NO_COPY(EggSyntaxNode_Identifier);
  public:
    std::string name;
  public:
    explicit EggSyntaxNode_Identifier(std::string name)
      : name(name) {
    }
  };

  class EggSyntaxNode_Literal : public EggSyntaxNodeChildren0<EggSyntaxNodeKind::Literal> {
    EGG_NO_COPY(EggSyntaxNode_Literal);
  public:
    EggTokenizerKind kind;
    EggTokenizerValue value;
  public:
    EggSyntaxNode_Literal(EggTokenizerKind kind, const EggTokenizerValue& value)
      : kind(kind), value(value) {
    }
  };
}
