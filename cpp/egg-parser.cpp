#include "yolk.h"
#include "lexers.h"
#include "egg-tokenizer.h"
#include "egg-syntax.h"
#include "egg-parser.h"

namespace {
  using namespace egg::yolk;

  // This is the integer representation of the enum bit-mask
  typedef std::underlying_type<EggParserAllowed>::type EggParserAllowedUnderlying;
  inline EggParserAllowed operator|(EggParserAllowed lhs, EggParserAllowed rhs) {
    return static_cast<EggParserAllowed>(static_cast<EggParserAllowedUnderlying>(lhs) | static_cast<EggParserAllowedUnderlying>(rhs));
  }

  // Constants
  const EggParserType typeVoid{ egg::lang::TypeStorage::Void };
  const EggParserType typeBool{ egg::lang::TypeStorage::Bool };
  const EggParserType typeInt{ egg::lang::TypeStorage::Int };
  const EggParserType typeFloat{ egg::lang::TypeStorage::Float };
  const EggParserType typeString{ egg::lang::TypeStorage::String };

  SyntaxException exceptionFromLocation(const IEggParserContext& context, const std::string& reason, const EggSyntaxNodeLocation& location) {
    return SyntaxException(reason, context.getResource(), location);
  }

  SyntaxException exceptionFromToken(const IEggParserContext& context, const std::string& reason, const EggSyntaxNodeBase& node) {
    auto token = node.token();
    return SyntaxException(reason + ": '" + token, context.getResource(), node, token);
  }

  void tagToStringComponent(std::string& dst, const char* text, bool bit) {
    if (bit) {
      if (!dst.empty()) {
        dst.append("|");
      }
      dst.append(text);
    }
  }

  class ParserDump {
  private:
    std::ostream* os;
  public:
    ParserDump(std::ostream& os, const char* text)
      :os(&os) {
      *this->os << '(' << text;
    }
    ~ParserDump() {
      *this->os << ')';
    }
    ParserDump& add(const std::string& text) {
      *this->os << ' ' << '\'' << text << '\'';
      return *this;
    }
    ParserDump& add(int64_t number) {
      *this->os << ' ' << number;
      return *this;
    }
    ParserDump& add(const std::shared_ptr<IEggParserNode>& child) {
      if (child != nullptr) {
        *this->os << ' ';
        child->dump(*this->os);
      } else {
        *this->os << ' ' << '-';
      }
      return *this;
    }
    template<typename T>
    ParserDump& add(const std::vector<std::shared_ptr<T>>& children) {
      for (auto& child : children) {
        this->add(child);
      }
      return *this;
    }
  };

  class EggParserNodeBase : public IEggParserNode {
  public:
    virtual ~EggParserNodeBase() {
    }
    virtual const EggParserType& getType() const override {
      return typeVoid;
    }
    static std::string unaryToString(EggParserUnary op);
    static std::string binaryToString(EggParserBinary op);
    static std::string assignToString(EggParserAssign op);
    static std::string mutateToString(EggParserMutate op);
  };

  class EggParserNode_Module : public EggParserNodeBase {
  private:
    std::vector<std::shared_ptr<IEggParserNode>> child;
  public:
    virtual void dump(std::ostream& os) const override {
      ParserDump(os, "module").add(this->child);
    }
    void addChild(const std::shared_ptr<IEggParserNode>& statement) {
      assert(statement != nullptr);
      this->child.push_back(statement);
    }
  };

  class EggParserNode_Block : public EggParserNodeBase {
  private:
    std::vector<std::shared_ptr<IEggParserNode>> child;
  public:
    virtual void dump(std::ostream& os) const override {
      ParserDump(os, "block").add(this->child);
    }
    void addChild(const std::shared_ptr<IEggParserNode>& statement) {
      assert(statement != nullptr);
      this->child.push_back(statement);
    }
  };

  class EggParserNode_Type : public EggParserNodeBase {
  private:
    EggParserType type;
  public:
    explicit EggParserNode_Type(EggParserType type)
      : type(type) {
    }
    virtual const EggParserType& getType() const override {
      return this->type;
    }
    virtual void dump(std::ostream& os) const override {
      ParserDump(os, "type").add(this->type.tagToString());
    }
  };

  class EggParserNode_Declare : public EggParserNodeBase {
  private:
    std::string name;
    std::shared_ptr<IEggParserNode> type;
    std::shared_ptr<IEggParserNode> init;
  public:
    EggParserNode_Declare(const std::string& name, const std::shared_ptr<IEggParserNode>& type, const std::shared_ptr<IEggParserNode>& init = nullptr)
      : name(name), type(type), init(init) {
      assert(type != nullptr);
    }
    virtual void dump(std::ostream& os) const override {
      ParserDump(os, "declare").add(this->name).add(this->type).add(this->init);
    }
  };

  class EggParserNode_Mutate : public EggParserNodeBase {
  private:
    EggParserMutate op;
    std::shared_ptr<IEggParserNode> lvalue;
  public:
    EggParserNode_Mutate(EggParserMutate op, const std::shared_ptr<IEggParserNode>& lvalue)
      : op(op), lvalue(lvalue) {
      assert(lvalue != nullptr);
    }
    virtual void dump(std::ostream& os) const override {
      ParserDump(os, "mutate").add(EggParserNodeBase::mutateToString(this->op)).add(this->lvalue);
    }
  };

  class EggParserNode_Break : public EggParserNodeBase {
  public:
    virtual void dump(std::ostream& os) const override {
      ParserDump(os, "break");
    }
  };

  class EggParserNode_Catch : public EggParserNodeBase {
  private:
    std::string name;
    std::shared_ptr<IEggParserNode> type;
    std::shared_ptr<IEggParserNode> block;
  public:
    EggParserNode_Catch(const std::string& name, const std::shared_ptr<IEggParserNode>& type, const std::shared_ptr<IEggParserNode>& block)
      : name(name), type(type), block(block) {
      assert(type != nullptr);
      assert(block != nullptr);
    }
    virtual void dump(std::ostream& os) const override {
      ParserDump(os, "catch").add(this->name).add(this->type).add(this->block);
    }
  };

  class EggParserNode_Continue : public EggParserNodeBase {
  public:
    virtual void dump(std::ostream& os) const override {
      ParserDump(os, "continue");
    }
  };

  class EggParserNode_Do : public EggParserNodeBase {
  private:
    std::shared_ptr<IEggParserNode> condition;
    std::shared_ptr<IEggParserNode> block;
  public:
    EggParserNode_Do(const std::shared_ptr<IEggParserNode>& condition, const std::shared_ptr<IEggParserNode>& block)
      : condition(condition), block(block) {
      assert(condition != nullptr);
      assert(block != nullptr);
    }
    virtual void dump(std::ostream& os) const override {
      ParserDump(os, "do").add(this->condition).add(this->block);
    }
  };

  class EggParserNode_If : public EggParserNodeBase {
  private:
    std::shared_ptr<IEggParserNode> condition;
    std::shared_ptr<IEggParserNode> trueBlock;
    std::shared_ptr<IEggParserNode> falseBlock;
  public:
    EggParserNode_If(const std::shared_ptr<IEggParserNode>& condition, const std::shared_ptr<IEggParserNode>& trueBlock, const std::shared_ptr<IEggParserNode>& falseBlock)
      : condition(condition), trueBlock(trueBlock), falseBlock(falseBlock) {
      assert(condition != nullptr);
      assert(trueBlock != nullptr);
      // falseBlock may be null if the 'else' clause is missing
    }
    virtual void dump(std::ostream& os) const override {
      ParserDump(os, "if").add(this->condition).add(this->trueBlock).add(this->falseBlock);
    }
  };

  class EggParserNode_For : public EggParserNodeBase {
  private:
    std::shared_ptr<IEggParserNode> pre;
    std::shared_ptr<IEggParserNode> cond;
    std::shared_ptr<IEggParserNode> post;
    std::shared_ptr<IEggParserNode> block;
  public:
    EggParserNode_For(const std::shared_ptr<IEggParserNode>& pre, const std::shared_ptr<IEggParserNode>& cond, const std::shared_ptr<IEggParserNode>& post, const std::shared_ptr<IEggParserNode>& block)
      : pre(pre), cond(cond), post(post), block(block) {
      // pre/cond/post may be null
      assert(block != nullptr);
    }
    virtual void dump(std::ostream& os) const override {
      ParserDump(os, "for").add(this->pre).add(this->cond).add(this->post).add(this->block);
    }
  };

  class EggParserNode_Foreach : public EggParserNodeBase {
  private:
    std::shared_ptr<IEggParserNode> target;
    std::shared_ptr<IEggParserNode> expr;
    std::shared_ptr<IEggParserNode> block;
  public:
    EggParserNode_Foreach(const std::shared_ptr<IEggParserNode>& target, const std::shared_ptr<IEggParserNode>& expr, const std::shared_ptr<IEggParserNode>& block)
      : target(target), expr(expr), block(block) {
      assert(target != nullptr);
      assert(expr != nullptr);
      assert(block != nullptr);
    }
    virtual void dump(std::ostream& os) const override {
      ParserDump(os, "foreach").add(this->target).add(this->expr).add(this->block);
    }
  };

  class EggParserNode_Return : public EggParserNodeBase {
  private:
    std::vector<std::shared_ptr<IEggParserNode>> child;
  public:
    virtual void dump(std::ostream& os) const override {
      ParserDump(os, "return").add(this->child);
    }
    void addChild(const std::shared_ptr<IEggParserNode>& value) {
      assert(value != nullptr);
      this->child.push_back(value);
    }
  };

  class EggParserNode_Case : public EggParserNodeBase {
  private:
    std::vector<std::shared_ptr<IEggParserNode>> child;
    std::shared_ptr<EggParserNode_Block> block;
  public:
    EggParserNode_Case()
      : block(std::make_shared<EggParserNode_Block>()) {
      assert(this->block != nullptr);
    }
    virtual void dump(std::ostream& os) const override {
      ParserDump(os, "case").add(this->child).add(this->block);
    }
    void addValue(const std::shared_ptr<IEggParserNode>& value) {
      assert(value != nullptr);
      this->child.push_back(value);
    }
    void addStatement(const std::shared_ptr<IEggParserNode>& statement) {
      assert(statement != nullptr);
      this->block->addChild(statement);
    }
  };

  class EggParserNode_Switch : public EggParserNodeBase {
  private:
    std::shared_ptr<IEggParserNode> expr;
    int64_t defaultIndex;
    std::vector<std::shared_ptr<EggParserNode_Case>> child;
  public:
    explicit EggParserNode_Switch(const std::shared_ptr<IEggParserNode>& expr)
      : expr(expr), defaultIndex(-1) {
      assert(expr != nullptr);
    }
    virtual void dump(std::ostream& os) const override {
      ParserDump(os, "switch").add(this->expr).add(this->defaultIndex).add(this->child);
    }
    void newClause() {
      this->child.emplace_back(std::make_shared<EggParserNode_Case>());
    }
    bool addCase(const std::shared_ptr<IEggParserNode>& value) {
      // TODO: Check for duplicate case values
      if (this->child.empty()) {
        return false;
      }
      this->child.back()->addValue(value);
      return true;
    }
    bool addDefault() {
      // Make sure we don't set this default twice
      if (!this->child.empty() && (this->defaultIndex < 0)) {
        this->defaultIndex = static_cast<int64_t>(this->child.size()) - 1;
        return true;
      }
      return false;
    }
    bool addStatement(const std::shared_ptr<IEggParserNode>& statement) {
      if (this->child.empty()) {
        return false;
      }
      this->child.back()->addStatement(statement);
      return true;
    }
  };

  class EggParserNode_Throw : public EggParserNodeBase {
  private:
    std::shared_ptr<IEggParserNode> expr;
  public:
    explicit EggParserNode_Throw(const std::shared_ptr<IEggParserNode>& expr = nullptr)
      : expr(expr) {
    }
    virtual void dump(std::ostream& os) const override {
      ParserDump(os, "throw").add(this->expr);
    }
  };

  class EggParserNode_Try : public EggParserNodeBase {
  private:
    std::shared_ptr<IEggParserNode> block;
    std::vector<std::shared_ptr<IEggParserNode>> catches;
    std::shared_ptr<IEggParserNode> final;
  public:
    explicit EggParserNode_Try(const std::shared_ptr<IEggParserNode>& block)
      : block(block) {
      assert(block != nullptr);
    }
    virtual void dump(std::ostream& os) const override {
      ParserDump(os, "try").add(this->block).add(this->catches).add(this->final);
    }
    void addCatch(const std::shared_ptr<IEggParserNode>& catchNode) {
      this->catches.emplace_back(catchNode);
    }
    void addFinally(const std::shared_ptr<IEggParserNode>& finallyNode) {
      assert(this->final == nullptr);
      this->final = finallyNode;
    }
  };

  class EggParserNode_Using : public EggParserNodeBase {
  private:
    std::shared_ptr<IEggParserNode> expr;
    std::shared_ptr<IEggParserNode> block;
  public:
    EggParserNode_Using(const std::shared_ptr<IEggParserNode>& expr, const std::shared_ptr<IEggParserNode>& block)
      : expr(expr), block(block) {
      assert(expr != nullptr);
      assert(block != nullptr);
    }
    virtual void dump(std::ostream& os) const override {
      ParserDump(os, "using").add(this->expr).add(this->block);
    }
  };

  class EggParserNode_While : public EggParserNodeBase {
  private:
    std::shared_ptr<IEggParserNode> condition;
    std::shared_ptr<IEggParserNode> block;
  public:
    EggParserNode_While(const std::shared_ptr<IEggParserNode>& condition, const std::shared_ptr<IEggParserNode>& block)
      : condition(condition), block(block) {
      assert(condition != nullptr);
      assert(block != nullptr);
    }
    virtual void dump(std::ostream& os) const override {
      ParserDump(os, "while").add(this->condition).add(this->block);
    }
  };

  class EggParserNode_Yield : public EggParserNodeBase {
  private:
    std::shared_ptr<IEggParserNode> expr;
  public:
    explicit EggParserNode_Yield(const std::shared_ptr<IEggParserNode>& expr)
      : expr(expr) {
      assert(expr != nullptr);
    }
    virtual void dump(std::ostream& os) const override {
      ParserDump(os, "yield").add(this->expr);
    }
  };

  class EggParserNode_Identifier : public EggParserNodeBase {
  private:
    std::string name;
  public:
    explicit EggParserNode_Identifier(const std::string& name)
      : name(name) {
    }
    virtual void dump(std::ostream& os) const override {
      ParserDump(os, "identifier").add(this->name);
    }
  };

  class EggParserNode_LiteralInteger : public EggParserNodeBase {
  private:
    int64_t value;
  public:
    explicit EggParserNode_LiteralInteger(int64_t value)
      : value(value) {
    }
    virtual const EggParserType& getType() const override {
      return typeInt;
    }
    virtual void dump(std::ostream& os) const override {
      ParserDump(os, "literal int").add(std::to_string(this->value));
    }
  };

  class EggParserNode_LiteralFloat : public EggParserNodeBase {
  private:
    double value;
  public:
    explicit EggParserNode_LiteralFloat(double value)
      : value(value) {
    }
    virtual const EggParserType& getType() const override {
      return typeFloat;
    }
    virtual void dump(std::ostream& os) const override {
      ParserDump(os, "literal float").add(std::to_string(this->value));
    }
  };

  class EggParserNode_LiteralString : public EggParserNodeBase {
  private:
    std::string value;
  public:
    explicit EggParserNode_LiteralString(const std::string& value)
      : value(value) {
    }
    virtual const EggParserType& getType() const override {
      return typeString;
    }
    virtual void dump(std::ostream& os) const override {
      ParserDump(os, "literal string").add(this->value);
    }
  };

  class EggParserNode_Unary : public EggParserNodeBase {
  protected:
    EggParserUnary op;
    std::shared_ptr<IEggParserNode> expr;
    EggParserNode_Unary(EggParserUnary op, const std::shared_ptr<IEggParserNode>& expr)
      : op(op), expr(expr) {
      assert(expr != nullptr);
    }
  public:
    virtual void dump(std::ostream& os) const override {
      ParserDump(os, "unary").add(EggParserNodeBase::unaryToString(this->op)).add(this->expr);
    }
  };

#define EGG_PARSER_UNARY_OPERATOR_DEFINE(name, text) \
  class EggParserNode_Unary##name : public EggParserNode_Unary { \
  public: \
    EggParserNode_Unary##name(const std::shared_ptr<IEggParserNode>& expr) \
      : EggParserNode_Unary(EggParserUnary::name, expr) { \
    } \
    virtual const EggParserType& getType() const override; \
  };
  EGG_PARSER_UNARY_OPERATORS(EGG_PARSER_UNARY_OPERATOR_DEFINE)

  class EggParserNode_Binary : public EggParserNodeBase {
  protected:
    EggParserBinary op;
    std::shared_ptr<IEggParserNode> lhs;
    std::shared_ptr<IEggParserNode> rhs;
    EggParserNode_Binary(EggParserBinary op, const std::shared_ptr<IEggParserNode>& lhs, const std::shared_ptr<IEggParserNode>& rhs)
      : op(op), lhs(lhs), rhs(rhs) {
      assert(lhs != nullptr);
      assert(rhs != nullptr);
    }
  public:
    virtual void dump(std::ostream& os) const override {
      ParserDump(os, "binary").add(EggParserNodeBase::binaryToString(this->op)).add(this->lhs).add(this->rhs);
    }
  };

#define EGG_PARSER_BINARY_OPERATOR_DEFINE(name, text) \
  class EggParserNode_Binary##name : public EggParserNode_Binary { \
  public: \
    EggParserNode_Binary##name(const std::shared_ptr<IEggParserNode>& lhs, const std::shared_ptr<IEggParserNode>& rhs) \
      : EggParserNode_Binary(EggParserBinary::name, lhs, rhs) { \
    } \
    virtual const EggParserType& getType() const override; \
  };
  EGG_PARSER_BINARY_OPERATORS(EGG_PARSER_BINARY_OPERATOR_DEFINE)

  class EggParserNode_Assign : public EggParserNodeBase {
  protected:
    EggParserAssign op;
    std::shared_ptr<IEggParserNode> lhs;
    std::shared_ptr<IEggParserNode> rhs;
    EggParserNode_Assign(EggParserAssign op, const std::shared_ptr<IEggParserNode>& lhs, const std::shared_ptr<IEggParserNode>& rhs)
      : op(op), lhs(lhs), rhs(rhs) {
      assert(lhs != nullptr);
      assert(rhs != nullptr);
    }
  public:
    virtual void dump(std::ostream& os) const override {
      ParserDump(os, "assign").add(EggParserNodeBase::assignToString(this->op)).add(this->lhs).add(this->rhs);
    }
  };

#define EGG_PARSER_ASSIGN_OPERATOR_DEFINE(name, text) \
  class EggParserNode_Assign##name : public EggParserNode_Assign { \
  public: \
    EggParserNode_Assign##name(const std::shared_ptr<IEggParserNode>& lhs, const std::shared_ptr<IEggParserNode>& rhs) \
      : EggParserNode_Assign(EggParserAssign::name, lhs, rhs) { \
    } \
  };
  EGG_PARSER_ASSIGN_OPERATORS(EGG_PARSER_ASSIGN_OPERATOR_DEFINE)

    class EggParserContextBase : public IEggParserContext {
  private:
    EggParserAllowedUnderlying allowed;
  public:
    explicit EggParserContextBase(EggParserAllowed allowed)
      : allowed(static_cast<EggParserAllowedUnderlying>(allowed)) {
    }
    virtual ~EggParserContextBase() {
    }
    virtual IEggParserBlockVisitor* getBlockVisitor() {
      return nullptr;
    }
    virtual bool isAllowed(EggParserAllowed bit) const override {
      return (this->allowed & static_cast<EggParserAllowedUnderlying>(bit)) != 0;
    }
    virtual EggParserAllowed inheritAllowed(EggParserAllowed allow, EggParserAllowed inherit) const {
      auto inherited = this->allowed & static_cast<EggParserAllowedUnderlying>(inherit);
      return static_cast<EggParserAllowed>(inherited | static_cast<EggParserAllowedUnderlying>(allow));
    }
  };


  class EggParserContext : public EggParserContextBase {
  private:
    std::string resource;
  public:
    explicit EggParserContext(const std::string& resource, EggParserAllowed allowed = EggParserAllowed::None)
      : EggParserContextBase(allowed), resource(resource) {
    }
    virtual std::string getResource() const {
      return this->resource;
    }
  };

  class EggParserContextNested : public EggParserContextBase {
  protected:
    IEggParserContext* parent;
  public:
    EggParserContextNested(IEggParserContext& parent, EggParserAllowed allowed, EggParserAllowed inherited = EggParserAllowed::None)
      : EggParserContextBase(parent.inheritAllowed(allowed, inherited)), parent(&parent) {
      assert(this->parent != nullptr);
    }
    virtual std::string getResource() const {
      return this->parent->getResource();
    }
  };

  class EggParserContextSwitch : public EggParserContextNested, public IEggParserBlockVisitor {
  private:
    const EggSyntaxNode_Switch* unpromoted;
    std::shared_ptr<EggParserNode_Switch> promoted;
    EggTokenizerKeyword previous;
  public:
    EggParserContextSwitch(IEggParserContext& parent, const EggSyntaxNode_Switch& switchStatement, const IEggSyntaxNode& switchExpression)
      : EggParserContextNested(parent, EggParserAllowed::Break|EggParserAllowed::Case|EggParserAllowed::Continue, EggParserAllowed::Rethrow|EggParserAllowed::Return|EggParserAllowed::Yield),
        unpromoted(&switchStatement),
        promoted(std::make_shared<EggParserNode_Switch>(switchExpression.promote(parent))),
        previous(EggTokenizerKeyword::Null) {
      assert(this->unpromoted != nullptr);
      assert(this->promoted != nullptr);
    }
    virtual IEggParserBlockVisitor* getBlockVisitor() {
      return this;
    }
    virtual void visitBegin() override {
      // Make sure we don't think we've seens any statements yet
      assert(this->previous == EggTokenizerKeyword::Null);
    }
    virtual void visitStatement(const IEggSyntaxNode& statement) override {
      auto keyword = statement.keyword();
      if (keyword == EggTokenizerKeyword::Case) {
        this->visitCase(statement);
      } else if (keyword == EggTokenizerKeyword::Default) {
        this->visitDefault(statement);
      } else {
        this->visitOther(statement);
      }
      this->previous = keyword;
      assert(this->previous != EggTokenizerKeyword::Null);
    }
    virtual std::shared_ptr<IEggParserNode> visitEnd() override {
      // Check that the last thing in each clause was 'break' or 'continue' or some other flow control
      this->clauseEnd(*this->unpromoted);
      return this->promoted;
    }
  private:
    void visitLabel(const IEggSyntaxNode& statement) {
      // See if we've started a new clause
      if ((this->previous != EggTokenizerKeyword::Case) && (this->previous != EggTokenizerKeyword::Default)) {
        if (this->previous != EggTokenizerKeyword::Null) {
          // We're not at the very beginning of the switch statement
          this->clauseEnd(statement.location());
        }
        this->promoted->newClause();
      }
    }
    void visitCase(const IEggSyntaxNode& statement) {
      // Promoting the 'case' statement produces the case expression
      this->visitLabel(statement);
      if (!this->promoted->addCase(statement.promote(*this))) {
        throw exceptionFromLocation(*this, "Duplicate 'case' constant in 'switch' statement", statement.location());
      }
    }
    void visitDefault(const IEggSyntaxNode& statement) {
      // There's no expression associated with the 'default'
      this->visitLabel(statement);
      if (!this->promoted->addDefault()) {
        throw exceptionFromLocation(*this, "More than one 'default' statement in 'switch' statement", statement.location());
      }
    }
    void visitOther(const IEggSyntaxNode& statement) {
      if (this->previous == EggTokenizerKeyword::Null) {
        // We're at the very beginning of the switch statement
        throw exceptionFromLocation(*this, "Expected 'case' or 'default' statement at beginning of 'switch' statement", statement.location());
      } else if (this->previous == EggTokenizerKeyword::Break) {
        // We're after an unconditional 'break'
        throw exceptionFromLocation(*this, "Expected 'case' or 'default' statement to follow 'break' in 'switch' statement", statement.location());
      } else if (this->previous == EggTokenizerKeyword::Continue) {
        // We're after an unconditional 'continue'
        throw exceptionFromLocation(*this, "Expected 'case' or 'default' statement to follow 'continue' in 'switch' statement", statement.location());
      }
      if (!this->promoted->addStatement(statement.promote(*this))) {
        throw exceptionFromLocation(*this, "Malformed 'switch' statement", statement.location());
      }
    }
    void clauseEnd(const EggSyntaxNodeLocation& location) {
      if ((this->previous == EggTokenizerKeyword::Case) || (this->previous == EggTokenizerKeyword::Default)) {
        throw exceptionFromLocation(*this, "Empty clause at end of 'switch' statement", location);
      }
      if ((this->previous != EggTokenizerKeyword::Break) && (this->previous != EggTokenizerKeyword::Continue) &&
          (this->previous != EggTokenizerKeyword::Throw) && (this->previous != EggTokenizerKeyword::Return)) {
        throw exceptionFromLocation(*this, "Expected 'break', 'continue', 'throw' or 'return' statement at end of 'switch' clause", location);
      }
    }
  };

  class EggParserModule : public IEggParser {
  public:
    virtual std::shared_ptr<IEggParserNode> parse(IEggTokenizer& tokenizer) override {
      auto syntax = EggParserFactory::createModuleSyntaxParser();
      auto ast = syntax->parse(tokenizer);
      EggParserContext context(tokenizer.resource());
      return ast->promote(context);
    }
  };
}

std::string egg::yolk::EggParserType::tagToString(Tag tag) {
  using egg::lang::TypeStorage;
  if (tag == TypeStorage::Inferred) {
    return "var";
  }
  if (tag == TypeStorage::Void) {
    return "void";
  }
  if (tag == TypeStorage::Any) {
    return "any";
  }
  if (tag == (TypeStorage::Null | TypeStorage::Any)) {
    return "any?";
  }
  std::string result;
  tagToStringComponent(result, "bool", tag.hasBit(TypeStorage::Bool));
  tagToStringComponent(result, "int", tag.hasBit(TypeStorage::Int));
  tagToStringComponent(result, "float", tag.hasBit(TypeStorage::Float));
  tagToStringComponent(result, "string", tag.hasBit(TypeStorage::String));
  tagToStringComponent(result, "type", tag.hasBit(TypeStorage::Type));
  tagToStringComponent(result, "object", tag.hasBit(TypeStorage::Object));
  if (tag.hasBit(TypeStorage::Void)) {
    result.append("?");
  }
  return result;
}

#define EGG_PARSER_OPERATOR_STRING(name, text) text,

std::string EggParserNodeBase::unaryToString(egg::yolk::EggParserUnary op) {
  static const char* const table[] = {
    EGG_PARSER_UNARY_OPERATORS(EGG_PARSER_OPERATOR_STRING)
  };
  auto index = static_cast<size_t>(op);
  assert(index < EGG_NELEMS(table));
  return table[index];
}

std::string EggParserNodeBase::binaryToString(egg::yolk::EggParserBinary op) {
  static const char* const table[] = {
    EGG_PARSER_BINARY_OPERATORS(EGG_PARSER_OPERATOR_STRING)
  };
  auto index = static_cast<size_t>(op);
  assert(index < EGG_NELEMS(table));
  return table[index];
}

std::string EggParserNodeBase::assignToString(egg::yolk::EggParserAssign op) {
  static const char* const table[] = {
    EGG_PARSER_ASSIGN_OPERATORS(EGG_PARSER_OPERATOR_STRING)
  };
  auto index = static_cast<size_t>(op);
  assert(index < EGG_NELEMS(table));
  return table[index];
}

std::string EggParserNodeBase::mutateToString(egg::yolk::EggParserMutate op) {
  static const char* const table[] = {
    EGG_PARSER_MUTATE_OPERATORS(EGG_PARSER_OPERATOR_STRING)
  };
  auto index = static_cast<size_t>(op);
  assert(index < EGG_NELEMS(table));
  return table[index];
}

std::shared_ptr<egg::yolk::IEggParserNode> egg::yolk::EggSyntaxNode_Empty::promote(egg::yolk::IEggParserContext& context) const {
  if (!context.isAllowed(EggParserAllowed::Empty)) {
    throw exceptionFromLocation(context, "Empty statements are not permitted in this context", *this);
  }
  return nullptr;
}

std::shared_ptr<egg::yolk::IEggParserNode> egg::yolk::EggSyntaxNode_Module::promote(egg::yolk::IEggParserContext& context) const {
  auto module = std::make_shared<EggParserNode_Module>();
  for (auto& statement : this->child) {
    module->addChild(statement->promote(context));
  }
  return module;
}

std::shared_ptr<egg::yolk::IEggParserNode> egg::yolk::EggSyntaxNode_Block::promote(egg::yolk::IEggParserContext& context) const {
  auto* blockVisitor = context.getBlockVisitor();
  if (blockVisitor != nullptr) {
    // Defer our processing to the 'switch' statement visitor
    blockVisitor->visitBegin();
    for (auto& statement : this->child) {
      blockVisitor->visitStatement(*statement);
    }
    return blockVisitor->visitEnd();
  }
  auto module = std::make_shared<EggParserNode_Block>();
  for (auto& statement : this->child) {
    module->addChild(statement->promote(context));
  }
  return module;
}

std::shared_ptr<egg::yolk::IEggParserNode> egg::yolk::EggSyntaxNode_Type::promote(egg::yolk::IEggParserContext&) const {
  auto type = std::make_shared<EggParserNode_Type>(EggParserType(this->tag));
  return type;
}

std::shared_ptr<egg::yolk::IEggParserNode> egg::yolk::EggSyntaxNode_VariableDeclaration::promote(egg::yolk::IEggParserContext& context) const {
  return std::make_shared<EggParserNode_Declare>(this->name, this->child->promote(context));
}

std::shared_ptr<egg::yolk::IEggParserNode> egg::yolk::EggSyntaxNode_VariableInitialization::promote(egg::yolk::IEggParserContext& context) const {
  return std::make_shared<EggParserNode_Declare>(this->name, this->child[0]->promote(context), this->child[1]->promote(context));
}

std::shared_ptr<egg::yolk::IEggParserNode> egg::yolk::EggSyntaxNode_Assignment::promote(egg::yolk::IEggParserContext& context) const {
  switch (this->op) {
    case EggTokenizerOperator::PercentEqual:
      return this->promoteAssign<EggParserNode_AssignRemainder>(context);
    case EggTokenizerOperator::AmpersandEqual:
      return this->promoteAssign<EggParserNode_AssignBitwiseAnd>(context);
    case EggTokenizerOperator::StarEqual:
      return this->promoteAssign<EggParserNode_AssignMultiply>(context);
    case EggTokenizerOperator::PlusEqual:
      return this->promoteAssign<EggParserNode_AssignPlus>(context);
    case EggTokenizerOperator::MinusEqual:
      return this->promoteAssign<EggParserNode_AssignMinus>(context);
    case EggTokenizerOperator::SlashEqual:
      return this->promoteAssign<EggParserNode_AssignDivide>(context);
    case EggTokenizerOperator::ShiftLeftEqual:
      return this->promoteAssign<EggParserNode_AssignShiftLeft>(context);
    case EggTokenizerOperator::Equal:
      return this->promoteAssign<EggParserNode_AssignEqual>(context);
    case EggTokenizerOperator::ShiftRightEqual:
      return this->promoteAssign<EggParserNode_AssignShiftRight>(context);
    case EggTokenizerOperator::ShiftRightUnsignedEqual:
      return this->promoteAssign<EggParserNode_AssignShiftRightUnsigned>(context);
    case EggTokenizerOperator::CaretEqual:
      return this->promoteAssign<EggParserNode_AssignBitwiseXor>(context);
    case EggTokenizerOperator::BarEqual:
      return this->promoteAssign<EggParserNode_AssignBitwiseOr>(context);
    case EggTokenizerOperator::Bang:
    case EggTokenizerOperator::BangEqual:
    case EggTokenizerOperator::Percent:
    case EggTokenizerOperator::Ampersand:
    case EggTokenizerOperator::AmpersandAmpersand:
    case EggTokenizerOperator::ParenthesisLeft:
    case EggTokenizerOperator::ParenthesisRight:
    case EggTokenizerOperator::Star:
    case EggTokenizerOperator::Plus:
    case EggTokenizerOperator::PlusPlus:
    case EggTokenizerOperator::Comma:
    case EggTokenizerOperator::Minus:
    case EggTokenizerOperator::MinusMinus:
    case EggTokenizerOperator::Lambda:
    case EggTokenizerOperator::Dot:
    case EggTokenizerOperator::Ellipsis:
    case EggTokenizerOperator::Slash:
    case EggTokenizerOperator::Colon:
    case EggTokenizerOperator::Semicolon:
    case EggTokenizerOperator::Less:
    case EggTokenizerOperator::ShiftLeft:
    case EggTokenizerOperator::LessEqual:
    case EggTokenizerOperator::EqualEqual:
    case EggTokenizerOperator::Greater:
    case EggTokenizerOperator::GreaterEqual:
    case EggTokenizerOperator::ShiftRight:
    case EggTokenizerOperator::ShiftRightUnsigned:
    case EggTokenizerOperator::Query:
    case EggTokenizerOperator::QueryQuery:
    case EggTokenizerOperator::BracketLeft:
    case EggTokenizerOperator::BracketRight:
    case EggTokenizerOperator::Caret:
    case EggTokenizerOperator::CurlyLeft:
    case EggTokenizerOperator::Bar:
    case EggTokenizerOperator::BarBar:
    case EggTokenizerOperator::CurlyRight:
    case EggTokenizerOperator::Tilde:
    default:
      throw exceptionFromToken(context, "Unknown assignment operator", *this);
  }
}

std::shared_ptr<egg::yolk::IEggParserNode> egg::yolk::EggSyntaxNode_Mutate::promote(egg::yolk::IEggParserContext& context) const {
  EggParserMutate mop;
  if (this->op == EggTokenizerOperator::PlusPlus) {
    mop = EggParserMutate::Increment;
  } else if (this->op == EggTokenizerOperator::MinusMinus) {
    mop = EggParserMutate::Decrement;
  } else {
    throw exceptionFromToken(context, "Unknown increment/decrement operator", *this);
  }
  return std::make_shared<EggParserNode_Mutate>(mop, this->child->promote(context));
}

std::shared_ptr<egg::yolk::IEggParserNode> egg::yolk::EggSyntaxNode_Break::promote(egg::yolk::IEggParserContext& context) const {
  if (!context.isAllowed(EggParserAllowed::Break)) {
    throw exceptionFromLocation(context, "The 'break' statement may only be used within loops or switch statements", *this);
  }
  return std::make_shared<EggParserNode_Break>();
}

std::shared_ptr<egg::yolk::IEggParserNode> egg::yolk::EggSyntaxNode_Case::promote(egg::yolk::IEggParserContext& context) const {
  // The logic is handled by the 'switch' node, so just promote the value expression
  if (!context.isAllowed(EggParserAllowed::Case)) {
    throw exceptionFromLocation(context, "The 'case' statement may only be used within switch statements", *this);
  }
  return this->child->promote(context);
}

std::shared_ptr<egg::yolk::IEggParserNode> egg::yolk::EggSyntaxNode_Catch::promote(egg::yolk::IEggParserContext& context) const {
  auto type = this->child[0]->promote(context);
  EggParserContextNested nested(context, EggParserAllowed::Rethrow|EggParserAllowed::Return|EggParserAllowed::Yield);
  auto block = this->child[1]->promote(nested);
  return std::make_shared<EggParserNode_Catch>(this->name, type, block);
}

std::shared_ptr<egg::yolk::IEggParserNode> egg::yolk::EggSyntaxNode_Continue::promote(egg::yolk::IEggParserContext& context) const {
  if (!context.isAllowed(EggParserAllowed::Continue)) {
    throw exceptionFromLocation(context, "The 'continue' statement may only be used within loops or switch statements", *this);
  }
  return std::make_shared<EggParserNode_Continue>();
}

std::shared_ptr<egg::yolk::IEggParserNode> egg::yolk::EggSyntaxNode_Default::promote(egg::yolk::IEggParserContext& context) const {
  // The logic is handled by the 'switch' node, so just assume it's a misplaced 'default'
  throw exceptionFromLocation(context, "The 'default' statement may only be used within switch statements", *this);
}

std::shared_ptr<egg::yolk::IEggParserNode> egg::yolk::EggSyntaxNode_Do::promote(egg::yolk::IEggParserContext& context) const {
  auto condition = this->child[0]->promote(context);
  EggParserContextNested nested(context, EggParserAllowed::Break|EggParserAllowed::Continue, EggParserAllowed::Rethrow|EggParserAllowed::Return|EggParserAllowed::Yield);
  auto block = this->child[1]->promote(nested);
  return std::make_shared<EggParserNode_Do>(condition, block);
}

std::shared_ptr<egg::yolk::IEggParserNode> egg::yolk::EggSyntaxNode_If::promote(egg::yolk::IEggParserContext& context) const {
  assert((this->child.size() == 2) || (this->child.size() == 3));
  auto condition = this->child[0]->promote(context);
  auto trueBlock = this->child[1]->promote(context);
  std::shared_ptr<egg::yolk::IEggParserNode> falseBlock = nullptr;
  if (this->child.size() == 3) {
    falseBlock = this->child[2]->promote(context);
  }
  return std::make_shared<EggParserNode_If>(condition, trueBlock, falseBlock);
}

std::shared_ptr<egg::yolk::IEggParserNode> egg::yolk::EggSyntaxNode_Finally::promote(egg::yolk::IEggParserContext& context) const {
  // The logic is handled by the 'try' node, so just assume it's a misplaced 'finally'
  throw exceptionFromLocation(context, "The 'finally' statement may only be used as part of a 'try' statement", *this);
}

std::shared_ptr<egg::yolk::IEggParserNode> egg::yolk::EggSyntaxNode_For::promote(egg::yolk::IEggParserContext& context) const {
  // We allow empty statements but not flow control in the three 'for' clauses
  EggParserContextNested nested1(context, EggParserAllowed::Empty);
  auto pre = this->child[0]->promote(nested1);
  auto cond = this->child[1]->promote(nested1);
  auto post = this->child[2]->promote(nested1);
  EggParserContextNested nested2(context, EggParserAllowed::Break|EggParserAllowed::Continue, EggParserAllowed::Rethrow|EggParserAllowed::Return|EggParserAllowed::Yield);
  auto block = this->child[3]->promote(nested2);
  return std::make_shared<EggParserNode_For>(pre, cond, post, block);
}

std::shared_ptr<egg::yolk::IEggParserNode> egg::yolk::EggSyntaxNode_Foreach::promote(egg::yolk::IEggParserContext& context) const {
  auto target = this->child[0]->promote(context);
  auto expr = this->child[1]->promote(context);
  EggParserContextNested nested(context, EggParserAllowed::Break|EggParserAllowed::Continue, EggParserAllowed::Rethrow|EggParserAllowed::Return|EggParserAllowed::Yield);
  auto block = this->child[2]->promote(nested);
  return std::make_shared<EggParserNode_Foreach>(target, expr, block);
}

std::shared_ptr<egg::yolk::IEggParserNode> egg::yolk::EggSyntaxNode_Return::promote(egg::yolk::IEggParserContext& context) const {
  if (!context.isAllowed(EggParserAllowed::Return)) {
    throw exceptionFromLocation(context, "The 'return' statement may only be used within functions", *this);
  }
  auto result = std::make_shared<EggParserNode_Return>();
  for (auto& i : this->child) {
    result->addChild(i->promote(context));
  }
  return result;
}

std::shared_ptr<egg::yolk::IEggParserNode> egg::yolk::EggSyntaxNode_Switch::promote(egg::yolk::IEggParserContext& context) const {
  EggParserContextSwitch nested(context, *this, *this->child[0]);
  return this->child[1]->promote(nested);
}

std::shared_ptr<egg::yolk::IEggParserNode> egg::yolk::EggSyntaxNode_Throw::promote(egg::yolk::IEggParserContext& context) const {
  // This is a throw or a rethrow: 'throw <expr>;' or 'throw;'
  std::shared_ptr<egg::yolk::IEggParserNode> expr = nullptr;
  if (!this->child.empty()) {
    expr = this->child[0]->promote(context);
  } else if (!context.isAllowed(EggParserAllowed::Rethrow)) {
    throw exceptionFromLocation(context, "The 'throw' statement may only be used to rethrow exceptions inside a 'catch' statement", *this);
  }
  return std::make_shared<EggParserNode_Throw>(expr);
}

std::shared_ptr<egg::yolk::IEggParserNode> egg::yolk::EggSyntaxNode_Try::promote(egg::yolk::IEggParserContext& context) const {
  // TODO There's some ambiguity amongst C++/Java/C# as to whether 'break' inside here breaks out of any enclosing loop
  // The 'switch' pattern is even more confusing, so we just disallow 'break'/'continue' inside 'try'/'catch'/'finally' for now
  EggParserContextNested nested1(context, EggParserAllowed::None, EggParserAllowed::Rethrow|EggParserAllowed::Return|EggParserAllowed::Yield);
  auto clauses = this->child.size();
  assert(clauses >= 2);
  auto block = this->child[0]->promote(nested1);
  auto result = std::make_shared<EggParserNode_Try>(block);
  EggParserContextNested nested2(context, EggParserAllowed::Rethrow, EggParserAllowed::Return|EggParserAllowed::Yield);
  for (size_t i = 1; i < clauses; ++i) {
    auto& clause = *this->child[i];
    if (clause.keyword() == EggTokenizerKeyword::Catch) {
      // We can have any number of 'catch' clauses; where we allow rethrows
      result->addCatch(clause.promote(nested2));
    } else if (clause.keyword() != EggTokenizerKeyword::Finally) {
      throw exceptionFromLocation(context, "Expected only 'catch' and 'finally' statements in 'try' statement", clause.location());
    } else if (i != (clauses - 1)) {
      throw exceptionFromLocation(context, "The 'finally' statement must be the last clause in a 'try' statement", clause.location());
    } else {
      // We can only have one 'finally' clause; where we allow inherited rethrows only
      result->addFinally(clause.promote(nested1));
    }
  }
  return result;
}

std::shared_ptr<egg::yolk::IEggParserNode> egg::yolk::EggSyntaxNode_Using::promote(egg::yolk::IEggParserContext& context) const {
  auto expr = this->child[0]->promote(context);
  auto block = this->child[1]->promote(context);
  return std::make_shared<EggParserNode_Using>(expr, block);
}

std::shared_ptr<egg::yolk::IEggParserNode> egg::yolk::EggSyntaxNode_While::promote(egg::yolk::IEggParserContext& context) const {
  auto condition = this->child[0]->promote(context);
  EggParserContextNested nested(context, EggParserAllowed::Break|EggParserAllowed::Continue, EggParserAllowed::Rethrow|EggParserAllowed::Return|EggParserAllowed::Yield);
  auto block = this->child[1]->promote(nested);
  return std::make_shared<EggParserNode_While>(condition, block);
}

std::shared_ptr<egg::yolk::IEggParserNode> egg::yolk::EggSyntaxNode_Yield::promote(egg::yolk::IEggParserContext& context) const {
  if (!context.isAllowed(EggParserAllowed::Yield)) {
    throw exceptionFromLocation(context, "The 'yield' statement may only be used within generator functions", *this);
  }
  return std::make_shared<EggParserNode_Yield>(this->child->promote(context));
}

std::shared_ptr<egg::yolk::IEggParserNode> egg::yolk::EggSyntaxNode_UnaryOperator::promote(egg::yolk::IEggParserContext& context) const {
  if (this->op == EggTokenizerOperator::Bang) {
    return this->promoteUnary<EggParserNode_UnaryLogicalNot>(context);
  }
  if (this->op == EggTokenizerOperator::Ampersand) {
    return this->promoteUnary<EggParserNode_UnaryRef>(context);
  }
  if (this->op == EggTokenizerOperator::Star) {
    return this->promoteUnary<EggParserNode_UnaryDeref>(context);
  }
  if (this->op == EggTokenizerOperator::Minus) {
    return this->promoteUnary<EggParserNode_UnaryNegate>(context);
  }
  if (this->op == EggTokenizerOperator::Ellipsis) {
    return this->promoteUnary<EggParserNode_UnaryEllipsis>(context);
  }
  if (this->op == EggTokenizerOperator::Tilde) {
    return this->promoteUnary<EggParserNode_UnaryBitwiseNot>(context);
  }
  throw exceptionFromToken(context, "Unknown unary operator", *this);
}

std::shared_ptr<egg::yolk::IEggParserNode> egg::yolk::EggSyntaxNode_BinaryOperator::promote(egg::yolk::IEggParserContext& context) const {
  switch (this->op) {
  case EggTokenizerOperator::Percent:
    return this->promoteBinary<EggParserNode_BinaryRemainder>(context);
  case EggTokenizerOperator::Ampersand:
    return this->promoteBinary<EggParserNode_BinaryBitwiseAnd>(context);
  case EggTokenizerOperator::AmpersandAmpersand:
    return this->promoteBinary<EggParserNode_BinaryLogicalAnd>(context);
  case EggTokenizerOperator::Star:
    return this->promoteBinary<EggParserNode_BinaryMultiply>(context);
  case EggTokenizerOperator::Plus:
    return this->promoteBinary<EggParserNode_BinaryPlus>(context);
  case EggTokenizerOperator::Minus:
    return this->promoteBinary<EggParserNode_BinaryMinus>(context);
  case EggTokenizerOperator::Lambda:
    return this->promoteBinary<EggParserNode_BinaryLambda>(context);
  case EggTokenizerOperator::Dot:
    return this->promoteBinary<EggParserNode_BinaryDot>(context);
  case EggTokenizerOperator::Slash:
    return this->promoteBinary<EggParserNode_BinaryDivide>(context);
  case EggTokenizerOperator::Less:
    return this->promoteBinary<EggParserNode_BinaryLess>(context);
  case EggTokenizerOperator::ShiftLeft:
    return this->promoteBinary<EggParserNode_BinaryShiftLeft>(context);
  case EggTokenizerOperator::LessEqual:
    return this->promoteBinary<EggParserNode_BinaryLessEqual>(context);
  case EggTokenizerOperator::EqualEqual:
    return this->promoteBinary<EggParserNode_BinaryEqual>(context);
  case EggTokenizerOperator::Greater:
    return this->promoteBinary<EggParserNode_BinaryGreater>(context);
  case EggTokenizerOperator::GreaterEqual:
    return this->promoteBinary<EggParserNode_BinaryGreaterEqual>(context);
  case EggTokenizerOperator::ShiftRight:
    return this->promoteBinary<EggParserNode_BinaryShiftRight>(context);
  case EggTokenizerOperator::ShiftRightUnsigned:
    return this->promoteBinary<EggParserNode_BinaryShiftRightUnsigned>(context);
  case EggTokenizerOperator::QueryQuery:
    return this->promoteBinary<EggParserNode_BinaryNullCoalescing>(context);
  case EggTokenizerOperator::BracketLeft:
    return this->promoteBinary<EggParserNode_BinaryBrackets>(context);
  case EggTokenizerOperator::Caret:
    return this->promoteBinary<EggParserNode_BinaryBitwiseXor>(context);
  case EggTokenizerOperator::Bar:
    return this->promoteBinary<EggParserNode_BinaryBitwiseOr>(context);
  case EggTokenizerOperator::BarBar:
    return this->promoteBinary<EggParserNode_BinaryLogicalOr>(context);
  case EggTokenizerOperator::Bang:
  case EggTokenizerOperator::BangEqual:
  case EggTokenizerOperator::PercentEqual:
  case EggTokenizerOperator::AmpersandEqual:
  case EggTokenizerOperator::ParenthesisLeft:
  case EggTokenizerOperator::ParenthesisRight:
  case EggTokenizerOperator::StarEqual:
  case EggTokenizerOperator::PlusPlus:
  case EggTokenizerOperator::PlusEqual:
  case EggTokenizerOperator::Comma:
  case EggTokenizerOperator::MinusMinus:
  case EggTokenizerOperator::MinusEqual:
  case EggTokenizerOperator::SlashEqual:
  case EggTokenizerOperator::Colon:
  case EggTokenizerOperator::Semicolon:
  case EggTokenizerOperator::Ellipsis:
  case EggTokenizerOperator::ShiftLeftEqual:
  case EggTokenizerOperator::Equal:
  case EggTokenizerOperator::ShiftRightEqual:
  case EggTokenizerOperator::ShiftRightUnsignedEqual:
  case EggTokenizerOperator::Query:
  case EggTokenizerOperator::BracketRight:
  case EggTokenizerOperator::CaretEqual:
  case EggTokenizerOperator::CurlyLeft:
  case EggTokenizerOperator::BarEqual:
  case EggTokenizerOperator::CurlyRight:
  case EggTokenizerOperator::Tilde:
  default:
    throw exceptionFromToken(context, "Unknown binary operator", *this);
  }
}

std::shared_ptr<egg::yolk::IEggParserNode> egg::yolk::EggSyntaxNode_TernaryOperator::promote(egg::yolk::IEggParserContext&) const {
  EGG_THROW(__FUNCTION__ " TODO"); // TODO
}

std::shared_ptr<egg::yolk::IEggParserNode> egg::yolk::EggSyntaxNode_Call::promote(egg::yolk::IEggParserContext&) const {
  EGG_THROW(__FUNCTION__ " TODO"); // TODO
}

std::shared_ptr<egg::yolk::IEggParserNode> egg::yolk::EggSyntaxNode_Named::promote(egg::yolk::IEggParserContext&) const {
  EGG_THROW(__FUNCTION__ " TODO"); // TODO
}

std::shared_ptr<egg::yolk::IEggParserNode> egg::yolk::EggSyntaxNode_Identifier::promote(egg::yolk::IEggParserContext&) const {
  return std::make_shared<EggParserNode_Identifier>(this->name);
}

std::shared_ptr<egg::yolk::IEggParserNode> egg::yolk::EggSyntaxNode_Literal::promote(egg::yolk::IEggParserContext& context) const {
  if (this->kind == EggTokenizerKind::Integer) {
    return std::make_shared<EggParserNode_LiteralInteger>(this->value.i);
  }
  if (this->kind == EggTokenizerKind::Float) {
    return std::make_shared<EggParserNode_LiteralFloat>(this->value.f);
  }
  if (this->kind == EggTokenizerKind::String) {
    return std::make_shared<EggParserNode_LiteralString>(this->value.s);
  }
  throw exceptionFromToken(context, "Unknown literal value type", *this);
}

const egg::yolk::EggParserType& EggParserNode_UnaryLogicalNot::getType() const {
  return typeBool;
}

const egg::yolk::EggParserType& EggParserNode_UnaryRef::getType() const {
  EGG_THROW(__FUNCTION__ " TODO"); // TODO
}

const egg::yolk::EggParserType& EggParserNode_UnaryDeref::getType() const {
  EGG_THROW(__FUNCTION__ " TODO"); // TODO
}

const egg::yolk::EggParserType& EggParserNode_UnaryNegate::getType() const {
  return this->expr->getType();
}

const egg::yolk::EggParserType& EggParserNode_UnaryEllipsis::getType() const {
  EGG_THROW(__FUNCTION__ " TODO"); // TODO
}

const egg::yolk::EggParserType& EggParserNode_UnaryBitwiseNot::getType() const {
  return typeInt;
}

const egg::yolk::EggParserType& EggParserNode_BinaryRemainder::getType() const {
  // See http://mindprod.com/jgloss/modulus.html
  return this->lhs->getType();
}

const egg::yolk::EggParserType& EggParserNode_BinaryBitwiseAnd::getType() const {
  return typeInt;
}

const egg::yolk::EggParserType& EggParserNode_BinaryLogicalAnd::getType() const {
  return typeBool;
}

const egg::yolk::EggParserType& EggParserNode_BinaryMultiply::getType() const {
  EGG_THROW(__FUNCTION__ " TODO"); // TODO
}

const egg::yolk::EggParserType& EggParserNode_BinaryPlus::getType() const {
  EGG_THROW(__FUNCTION__ " TODO"); // TODO
}

const egg::yolk::EggParserType& EggParserNode_BinaryMinus::getType() const {
  EGG_THROW(__FUNCTION__ " TODO"); // TODO
}

const egg::yolk::EggParserType& EggParserNode_BinaryLambda::getType() const {
  EGG_THROW(__FUNCTION__ " TODO"); // TODO
}

const egg::yolk::EggParserType& EggParserNode_BinaryDot::getType() const {
  EGG_THROW(__FUNCTION__ " TODO"); // TODO
}

const egg::yolk::EggParserType& EggParserNode_BinaryDivide::getType() const {
  EGG_THROW(__FUNCTION__ " TODO"); // TODO
}

const egg::yolk::EggParserType& EggParserNode_BinaryLess::getType() const {
  return typeBool;
}

const egg::yolk::EggParserType& EggParserNode_BinaryShiftLeft::getType() const {
  return typeInt;
}

const egg::yolk::EggParserType& EggParserNode_BinaryLessEqual::getType() const {
  return typeBool;
}

const egg::yolk::EggParserType& EggParserNode_BinaryEqual::getType() const {
  return typeBool;
}

const egg::yolk::EggParserType& EggParserNode_BinaryGreater::getType() const {
  return typeBool;
}

const egg::yolk::EggParserType& EggParserNode_BinaryGreaterEqual::getType() const {
  return typeBool;
}

const egg::yolk::EggParserType& EggParserNode_BinaryShiftRight::getType() const {
  return typeInt;
}

const egg::yolk::EggParserType& EggParserNode_BinaryShiftRightUnsigned::getType() const {
  return typeInt;
}

const egg::yolk::EggParserType& EggParserNode_BinaryNullCoalescing::getType() const {
  EGG_THROW(__FUNCTION__ " TODO"); // TODO
}

const egg::yolk::EggParserType& EggParserNode_BinaryBrackets::getType() const {
  EGG_THROW(__FUNCTION__ " TODO"); // TODO
}

const egg::yolk::EggParserType& EggParserNode_BinaryBitwiseXor::getType() const {
  return typeInt;
}

const egg::yolk::EggParserType& EggParserNode_BinaryBitwiseOr::getType() const {
  return typeInt;
}

const egg::yolk::EggParserType& EggParserNode_BinaryLogicalOr::getType() const {
  return typeBool;
}

std::shared_ptr<egg::yolk::IEggParser> egg::yolk::EggParserFactory::createModuleParser() {
  return std::make_shared<EggParserModule>();
}
