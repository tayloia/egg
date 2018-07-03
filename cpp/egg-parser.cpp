#include "yolk.h"
#include "lexers.h"
#include "egg-tokenizer.h"
#include "egg-syntax.h"
#include "egg-parser.h"
#include "egg-engine.h"
#include "egg-program.h"

namespace {
  using namespace egg::yolk;

  inline EggParserAllowed operator|(EggParserAllowed lhs, EggParserAllowed rhs) {
    return egg::lang::Bits::set(lhs, rhs);
  }

  template<typename T, typename... ARGS>
  std::shared_ptr<T> makeParserNode(const IEggParserContext& context, const IEggSyntaxNode& node, ARGS&&... args) {
    // Fetch the syntax node's location and create a new 'T' based on it
    egg::lang::LocationSource location(context.getResourceName(), node.location().begin.line, node.location().begin.column);
    return std::make_shared<T>(location, std::forward<ARGS>(args)...);
  }

  template<typename T>
  std::shared_ptr<IEggProgramNode> promoteUnary(IEggParserContext& context, const IEggSyntaxNode& node, const std::unique_ptr<IEggSyntaxNode>& child) {
    assert(child != nullptr);
    auto expr = context.promote(*child);
    return makeParserNode<T>(context, node, expr);
  }

  template<typename T>
  std::shared_ptr<IEggProgramNode> promoteBinary(IEggParserContext& context, const IEggSyntaxNode& node, const std::unique_ptr<IEggSyntaxNode> children[2]) {
    assert(children[0] != nullptr);
    assert(children[1] != nullptr);
    auto lhs = context.promote(*children[0]);
    auto rhs = context.promote(*children[1]);
    return makeParserNode<T>(context, node, lhs, rhs);
  }

  enum class EggParserArithmetic {
    None,
    Int,
    Float,
    Both
  };

  EggParserArithmetic getArithmeticTypes(const IEggProgramNode& node) {
    auto underlying = node.getType();
    if (underlying->hasNativeType(egg::lang::Discriminator::Float)) {
      return underlying->hasNativeType(egg::lang::Discriminator::Int) ? EggParserArithmetic::Both : EggParserArithmetic::Float;
    }
    return underlying->hasNativeType(egg::lang::Discriminator::Int) ? EggParserArithmetic::Int : EggParserArithmetic::None;
  }

  egg::lang::ITypeRef makeArithmeticType(EggParserArithmetic arithmetic) {
    switch (arithmetic) {
    case EggParserArithmetic::Int:
      return egg::lang::Type::Int;
    case EggParserArithmetic::Float:
      return egg::lang::Type::Float;
    case EggParserArithmetic::Both:
      return egg::lang::Type::Arithmetic;
    case EggParserArithmetic::None:
    default:
      break;
    }
    return egg::lang::Type::Void;
  }

  egg::lang::ITypeRef binaryArithmeticTypes(const std::shared_ptr<IEggProgramNode>& lhs, const std::shared_ptr<IEggProgramNode>& rhs) {
    auto lhsa = getArithmeticTypes(*lhs);
    auto rhsa = getArithmeticTypes(*rhs);
    if ((lhsa == EggParserArithmetic::None) || (rhsa == EggParserArithmetic::None)) {
      return egg::lang::Type::Void;
    }
    if ((lhsa == EggParserArithmetic::Int) && (rhsa == EggParserArithmetic::Int)) {
      return egg::lang::Type::Int;
    }
    if ((lhsa == EggParserArithmetic::Float) || (rhsa == EggParserArithmetic::Float)) {
      return egg::lang::Type::Float;
    }
    return egg::lang::Type::Arithmetic;
  }

  SyntaxException exceptionFromLocation(const IEggParserContext& context, const std::string& reason, const EggSyntaxNodeLocation& location) {
    return SyntaxException(reason, context.getResourceName()->toUTF8(), location);
  }

  SyntaxException exceptionFromToken(const IEggParserContext& context, const std::string& reason, const EggSyntaxNodeBase& node) {
    auto token = node.token().toUTF8();
    return SyntaxException(reason + ": '" + token + "'", context.getResourceName().toUTF8(), node, token);
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
    ParserDump& add(const egg::lang::String& text) {
      *this->os << ' ' << '\'' << text << '\'';
      return *this;
    }
    ParserDump& add(const std::shared_ptr<IEggProgramNode>& child) {
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
    template<typename T>
    ParserDump& raw(T item) {
      *this->os << ' ' << item;
      return *this;
    }
  };

  class EggParserNodeBase : public IEggProgramNode {
  protected:
    egg::lang::LocationSource locationSource;
  public:
    explicit EggParserNodeBase(const egg::lang::LocationSource& locationSource)
      : locationSource(locationSource) {
    }
    virtual egg::lang::ITypeRef getType() const override {
      // By default, nodes are statements (i.e. void return type)
      return egg::lang::Type::Void;
    }
    virtual egg::lang::LocationSource location() const override {
      // Just return the source location
      return this->locationSource;
    }
    virtual bool symbol(egg::lang::String&, egg::lang::ITypeRef&) const override {
      // By default, nodes do not declare symbols
      return false;
    }
    virtual void empredicate(EggProgramContext&, std::shared_ptr<IEggProgramNode>&) override {
      // By default, nodes do not change when they're used as predicates in function call parameters
    }
    virtual EggProgramNodeFlags addressable(EggProgramContext& context) override {
      // By default, nothing is addressable
      return context.compilerError(this->locationSource, "Invalid operand for reference '&' operator");
    }
    virtual std::unique_ptr<IEggProgramAssignee> assignee(EggProgramContext&) const override {
      // By default, we fail if asked to create an assignee
      return nullptr;
    }
  };

  class EggParserNode_Module : public EggParserNodeBase {
  private:
    std::vector<std::shared_ptr<IEggProgramNode>> child;
  public:
    explicit EggParserNode_Module(const egg::lang::LocationSource& locationSource)
      : EggParserNodeBase(locationSource) {
    }
    virtual EggProgramNodeFlags prepare(EggProgramContext& context) override {
      return context.prepareModule(this->child);
    }
    virtual egg::lang::Value execute(EggProgramContext& context) const override {
      return context.executeModule(*this, this->child);
    }
    virtual void dump(std::ostream& os) const override {
      ParserDump(os, "module").add(this->child);
    }
    void addChild(const std::shared_ptr<IEggProgramNode>& statement) {
      assert(statement != nullptr);
      this->child.push_back(statement);
    }
  };

  class EggParserNode_Block : public EggParserNodeBase {
  private:
    std::vector<std::shared_ptr<IEggProgramNode>> child;
  public:
    explicit EggParserNode_Block(const egg::lang::LocationSource& locationSource)
      : EggParserNodeBase(locationSource) {
    }
    virtual EggProgramNodeFlags prepare(EggProgramContext& context) override {
      return context.prepareBlock(this->child);
    }
    virtual egg::lang::Value execute(EggProgramContext& context) const override {
      return context.executeBlock(*this, this->child);
    }
    virtual void dump(std::ostream& os) const override {
      ParserDump(os, "block").add(this->child);
    }
    void addChild(const std::shared_ptr<IEggProgramNode>& statement) {
      assert(statement != nullptr);
      this->child.push_back(statement);
    }
  };

  class EggParserNode_Type : public EggParserNodeBase {
  private:
    egg::lang::ITypeRef type;
  public:
    EggParserNode_Type(const egg::lang::LocationSource& locationSource, const egg::lang::ITypeRef& type)
      : EggParserNodeBase(locationSource), type(type) {
    }
    virtual egg::lang::ITypeRef getType() const override {
      return this->type;
    }
    virtual EggProgramNodeFlags prepare(EggProgramContext&) override {
      // Nothing to do: we're just a holder for the type
      return EggProgramNodeFlags::None;
    }
    virtual egg::lang::Value execute(EggProgramContext& context) const override {
      return context.raiseFormat("Internal parser error: Inappropriate 'execute' call for 'type' node");
    }
    virtual void dump(std::ostream& os) const override {
      ParserDump(os, "type").add(this->type->toString());
    }
  };

  class EggParserNode_Declare : public EggParserNodeBase {
  private:
    egg::lang::String name;
    egg::lang::ITypeRef type;
    std::shared_ptr<IEggProgramNode> init;
  public:
    EggParserNode_Declare(const egg::lang::LocationSource& locationSource, const egg::lang::String& name, const egg::lang::ITypeRef& type, const std::shared_ptr<IEggProgramNode>& init = nullptr)
      : EggParserNodeBase(locationSource), name(name), type(type), init(init) {
    }
    virtual bool symbol(egg::lang::String& nameOut, egg::lang::ITypeRef& typeOut) const override {
      // The symbol is obviously the variable being declared
      nameOut = this->name;
      typeOut = this->type;
      return true;
    }
    virtual std::unique_ptr<IEggProgramAssignee> assignee(EggProgramContext& context) const override {
      // The assignee is just the variable in the scope
      return context.assigneeIdentifier(*this, this->name);
    }
    virtual EggProgramNodeFlags prepare(EggProgramContext& context) override {
      return context.prepareDeclare(this->locationSource, this->name, this->type, this->init.get());
    }
    virtual egg::lang::Value execute(EggProgramContext& context) const override {
      return context.executeDeclare(*this, this->name, this->type, this->init.get());
    }
    virtual void dump(std::ostream& os) const override {
      ParserDump(os, "declare").add(this->name).add(this->type->toString()).add(this->init);
    }
  };

  class EggParserNode_Guard : public EggParserNodeBase {
  private:
    egg::lang::String name;
    egg::lang::ITypeRef type;
    std::shared_ptr<IEggProgramNode> expr;
  public:
    EggParserNode_Guard(const egg::lang::LocationSource& locationSource, const egg::lang::String& name, const egg::lang::ITypeRef& type, const std::shared_ptr<IEggProgramNode>& expr)
      : EggParserNodeBase(locationSource), name(name), type(type), expr(expr) {
    }
    virtual bool symbol(egg::lang::String& nameOut, egg::lang::ITypeRef& typeOut) const override {
      // The symbol is obviously the variable being declared
      nameOut = this->name;
      typeOut = this->type;
      return true;
    }
    virtual std::unique_ptr<IEggProgramAssignee> assignee(EggProgramContext& context) const override {
      // The assignee is just the variable in the scope
      return context.assigneeIdentifier(*this, this->name);
    }
    virtual EggProgramNodeFlags prepare(EggProgramContext& context) override {
      return context.prepareGuard(this->locationSource, this->name, this->type, *this->expr);
    }
    virtual egg::lang::Value execute(EggProgramContext& context) const override {
      return context.executeGuard(*this, this->name, this->type, *this->expr);
    }
    virtual void dump(std::ostream& os) const override {
      ParserDump(os, "guard").add(this->name).add(this->type->toString()).add(this->expr);
    }
  };

  class EggParserNode_Mutate : public EggParserNodeBase {
  private:
    EggProgramMutate op;
    std::shared_ptr<IEggProgramNode> lvalue;
  public:
    EggParserNode_Mutate(const egg::lang::LocationSource& locationSource, EggProgramMutate op, const std::shared_ptr<IEggProgramNode>& lvalue)
      : EggParserNodeBase(locationSource), op(op), lvalue(lvalue) {
      assert(lvalue != nullptr);
    }
    virtual EggProgramNodeFlags prepare(EggProgramContext& context) override {
      return context.prepareMutate(this->locationSource, this->op, *this->lvalue);
    }
    virtual egg::lang::Value execute(EggProgramContext& context) const override {
      return context.executeMutate(*this, this->op, *this->lvalue);
    }
    virtual void dump(std::ostream& os) const override {
      ParserDump(os, "mutate").add(EggProgram::mutateToString(this->op)).add(this->lvalue);
    }
  };

  class EggParserNode_Break : public EggParserNodeBase {
  public:
    explicit EggParserNode_Break(const egg::lang::LocationSource& locationSource)
      : EggParserNodeBase(locationSource) {
    }
    virtual EggProgramNodeFlags prepare(EggProgramContext&) override {
      // Nothing to prepare, but we fallthrough
      return EggProgramNodeFlags::Fallthrough;
    }
    virtual egg::lang::Value execute(EggProgramContext& context) const override {
      return context.executeBreak(*this);
    }
    virtual void dump(std::ostream& os) const override {
      ParserDump(os, "break");
    }
  };

  class EggParserNode_Catch : public EggParserNodeBase {
  private:
    egg::lang::String name;
    std::shared_ptr<IEggProgramNode> type;
    std::shared_ptr<IEggProgramNode> block;
  public:
    EggParserNode_Catch(const egg::lang::LocationSource& locationSource, const egg::lang::String& name, const std::shared_ptr<IEggProgramNode>& type, const std::shared_ptr<IEggProgramNode>& block)
      : EggParserNodeBase(locationSource), name(name), type(type), block(block) {
      assert(type != nullptr);
      assert(block != nullptr);
    }
    virtual EggProgramNodeFlags prepare(EggProgramContext& context) override {
      return context.prepareCatch(this->name, *this->type, *this->block);
    }
    virtual egg::lang::Value execute(EggProgramContext& context) const override {
      return context.executeCatch(*this, this->name, *this->type, *this->block);
    }
    virtual void dump(std::ostream& os) const override {
      ParserDump(os, "catch").add(this->name).add(this->type).add(this->block);
    }
  };

  class EggParserNode_Continue : public EggParserNodeBase {
  public:
    explicit EggParserNode_Continue(const egg::lang::LocationSource& locationSource)
      : EggParserNodeBase(locationSource) {
    }
    virtual EggProgramNodeFlags prepare(EggProgramContext&) override {
      // Nothing to prepare, but no fallthrough
      return EggProgramNodeFlags::None;
    }
    virtual egg::lang::Value execute(EggProgramContext& context) const override {
      return context.executeContinue(*this);
    }
    virtual void dump(std::ostream& os) const override {
      ParserDump(os, "continue");
    }
  };

  class EggParserNode_Do : public EggParserNodeBase {
  private:
    std::shared_ptr<IEggProgramNode> condition;
    std::shared_ptr<IEggProgramNode> block;
  public:
    EggParserNode_Do(const egg::lang::LocationSource& locationSource, const std::shared_ptr<IEggProgramNode>& condition, const std::shared_ptr<IEggProgramNode>& block)
      : EggParserNodeBase(locationSource), condition(condition), block(block) {
      assert(condition != nullptr);
      assert(block != nullptr);
    }
    virtual EggProgramNodeFlags prepare(EggProgramContext& context) override {
      return context.prepareDo(*this->condition, *this->block);
    }
    virtual egg::lang::Value execute(EggProgramContext& context) const override {
      return context.executeDo(*this, *this->condition, *this->block);
    }
    virtual void dump(std::ostream& os) const override {
      ParserDump(os, "do").add(this->condition).add(this->block);
    }
  };

  class EggParserNode_If : public EggParserNodeBase {
  private:
    std::shared_ptr<IEggProgramNode> condition;
    std::shared_ptr<IEggProgramNode> trueBlock;
    std::shared_ptr<IEggProgramNode> falseBlock;
  public:
    EggParserNode_If(const egg::lang::LocationSource& locationSource, const std::shared_ptr<IEggProgramNode>& condition, const std::shared_ptr<IEggProgramNode>& trueBlock, const std::shared_ptr<IEggProgramNode>& falseBlock)
      : EggParserNodeBase(locationSource), condition(condition), trueBlock(trueBlock), falseBlock(falseBlock) {
      assert(condition != nullptr);
      assert(trueBlock != nullptr);
      // falseBlock may be null if the 'else' clause is missing
    }
    virtual EggProgramNodeFlags prepare(EggProgramContext& context) override {
      return context.prepareIf(*this->condition, *this->trueBlock, this->falseBlock.get());
    }
    virtual egg::lang::Value execute(EggProgramContext& context) const override {
      return context.executeIf(*this, *this->condition, *this->trueBlock, this->falseBlock.get());
    }
    virtual void dump(std::ostream& os) const override {
      ParserDump(os, "if").add(this->condition).add(this->trueBlock).add(this->falseBlock);
    }
  };

  class EggParserNode_For : public EggParserNodeBase {
  private:
    std::shared_ptr<IEggProgramNode> pre;
    std::shared_ptr<IEggProgramNode> cond;
    std::shared_ptr<IEggProgramNode> post;
    std::shared_ptr<IEggProgramNode> block;
  public:
    EggParserNode_For(const egg::lang::LocationSource& locationSource, const std::shared_ptr<IEggProgramNode>& pre, const std::shared_ptr<IEggProgramNode>& cond, const std::shared_ptr<IEggProgramNode>& post, const std::shared_ptr<IEggProgramNode>& block)
      : EggParserNodeBase(locationSource), pre(pre), cond(cond), post(post), block(block) {
      // pre/cond/post may be null
      assert(block != nullptr);
    }
    virtual EggProgramNodeFlags prepare(EggProgramContext& context) override {
      return context.prepareFor(this->pre.get(), this->cond.get(), this->post.get(), *this->block);
    }
    virtual egg::lang::Value execute(EggProgramContext& context) const override {
      return context.executeFor(*this, this->pre.get(), this->cond.get(), this->post.get(), *this->block);
    }
    virtual void dump(std::ostream& os) const override {
      ParserDump(os, "for").add(this->pre).add(this->cond).add(this->post).add(this->block);
    }
  };

  class EggParserNode_Foreach : public EggParserNodeBase {
  private:
    std::shared_ptr<IEggProgramNode> target;
    std::shared_ptr<IEggProgramNode> expr;
    std::shared_ptr<IEggProgramNode> block;
  public:
    EggParserNode_Foreach(const egg::lang::LocationSource& locationSource, const std::shared_ptr<IEggProgramNode>& target, const std::shared_ptr<IEggProgramNode>& expr, const std::shared_ptr<IEggProgramNode>& block)
      : EggParserNodeBase(locationSource), target(target), expr(expr), block(block) {
      assert(target != nullptr);
      assert(expr != nullptr);
      assert(block != nullptr);
    }
    virtual EggProgramNodeFlags prepare(EggProgramContext& context) override {
      return context.prepareForeach(*this->target, *this->expr, *this->block);
    }
    virtual egg::lang::Value execute(EggProgramContext& context) const override {
      return context.executeForeach(*this, *this->target, *this->expr, *this->block);
    }
    virtual void dump(std::ostream& os) const override {
      ParserDump(os, "foreach").add(this->target).add(this->expr).add(this->block);
    }
  };

  class EggParserNode_FunctionDefinition : public EggParserNodeBase {
  private:
    egg::lang::String name;
    egg::lang::ITypeRef type;
    std::shared_ptr<IEggProgramNode> block;
  public:
    EggParserNode_FunctionDefinition(const egg::lang::LocationSource& locationSource, const egg::lang::String& name, const egg::lang::ITypeRef& type, const std::shared_ptr<IEggProgramNode>& block)
      : EggParserNodeBase(locationSource), name(name), type(type), block(block) {
      assert(block != nullptr);
    }
    virtual bool symbol(egg::lang::String& nameOut, egg::lang::ITypeRef& typeOut) const override {
      // The symbol is obviously the identifier being defined
      nameOut = this->name;
      typeOut = this->type;
      return true;
    }
    virtual EggProgramNodeFlags prepare(EggProgramContext& context) override {
      return context.prepareFunctionDefinition(this->name, this->type, this->block);
    }
    virtual egg::lang::Value execute(EggProgramContext& context) const override {
      return context.executeFunctionDefinition(*this, this->name, this->type, this->block);
    }
    virtual void dump(std::ostream& os) const override {
      ParserDump(os, "function").add(this->name).add(this->type->toString()).add(this->block);
    }
  };

  class EggParserNode_FunctionParameter : public EggParserNodeBase {
  private:
    egg::lang::String name;
    egg::lang::ITypeRef type;
    bool optional;
  public:
    EggParserNode_FunctionParameter(const egg::lang::LocationSource& locationSource, const egg::lang::String& name, const egg::lang::ITypeRef& type, bool optional)
      : EggParserNodeBase(locationSource), name(name), type(type), optional(optional) {
    }
    virtual bool symbol(egg::lang::String& nameOut, egg::lang::ITypeRef& typeOut) const override {
      // Beware: the return value is the optionality flag!
      nameOut = this->name;
      typeOut = this->type;
      return this->optional;
    }
    virtual EggProgramNodeFlags prepare(EggProgramContext& context) override {
      return context.compilerError(this->locationSource, "Internal parser error: Inappropriate 'prepare' call for function parameter");
    }
    virtual egg::lang::Value execute(EggProgramContext& context) const override {
      return context.raiseFormat("Internal parser error: Inappropriate 'execute' call for function parameter");
    }
    virtual void dump(std::ostream& os) const override {
      ParserDump(os, this->optional ? "parameter?" : "parameter").add(this->name).add(this->type->toString());
    }
  };

  class EggParserNode_Return : public EggParserNodeBase {
  private:
    std::shared_ptr<IEggProgramNode> expr;
  public:
    EggParserNode_Return(const egg::lang::LocationSource& locationSource, const std::shared_ptr<IEggProgramNode>& expr = nullptr)
      : EggParserNodeBase(locationSource), expr(expr) {
    }
    virtual EggProgramNodeFlags prepare(EggProgramContext& context) override {
      return context.prepareReturn(this->locationSource, this->expr.get());
    }
    virtual egg::lang::Value execute(EggProgramContext& context) const override {
      return context.executeReturn(*this, this->expr.get());
    }
    virtual void dump(std::ostream& os) const override {
      ParserDump(os, "return").add(this->expr);
    }
  };

  class EggParserNode_Case : public EggParserNodeBase {
  private:
    std::vector<std::shared_ptr<IEggProgramNode>> child;
    std::shared_ptr<EggParserNode_Block> block;
  public:
    explicit EggParserNode_Case(const egg::lang::LocationSource& locationSource)
      : EggParserNodeBase(locationSource), block(std::make_shared<EggParserNode_Block>(locationSource)) {
      assert(this->block != nullptr);
    }
    virtual EggProgramNodeFlags prepare(EggProgramContext& context) override {
      return context.prepareCase(this->child, *this->block);
    }
    virtual egg::lang::Value execute(EggProgramContext& context) const override {
      return context.executeCase(*this, this->child, *this->block);
    }
    virtual void dump(std::ostream& os) const override {
      ParserDump(os, "case").add(this->child).add(this->block);
    }
    void addValue(const std::shared_ptr<IEggProgramNode>& value) {
      assert(value != nullptr);
      this->child.push_back(value);
    }
    void addStatement(const std::shared_ptr<IEggProgramNode>& statement) {
      assert(statement != nullptr);
      this->block->addChild(statement);
    }
  };

  class EggParserNode_Switch : public EggParserNodeBase {
  private:
    std::shared_ptr<IEggProgramNode> expr;
    int64_t defaultIndex;
    std::vector<std::shared_ptr<IEggProgramNode>> child;
    std::shared_ptr<EggParserNode_Case> latest;
  public:
    EggParserNode_Switch(const egg::lang::LocationSource& locationSource, const std::shared_ptr<IEggProgramNode>& expr)
      : EggParserNodeBase(locationSource), expr(expr), defaultIndex(-1) {
      assert(expr != nullptr);
    }
    virtual EggProgramNodeFlags prepare(EggProgramContext& context) override {
      return context.prepareSwitch(*this->expr, this->defaultIndex, this->child);
    }
    virtual egg::lang::Value execute(EggProgramContext& context) const override {
      return context.executeSwitch(*this, *this->expr, this->defaultIndex, this->child);
    }
    virtual void dump(std::ostream& os) const override {
      ParserDump(os, "switch").add(this->expr).raw(this->defaultIndex).add(this->child);
    }
    void newClause(const IEggParserContext& context, const IEggSyntaxNode& statement) {
      this->latest = makeParserNode<EggParserNode_Case>(context, statement);
      this->child.push_back(latest);
    }
    bool addCase(const std::shared_ptr<IEggProgramNode>& value) {
      // TODO: Check for duplicate case values
      if (this->latest == nullptr) {
        return false;
      }
      this->latest->addValue(value);
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
    bool addStatement(const std::shared_ptr<IEggProgramNode>& statement) {
      if (this->latest == nullptr) {
        return false;
      }
      this->latest->addStatement(statement);
      return true;
    }
  };

  class EggParserNode_Throw : public EggParserNodeBase {
  private:
    std::shared_ptr<IEggProgramNode> expr;
  public:
    explicit EggParserNode_Throw(const egg::lang::LocationSource& locationSource, const std::shared_ptr<IEggProgramNode>& expr = nullptr)
      : EggParserNodeBase(locationSource), expr(expr) {
    }
    virtual EggProgramNodeFlags prepare(EggProgramContext& context) override {
      return context.prepareThrow(this->expr.get());
    }
    virtual egg::lang::Value execute(EggProgramContext& context) const override {
      return context.executeThrow(*this, this->expr.get());
    }
    virtual void dump(std::ostream& os) const override {
      ParserDump(os, "throw").add(this->expr);
    }
  };

  class EggParserNode_Try : public EggParserNodeBase {
  private:
    std::shared_ptr<IEggProgramNode> block;
    std::vector<std::shared_ptr<IEggProgramNode>> catches;
    std::shared_ptr<IEggProgramNode> final;
  public:
    EggParserNode_Try(const egg::lang::LocationSource& locationSource, const std::shared_ptr<IEggProgramNode>& block)
      : EggParserNodeBase(locationSource), block(block) {
      assert(block != nullptr);
    }
    virtual EggProgramNodeFlags prepare(EggProgramContext& context) override {
      return context.prepareTry(*this->block, this->catches, this->final.get());
    }
    virtual egg::lang::Value execute(EggProgramContext& context) const override {
      return context.executeTry(*this, *this->block, this->catches, this->final.get());
    }
    virtual void dump(std::ostream& os) const override {
      ParserDump(os, "try").add(this->block).add(this->catches).add(this->final);
    }
    void addCatch(const std::shared_ptr<IEggProgramNode>& catchNode) {
      this->catches.push_back(catchNode);
    }
    void addFinally(const std::shared_ptr<IEggProgramNode>& finallyNode) {
      assert(this->final == nullptr);
      this->final = finallyNode;
    }
  };

  class EggParserNode_While : public EggParserNodeBase {
  private:
    std::shared_ptr<IEggProgramNode> condition;
    std::shared_ptr<IEggProgramNode> block;
  public:
    EggParserNode_While(const egg::lang::LocationSource& locationSource, const std::shared_ptr<IEggProgramNode>& condition, const std::shared_ptr<IEggProgramNode>& block)
      : EggParserNodeBase(locationSource), condition(condition), block(block) {
      assert(condition != nullptr);
      assert(block != nullptr);
    }
    virtual EggProgramNodeFlags prepare(EggProgramContext& context) override {
      return context.prepareWhile(*this->condition, *this->block);
    }
    virtual egg::lang::Value execute(EggProgramContext& context) const override {
      return context.executeWhile(*this, *this->condition, *this->block);
    }
    virtual void dump(std::ostream& os) const override {
      ParserDump(os, "while").add(this->condition).add(this->block);
    }
  };

  class EggParserNode_Yield : public EggParserNodeBase {
  private:
    std::shared_ptr<IEggProgramNode> expr;
  public:
    EggParserNode_Yield(const egg::lang::LocationSource& locationSource, const std::shared_ptr<IEggProgramNode>& expr)
      : EggParserNodeBase(locationSource), expr(expr) {
      assert(expr != nullptr);
    }
    virtual EggProgramNodeFlags prepare(EggProgramContext& context) override {
      return context.prepareYield(*this->expr);
    }
    virtual egg::lang::Value execute(EggProgramContext& context) const override {
      return context.executeYield(*this, *this->expr);
    }
    virtual void dump(std::ostream& os) const override {
      ParserDump(os, "yield").add(this->expr);
    }
  };

  class EggParserNode_Named : public EggParserNodeBase {
  private:
    egg::lang::String name;
    std::shared_ptr<IEggProgramNode> expr;
  public:
    EggParserNode_Named(const egg::lang::LocationSource& locationSource, const egg::lang::String& name, const std::shared_ptr<IEggProgramNode>& expr)
      : EggParserNodeBase(locationSource), name(name), expr(expr) {
      assert(expr != nullptr);
    }
    virtual bool symbol(egg::lang::String& nameOut, egg::lang::ITypeRef& typeOut) const override {
      // Use the symbol to extract the parameter name
      nameOut = this->name;
      typeOut = this->expr->getType();
      return true;
    }
    virtual EggProgramNodeFlags prepare(EggProgramContext& context) override {
      return this->expr->prepare(context);
    }
    virtual egg::lang::Value execute(EggProgramContext& context) const override {
      // The name has already been extracted via 'symbol' so just propagate the value
      return this->expr->execute(context);
    }
    virtual void dump(std::ostream& os) const override {
      ParserDump(os, "named").add(this->name).add(this->expr);
    }
  };

  class EggParserNode_Array : public EggParserNodeBase {
  private:
    std::vector<std::shared_ptr<IEggProgramNode>> child;
  public:
    explicit EggParserNode_Array(const egg::lang::LocationSource& locationSource)
      : EggParserNodeBase(locationSource) {
    }
    virtual egg::lang::ITypeRef getType() const override {
      return egg::lang::ITypeRef(&EggProgram::VanillaArray);
    }
    virtual EggProgramNodeFlags prepare(EggProgramContext& context) override {
      return context.prepareArray(this->child);
    }
    virtual egg::lang::Value execute(EggProgramContext& context) const override {
      return context.executeArray(*this, this->child);
    }
    virtual void dump(std::ostream& os) const override {
      ParserDump(os, "array").add(this->child);
    }
    void addValue(const std::shared_ptr<IEggProgramNode>& value) {
      this->child.emplace_back(value);
    }
  };

  class EggParserNode_Object : public EggParserNodeBase {
  private:
    std::vector<std::shared_ptr<IEggProgramNode>> child; // All EggParserNode_Named instances in order
  public:
    explicit EggParserNode_Object(const egg::lang::LocationSource& locationSource)
      : EggParserNodeBase(locationSource) {
    }
    virtual egg::lang::ITypeRef getType() const override {
      return egg::lang::ITypeRef(&EggProgram::VanillaObject);
    }
    virtual EggProgramNodeFlags prepare(EggProgramContext& context) override {
      return context.prepareObject(this->child);
    }
    virtual egg::lang::Value execute(EggProgramContext& context) const override {
      return context.executeObject(*this, this->child);
    }
    virtual void dump(std::ostream& os) const override {
      ParserDump(os, "object").add(this->child);
    }
    void addValue(const std::shared_ptr<IEggProgramNode>& value) {
      this->child.emplace_back(value);
    }
  };

  class EggParserNode_Call : public EggParserNodeBase {
  private:
    std::shared_ptr<IEggProgramNode> callee;
    std::vector<std::shared_ptr<IEggProgramNode>> child;
  public:
    EggParserNode_Call(const egg::lang::LocationSource& locationSource, const std::shared_ptr<IEggProgramNode>& callee)
      : EggParserNodeBase(locationSource), callee(callee) {
      assert(callee != nullptr);
    }
    virtual egg::lang::ITypeRef getType() const override {
      // Get this from the function signature, if possible
      auto* signature = this->callee->getType()->callable();
      if (signature == nullptr) {
        return egg::lang::Type::Void;
      }
      return signature->getReturnType();
    }
    virtual EggProgramNodeFlags prepare(EggProgramContext& context) override {
      return context.prepareCall(*this->callee, this->child);
    }
    virtual egg::lang::Value execute(EggProgramContext& context) const override {
      return context.executeCall(*this, *this->callee, this->child);
    }
    virtual void dump(std::ostream& os) const override {
      ParserDump(os, "call").add(this->callee).add(this->child);
    }
    void addParameter(const std::shared_ptr<IEggProgramNode>& parameter) {
      this->child.emplace_back(parameter);
    }
  };

  class EggParserNode_Identifier : public EggParserNodeBase {
  private:
    egg::lang::String name;
    egg::lang::ITypeRef type; // Initially 'Void' because we don't know until we're prepared
    bool byref;
  public:
    EggParserNode_Identifier(const egg::lang::LocationSource& locationSource, const egg::lang::String& name)
      : EggParserNodeBase(locationSource), name(name), type(egg::lang::Type::Void), byref(false) {
    }
    virtual egg::lang::ITypeRef getType() const override {
      return this->type;
    }
    virtual EggProgramNodeFlags prepare(EggProgramContext& context) override {
      return context.prepareIdentifier(this->locationSource, this->name, this->type);
    }
    virtual EggProgramNodeFlags addressable(EggProgramContext&) override {
      // Identifiers are always addressable
      this->byref = true;
      return EggProgramNodeFlags::None;
    }
    virtual egg::lang::Value execute(EggProgramContext& context) const override {
      return context.executeIdentifier(*this, this->name, this->byref);
    }
    virtual std::unique_ptr<IEggProgramAssignee> assignee(EggProgramContext& context) const override {
      return context.assigneeIdentifier(*this, this->name);
    }
    virtual void dump(std::ostream& os) const override {
      ParserDump(os, "identifier").add(this->name);
    }
  };

  class EggParserNode_LiteralNull : public EggParserNodeBase {
  public:
    explicit EggParserNode_LiteralNull(const egg::lang::LocationSource& locationSource)
      : EggParserNodeBase(locationSource) {
    }
    virtual egg::lang::ITypeRef getType() const override {
      return egg::lang::Type::Null;
    }
    virtual EggProgramNodeFlags prepare(EggProgramContext&) override {
      return EggProgramNodeFlags::Constant;
    }
    virtual egg::lang::Value execute(EggProgramContext& context) const override {
      return context.executeLiteral(*this, egg::lang::Value::Null);
    }
    virtual void dump(std::ostream& os) const override {
      ParserDump(os, "literal null");
    }
  };

  class EggParserNode_LiteralBool : public EggParserNodeBase {
  private:
    bool value;
  public:
    EggParserNode_LiteralBool(const egg::lang::LocationSource& locationSource, bool value)
      : EggParserNodeBase(locationSource), value(value) {
    }
    virtual egg::lang::ITypeRef getType() const override {
      return egg::lang::Type::Bool;
    }
    virtual EggProgramNodeFlags prepare(EggProgramContext&) override {
      return EggProgramNodeFlags::Constant;
    }
    virtual egg::lang::Value execute(EggProgramContext& context) const override {
      return context.executeLiteral(*this, egg::lang::Value(this->value));
    }
    virtual void dump(std::ostream& os) const override {
      ParserDump(os, "literal bool").raw(this->value);
    }
  };

  class EggParserNode_LiteralInteger : public EggParserNodeBase {
  private:
    int64_t value;
  public:
    EggParserNode_LiteralInteger(const egg::lang::LocationSource& locationSource, int64_t value)
      : EggParserNodeBase(locationSource), value(value) {
    }
    virtual egg::lang::ITypeRef getType() const override {
      return egg::lang::Type::Int;
    }
    virtual EggProgramNodeFlags prepare(EggProgramContext&) override {
      return EggProgramNodeFlags::Constant;
    }
    virtual egg::lang::Value execute(EggProgramContext& context) const override {
      return context.executeLiteral(*this, egg::lang::Value(this->value));
    }
    virtual void dump(std::ostream& os) const override {
      ParserDump(os, "literal int").raw(this->value);
    }
  };

  class EggParserNode_LiteralFloat : public EggParserNodeBase {
  private:
    double value;
  public:
    EggParserNode_LiteralFloat(const egg::lang::LocationSource& locationSource, double value)
      : EggParserNodeBase(locationSource), value(value) {
    }
    virtual egg::lang::ITypeRef getType() const override {
      return egg::lang::Type::Float;
    }
    virtual EggProgramNodeFlags prepare(EggProgramContext&) override {
      return EggProgramNodeFlags::Constant;
    }
    virtual egg::lang::Value execute(EggProgramContext& context) const override {
      return context.executeLiteral(*this, egg::lang::Value(this->value));
    }
    virtual void dump(std::ostream& os) const override {
      ParserDump(os, "literal float").raw(this->value);
    }
  };

  class EggParserNode_LiteralString : public EggParserNodeBase {
  private:
    egg::lang::String value;
  public:
    EggParserNode_LiteralString(const egg::lang::LocationSource& locationSource, const egg::lang::String& value)
      : EggParserNodeBase(locationSource), value(value) {
    }
    virtual egg::lang::ITypeRef getType() const override {
      return egg::lang::Type::String;
    }
    virtual EggProgramNodeFlags prepare(EggProgramContext&) override {
      return EggProgramNodeFlags::Constant;
    }
    virtual egg::lang::Value execute(EggProgramContext& context) const override {
      return context.executeLiteral(*this, egg::lang::Value(this->value));
    }
    virtual void dump(std::ostream& os) const override {
      ParserDump(os, "literal string").add(this->value);
    }
  };

  class EggParserNode_Brackets : public EggParserNodeBase {
  protected:
    std::shared_ptr<IEggProgramNode> lhs;
    std::shared_ptr<IEggProgramNode> rhs;
  public:
    EggParserNode_Brackets(const egg::lang::LocationSource& locationSource, const std::shared_ptr<IEggProgramNode>& lhs, const std::shared_ptr<IEggProgramNode>& rhs)
      : EggParserNodeBase(locationSource), lhs(lhs), rhs(rhs) {
      assert(lhs != nullptr);
      assert(rhs != nullptr);
    }
    virtual egg::lang::ITypeRef getType() const override;
    virtual EggProgramNodeFlags prepare(EggProgramContext& context) override {
      return context.prepareBrackets(this->locationSource, *this->lhs, *this->rhs);
    }
    virtual egg::lang::Value execute(EggProgramContext& context) const override {
      return context.executeBrackets(*this, *this->lhs, *this->rhs);
    }
    virtual std::unique_ptr<IEggProgramAssignee> assignee(EggProgramContext& context) const override {
      // Something like "obj[index] += value;"
      return context.assigneeBrackets(*this, this->lhs, this->rhs);
    }
    virtual void dump(std::ostream& os) const override {
      ParserDump(os, "brackets").add(this->lhs).add(this->rhs);
    }
  };

  class EggParserNode_Dot : public EggParserNodeBase {
  protected:
    std::shared_ptr<IEggProgramNode> lhs;
    egg::lang::String rhs;
  public:
    EggParserNode_Dot(const egg::lang::LocationSource& locationSource, const std::shared_ptr<IEggProgramNode>& lhs, const egg::lang::String& rhs)
      : EggParserNodeBase(locationSource), lhs(lhs), rhs(rhs) {
      assert(lhs != nullptr);
    }
    virtual EggProgramNodeFlags prepare(EggProgramContext& context) override {
      return context.prepareDot(this->locationSource, *this->lhs, this->rhs);
    }
    virtual egg::lang::ITypeRef getType() const override;
    virtual egg::lang::Value execute(EggProgramContext& context) const override {
      return context.executeDot(*this, *this->lhs, this->rhs);
    }
    virtual std::unique_ptr<IEggProgramAssignee> assignee(EggProgramContext& context) const override {
      // Something like "obj.prop += value;"
      return context.assigneeDot(*this, this->lhs, this->rhs);
    }
    virtual void dump(std::ostream& os) const override {
      ParserDump(os, "dot").add(this->lhs).add(this->rhs);
    }
  };

  class EggParserNode_Unary : public EggParserNodeBase {
  protected:
    EggProgramUnary op;
    std::shared_ptr<IEggProgramNode> expr;
    EggParserNode_Unary(const egg::lang::LocationSource& locationSource, EggProgramUnary op, const std::shared_ptr<IEggProgramNode>& expr)
      : EggParserNodeBase(locationSource), op(op), expr(expr) {
      assert(expr != nullptr);
    }
  public:
    virtual EggProgramNodeFlags prepare(EggProgramContext& context) override {
      return context.prepareUnary(this->locationSource, this->op, *this->expr);
    }
    virtual egg::lang::Value execute(EggProgramContext& context) const override {
      return context.executeUnary(*this, this->op, *this->expr);
    }
    virtual std::unique_ptr<IEggProgramAssignee> assignee(EggProgramContext& context) const override {
      if (op == EggProgramUnary::Deref) {
        // Something like "*ptr += value;"
        return context.assigneeDeref(*this, this->expr);
      }
      return nullptr;
    }
    virtual void dump(std::ostream& os) const override {
      ParserDump(os, "unary").add(EggProgram::unaryToString(this->op)).add(this->expr);
    }
  };

#define EGG_PARSER_UNARY_OPERATOR_DEFINE(name, text) \
  class EggParserNode_Unary##name : public EggParserNode_Unary { \
  public: \
    EggParserNode_Unary##name(const egg::lang::LocationSource& locationSource, const std::shared_ptr<IEggProgramNode>& expr) \
      : EggParserNode_Unary(locationSource, EggProgramUnary::name, expr) { \
    } \
    virtual egg::lang::ITypeRef getType() const override; \
  };
  EGG_PROGRAM_UNARY_OPERATORS(EGG_PARSER_UNARY_OPERATOR_DEFINE)

  class EggParserNode_Binary : public EggParserNodeBase {
  protected:
    EggProgramBinary op;
    std::shared_ptr<IEggProgramNode> lhs;
    std::shared_ptr<IEggProgramNode> rhs;
    EggParserNode_Binary(const egg::lang::LocationSource& locationSource, EggProgramBinary op, const std::shared_ptr<IEggProgramNode>& lhs, const std::shared_ptr<IEggProgramNode>& rhs)
      : EggParserNodeBase(locationSource), op(op), lhs(lhs), rhs(rhs) {
      assert(lhs != nullptr);
      assert(rhs != nullptr);
    }
  public:
    virtual void empredicate(EggProgramContext& context, std::shared_ptr<IEggProgramNode>& node) override {
      node = context.empredicateBinary(node, this->op, this->lhs, this->rhs);
    }
    virtual EggProgramNodeFlags prepare(EggProgramContext& context) override {
      return context.prepareBinary(this->locationSource, this->op, *this->lhs, *this->rhs);
    }
    virtual egg::lang::Value execute(EggProgramContext& context) const override {
      return context.executeBinary(*this, this->op, *this->lhs, *this->rhs);
    }
    virtual void dump(std::ostream& os) const override {
      ParserDump(os, "binary").add(EggProgram::binaryToString(this->op)).add(this->lhs).add(this->rhs);
    }
  };

#define EGG_PARSER_BINARY_OPERATOR_DEFINE(name, text) \
  class EggParserNode_Binary##name : public EggParserNode_Binary { \
  public: \
    EggParserNode_Binary##name(const egg::lang::LocationSource& locationSource, const std::shared_ptr<IEggProgramNode>& lhs, const std::shared_ptr<IEggProgramNode>& rhs) \
      : EggParserNode_Binary(locationSource, EggProgramBinary::name, lhs, rhs) { \
    } \
    virtual egg::lang::ITypeRef getType() const override; \
  };
  EGG_PROGRAM_BINARY_OPERATORS(EGG_PARSER_BINARY_OPERATOR_DEFINE)

  class EggParserNode_Ternary : public EggParserNodeBase {
  private:
    std::shared_ptr<IEggProgramNode> condition;
    std::shared_ptr<IEggProgramNode> whenTrue;
    std::shared_ptr<IEggProgramNode> whenFalse;
  public:
    EggParserNode_Ternary(const egg::lang::LocationSource& locationSource, const std::shared_ptr<IEggProgramNode>& condition, const std::shared_ptr<IEggProgramNode>& whenTrue, const std::shared_ptr<IEggProgramNode>& whenFalse)
      : EggParserNodeBase(locationSource), condition(condition), whenTrue(whenTrue), whenFalse(whenFalse) {
      assert(condition != nullptr);
      assert(whenTrue != nullptr);
      assert(whenFalse != nullptr);
    }
    virtual EggProgramNodeFlags prepare(EggProgramContext& context) override {
      return context.prepareTernary(this->locationSource, *this->condition, *this->whenTrue, *this->whenFalse);
    }
    virtual egg::lang::Value execute(EggProgramContext& context) const override {
      return context.executeTernary(*this, *this->condition, *this->whenTrue, *this->whenFalse);
    }
    virtual void dump(std::ostream& os) const override {
      ParserDump(os, "ternary").add(this->condition).add(this->whenTrue).add(this->whenFalse);
    }
    virtual egg::lang::ITypeRef getType() const override;
  };

  class EggParserNode_Assign : public EggParserNodeBase {
  protected:
    EggProgramAssign op;
    std::shared_ptr<IEggProgramNode> lhs;
    std::shared_ptr<IEggProgramNode> rhs;
    EggParserNode_Assign(const egg::lang::LocationSource& locationSource, EggProgramAssign op, const std::shared_ptr<IEggProgramNode>& lhs, const std::shared_ptr<IEggProgramNode>& rhs)
      : EggParserNodeBase(locationSource), op(op), lhs(lhs), rhs(rhs) {
      assert(lhs != nullptr);
      assert(rhs != nullptr);
    }
  public:
    virtual EggProgramNodeFlags prepare(EggProgramContext& context) override {
      return context.prepareAssign(this->locationSource, this->op, *this->lhs, *this->rhs);
    }
    virtual egg::lang::Value execute(EggProgramContext& context) const override {
      return context.executeAssign(*this, this->op, *this->lhs, *this->rhs);
    }
    virtual void dump(std::ostream& os) const override {
      ParserDump(os, "assign").add(EggProgram::assignToString(this->op)).add(this->lhs).add(this->rhs);
    }
  };

#define EGG_PARSER_ASSIGN_OPERATOR_DEFINE(name, text) \
  class EggParserNode_Assign##name : public EggParserNode_Assign { \
  public: \
    EggParserNode_Assign##name(const egg::lang::LocationSource& locationSource, const std::shared_ptr<IEggProgramNode>& lhs, const std::shared_ptr<IEggProgramNode>& rhs) \
      : EggParserNode_Assign(locationSource, EggProgramAssign::name, lhs, rhs) { \
    } \
  };
  EGG_PROGRAM_ASSIGN_OPERATORS(EGG_PARSER_ASSIGN_OPERATOR_DEFINE)

  class EggParserNode_Predicate : public EggParserNodeBase {
  private:
    EggProgramBinary op;
    std::shared_ptr<IEggProgramNode> lhs;
    std::shared_ptr<IEggProgramNode> rhs;
  public:
    EggParserNode_Predicate(const egg::lang::LocationSource& locationSource, EggProgramBinary op, const std::shared_ptr<IEggProgramNode>& lhs, const std::shared_ptr<IEggProgramNode>& rhs)
      : EggParserNodeBase(locationSource), op(op), lhs(lhs), rhs(rhs) {
      assert(lhs != nullptr);
      assert(rhs != nullptr);
    }
  public:
    virtual EggProgramNodeFlags prepare(EggProgramContext& context) override {
      return context.preparePredicate(this->locationSource, this->op, *this->lhs, *this->rhs);
    }
    virtual egg::lang::Value execute(EggProgramContext& context) const override {
      return context.executePredicate(*this, this->op, *this->lhs, *this->rhs);
    }
    virtual void dump(std::ostream& os) const override {
      ParserDump(os, "predicate ").add(EggProgram::binaryToString(this->op)).add(this->lhs).add(this->rhs);
    }
  };

  class EggParserContextBase : public IEggParserContext {
  private:
    EggParserAllowed allowed;
  public:
    explicit EggParserContextBase(EggParserAllowed allowed)
      : allowed(allowed) {
    }
    virtual bool isAllowed(EggParserAllowed bit) const override {
      return egg::lang::Bits::hasAnySet(this->allowed, bit);
    }
    virtual EggParserAllowed inheritAllowed(EggParserAllowed allow, EggParserAllowed inherit) const override {
      return egg::lang::Bits::mask(this->allowed, inherit) | allow;
    }
    virtual std::shared_ptr<IEggProgramNode> promote(const IEggSyntaxNode& node) override {
      return node.promote(*this);
    }
  };

  class EggParserContext : public EggParserContextBase {
  private:
    egg::lang::String resource;
  public:
    explicit EggParserContext(const egg::lang::String& resource, EggParserAllowed allowed = EggParserAllowed::None)
      : EggParserContextBase(allowed), resource(resource) {
    }
    virtual egg::lang::String getResourceName() const override {
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
    virtual egg::lang::String getResourceName() const override {
      return this->parent->getResourceName();
    }
  };

  class EggParserContextSwitch : public IEggParserContext {
  private:
    EggParserContextNested nested;
    std::shared_ptr<EggParserNode_Switch> promoted;
    EggTokenizerKeyword previous;
  public:
    EggParserContextSwitch(IEggParserContext& parent, const IEggSyntaxNode& switchStatement, const IEggSyntaxNode& switchExpression)
      : nested(parent, EggParserAllowed::Break|EggParserAllowed::Case|EggParserAllowed::Continue, EggParserAllowed::Rethrow|EggParserAllowed::Return|EggParserAllowed::Yield),
        promoted(makeParserNode<EggParserNode_Switch>(parent, switchStatement, parent.promote(switchExpression))),
        previous(EggTokenizerKeyword::Null) {
      assert(this->promoted != nullptr);
    }
    virtual egg::lang::String getResourceName() const override {
      return this->nested.getResourceName();
    }
    virtual bool isAllowed(EggParserAllowed bit) const override {
      return this->nested.isAllowed(bit);
    }
    virtual EggParserAllowed inheritAllowed(EggParserAllowed allow, EggParserAllowed inherit) const override {
      return this->nested.inheritAllowed(allow, inherit);
    }
    virtual std::shared_ptr<IEggProgramNode> promote(const IEggSyntaxNode& node) override {
      // This will be the block of the switch statement
      auto* children = node.children();
      if (children == nullptr) {
        EGG_THROW("Internal parser error: 'switch' statement context");
      }
      // Make sure we don't think we've seens any statements yet
      assert(this->previous == EggTokenizerKeyword::Null);
      for (auto& statement : *children) {
        auto keyword = statement->keyword();
        if (keyword == EggTokenizerKeyword::Case) {
          this->visitCase(*statement);
        } else if (keyword == EggTokenizerKeyword::Default) {
          this->visitDefault(*statement);
        } else {
          this->visitOther(*statement);
        }
        this->previous = keyword;
        assert(this->previous != EggTokenizerKeyword::Null);
      }
      // Check that the last thing in each clause was 'break' or 'continue' or some other flow control
      this->clauseEnd(node.location());
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
        this->promoted->newClause(*this, statement);
      }
    }
    void visitCase(const IEggSyntaxNode& statement) {
      // Promoting the 'case' statement produces the case expression
      this->visitLabel(statement);
      if (!this->promoted->addCase(this->nested.promote(statement))) {
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
      if (!this->promoted->addStatement(this->nested.promote(statement))) {
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
    virtual std::shared_ptr<IEggProgramNode> parse(IEggTokenizer& tokenizer) override {
      auto syntax = EggParserFactory::createModuleSyntaxParser();
      auto ast = syntax->parse(tokenizer);
      EggParserContext context(tokenizer.resource());
      return context.promote(*ast);
    }
  };

  class EggParserExpression : public IEggParser {
  public:
    virtual std::shared_ptr<IEggProgramNode> parse(IEggTokenizer& tokenizer) override {
      auto syntax = EggParserFactory::createExpressionSyntaxParser();
      auto ast = syntax->parse(tokenizer);
      EggParserContext context(tokenizer.resource());
      return context.promote(*ast);
    }
  };
}

std::shared_ptr<egg::yolk::IEggProgramNode> egg::yolk::EggSyntaxNode_Empty::promote(egg::yolk::IEggParserContext& context) const {
  if (!context.isAllowed(EggParserAllowed::Empty)) {
    throw exceptionFromLocation(context, "Empty statements are not permitted in this context", *this);
  }
  return nullptr;
}

std::shared_ptr<egg::yolk::IEggProgramNode> egg::yolk::EggSyntaxNode_Module::promote(egg::yolk::IEggParserContext& context) const {
  auto module = makeParserNode<EggParserNode_Module>(context, *this);
  for (auto& statement : this->child) {
    module->addChild(context.promote(*statement));
  }
  return module;
}

std::shared_ptr<egg::yolk::IEggProgramNode> egg::yolk::EggSyntaxNode_Block::promote(egg::yolk::IEggParserContext& context) const {
  auto module = makeParserNode<EggParserNode_Block>(context, *this);
  for (auto& statement : this->child) {
    module->addChild(context.promote(*statement));
  }
  return module;
}

std::shared_ptr<egg::yolk::IEggProgramNode> egg::yolk::EggSyntaxNode_Type::promote(egg::yolk::IEggParserContext& context) const {
  return makeParserNode<EggParserNode_Type>(context, *this, this->type);
}

std::shared_ptr<egg::yolk::IEggProgramNode> egg::yolk::EggSyntaxNode_Declare::promote(egg::yolk::IEggParserContext& context) const {
  auto type = context.promote(*this->child[0])->getType();
  if (this->child.size() == 1) {
    return makeParserNode<EggParserNode_Declare>(context, *this, this->name, type);
  }
  assert(this->child.size() == 2);
  return makeParserNode<EggParserNode_Declare>(context, *this, this->name, type, context.promote(*this->child[1]));
}

std::shared_ptr<egg::yolk::IEggProgramNode> egg::yolk::EggSyntaxNode_Guard::promote(egg::yolk::IEggParserContext& context) const {
  auto type = context.promote(*this->child[0])->getType();
  return makeParserNode<EggParserNode_Guard>(context, *this, this->name, type, context.promote(*this->child[1]));
}

std::shared_ptr<egg::yolk::IEggProgramNode> egg::yolk::EggSyntaxNode_Assignment::promote(egg::yolk::IEggParserContext& context) const {
  switch (this->op) {
    case EggTokenizerOperator::PercentEqual:
      return promoteBinary<EggParserNode_AssignRemainder>(context, *this, this->child);
    case EggTokenizerOperator::AmpersandEqual:
      return promoteBinary<EggParserNode_AssignBitwiseAnd>(context, *this, this->child);
    case EggTokenizerOperator::StarEqual:
      return promoteBinary<EggParserNode_AssignMultiply>(context, *this, this->child);
    case EggTokenizerOperator::PlusEqual:
      return promoteBinary<EggParserNode_AssignPlus>(context, *this, this->child);
    case EggTokenizerOperator::MinusEqual:
      return promoteBinary<EggParserNode_AssignMinus>(context, *this, this->child);
    case EggTokenizerOperator::SlashEqual:
      return promoteBinary<EggParserNode_AssignDivide>(context, *this, this->child);
    case EggTokenizerOperator::ShiftLeftEqual:
      return promoteBinary<EggParserNode_AssignShiftLeft>(context, *this, this->child);
    case EggTokenizerOperator::Equal:
      return promoteBinary<EggParserNode_AssignEqual>(context, *this, this->child);
    case EggTokenizerOperator::ShiftRightEqual:
      return promoteBinary<EggParserNode_AssignShiftRight>(context, *this, this->child);
    case EggTokenizerOperator::ShiftRightUnsignedEqual:
      return promoteBinary<EggParserNode_AssignShiftRightUnsigned>(context, *this, this->child);
    case EggTokenizerOperator::CaretEqual:
      return promoteBinary<EggParserNode_AssignBitwiseXor>(context, *this, this->child);
    case EggTokenizerOperator::BarEqual:
      return promoteBinary<EggParserNode_AssignBitwiseOr>(context, *this, this->child);
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

std::shared_ptr<egg::yolk::IEggProgramNode> egg::yolk::EggSyntaxNode_Mutate::promote(egg::yolk::IEggParserContext& context) const {
  EggProgramMutate mop;
  if (this->op == EggTokenizerOperator::PlusPlus) {
    mop = EggProgramMutate::Increment;
  } else if (this->op == EggTokenizerOperator::MinusMinus) {
    mop = EggProgramMutate::Decrement;
  } else {
    throw exceptionFromToken(context, "Unknown increment/decrement operator", *this);
  }
  return makeParserNode<EggParserNode_Mutate>(context, *this, mop, context.promote(*this->child));
}

std::shared_ptr<egg::yolk::IEggProgramNode> egg::yolk::EggSyntaxNode_Break::promote(egg::yolk::IEggParserContext& context) const {
  if (!context.isAllowed(EggParserAllowed::Break)) {
    throw exceptionFromLocation(context, "The 'break' statement may only be used within loops or switch statements", *this);
  }
  return makeParserNode<EggParserNode_Break>(context, *this);
}

std::shared_ptr<egg::yolk::IEggProgramNode> egg::yolk::EggSyntaxNode_Case::promote(egg::yolk::IEggParserContext& context) const {
  // The logic is handled by the 'switch' node, so just promote the value expression
  if (!context.isAllowed(EggParserAllowed::Case)) {
    throw exceptionFromLocation(context, "The 'case' statement may only be used within switch statements", *this);
  }
  return context.promote(*this->child);
}

std::shared_ptr<egg::yolk::IEggProgramNode> egg::yolk::EggSyntaxNode_Catch::promote(egg::yolk::IEggParserContext& context) const {
  auto type = context.promote(*this->child[0]);
  EggParserContextNested nested(context, EggParserAllowed::Rethrow|EggParserAllowed::Return|EggParserAllowed::Yield);
  auto block = context.promote(*this->child[1]);
  return makeParserNode<EggParserNode_Catch>(context, *this, this->name, type, block);
}

std::shared_ptr<egg::yolk::IEggProgramNode> egg::yolk::EggSyntaxNode_Continue::promote(egg::yolk::IEggParserContext& context) const {
  if (!context.isAllowed(EggParserAllowed::Continue)) {
    throw exceptionFromLocation(context, "The 'continue' statement may only be used within loops or switch statements", *this);
  }
  return makeParserNode<EggParserNode_Continue>(context, *this);
}

std::shared_ptr<egg::yolk::IEggProgramNode> egg::yolk::EggSyntaxNode_Default::promote(egg::yolk::IEggParserContext& context) const {
  // The logic is handled by the 'switch' node, so just assume it's a misplaced 'default'
  throw exceptionFromLocation(context, "The 'default' statement may only be used within switch statements", *this);
}

std::shared_ptr<egg::yolk::IEggProgramNode> egg::yolk::EggSyntaxNode_Do::promote(egg::yolk::IEggParserContext& context) const {
  auto condition = context.promote(*this->child[0]);
  EggParserContextNested nested(context, EggParserAllowed::Break|EggParserAllowed::Continue, EggParserAllowed::Rethrow|EggParserAllowed::Return|EggParserAllowed::Yield);
  auto block = context.promote(*this->child[1]);
  return makeParserNode<EggParserNode_Do>(context, *this, condition, block);
}

std::shared_ptr<egg::yolk::IEggProgramNode> egg::yolk::EggSyntaxNode_If::promote(egg::yolk::IEggParserContext& context) const {
  assert((this->child.size() == 2) || (this->child.size() == 3));
  auto condition = context.promote(*this->child[0]);
  auto trueBlock = context.promote(*this->child[1]);
  std::shared_ptr<egg::yolk::IEggProgramNode> falseBlock = nullptr;
  if (this->child.size() == 3) {
    falseBlock = context.promote(*this->child[2]);
  }
  return makeParserNode<EggParserNode_If>(context, *this, condition, trueBlock, falseBlock);
}

std::shared_ptr<egg::yolk::IEggProgramNode> egg::yolk::EggSyntaxNode_Finally::promote(egg::yolk::IEggParserContext& context) const {
  // The logic is handled by the 'try' node, so just assume it's a misplaced 'finally'
  throw exceptionFromLocation(context, "The 'finally' statement may only be used as part of a 'try' statement", *this);
}

std::shared_ptr<egg::yolk::IEggProgramNode> egg::yolk::EggSyntaxNode_For::promote(egg::yolk::IEggParserContext& context) const {
  // We allow empty statements but not flow control in the three 'for' clauses
  EggParserContextNested nested1(context, EggParserAllowed::Empty);
  auto pre = nested1.promote(*this->child[0]);
  auto cond = nested1.promote(*this->child[1]);
  auto post = nested1.promote(*this->child[2]);
  EggParserContextNested nested2(context, EggParserAllowed::Break|EggParserAllowed::Continue, EggParserAllowed::Rethrow|EggParserAllowed::Return|EggParserAllowed::Yield);
  auto block = nested2.promote(*this->child[3]);
  return makeParserNode<EggParserNode_For>(context, *this, pre, cond, post, block);
}

std::shared_ptr<egg::yolk::IEggProgramNode> egg::yolk::EggSyntaxNode_Foreach::promote(egg::yolk::IEggParserContext& context) const {
  auto target = context.promote(*this->child[0]);
  auto expr = context.promote(*this->child[1]);
  EggParserContextNested nested(context, EggParserAllowed::Break|EggParserAllowed::Continue, EggParserAllowed::Rethrow|EggParserAllowed::Return|EggParserAllowed::Yield);
  auto block = nested.promote(*this->child[2]);
  return makeParserNode<EggParserNode_Foreach>(context, *this, target, expr, block);
}

std::shared_ptr<egg::yolk::IEggProgramNode> egg::yolk::EggSyntaxNode_FunctionDefinition::promote(egg::yolk::IEggParserContext& context) const {
  // The children are: <type> <parameter>* <block>
  assert(this->child.size() >= 2);
  size_t parameters = this->child.size() - 2;
  auto rettype = context.promote(*this->child[0])->getType();
  auto* underlying = new egg::yolk::FunctionType(this->name, rettype);
  egg::lang::ITypeRef function{ underlying }; // takes ownership
  egg::lang::String parameter_name;
  auto parameter_type = egg::lang::Type::Void;
  for (size_t i = 1; i <= parameters; ++i) {
    // We promote the parameter, extract the name/type/optional information and then discard it
    auto parameter = context.promote(*this->child[i]);
    auto parameter_optional = parameter->symbol(parameter_name, parameter_type);
    auto parameter_flags = parameter_optional ? egg::lang::IFunctionSignatureParameter::Flags::None : egg::lang::IFunctionSignatureParameter::Flags::Required;
    underlying->addParameter(parameter_name, parameter_type, parameter_flags);
  }
  EggParserContextNested nested(context, EggParserAllowed::Return|EggParserAllowed::Yield);
  auto block = nested.promote(*this->child[parameters + 1]);
  return makeParserNode<EggParserNode_FunctionDefinition>(context, *this, this->name, function, block);
}

std::shared_ptr<egg::yolk::IEggProgramNode> egg::yolk::EggSyntaxNode_Parameter::promote(egg::yolk::IEggParserContext& context) const {
  auto type = context.promote(*this->child)->getType();
  return makeParserNode<EggParserNode_FunctionParameter>(context, *this, this->name, type, this->optional);
}

std::shared_ptr<egg::yolk::IEggProgramNode> egg::yolk::EggSyntaxNode_Return::promote(egg::yolk::IEggParserContext& context) const {
  if (!context.isAllowed(EggParserAllowed::Return)) {
    throw exceptionFromLocation(context, "The 'return' statement may only be used within functions", *this);
  }
  if (this->child.empty()) {
    return makeParserNode<EggParserNode_Return>(context, *this);
  }
  assert(this->child.size() == 1);
  return makeParserNode<EggParserNode_Return>(context, *this, context.promote(*this->child[0]));
}

std::shared_ptr<egg::yolk::IEggProgramNode> egg::yolk::EggSyntaxNode_Switch::promote(egg::yolk::IEggParserContext& context) const {
  EggParserContextSwitch nested(context, *this, *this->child[0]);
  return nested.promote(*this->child[1]);
}

std::shared_ptr<egg::yolk::IEggProgramNode> egg::yolk::EggSyntaxNode_Throw::promote(egg::yolk::IEggParserContext& context) const {
  // This is a throw or a rethrow: 'throw <expr>;' or 'throw;'
  std::shared_ptr<egg::yolk::IEggProgramNode> expr = nullptr;
  if (!this->child.empty()) {
    expr = context.promote(*this->child[0]);
  } else if (!context.isAllowed(EggParserAllowed::Rethrow)) {
    throw exceptionFromLocation(context, "The 'throw' statement may only be used to rethrow exceptions inside a 'catch' statement", *this);
  }
  return makeParserNode<EggParserNode_Throw>(context, *this, expr);
}

std::shared_ptr<egg::yolk::IEggProgramNode> egg::yolk::EggSyntaxNode_Try::promote(egg::yolk::IEggParserContext& context) const {
  // TODO There's some ambiguity amongst C++/Java/C# as to whether 'break' inside here breaks out of any enclosing loop
  // The 'switch' pattern is even more confusing, so we just disallow 'break'/'continue' inside 'try'/'catch'/'finally' for now
  EggParserContextNested nested1(context, EggParserAllowed::None, EggParserAllowed::Rethrow|EggParserAllowed::Return|EggParserAllowed::Yield);
  auto clauses = this->child.size();
  assert(clauses >= 2);
  auto block = nested1.promote(*this->child[0]);
  auto result = makeParserNode<EggParserNode_Try>(context, *this, block);
  EggParserContextNested nested2(context, EggParserAllowed::Rethrow, EggParserAllowed::Return|EggParserAllowed::Yield);
  for (size_t i = 1; i < clauses; ++i) {
    auto& clause = *this->child[i];
    if (clause.keyword() == EggTokenizerKeyword::Catch) {
      // We can have any number of 'catch' clauses; where we allow rethrows
      result->addCatch(nested2.promote(clause));
    } else if (clause.keyword() != EggTokenizerKeyword::Finally) {
      throw exceptionFromLocation(context, "Expected only 'catch' and 'finally' statements in 'try' statement", clause.location());
    } else if (i != (clauses - 1)) {
      throw exceptionFromLocation(context, "The 'finally' statement must be the last clause in a 'try' statement", clause.location());
    } else {
      // We can only have one 'finally' clause; where we allow inherited rethrows only
      result->addFinally(nested1.promote(clause));
    }
  }
  return result;
}

std::shared_ptr<egg::yolk::IEggProgramNode> egg::yolk::EggSyntaxNode_While::promote(egg::yolk::IEggParserContext& context) const {
  auto condition = context.promote(*this->child[0]);
  EggParserContextNested nested(context, EggParserAllowed::Break|EggParserAllowed::Continue, EggParserAllowed::Rethrow|EggParserAllowed::Return|EggParserAllowed::Yield);
  auto block = nested.promote(*this->child[1]);
  return makeParserNode<EggParserNode_While>(context, *this, condition, block);
}

std::shared_ptr<egg::yolk::IEggProgramNode> egg::yolk::EggSyntaxNode_Yield::promote(egg::yolk::IEggParserContext& context) const {
  if (!context.isAllowed(EggParserAllowed::Yield)) {
    throw exceptionFromLocation(context, "The 'yield' statement may only be used within generator functions", *this);
  }
  return makeParserNode<EggParserNode_Yield>(context, *this, context.promote(*this->child));
}

std::shared_ptr<egg::yolk::IEggProgramNode> egg::yolk::EggSyntaxNode_Dot::promote(egg::yolk::IEggParserContext& context) const {
  // TODO query
  auto instance = context.promote(*this->child);
  return makeParserNode<EggParserNode_Dot>(context, *this, instance, this->property);
}
    
std::shared_ptr<egg::yolk::IEggProgramNode> egg::yolk::EggSyntaxNode_UnaryOperator::promote(egg::yolk::IEggParserContext& context) const {
  if (this->op == EggTokenizerOperator::Bang) {
    return promoteUnary<EggParserNode_UnaryLogicalNot>(context, *this, this->child);
  }
  if (this->op == EggTokenizerOperator::Ampersand) {
    return promoteUnary<EggParserNode_UnaryRef>(context, *this, this->child);
  }
  if (this->op == EggTokenizerOperator::Star) {
    return promoteUnary<EggParserNode_UnaryDeref>(context, *this, this->child);
  }
  if (this->op == EggTokenizerOperator::Minus) {
    return promoteUnary<EggParserNode_UnaryNegate>(context, *this, this->child);
  }
  if (this->op == EggTokenizerOperator::Ellipsis) {
    return promoteUnary<EggParserNode_UnaryEllipsis>(context, *this, this->child);
  }
  if (this->op == EggTokenizerOperator::Tilde) {
    return promoteUnary<EggParserNode_UnaryBitwiseNot>(context, *this, this->child);
  }
  throw exceptionFromToken(context, "Unknown unary operator", *this);
}

std::shared_ptr<egg::yolk::IEggProgramNode> egg::yolk::EggSyntaxNode_BinaryOperator::promote(egg::yolk::IEggParserContext& context) const {
  switch (this->op) {
  case EggTokenizerOperator::BangEqual:
    return promoteBinary<EggParserNode_BinaryUnequal>(context, *this, this->child);
  case EggTokenizerOperator::Percent:
    return promoteBinary<EggParserNode_BinaryRemainder>(context, *this, this->child);
  case EggTokenizerOperator::Ampersand:
    return promoteBinary<EggParserNode_BinaryBitwiseAnd>(context, *this, this->child);
  case EggTokenizerOperator::AmpersandAmpersand:
    return promoteBinary<EggParserNode_BinaryLogicalAnd>(context, *this, this->child);
  case EggTokenizerOperator::Star:
    return promoteBinary<EggParserNode_BinaryMultiply>(context, *this, this->child);
  case EggTokenizerOperator::Plus:
    return promoteBinary<EggParserNode_BinaryPlus>(context, *this, this->child);
  case EggTokenizerOperator::Minus:
    return promoteBinary<EggParserNode_BinaryMinus>(context, *this, this->child);
  case EggTokenizerOperator::Lambda:
    return promoteBinary<EggParserNode_BinaryLambda>(context, *this, this->child);
  case EggTokenizerOperator::Slash:
    return promoteBinary<EggParserNode_BinaryDivide>(context, *this, this->child);
  case EggTokenizerOperator::Less:
    return promoteBinary<EggParserNode_BinaryLess>(context, *this, this->child);
  case EggTokenizerOperator::ShiftLeft:
    return promoteBinary<EggParserNode_BinaryShiftLeft>(context, *this, this->child);
  case EggTokenizerOperator::LessEqual:
    return promoteBinary<EggParserNode_BinaryLessEqual>(context, *this, this->child);
  case EggTokenizerOperator::EqualEqual:
    return promoteBinary<EggParserNode_BinaryEqual>(context, *this, this->child);
  case EggTokenizerOperator::Greater:
    return promoteBinary<EggParserNode_BinaryGreater>(context, *this, this->child);
  case EggTokenizerOperator::GreaterEqual:
    return promoteBinary<EggParserNode_BinaryGreaterEqual>(context, *this, this->child);
  case EggTokenizerOperator::ShiftRight:
    return promoteBinary<EggParserNode_BinaryShiftRight>(context, *this, this->child);
  case EggTokenizerOperator::ShiftRightUnsigned:
    return promoteBinary<EggParserNode_BinaryShiftRightUnsigned>(context, *this, this->child);
  case EggTokenizerOperator::QueryQuery:
    return promoteBinary<EggParserNode_BinaryNullCoalescing>(context, *this, this->child);
  case EggTokenizerOperator::BracketLeft:
    return promoteBinary<EggParserNode_Brackets>(context, *this, this->child);
  case EggTokenizerOperator::Caret:
    return promoteBinary<EggParserNode_BinaryBitwiseXor>(context, *this, this->child);
  case EggTokenizerOperator::Bar:
    return promoteBinary<EggParserNode_BinaryBitwiseOr>(context, *this, this->child);
  case EggTokenizerOperator::BarBar:
    return promoteBinary<EggParserNode_BinaryLogicalOr>(context, *this, this->child);
  case EggTokenizerOperator::Bang:
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
  case EggTokenizerOperator::Dot:
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

std::shared_ptr<egg::yolk::IEggProgramNode> egg::yolk::EggSyntaxNode_TernaryOperator::promote(egg::yolk::IEggParserContext& context) const {
  auto condition = context.promote(*this->child[0]);
  auto whenTrue = context.promote(*this->child[1]);
  auto whenFalse = context.promote(*this->child[2]);
  return makeParserNode<EggParserNode_Ternary>(context, *this, condition, whenTrue, whenFalse);
}

std::shared_ptr<egg::yolk::IEggProgramNode> egg::yolk::EggSyntaxNode_Array::promote(egg::yolk::IEggParserContext& context) const {
  auto result = makeParserNode<EggParserNode_Array>(context, *this);
  for (auto& i : this->child) {
    result->addValue(context.promote(*i));
  }
  return result;
}

std::shared_ptr<egg::yolk::IEggProgramNode> egg::yolk::EggSyntaxNode_Object::promote(egg::yolk::IEggParserContext& context) const {
  auto result = makeParserNode<EggParserNode_Object>(context, *this);
  for (auto& i : this->child) {
    result->addValue(context.promote(*i));
  }
  return result;
}

std::shared_ptr<egg::yolk::IEggProgramNode> egg::yolk::EggSyntaxNode_Call::promote(egg::yolk::IEggParserContext& context) const {
  auto children = this->child.size();
  assert(children >= 1);
  auto callee = context.promote(*this->child[0]);
  auto result = makeParserNode<EggParserNode_Call>(context, *this, callee);
  for (size_t i = 1; i < children; ++i) {
    result->addParameter(context.promote(*this->child[i]));
  }
  return result;
}

std::shared_ptr<egg::yolk::IEggProgramNode> egg::yolk::EggSyntaxNode_Named::promote(egg::yolk::IEggParserContext& context) const {
  return makeParserNode<EggParserNode_Named>(context, *this, this->name, context.promote(*this->child));
}

std::shared_ptr<egg::yolk::IEggProgramNode> egg::yolk::EggSyntaxNode_Identifier::promote(egg::yolk::IEggParserContext& context) const {
  return makeParserNode<EggParserNode_Identifier>(context, *this, this->name);
}

std::shared_ptr<egg::yolk::IEggProgramNode> egg::yolk::EggSyntaxNode_Literal::promote(egg::yolk::IEggParserContext& context) const {
  if (this->kind == EggTokenizerKind::Integer) {
    return makeParserNode<EggParserNode_LiteralInteger>(context, *this, this->value.i);
  }
  if (this->kind == EggTokenizerKind::Float) {
    return makeParserNode<EggParserNode_LiteralFloat>(context, *this, this->value.f);
  }
  if (this->kind == EggTokenizerKind::String) {
    return makeParserNode<EggParserNode_LiteralString>(context, *this, this->value.s);
  }
  if (this->kind != EggTokenizerKind::Keyword) {
    throw exceptionFromToken(context, "Unknown literal value type", *this);
  }
  if (this->value.k == EggTokenizerKeyword::Null) {
    return makeParserNode<EggParserNode_LiteralNull>(context, *this);
  }
  if (this->value.k == EggTokenizerKeyword::False) {
    return makeParserNode<EggParserNode_LiteralBool>(context, *this, false);
  }
  if (this->value.k == EggTokenizerKeyword::True) {
    return makeParserNode<EggParserNode_LiteralBool>(context, *this, true);
  }
  throw exceptionFromToken(context, "Unknown literal value keyword", *this);
}

egg::lang::ITypeRef EggParserNode_Brackets::getType() const {
  return egg::lang::Type::AnyQ; // TODO
}

egg::lang::ITypeRef EggParserNode_Dot::getType() const {
  return egg::lang::Type::AnyQ; // TODO
}

egg::lang::ITypeRef EggParserNode_UnaryLogicalNot::getType() const {
  return egg::lang::Type::Bool;
}

egg::lang::ITypeRef EggParserNode_UnaryRef::getType() const {
  auto underlying = this->expr->getType();
  return underlying->pointerType();
}

egg::lang::ITypeRef EggParserNode_UnaryDeref::getType() const {
  // Returns type 'Void' if not dereferencable
  auto underlying = this->expr->getType();
  return underlying->pointeeType();
}

egg::lang::ITypeRef EggParserNode_UnaryNegate::getType() const {
  auto arithmetic = getArithmeticTypes(*this->expr);
  return makeArithmeticType(arithmetic);
}

egg::lang::ITypeRef EggParserNode_UnaryEllipsis::getType() const {
  EGG_THROW("TODO"); // TODO
}

egg::lang::ITypeRef EggParserNode_UnaryBitwiseNot::getType() const {
  return egg::lang::Type::Int;
}

egg::lang::ITypeRef EggParserNode_BinaryUnequal::getType() const {
  return egg::lang::Type::Bool;
}

egg::lang::ITypeRef EggParserNode_BinaryRemainder::getType() const {
  // See http://mindprod.com/jgloss/modulus.html
  // Turn out this equates to the rules for binary-multiply too
  return binaryArithmeticTypes(this->lhs, this->rhs);
}

egg::lang::ITypeRef EggParserNode_BinaryBitwiseAnd::getType() const {
  return egg::lang::Type::Int;
}

egg::lang::ITypeRef EggParserNode_BinaryLogicalAnd::getType() const {
  return egg::lang::Type::Bool;
}

egg::lang::ITypeRef EggParserNode_BinaryMultiply::getType() const {
  return binaryArithmeticTypes(this->lhs, this->rhs);
}

egg::lang::ITypeRef EggParserNode_BinaryPlus::getType() const {
  return binaryArithmeticTypes(this->lhs, this->rhs);
}

egg::lang::ITypeRef EggParserNode_BinaryMinus::getType() const {
  return binaryArithmeticTypes(this->lhs, this->rhs);
}

egg::lang::ITypeRef EggParserNode_BinaryLambda::getType() const {
  EGG_THROW("TODO"); // TODO
}

egg::lang::ITypeRef EggParserNode_BinaryDivide::getType() const {
  return binaryArithmeticTypes(this->lhs, this->rhs);
}

egg::lang::ITypeRef EggParserNode_BinaryLess::getType() const {
  return egg::lang::Type::Bool;
}

egg::lang::ITypeRef EggParserNode_BinaryShiftLeft::getType() const {
  return egg::lang::Type::Int;
}

egg::lang::ITypeRef EggParserNode_BinaryLessEqual::getType() const {
  return egg::lang::Type::Bool;
}

egg::lang::ITypeRef EggParserNode_BinaryEqual::getType() const {
  return egg::lang::Type::Bool;
}

egg::lang::ITypeRef EggParserNode_BinaryGreater::getType() const {
  return egg::lang::Type::Bool;
}

egg::lang::ITypeRef EggParserNode_BinaryGreaterEqual::getType() const {
  return egg::lang::Type::Bool;
}

egg::lang::ITypeRef EggParserNode_BinaryShiftRight::getType() const {
  return egg::lang::Type::Int;
}

egg::lang::ITypeRef EggParserNode_BinaryShiftRightUnsigned::getType() const {
  return egg::lang::Type::Int;
}

egg::lang::ITypeRef EggParserNode_BinaryNullCoalescing::getType() const {
  auto type1 = this->lhs->getType();
  if (!type1->hasNativeType(egg::lang::Discriminator::Null)) {
    // The left-hand-side cannot be null, so the right side is irrelevant
    return type1;
  }
  auto type2 = this->rhs->getType();
  type1 = type1->denulledType();
  if (type1->getSimpleTypes() == egg::lang::Discriminator::Void) {
    // The left-hand-side is only ever null, so only the right side is relevant
    return type2;
  }
  return type1->unionWith(*type2);
}

egg::lang::ITypeRef EggParserNode_BinaryBitwiseXor::getType() const {
  return egg::lang::Type::Int;
}

egg::lang::ITypeRef EggParserNode_BinaryBitwiseOr::getType() const {
  return egg::lang::Type::Int;
}

egg::lang::ITypeRef EggParserNode_BinaryLogicalOr::getType() const {
  return egg::lang::Type::Bool;
}

std::shared_ptr<IEggProgramNode> EggProgramContext::empredicateBinary(const std::shared_ptr<IEggProgramNode>& node, EggProgramBinary op, const std::shared_ptr<IEggProgramNode>& lhs, const std::shared_ptr<IEggProgramNode>& rhs) {
  // Wrap the expression in an appropriate predicate
  // We currently only handle comparator operators
  if ((op == EggProgramBinary::Equal) ||
      (op == EggProgramBinary::Unequal) ||
      (op == EggProgramBinary::Less) ||
      (op == EggProgramBinary::LessEqual) ||
      (op == EggProgramBinary::Greater) ||
      (op == EggProgramBinary::GreaterEqual)) {
    return std::make_shared<EggParserNode_Predicate>(node->location(), op, lhs, rhs);
  }
  return node;
}

egg::lang::ITypeRef EggParserNode_Ternary::getType() const {
  auto type1 = this->condition->getType();
  if (!type1->hasNativeType(egg::lang::Discriminator::Bool)) {
    // The condition is not a bool, so the other values are irrelevant
    return egg::lang::Type::Void;
  }
  auto type2 = this->whenTrue->getType();
  auto type3 = this->whenFalse->getType();
  return type2->unionWith(*type3);
}

std::shared_ptr<egg::yolk::IEggProgramNode> egg::yolk::EggParserFactory::parseModule(TextStream& stream) {
  auto lexer = LexerFactory::createFromTextStream(stream);
  auto tokenizer = EggTokenizerFactory::createFromLexer(lexer);
  EggParserModule parser;
  return parser.parse(*tokenizer);
}

std::shared_ptr<egg::yolk::IEggParser> egg::yolk::EggParserFactory::createModuleParser() {
  return std::make_shared<EggParserModule>();
}

std::shared_ptr<egg::yolk::IEggParser> egg::yolk::EggParserFactory::createExpressionParser() {
  return std::make_shared<EggParserExpression>();
}
