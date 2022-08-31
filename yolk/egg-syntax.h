namespace egg::yolk {
  class IEggProgramNode;
  class IEggParserContext;

  struct EggSyntaxNodeLocation : public ExceptionLocationRange {
    explicit EggSyntaxNodeLocation(const EggTokenizerItem& token) {
      this->setLocationBegin(token);
      this->setLocationEnd(token, token.width());
    }
    EggSyntaxNodeLocation(const EggTokenizerItem& token, size_t width) {
      this->setLocationBegin(token);
      this->setLocationEnd(token, width);
    }
    void setLocationBegin(const EggTokenizerItem& token) {
      this->begin.line = token.line;
      this->begin.column = token.column;
    }
    void setLocationEnd(const EggTokenizerItem& token, size_t width) {
      this->end.line = token.line;
      this->end.column = token.column + width;
    }
  };

  class IEggSyntaxNode {
  public:
    virtual ~IEggSyntaxNode() {}
    virtual EggTokenizerKeyword keyword() const = 0;
    virtual const EggSyntaxNodeLocation& location() const = 0;
    virtual const std::vector<std::unique_ptr<IEggSyntaxNode>>* children() const = 0;
    virtual std::shared_ptr<IEggProgramNode> promote(IEggParserContext& context) const = 0;
    virtual bool negate() = 0;
    virtual void dump(std::ostream& os) const = 0;
  };

  class EggSyntaxNodeBase : public IEggSyntaxNode, public EggSyntaxNodeLocation {
  public:
    explicit EggSyntaxNodeBase(const EggSyntaxNodeLocation& location)
      : EggSyntaxNodeLocation(location) {
    }
    virtual EggTokenizerKeyword keyword() const;
    virtual const EggSyntaxNodeLocation& location() const;
    virtual const std::vector<std::unique_ptr<IEggSyntaxNode>>* children() const;
    virtual bool negate();
    virtual egg::ovum::String token() const;
  };

  class EggSyntaxNodeChildren1 : public EggSyntaxNodeBase {
    EGG_NO_COPY(EggSyntaxNodeChildren1)
  protected:
    std::unique_ptr<IEggSyntaxNode> child;
    EggSyntaxNodeChildren1(const EggSyntaxNodeLocation& location, std::unique_ptr<IEggSyntaxNode>&& child)
      : EggSyntaxNodeBase(location), child(std::move(child)) {
      assert(this->child != nullptr);
    }
  };

  template<size_t N>
  class EggSyntaxNodeChildrenN : public EggSyntaxNodeBase {
    EGG_NO_COPY(EggSyntaxNodeChildrenN)
  protected:
    static const size_t count = N;
    std::unique_ptr<IEggSyntaxNode> child[N];
    explicit EggSyntaxNodeChildrenN(const EggSyntaxNodeLocation& location)
      : EggSyntaxNodeBase(location) {
    }
    EggSyntaxNodeChildrenN(const EggSyntaxNodeLocation& location, std::unique_ptr<IEggSyntaxNode>&& child0, std::unique_ptr<IEggSyntaxNode>&& child1)
      : EggSyntaxNodeBase(location) {
      // It's quite common to have exactly two children, so this construct makes derived classes more concise
      assert(child0 != nullptr);
      assert(child1 != nullptr);
      this->child[0] = std::move(child0);
      this->child[1] = std::move(child1);
    }
  };

  class EggSyntaxNodeChildrenV : public EggSyntaxNodeBase {
    EGG_NO_COPY(EggSyntaxNodeChildrenV)
  protected:
    std::vector<std::unique_ptr<IEggSyntaxNode>> child;
    explicit EggSyntaxNodeChildrenV(const EggSyntaxNodeLocation& location)
      : EggSyntaxNodeBase(location) {
    }
  public:
    void addChild(std::unique_ptr<IEggSyntaxNode>&& node) {
      this->child.emplace_back(std::move(node));
    }
    virtual const std::vector<std::unique_ptr<IEggSyntaxNode>>* children() const override {
      return &this->child;
    }
  };

  class EggSyntaxNode_Module : public EggSyntaxNodeChildrenV {
    EGG_NO_COPY(EggSyntaxNode_Module)
  public:
    explicit EggSyntaxNode_Module(const EggSyntaxNodeLocation& location)
      : EggSyntaxNodeChildrenV(location) {
    }
    virtual void dump(std::ostream& os) const override;
    virtual std::shared_ptr<IEggProgramNode> promote(IEggParserContext& context) const override;
  };

  class EggSyntaxNode_Block : public EggSyntaxNodeChildrenV {
    EGG_NO_COPY(EggSyntaxNode_Block)
  public:
    explicit EggSyntaxNode_Block(const EggSyntaxNodeLocation& location)
      : EggSyntaxNodeChildrenV(location) {
    }
    virtual void dump(std::ostream& os) const override;
    virtual std::shared_ptr<IEggProgramNode> promote(IEggParserContext& context) const override;
  };

  class EggSyntaxNode_Type : public EggSyntaxNodeBase {
    EGG_NO_COPY(EggSyntaxNode_Type)
  private:
    egg::ovum::Type type;
  public:
    EggSyntaxNode_Type(const EggSyntaxNodeLocation& location, const egg::ovum::IType* type)
      : EggSyntaxNodeBase(location), type(type) {
    }
    virtual void dump(std::ostream& os) const override;
    virtual egg::ovum::String token() const override;
    virtual std::shared_ptr<IEggProgramNode> promote(IEggParserContext& context) const override;
  };

  class EggSyntaxNode_Declare : public EggSyntaxNodeChildrenV {
    EGG_NO_COPY(EggSyntaxNode_Declare)
  private:
    egg::ovum::String name;
  public:
    EggSyntaxNode_Declare(const EggSyntaxNodeLocation& location, const egg::ovum::String& name, std::unique_ptr<IEggSyntaxNode>&& type)
      : EggSyntaxNodeChildrenV(location), name(name) {
      assert(type != nullptr);
      this->addChild(std::move(type));
    }
    EggSyntaxNode_Declare(const EggSyntaxNodeLocation& location, const egg::ovum::String& name, std::unique_ptr<IEggSyntaxNode>&& type, std::unique_ptr<IEggSyntaxNode>&& expr)
      : EggSyntaxNodeChildrenV(location), name(name) {
      assert(type != nullptr);
      assert(expr != nullptr);
      this->addChild(std::move(type));
      this->addChild(std::move(expr));
    }
    virtual void dump(std::ostream& os) const override;
    virtual egg::ovum::String token() const override;
    virtual std::shared_ptr<IEggProgramNode> promote(IEggParserContext& context) const override;
  };

  class EggSyntaxNode_Guard : public EggSyntaxNodeChildrenN<2> {
    EGG_NO_COPY(EggSyntaxNode_Guard)
  private:
    egg::ovum::String name;
  public:
    EggSyntaxNode_Guard(const EggSyntaxNodeLocation& location, const egg::ovum::String& name, std::unique_ptr<IEggSyntaxNode>&& type, std::unique_ptr<IEggSyntaxNode>&& expr)
      : EggSyntaxNodeChildrenN(location, std::move(type), std::move(expr)), name(name) {
    }
    virtual void dump(std::ostream& os) const override;
    virtual egg::ovum::String token() const override;
    virtual std::shared_ptr<IEggProgramNode> promote(IEggParserContext& context) const override;
  };

  class EggSyntaxNode_Assignment : public EggSyntaxNodeChildrenN<2> {
    EGG_NO_COPY(EggSyntaxNode_Assignment)
  private:
    EggTokenizerOperator op;
  public:
    EggSyntaxNode_Assignment(const EggSyntaxNodeLocation& location, EggTokenizerOperator op, std::unique_ptr<IEggSyntaxNode>&& lhs, std::unique_ptr<IEggSyntaxNode>&& rhs)
      : EggSyntaxNodeChildrenN(location, std::move(lhs), std::move(rhs)), op(op) {
    }
    virtual void dump(std::ostream& os) const override;
    virtual egg::ovum::String token() const override;
    virtual std::shared_ptr<IEggProgramNode> promote(IEggParserContext& context) const override;
  };

  class EggSyntaxNode_Mutate : public EggSyntaxNodeChildren1 {
    EGG_NO_COPY(EggSyntaxNode_Mutate)
  private:
    EggTokenizerOperator op;
  public:
    EggSyntaxNode_Mutate(const EggSyntaxNodeLocation& location, EggTokenizerOperator op, std::unique_ptr<IEggSyntaxNode>&& expr)
      : EggSyntaxNodeChildren1(location, std::move(expr)), op(op) {
    }
    virtual void dump(std::ostream& os) const override;
    virtual egg::ovum::String token() const override;
    virtual std::shared_ptr<IEggProgramNode> promote(IEggParserContext& context) const override;
  };

  class EggSyntaxNode_Break : public EggSyntaxNodeBase {
    EGG_NO_COPY(EggSyntaxNode_Break)
  public:
    explicit EggSyntaxNode_Break(const EggSyntaxNodeLocation& location)
      : EggSyntaxNodeBase(location) {
    }
    virtual EggTokenizerKeyword keyword() const override;
    virtual void dump(std::ostream& os) const override;
    virtual std::shared_ptr<IEggProgramNode> promote(IEggParserContext& context) const override;
  };

  class EggSyntaxNode_Case : public EggSyntaxNodeChildren1 {
    EGG_NO_COPY(EggSyntaxNode_Case)
  public:
    EggSyntaxNode_Case(const EggSyntaxNodeLocation& location, std::unique_ptr<IEggSyntaxNode>&& expr)
      : EggSyntaxNodeChildren1(location, std::move(expr)) {
    }
    virtual EggTokenizerKeyword keyword() const override;
    virtual void dump(std::ostream& os) const override;
    virtual std::shared_ptr<IEggProgramNode> promote(IEggParserContext& context) const override;
  };

  class EggSyntaxNode_Catch : public EggSyntaxNodeChildrenN<2> {
    EGG_NO_COPY(EggSyntaxNode_Catch)
  private:
    egg::ovum::String name;
  public:
    EggSyntaxNode_Catch(const EggSyntaxNodeLocation& location, const egg::ovum::String& name, std::unique_ptr<IEggSyntaxNode>&& type, std::unique_ptr<IEggSyntaxNode>&& block)
      : EggSyntaxNodeChildrenN(location, std::move(type), std::move(block)), name(name) {
    }
    virtual EggTokenizerKeyword keyword() const override;
    virtual void dump(std::ostream& os) const override;
    virtual egg::ovum::String token() const override;
    virtual std::shared_ptr<IEggProgramNode> promote(IEggParserContext& context) const override;
  };

  class EggSyntaxNode_Continue : public EggSyntaxNodeBase {
    EGG_NO_COPY(EggSyntaxNode_Continue)
  public:
    explicit EggSyntaxNode_Continue(const EggSyntaxNodeLocation& location)
      : EggSyntaxNodeBase(location) {
    }
    virtual EggTokenizerKeyword keyword() const override;
    virtual void dump(std::ostream& os) const override;
    virtual std::shared_ptr<IEggProgramNode> promote(IEggParserContext& context) const override;
  };

  class EggSyntaxNode_Default : public EggSyntaxNodeBase {
    EGG_NO_COPY(EggSyntaxNode_Default)
  public:
    explicit EggSyntaxNode_Default(const EggSyntaxNodeLocation& location)
      : EggSyntaxNodeBase(location) {
    }
    virtual EggTokenizerKeyword keyword() const override;
    virtual void dump(std::ostream& os) const override;
    virtual std::shared_ptr<IEggProgramNode> promote(IEggParserContext& context) const override;
  };

  class EggSyntaxNode_Do : public EggSyntaxNodeChildrenN<2> {
    EGG_NO_COPY(EggSyntaxNode_Do)
  public:
    EggSyntaxNode_Do(const EggSyntaxNodeLocation& location, std::unique_ptr<IEggSyntaxNode>&& cond, std::unique_ptr<IEggSyntaxNode>&& block)
      : EggSyntaxNodeChildrenN(location, std::move(cond), std::move(block)) {
    }
    virtual EggTokenizerKeyword keyword() const override;
    virtual void dump(std::ostream& os) const override;
    virtual std::shared_ptr<IEggProgramNode> promote(IEggParserContext& context) const override;
  };

  class EggSyntaxNode_If : public EggSyntaxNodeChildrenV {
    EGG_NO_COPY(EggSyntaxNode_If)
  public:
    EggSyntaxNode_If(const EggSyntaxNodeLocation& location, std::unique_ptr<IEggSyntaxNode>&& cond, std::unique_ptr<IEggSyntaxNode>&& block)
      : EggSyntaxNodeChildrenV(location) {
      this->addChild(std::move(cond));
      this->addChild(std::move(block));
    }
    virtual EggTokenizerKeyword keyword() const override;
    virtual void dump(std::ostream& os) const override;
    virtual std::shared_ptr<IEggProgramNode> promote(IEggParserContext& context) const override;
  };

  class EggSyntaxNode_Finally : public EggSyntaxNodeChildren1 {
    EGG_NO_COPY(EggSyntaxNode_Finally)
  public:
    EggSyntaxNode_Finally(const EggSyntaxNodeLocation& location, std::unique_ptr<IEggSyntaxNode>&& block)
      : EggSyntaxNodeChildren1(location, std::move(block)) {
    }
    virtual EggTokenizerKeyword keyword() const override;
    virtual void dump(std::ostream& os) const override;
    virtual std::shared_ptr<IEggProgramNode> promote(IEggParserContext& context) const override;
  };

  class EggSyntaxNode_For : public EggSyntaxNodeChildrenV {
    EGG_NO_COPY(EggSyntaxNode_For)
  public:
    EggSyntaxNode_For(const EggSyntaxNodeLocation& location, std::unique_ptr<IEggSyntaxNode>&& pre, std::unique_ptr<IEggSyntaxNode>&& cond, std::unique_ptr<IEggSyntaxNode>&& post, std::unique_ptr<IEggSyntaxNode>&& block)
      : EggSyntaxNodeChildrenV(location) {
      this->addChild(std::move(pre)); // may be null
      this->addChild(std::move(cond)); // may be null
      this->addChild(std::move(post)); // may be null
      assert(block != nullptr);
      this->addChild(std::move(block));
    }
    virtual EggTokenizerKeyword keyword() const override;
    virtual void dump(std::ostream& os) const override;
    virtual std::shared_ptr<IEggProgramNode> promote(IEggParserContext& context) const override;
  };

  class EggSyntaxNode_Foreach : public EggSyntaxNodeChildrenN<3> {
    EGG_NO_COPY(EggSyntaxNode_Foreach)
  public:
    EggSyntaxNode_Foreach(const EggSyntaxNodeLocation& location, std::unique_ptr<IEggSyntaxNode>&& target, std::unique_ptr<IEggSyntaxNode>&& expr, std::unique_ptr<IEggSyntaxNode>&& block)
      : EggSyntaxNodeChildrenN(location, std::move(target), std::move(expr)) {
      assert(block != nullptr);
      this->child[2] = std::move(block);
    }
    virtual EggTokenizerKeyword keyword() const override;
    virtual void dump(std::ostream& os) const override;
    virtual std::shared_ptr<IEggProgramNode> promote(IEggParserContext& context) const override;
  };

  class EggSyntaxNode_FunctionDefinition : public EggSyntaxNodeChildrenV {
    EGG_NO_COPY(EggSyntaxNode_FunctionDefinition)
  private:
    egg::ovum::String name;
    bool generator;
  public:
    EggSyntaxNode_FunctionDefinition(const EggSyntaxNodeLocation& location, const egg::ovum::String& name, std::unique_ptr<IEggSyntaxNode>&& type, bool generator)
      : EggSyntaxNodeChildrenV(location), name(name), generator(generator) {
      this->addChild(std::move(type));
    }
    virtual void dump(std::ostream& os) const override;
    virtual egg::ovum::String token() const override;
    virtual std::shared_ptr<IEggProgramNode> promote(IEggParserContext& context) const override;
  };

  class EggSyntaxNode_Parameter : public EggSyntaxNodeChildren1 {
    EGG_NO_COPY(EggSyntaxNode_Parameter)
  private:
    egg::ovum::String name;
    bool optional;
  public:
    EggSyntaxNode_Parameter(const EggSyntaxNodeLocation& location, const egg::ovum::String& name, std::unique_ptr<IEggSyntaxNode>&& type, bool optional)
      : EggSyntaxNodeChildren1(location, std::move(type)), name(name), optional(optional) {
    }
    virtual void dump(std::ostream& os) const override;
    virtual egg::ovum::String token() const override;
    virtual std::shared_ptr<IEggProgramNode> promote(IEggParserContext& context) const override;
  };

  class EggSyntaxNode_Return : public EggSyntaxNodeChildrenV {
    EGG_NO_COPY(EggSyntaxNode_Return)
  public:
    explicit EggSyntaxNode_Return(const EggSyntaxNodeLocation& location)
      : EggSyntaxNodeChildrenV(location) {
    }
    virtual EggTokenizerKeyword keyword() const override;
    virtual void dump(std::ostream& os) const override;
    virtual std::shared_ptr<IEggProgramNode> promote(IEggParserContext& context) const override;
  };

  class EggSyntaxNode_Switch : public EggSyntaxNodeChildrenN<2> {
    EGG_NO_COPY(EggSyntaxNode_Switch)
  public:
    EggSyntaxNode_Switch(const EggSyntaxNodeLocation& location, std::unique_ptr<IEggSyntaxNode>&& expr, std::unique_ptr<IEggSyntaxNode>&& block)
      : EggSyntaxNodeChildrenN(location, std::move(expr), std::move(block)) {
    }
    virtual EggTokenizerKeyword keyword() const override;
    virtual void dump(std::ostream& os) const override;
    virtual std::shared_ptr<IEggProgramNode> promote(IEggParserContext& context) const override;
  };

  class EggSyntaxNode_Throw : public EggSyntaxNodeChildrenV {
    EGG_NO_COPY(EggSyntaxNode_Throw)
  public:
    explicit EggSyntaxNode_Throw(const EggSyntaxNodeLocation& location)
      : EggSyntaxNodeChildrenV(location) {
    }
    virtual EggTokenizerKeyword keyword() const override;
    virtual void dump(std::ostream& os) const override;
    virtual std::shared_ptr<IEggProgramNode> promote(IEggParserContext& context) const override;
  };

  class EggSyntaxNode_Try : public EggSyntaxNodeChildrenV {
    EGG_NO_COPY(EggSyntaxNode_Try)
  public:
    EggSyntaxNode_Try(const EggSyntaxNodeLocation& location, std::unique_ptr<IEggSyntaxNode>&& block)
      : EggSyntaxNodeChildrenV(location) {
      this->addChild(std::move(block));
    }
    virtual EggTokenizerKeyword keyword() const override;
    virtual void dump(std::ostream& os) const override;
    virtual std::shared_ptr<IEggProgramNode> promote(IEggParserContext& context) const override;
  };

  class EggSyntaxNode_Typedef : public EggSyntaxNodeChildrenV {
    EGG_NO_COPY(EggSyntaxNode_Typedef)
  private:
    egg::ovum::String name;
  public:
    EggSyntaxNode_Typedef(const EggSyntaxNodeLocation& location, const egg::ovum::String& name)
      : EggSyntaxNodeChildrenV(location), name(name) {
    }
    virtual EggTokenizerKeyword keyword() const override;
    virtual void dump(std::ostream& os) const override;
    virtual std::shared_ptr<IEggProgramNode> promote(IEggParserContext& context) const override;
  };

  class EggSyntaxNode_TypeConstraint : public EggSyntaxNodeBase {
    EGG_NO_COPY(EggSyntaxNode_TypeConstraint)
  private:
    egg::ovum::Type type;
  public:
    EggSyntaxNode_TypeConstraint(const EggSyntaxNodeLocation& location, const egg::ovum::IType* type)
      : EggSyntaxNodeBase(location), type(type) {
    }
    virtual void dump(std::ostream& os) const override;
    virtual egg::ovum::String token() const override;
    virtual std::shared_ptr<IEggProgramNode> promote(IEggParserContext& context) const override;
  };

  class EggSyntaxNode_While : public EggSyntaxNodeChildrenN<2> {
    EGG_NO_COPY(EggSyntaxNode_While)
  public:
    EggSyntaxNode_While(const EggSyntaxNodeLocation& location, std::unique_ptr<IEggSyntaxNode>&& cond, std::unique_ptr<IEggSyntaxNode>&& block)
      : EggSyntaxNodeChildrenN(location, std::move(cond), std::move(block)) {
    }
    virtual EggTokenizerKeyword keyword() const override;
    virtual void dump(std::ostream& os) const override;
    virtual std::shared_ptr<IEggProgramNode> promote(IEggParserContext& context) const override;
  };

  class EggSyntaxNode_Yield : public EggSyntaxNodeChildren1 {
    EGG_NO_COPY(EggSyntaxNode_Yield)
  public:
    EggSyntaxNode_Yield(const EggSyntaxNodeLocation& location, std::unique_ptr<IEggSyntaxNode>&& expr)
      : EggSyntaxNodeChildren1(location, std::move(expr)) {
    }
    virtual EggTokenizerKeyword keyword() const override;
    virtual void dump(std::ostream& os) const override;
    virtual std::shared_ptr<IEggProgramNode> promote(IEggParserContext& context) const override;
  };

  class EggSyntaxNode_Dot : public EggSyntaxNodeChildren1 {
    EGG_NO_COPY(EggSyntaxNode_Dot)
  private:
    egg::ovum::String property;
    bool query; // TODO
  public:
    EggSyntaxNode_Dot(const EggSyntaxNodeLocation& location, std::unique_ptr<IEggSyntaxNode>&& instance, const egg::ovum::String& property, bool query)
      : EggSyntaxNodeChildren1(location, std::move(instance)), property(property), query(query) {
    }
    virtual void dump(std::ostream& os) const override;
    virtual egg::ovum::String token() const override;
    virtual std::shared_ptr<IEggProgramNode> promote(IEggParserContext& context) const override;
  };

  class EggSyntaxNode_UnaryOperator : public EggSyntaxNodeChildren1 {
    EGG_NO_COPY(EggSyntaxNode_UnaryOperator)
  private:
    EggTokenizerOperator op;
  public:
    EggSyntaxNode_UnaryOperator(const EggSyntaxNodeLocation& location, EggTokenizerOperator op, std::unique_ptr<IEggSyntaxNode>&& expr)
      : EggSyntaxNodeChildren1(location, std::move(expr)), op(op) {
    }
    virtual void dump(std::ostream& os) const override;
    virtual egg::ovum::String token() const override;
    virtual std::shared_ptr<IEggProgramNode> promote(IEggParserContext& context) const override;
  };

  class EggSyntaxNode_BinaryOperator : public EggSyntaxNodeChildrenN<2> {
    EGG_NO_COPY(EggSyntaxNode_BinaryOperator)
  private:
    EggTokenizerOperator op;
  public:
    EggSyntaxNode_BinaryOperator(const EggSyntaxNodeLocation& location, EggTokenizerOperator op, std::unique_ptr<IEggSyntaxNode>&& lhs, std::unique_ptr<IEggSyntaxNode>&& rhs)
      : EggSyntaxNodeChildrenN(location, std::move(lhs), std::move(rhs)), op(op) {
    }
    virtual void dump(std::ostream& os) const override;
    virtual egg::ovum::String token() const override;
    virtual std::shared_ptr<IEggProgramNode> promote(IEggParserContext& context) const override;
  };

  class EggSyntaxNode_TernaryOperator : public EggSyntaxNodeChildrenN<3> {
    EGG_NO_COPY(EggSyntaxNode_TernaryOperator)
  public:
    EggSyntaxNode_TernaryOperator(const EggSyntaxNodeLocation& location, std::unique_ptr<IEggSyntaxNode>&& condition, std::unique_ptr<IEggSyntaxNode>&& whenTrue, std::unique_ptr<IEggSyntaxNode>&& whenFalse)
      : EggSyntaxNodeChildrenN(location) {
      assert(condition != nullptr);
      assert(whenTrue != nullptr);
      assert(whenFalse != nullptr);
      this->child[0] = std::move(condition);
      this->child[1] = std::move(whenTrue);
      this->child[2] = std::move(whenFalse);
    }
    virtual void dump(std::ostream& os) const override;
    virtual egg::ovum::String token() const override;
    virtual std::shared_ptr<IEggProgramNode> promote(IEggParserContext& context) const override;
  };

  class EggSyntaxNode_Array : public EggSyntaxNodeChildrenV {
    EGG_NO_COPY(EggSyntaxNode_Array)
  public:
    explicit EggSyntaxNode_Array(const EggSyntaxNodeLocation& location)
      : EggSyntaxNodeChildrenV(location) {
    }
    virtual void dump(std::ostream& os) const override;
    virtual std::shared_ptr<IEggProgramNode> promote(IEggParserContext& context) const override;
  };

  class EggSyntaxNode_Object : public EggSyntaxNodeChildrenV {
    EGG_NO_COPY(EggSyntaxNode_Object)
  public:
    explicit EggSyntaxNode_Object(const EggSyntaxNodeLocation& location)
      : EggSyntaxNodeChildrenV(location) {
    }
    virtual void dump(std::ostream& os) const override;
    virtual std::shared_ptr<IEggProgramNode> promote(IEggParserContext& context) const override;
  };

  class EggSyntaxNode_Call : public EggSyntaxNodeChildrenV {
    EGG_NO_COPY(EggSyntaxNode_Call)
  public:
    EggSyntaxNode_Call(const EggSyntaxNodeLocation& location, std::unique_ptr<IEggSyntaxNode>&& callee)
      : EggSyntaxNodeChildrenV(location) {
      this->addChild(std::move(callee));
    }
    virtual void dump(std::ostream& os) const override;
    virtual std::shared_ptr<IEggProgramNode> promote(IEggParserContext& context) const override;
  };

  class EggSyntaxNode_Named : public EggSyntaxNodeChildren1 {
    EGG_NO_COPY(EggSyntaxNode_Named)
  private:
    egg::ovum::String name;
  public:
    EggSyntaxNode_Named(const EggSyntaxNodeLocation& location, const egg::ovum::String& name, std::unique_ptr<IEggSyntaxNode>&& expr)
      : EggSyntaxNodeChildren1(location, std::move(expr)), name(name) {
    }
    virtual void dump(std::ostream& os) const override;
    virtual egg::ovum::String token() const override;
    virtual std::shared_ptr<IEggProgramNode> promote(IEggParserContext& context) const override;
  };

  class EggSyntaxNode_Identifier : public EggSyntaxNodeBase {
    EGG_NO_COPY(EggSyntaxNode_Identifier)
  private:
    egg::ovum::String name;
  public:
    EggSyntaxNode_Identifier(const EggSyntaxNodeLocation& location, const egg::ovum::String& name)
      : EggSyntaxNodeBase(location), name(name) {
    }
    virtual void dump(std::ostream& os) const override;
    virtual egg::ovum::String token() const override;
    virtual std::shared_ptr<IEggProgramNode> promote(IEggParserContext& context) const override;
  };

  class EggSyntaxNode_Literal : public EggSyntaxNodeBase {
    EGG_NO_COPY(EggSyntaxNode_Literal)
  private:
    EggTokenizerKind kind;
    EggTokenizerValue value;
  public:
    EggSyntaxNode_Literal(const EggSyntaxNodeLocation& location, EggTokenizerKind kind, const EggTokenizerValue& value)
      : EggSyntaxNodeBase(location), kind(kind), value(value) {
    }
    virtual void dump(std::ostream& os) const override;
    virtual egg::ovum::String token() const override;
    virtual std::shared_ptr<IEggProgramNode> promote(IEggParserContext& context) const override;
    virtual bool negate() override;
  };
}
