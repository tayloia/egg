#include "yolk.h"
#include "lexers.h"
#include "egg-tokenizer.h"
#include "egg-syntax.h"
#include "egg-parser.h"
#include "egg-program.h"

namespace {
  using namespace egg::yolk;

  // This is the integer representation of the enum bit-mask
  typedef std::underlying_type_t<EggParserAllowed> EggParserAllowedUnderlying;
  inline EggParserAllowed operator|(EggParserAllowed lhs, EggParserAllowed rhs) {
    return static_cast<EggParserAllowed>(static_cast<EggParserAllowedUnderlying>(lhs) | static_cast<EggParserAllowedUnderlying>(rhs));
  }

  class EggParserTypeBase : public IEggProgramType, public std::enable_shared_from_this<EggParserTypeBase> {
  public:
  protected:
    std::shared_ptr<IEggProgramType> share() const {
      return const_cast<EggParserTypeBase*>(this)->shared_from_this();
    }
  };

  class EggParserTypeSimple : public EggParserTypeBase {
  private:
    egg::lang::Discriminator tag;
  public:
    explicit EggParserTypeSimple(egg::lang::Discriminator tag) : tag(tag) {
    }
    virtual bool hasSimpleType(egg::lang::Discriminator bit) const {
      return egg::lang::Bits::hasAnySet(this->tag, bit);
    }
    virtual egg::lang::Discriminator arithmeticTypes() const {
      return egg::lang::Bits::mask(this->tag, egg::lang::Discriminator::Arithmetic);
    }
    virtual std::shared_ptr<IEggProgramType> dereferencedType() const {
      // TODO Return a dummy value for now
      return std::make_shared<EggParserTypeSimple>(egg::lang::Discriminator::Void);
    }
    virtual std::shared_ptr<IEggProgramType> nullableType(bool nullable) const {
      if (nullable ^ egg::lang::Bits::hasAnySet(this->tag, egg::lang::Discriminator::Null)) {
        // We need to flip the bit
        return std::make_shared<EggParserTypeSimple>(egg::lang::Bits::invert(this->tag, egg::lang::Discriminator::Null));
      }
      return this->share();
    }
    virtual std::shared_ptr<IEggProgramType> unionWith(IEggProgramType& other) const {
      return other.unionWithSimple(this->tag);
    }
    virtual std::shared_ptr<IEggProgramType> unionWithSimple(egg::lang::Discriminator other) const {
      return std::make_shared<EggParserTypeSimple>(egg::lang::Bits::set(this->tag, other));
    }
    virtual egg::lang::Value decantParameters(const egg::lang::IParameters&, Setter) const {
      return egg::lang::Value::raise("Internal parse error: Cannot decant parameters for simple type");
    }
    virtual egg::lang::String toString() const {
      return egg::lang::String::fromUTF8(egg::lang::Value::getTagString(this->tag));
    }
  };

  class EggParserTypeReference : public EggParserTypeBase {
  private:
    std::shared_ptr<IEggProgramType> underlying;
    bool canBeNull;
  public:
    EggParserTypeReference(const std::shared_ptr<IEggProgramType>& underlying, bool nullable)
      : underlying(underlying), canBeNull(nullable) {
    }
    virtual bool hasSimpleType(egg::lang::Discriminator tag) const {
      return (tag == egg::lang::Discriminator::Null) && this->canBeNull;
    }
    virtual egg::lang::Discriminator arithmeticTypes() const {
      return egg::lang::Discriminator::None;
    }
    virtual std::shared_ptr<IEggProgramType> dereferencedType() const {
      return this->underlying;
    }
    virtual std::shared_ptr<IEggProgramType> nullableType(bool nullable) const {
      return (nullable ^ this->canBeNull) ? std::make_shared<EggParserTypeReference>(this->underlying, nullable) : this->share();
    }
    virtual std::shared_ptr<IEggProgramType> unionWith(IEggProgramType&) const {
      EGG_THROW(__FUNCTION__ " TODO"); // TODO
    }
    virtual std::shared_ptr<IEggProgramType> unionWithSimple(egg::lang::Discriminator) const {
      EGG_THROW(__FUNCTION__ " TODO"); // TODO
    }
    virtual egg::lang::Value decantParameters(const egg::lang::IParameters&, Setter) const {
      return egg::lang::Value::raise("Internal parse error: Cannot decant parameters for reference type");
    }
    virtual egg::lang::String toString() const {
      return egg::lang::String::concat(this->underlying->toString(), "*");
    }
  };

  class EggParserTypeFunction : public EggParserTypeBase {
  public:
    struct Parameter {
      egg::lang::String name;
      std::shared_ptr<IEggProgramType> type;
      bool optional;
      Parameter(const egg::lang::String& name, const std::shared_ptr<IEggProgramType>& type, bool optional)
        : name(name), type(type), optional(optional) {
        assert(type != nullptr);
      }
    };
  private:
    std::shared_ptr<IEggProgramType> retval;
    std::vector<Parameter> expected;
  public:
    explicit EggParserTypeFunction(const std::shared_ptr<IEggProgramType>& retval)
      : retval(retval) {
      assert(retval != nullptr);
    }
    virtual bool hasSimpleType(egg::lang::Discriminator) const override {
      return false;
    }
    virtual egg::lang::Discriminator arithmeticTypes() const override {
      return egg::lang::Discriminator::None;
    }
    virtual std::shared_ptr<IEggProgramType> dereferencedType() const override {
      // TODO Return a dummy value for now
      return std::make_shared<EggParserTypeSimple>(egg::lang::Discriminator::Void);
    }
    virtual std::shared_ptr<IEggProgramType> nullableType(bool) const override {
      EGG_THROW(__FUNCTION__ " TODO"); // TODO
    }
    virtual std::shared_ptr<IEggProgramType> unionWith(IEggProgramType&) const override {
      EGG_THROW(__FUNCTION__ " TODO"); // TODO
    }
    virtual std::shared_ptr<IEggProgramType> unionWithSimple(egg::lang::Discriminator) const override {
      EGG_THROW(__FUNCTION__ " TODO"); // TODO
    }
    virtual egg::lang::Value decantParameters(const egg::lang::IParameters& supplied, Setter setter) const {
      if (supplied.getNamedCount() > 0) {
        return egg::lang::Value::raise("Named parameters in function calls are not yet supported"); // TODO
      }
      size_t given = supplied.getPositionalCount();
      if (given < this->expected.size()) {
        auto message = "Too few parameters in function call: Expected " + std::to_string(this->expected.size()) + ", but got " + std::to_string(given);
        return egg::lang::Value::raise(message);
      }
      if (given > this->expected.size()) {
        auto message = "Too many parameters in function call: Expected " + std::to_string(this->expected.size()) + ", but got " + std::to_string(given);
        return egg::lang::Value::raise(message);
      }
      // TODO: Value type checking
      for (size_t i = 0; i < given; ++i) {
        setter(this->expected[i].name, supplied.getPositional(i));
      }
      return egg::lang::Value::Void;
    }
    virtual egg::lang::String toString() const override {
      return egg::lang::String::fromUTF8("function"); // TODO
    }
    void addParameter(const egg::lang::String& name, const std::shared_ptr<IEggProgramType>& type, bool optional) {
      this->expected.emplace_back(name, type, optional);
    }
  };

  // Constants
  const std::shared_ptr<IEggProgramType> typeVoid = std::make_shared<EggParserTypeSimple>(egg::lang::Discriminator::Void);
  const std::shared_ptr<IEggProgramType> typeNull = std::make_shared<EggParserTypeSimple>(egg::lang::Discriminator::Null);
  const std::shared_ptr<IEggProgramType> typeBool = std::make_shared<EggParserTypeSimple>(egg::lang::Discriminator::Bool);
  const std::shared_ptr<IEggProgramType> typeInt = std::make_shared<EggParserTypeSimple>(egg::lang::Discriminator::Int);
  const std::shared_ptr<IEggProgramType> typeFloat = std::make_shared<EggParserTypeSimple>(egg::lang::Discriminator::Float);
  const std::shared_ptr<IEggProgramType> typeArithmetic = std::make_shared<EggParserTypeSimple>(egg::lang::Discriminator::Arithmetic);
  const std::shared_ptr<IEggProgramType> typeString = std::make_shared<EggParserTypeSimple>(egg::lang::Discriminator::String);

  enum class EggParserArithmetic {
    None = int(egg::lang::Discriminator::None),
    Int = int(egg::lang::Discriminator::Int),
    Float = int(egg::lang::Discriminator::Float),
    Both = int(egg::lang::Discriminator::Arithmetic)
  };

  EggParserArithmetic getArithmeticTypes(const std::shared_ptr<IEggProgramNode>& node) {
    auto underlying = node->getType();
    assert(underlying != nullptr);
    return EggParserArithmetic(underlying->arithmeticTypes());
  }

  std::shared_ptr<IEggProgramType> makeArithmeticType(EggParserArithmetic arithmetic) {
    switch (arithmetic) {
    case EggParserArithmetic::Int:
      return typeInt;
    case EggParserArithmetic::Float:
      return typeFloat;
    case EggParserArithmetic::Both:
      return typeArithmetic;
    case EggParserArithmetic::None:
    default:
      break;
    }
    return nullptr;
  }

  std::shared_ptr<IEggProgramType> binaryArithmeticTypes(const std::shared_ptr<IEggProgramNode>& lhs, const std::shared_ptr<IEggProgramNode>& rhs) {
    auto lhsa = getArithmeticTypes(lhs);
    auto rhsa = getArithmeticTypes(rhs);
    if ((lhsa == EggParserArithmetic::None) || (rhsa == EggParserArithmetic::None)) {
      return typeVoid;
    }
    if ((lhsa == EggParserArithmetic::Int) && (rhsa == EggParserArithmetic::Int)) {
      return typeInt;
    }
    if ((lhsa == EggParserArithmetic::Float) || (rhsa == EggParserArithmetic::Float)) {
      return typeFloat;
    }
    return typeArithmetic;
  }

  SyntaxException exceptionFromLocation(const IEggParserContext& context, const std::string& reason, const EggSyntaxNodeLocation& location) {
    return SyntaxException(reason, context.getResource(), location);
  }

  SyntaxException exceptionFromToken(const IEggParserContext& context, const std::string& reason, const EggSyntaxNodeBase& node) {
    auto token = node.token().toUTF8();
    return SyntaxException(reason + ": '" + token + "'", context.getResource(), node, token);
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
  public:
    virtual std::shared_ptr<IEggProgramType> getType() const override {
      // By default, nodes are statements (i.e. void return type)
      return typeVoid;
    }
    virtual bool symbol(egg::lang::String&, std::shared_ptr<IEggProgramType>&) const override {
      // By default, nodes do not declare symbols
      return false;
    }
    virtual egg::lang::Value executeWithExpression(EggProgramContext&, const egg::lang::Value&) const override {
      // By default, we fail if asked to execute with an expression (used only in switch/catch statements, etc)
      return egg::lang::Value::raise("Internal parser error: Inappropriate 'executeWithExpression' call");
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
    std::shared_ptr<IEggProgramType> type;
  public:
    explicit EggParserNode_Type(const std::shared_ptr<IEggProgramType>& type)
      : type(type) {
      assert(type != nullptr);
    }
    virtual std::shared_ptr<IEggProgramType> getType() const override {
      return this->type;
    }
    virtual egg::lang::Value execute(EggProgramContext&) const override {
      return egg::lang::Value::raise("Internal parser error: Inappropriate 'execute' call for 'type' node");
    }
    virtual void dump(std::ostream& os) const override {
      ParserDump(os, "type").add(this->type->toString());
    }
  };

  class EggParserNode_Declare : public EggParserNodeBase {
  private:
    egg::lang::String name;
    std::shared_ptr<IEggProgramType> type;
    std::shared_ptr<IEggProgramNode> init;
  public:
    EggParserNode_Declare(const egg::lang::String& name, const std::shared_ptr<IEggProgramType>& type, const std::shared_ptr<IEggProgramNode>& init = nullptr)
      : name(name), type(type), init(init) {
      assert(type != nullptr);
    }
    virtual bool symbol(egg::lang::String& nameOut, std::shared_ptr<IEggProgramType>& typeOut) const override {
      // The symbol is obviously the variable being declared
      nameOut = this->name;
      typeOut = this->type;
      return true;
    }
    virtual egg::lang::Value execute(EggProgramContext& context) const override {
      return context.executeDeclare(*this, this->name, *this->type, this->init.get());
    }
    virtual void dump(std::ostream& os) const override {
      ParserDump(os, "declare").add(this->name).add(this->type->toString()).add(this->init);
    }
  };

  class EggParserNode_Mutate : public EggParserNodeBase {
  private:
    EggProgramMutate op;
    std::shared_ptr<IEggProgramNode> lvalue;
  public:
    EggParserNode_Mutate(EggProgramMutate op, const std::shared_ptr<IEggProgramNode>& lvalue)
      : op(op), lvalue(lvalue) {
      assert(lvalue != nullptr);
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
    EggParserNode_Catch(const egg::lang::String& name, const std::shared_ptr<IEggProgramNode>& type, const std::shared_ptr<IEggProgramNode>& block)
      : name(name), type(type), block(block) {
      assert(type != nullptr);
      assert(block != nullptr);
    }
    virtual bool symbol(egg::lang::String& nameOut, std::shared_ptr<IEggProgramType>& typeOut) const override {
      // A symbol is always declared in a catch clause
      nameOut = this->name;
      typeOut = this->type->getType();
      return true;
    }
    virtual egg::lang::Value execute(EggProgramContext&) const override {
      return egg::lang::Value::raise("Internal parser error: Inappropriate 'execute' call for 'catch' statement");
    }
    virtual egg::lang::Value executeWithExpression(EggProgramContext& context, const egg::lang::Value& expression) const override {
      return context.executeCatch(*this, this->name, *this->type, *this->block, expression);
    }
    virtual void dump(std::ostream& os) const override {
      ParserDump(os, "catch").add(this->name).add(this->type).add(this->block);
    }
  };

  class EggParserNode_Continue : public EggParserNodeBase {
  public:
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
    EggParserNode_Do(const std::shared_ptr<IEggProgramNode>& condition, const std::shared_ptr<IEggProgramNode>& block)
      : condition(condition), block(block) {
      assert(condition != nullptr);
      assert(block != nullptr);
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
    EggParserNode_If(const std::shared_ptr<IEggProgramNode>& condition, const std::shared_ptr<IEggProgramNode>& trueBlock, const std::shared_ptr<IEggProgramNode>& falseBlock)
      : condition(condition), trueBlock(trueBlock), falseBlock(falseBlock) {
      assert(condition != nullptr);
      assert(trueBlock != nullptr);
      // falseBlock may be null if the 'else' clause is missing
    }
    virtual bool symbol(egg::lang::String& nameOut, std::shared_ptr<IEggProgramType>& typeOut) const override {
      // A symbol may be declared in the condition
      return this->condition->symbol(nameOut, typeOut);
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
    EggParserNode_For(const std::shared_ptr<IEggProgramNode>& pre, const std::shared_ptr<IEggProgramNode>& cond, const std::shared_ptr<IEggProgramNode>& post, const std::shared_ptr<IEggProgramNode>& block)
      : pre(pre), cond(cond), post(post), block(block) {
      // pre/cond/post may be null
      assert(block != nullptr);
    }
    virtual bool symbol(egg::lang::String& nameOut, std::shared_ptr<IEggProgramType>& typeOut) const override {
      // A symbol may be declared in the first clause
      return (this->pre != nullptr) && this->pre->symbol(nameOut, typeOut);
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
    EggParserNode_Foreach(const std::shared_ptr<IEggProgramNode>& target, const std::shared_ptr<IEggProgramNode>& expr, const std::shared_ptr<IEggProgramNode>& block)
      : target(target), expr(expr), block(block) {
      assert(target != nullptr);
      assert(expr != nullptr);
      assert(block != nullptr);
    }
    virtual bool symbol(egg::lang::String& nameOut, std::shared_ptr<IEggProgramType>& typeOut) const override {
      // A symbol may be declared in the target
      return this->target->symbol(nameOut, typeOut);
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
    std::shared_ptr<IEggProgramType> type;
    std::shared_ptr<IEggProgramNode> block;
  public:
    EggParserNode_FunctionDefinition(const egg::lang::String& name, const std::shared_ptr<IEggProgramType>& type, const std::shared_ptr<IEggProgramNode>& block)
      : name(name), type(type), block(block) {
      assert(type != nullptr);
      assert(block != nullptr);
    }
    virtual bool symbol(egg::lang::String& nameOut, std::shared_ptr<IEggProgramType>& typeOut) const override {
      // The symbol is obviously the identifier being defined
      nameOut = this->name;
      typeOut = this->type;
      return true;
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
    std::shared_ptr<IEggProgramType> type;
    bool optional;
  public:
    EggParserNode_FunctionParameter(const egg::lang::String& name, const std::shared_ptr<IEggProgramType>& type, bool optional)
      : name(name), type(type), optional(optional) {
      assert(type != nullptr);
    }
    virtual bool symbol(egg::lang::String& nameOut, std::shared_ptr<IEggProgramType>& typeOut) const override {
      // Beware: the return value is the optionality flag!
      nameOut = this->name;
      typeOut = this->type;
      return this->optional;
    }
    virtual egg::lang::Value execute(EggProgramContext&) const override {
      return egg::lang::Value::raise("Internal parser error: Inappropriate 'execute' call for function parameter");
    }
    virtual void dump(std::ostream& os) const override {
      ParserDump(os, this->optional ? "parameter?" : "parameter").add(this->name).add(this->type->toString());
    }
  };

  class EggParserNode_Return : public EggParserNodeBase {
  private:
    std::shared_ptr<IEggProgramNode> expr;
  public:
    explicit EggParserNode_Return(const std::shared_ptr<IEggProgramNode>& expr = nullptr)
      : expr(expr) {
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
    EggParserNode_Case()
      : block(std::make_shared<EggParserNode_Block>()) {
      assert(this->block != nullptr);
    }
    virtual egg::lang::Value execute(EggProgramContext& context) const override {
      // We actually want to execute the block of the 'case' statement
      return context.executeCase(*this, this->child, *this->block, nullptr);
    }
    virtual egg::lang::Value executeWithExpression(EggProgramContext& context, const egg::lang::Value& expression) const override {
      // We're matching values in the 'case' statement
      return context.executeCase(*this, this->child, *this->block, &expression);
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
    explicit EggParserNode_Switch(const std::shared_ptr<IEggProgramNode>& expr)
      : expr(expr), defaultIndex(-1) {
      assert(expr != nullptr);
    }
    virtual bool symbol(egg::lang::String& nameOut, std::shared_ptr<IEggProgramType>& typeOut) const override {
      // A symbol may be declared in the expression
      return this->expr->symbol(nameOut, typeOut);
    }
    virtual egg::lang::Value execute(EggProgramContext& context) const override {
      return context.executeSwitch(*this, *this->expr, this->defaultIndex, this->child);
    }
    virtual void dump(std::ostream& os) const override {
      ParserDump(os, "switch").add(this->expr).raw(this->defaultIndex).add(this->child);
    }
    void newClause() {
      this->latest = std::make_shared<EggParserNode_Case>();
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
    explicit EggParserNode_Throw(const std::shared_ptr<IEggProgramNode>& expr = nullptr)
      : expr(expr) {
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
    explicit EggParserNode_Try(const std::shared_ptr<IEggProgramNode>& block)
      : block(block) {
      assert(block != nullptr);
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

  class EggParserNode_Using : public EggParserNodeBase {
  private:
    std::shared_ptr<IEggProgramNode> expr;
    std::shared_ptr<IEggProgramNode> block;
  public:
    EggParserNode_Using(const std::shared_ptr<IEggProgramNode>& expr, const std::shared_ptr<IEggProgramNode>& block)
      : expr(expr), block(block) {
      assert(expr != nullptr);
      assert(block != nullptr);
    }
    virtual bool symbol(egg::lang::String& nameOut, std::shared_ptr<IEggProgramType>& typeOut) const override {
      // A symbol may be declared in the expression
      return this->expr->symbol(nameOut, typeOut);
    }
    virtual egg::lang::Value execute(EggProgramContext& context) const override {
      return context.executeUsing(*this, *this->expr, *this->block);
    }
    virtual void dump(std::ostream& os) const override {
      ParserDump(os, "using").add(this->expr).add(this->block);
    }
  };

  class EggParserNode_While : public EggParserNodeBase {
  private:
    std::shared_ptr<IEggProgramNode> condition;
    std::shared_ptr<IEggProgramNode> block;
  public:
    EggParserNode_While(const std::shared_ptr<IEggProgramNode>& condition, const std::shared_ptr<IEggProgramNode>& block)
      : condition(condition), block(block) {
      assert(condition != nullptr);
      assert(block != nullptr);
    }
    virtual bool symbol(egg::lang::String& nameOut, std::shared_ptr<IEggProgramType>& typeOut) const override {
      // A symbol may be declared in the condition
      return this->condition->symbol(nameOut, typeOut);
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
    explicit EggParserNode_Yield(const std::shared_ptr<IEggProgramNode>& expr)
      : expr(expr) {
      assert(expr != nullptr);
    }
    virtual egg::lang::Value execute(EggProgramContext& context) const override {
      return context.executeYield(*this, *this->expr);
    }
    virtual void dump(std::ostream& os) const override {
      ParserDump(os, "yield").add(this->expr);
    }
  };

  class EggParserNode_Call : public EggParserNodeBase {
  private:
    std::shared_ptr<IEggProgramNode> callee;
    std::vector<std::shared_ptr<IEggProgramNode>> child;
  public:
    explicit EggParserNode_Call(const std::shared_ptr<IEggProgramNode>& callee)
      : callee(callee) {
      assert(callee != nullptr);
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

  class EggParserNode_Named : public EggParserNodeBase {
  private:
    egg::lang::String name;
    std::shared_ptr<IEggProgramNode> expr;
  public:
    EggParserNode_Named(const egg::lang::String& name, const std::shared_ptr<IEggProgramNode>& expr)
      : name(name), expr(expr) {
      assert(expr != nullptr);
    }
    virtual bool symbol(egg::lang::String& nameOut, std::shared_ptr<IEggProgramType>& typeOut) const override {
      // Use the symbol to extract the parameter name
      nameOut = this->name;
      typeOut = this->expr->getType();
      return true;
    }
    virtual egg::lang::Value execute(EggProgramContext& context) const override {
      // The name has already been extracted via 'symbol' so just propagate the value
      return this->expr->execute(context);
    }
    virtual void dump(std::ostream& os) const override {
      ParserDump(os, "named").add(this->name).add(this->expr);
    }
  };

  class EggParserNode_Identifier : public EggParserNodeBase {
  private:
    egg::lang::String name;
  public:
    explicit EggParserNode_Identifier(const egg::lang::String& name)
      : name(name) {
    }
    virtual egg::lang::Value execute(EggProgramContext& context) const override {
      return context.executeIdentifier(*this, this->name);
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
    virtual std::shared_ptr<IEggProgramType> getType() const override {
      return typeNull;
    }
    virtual egg::lang::Value execute(EggProgramContext& context) const override {
      return context.executeLiteral(*this, egg::lang::Value::Null);
    }
    virtual void dump(std::ostream& os) const override {
      ParserDump(os, "literal null");
    }
  };
  const std::shared_ptr<IEggProgramNode> constNull = std::make_shared<EggParserNode_LiteralNull>();

  class EggParserNode_LiteralBool : public EggParserNodeBase {
  private:
    bool value;
  public:
    explicit EggParserNode_LiteralBool(bool value)
      : value(value) {
    }
    virtual std::shared_ptr<IEggProgramType> getType() const override {
      return typeBool;
    }
    virtual egg::lang::Value execute(EggProgramContext& context) const override {
      return context.executeLiteral(*this, egg::lang::Value(this->value));
    }
    virtual void dump(std::ostream& os) const override {
      ParserDump(os, "literal bool").raw(this->value);
    }
  };
  const std::shared_ptr<IEggProgramNode> constFalse = std::make_shared<EggParserNode_LiteralBool>(false);
  const std::shared_ptr<IEggProgramNode> constTrue = std::make_shared<EggParserNode_LiteralBool>(true);

  class EggParserNode_LiteralInteger : public EggParserNodeBase {
  private:
    int64_t value;
  public:
    explicit EggParserNode_LiteralInteger(int64_t value)
      : value(value) {
    }
    virtual std::shared_ptr<IEggProgramType> getType() const override {
      return typeInt;
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
    explicit EggParserNode_LiteralFloat(double value)
      : value(value) {
    }
    virtual std::shared_ptr<IEggProgramType> getType() const override {
      return typeFloat;
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
    explicit EggParserNode_LiteralString(const egg::lang::String& value)
      : value(value) {
    }
    virtual std::shared_ptr<IEggProgramType> getType() const override {
      return typeString;
    }
    virtual egg::lang::Value execute(EggProgramContext& context) const override {
      return context.executeLiteral(*this, egg::lang::Value(this->value));
    }
    virtual void dump(std::ostream& os) const override {
      ParserDump(os, "literal string").add(this->value);
    }
  };

  class EggParserNode_Unary : public EggParserNodeBase {
  protected:
    EggProgramUnary op;
    std::shared_ptr<IEggProgramNode> expr;
    EggParserNode_Unary(EggProgramUnary op, const std::shared_ptr<IEggProgramNode>& expr)
      : op(op), expr(expr) {
      assert(expr != nullptr);
    }
  public:
    virtual egg::lang::Value execute(EggProgramContext& context) const override {
      return context.executeUnary(*this, this->op, *this->expr);
    }
    virtual void dump(std::ostream& os) const override {
      ParserDump(os, "unary").add(EggProgram::unaryToString(this->op)).add(this->expr);
    }
  };

#define EGG_PARSER_UNARY_OPERATOR_DEFINE(name, text) \
  class EggParserNode_Unary##name : public EggParserNode_Unary { \
  public: \
    EggParserNode_Unary##name(const std::shared_ptr<IEggProgramNode>& expr) \
      : EggParserNode_Unary(EggProgramUnary::name, expr) { \
    } \
    virtual std::shared_ptr<IEggProgramType> getType() const override; \
  };
  EGG_PROGRAM_UNARY_OPERATORS(EGG_PARSER_UNARY_OPERATOR_DEFINE)

  class EggParserNode_Binary : public EggParserNodeBase {
  protected:
    EggProgramBinary op;
    std::shared_ptr<IEggProgramNode> lhs;
    std::shared_ptr<IEggProgramNode> rhs;
    EggParserNode_Binary(EggProgramBinary op, const std::shared_ptr<IEggProgramNode>& lhs, const std::shared_ptr<IEggProgramNode>& rhs)
      : op(op), lhs(lhs), rhs(rhs) {
      assert(lhs != nullptr);
      assert(rhs != nullptr);
    }
  public:
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
    EggParserNode_Binary##name(const std::shared_ptr<IEggProgramNode>& lhs, const std::shared_ptr<IEggProgramNode>& rhs) \
      : EggParserNode_Binary(EggProgramBinary::name, lhs, rhs) { \
    } \
    virtual std::shared_ptr<IEggProgramType> getType() const override; \
  };
  EGG_PROGRAM_BINARY_OPERATORS(EGG_PARSER_BINARY_OPERATOR_DEFINE)

  class EggParserNode_Ternary : public EggParserNodeBase {
  private:
    std::shared_ptr<IEggProgramNode> condition;
    std::shared_ptr<IEggProgramNode> whenTrue;
    std::shared_ptr<IEggProgramNode> whenFalse;
  public:
    EggParserNode_Ternary(const std::shared_ptr<IEggProgramNode>& condition, const std::shared_ptr<IEggProgramNode>& whenTrue, const std::shared_ptr<IEggProgramNode>& whenFalse)
      : condition(condition), whenTrue(whenTrue), whenFalse(whenFalse) {
      assert(condition != nullptr);
      assert(whenTrue != nullptr);
      assert(whenFalse != nullptr);
    }
    virtual egg::lang::Value execute(EggProgramContext& context) const override {
      return context.executeTernary(*this, *this->condition, *this->whenTrue, *this->whenFalse);
    }
    virtual void dump(std::ostream& os) const override {
      ParserDump(os, "ternary").add(this->condition).add(this->whenTrue).add(this->whenFalse);
    }
    virtual std::shared_ptr<IEggProgramType> getType() const override;
  };

  class EggParserNode_Assign : public EggParserNodeBase {
  protected:
    EggProgramAssign op;
    std::shared_ptr<IEggProgramNode> lhs;
    std::shared_ptr<IEggProgramNode> rhs;
    EggParserNode_Assign(EggProgramAssign op, const std::shared_ptr<IEggProgramNode>& lhs, const std::shared_ptr<IEggProgramNode>& rhs)
      : op(op), lhs(lhs), rhs(rhs) {
      assert(lhs != nullptr);
      assert(rhs != nullptr);
    }
  public:
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
    EggParserNode_Assign##name(const std::shared_ptr<IEggProgramNode>& lhs, const std::shared_ptr<IEggProgramNode>& rhs) \
      : EggParserNode_Assign(EggProgramAssign::name, lhs, rhs) { \
    } \
  };
  EGG_PROGRAM_ASSIGN_OPERATORS(EGG_PARSER_ASSIGN_OPERATOR_DEFINE)

  class EggParserContextBase : public IEggParserContext {
  private:
    EggParserAllowedUnderlying allowed;
  public:
    explicit EggParserContextBase(EggParserAllowed allowed)
      : allowed(static_cast<EggParserAllowedUnderlying>(allowed)) {
    }
    virtual bool isAllowed(EggParserAllowed bit) const override {
      return (this->allowed & static_cast<EggParserAllowedUnderlying>(bit)) != 0;
    }
    virtual EggParserAllowed inheritAllowed(EggParserAllowed allow, EggParserAllowed inherit) const {
      auto inherited = this->allowed & static_cast<EggParserAllowedUnderlying>(inherit);
      return static_cast<EggParserAllowed>(inherited | static_cast<EggParserAllowedUnderlying>(allow));
    }
    virtual std::shared_ptr<IEggProgramNode> promote(const IEggSyntaxNode& node) override {
      return node.promote(*this);
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

  class EggParserContextSwitch : public IEggParserContext {
  private:
    EggParserContextNested nested;
    std::shared_ptr<EggParserNode_Switch> promoted;
    EggTokenizerKeyword previous;
  public:
    EggParserContextSwitch(IEggParserContext& parent, const IEggSyntaxNode& switchExpression)
      : nested(parent, EggParserAllowed::Break|EggParserAllowed::Case|EggParserAllowed::Continue, EggParserAllowed::Rethrow|EggParserAllowed::Return|EggParserAllowed::Yield),
        promoted(std::make_shared<EggParserNode_Switch>(parent.promote(switchExpression))),
        previous(EggTokenizerKeyword::Null) {
      assert(this->promoted != nullptr);
    }
    virtual std::string getResource() const {
      return this->nested.getResource();
    }
    virtual bool isAllowed(EggParserAllowed bit) const override {
      return this->nested.isAllowed(bit);
    }
    virtual EggParserAllowed inheritAllowed(EggParserAllowed allow, EggParserAllowed inherit) const {
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
        this->promoted->newClause();
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
  auto module = std::make_shared<EggParserNode_Module>();
  for (auto& statement : this->child) {
    module->addChild(context.promote(*statement));
  }
  return module;
}

std::shared_ptr<egg::yolk::IEggProgramNode> egg::yolk::EggSyntaxNode_Block::promote(egg::yolk::IEggParserContext& context) const {
  auto module = std::make_shared<EggParserNode_Block>();
  for (auto& statement : this->child) {
    module->addChild(context.promote(*statement));
  }
  return module;
}

std::shared_ptr<egg::yolk::IEggProgramNode> egg::yolk::EggSyntaxNode_Type::promote(egg::yolk::IEggParserContext&) const {
  auto type = std::make_shared<EggParserTypeSimple>(this->tag);
  return std::make_shared<EggParserNode_Type>(type);
}

std::shared_ptr<egg::yolk::IEggProgramNode> egg::yolk::EggSyntaxNode_Declare::promote(egg::yolk::IEggParserContext& context) const {
  auto type = context.promote(*this->child[0])->getType();
  if (this->child.size() == 1) {
    return std::make_shared<EggParserNode_Declare>(this->name, type);
  }
  assert(this->child.size() == 2);
  return std::make_shared<EggParserNode_Declare>(this->name, type, context.promote(*this->child[1]));
}

std::shared_ptr<egg::yolk::IEggProgramNode> egg::yolk::EggSyntaxNode_Assignment::promote(egg::yolk::IEggParserContext& context) const {
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

std::shared_ptr<egg::yolk::IEggProgramNode> egg::yolk::EggSyntaxNode_Mutate::promote(egg::yolk::IEggParserContext& context) const {
  EggProgramMutate mop;
  if (this->op == EggTokenizerOperator::PlusPlus) {
    mop = EggProgramMutate::Increment;
  } else if (this->op == EggTokenizerOperator::MinusMinus) {
    mop = EggProgramMutate::Decrement;
  } else {
    throw exceptionFromToken(context, "Unknown increment/decrement operator", *this);
  }
  return std::make_shared<EggParserNode_Mutate>(mop, context.promote(*this->child));
}

std::shared_ptr<egg::yolk::IEggProgramNode> egg::yolk::EggSyntaxNode_Break::promote(egg::yolk::IEggParserContext& context) const {
  if (!context.isAllowed(EggParserAllowed::Break)) {
    throw exceptionFromLocation(context, "The 'break' statement may only be used within loops or switch statements", *this);
  }
  return std::make_shared<EggParserNode_Break>();
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
  return std::make_shared<EggParserNode_Catch>(this->name, type, block);
}

std::shared_ptr<egg::yolk::IEggProgramNode> egg::yolk::EggSyntaxNode_Continue::promote(egg::yolk::IEggParserContext& context) const {
  if (!context.isAllowed(EggParserAllowed::Continue)) {
    throw exceptionFromLocation(context, "The 'continue' statement may only be used within loops or switch statements", *this);
  }
  return std::make_shared<EggParserNode_Continue>();
}

std::shared_ptr<egg::yolk::IEggProgramNode> egg::yolk::EggSyntaxNode_Default::promote(egg::yolk::IEggParserContext& context) const {
  // The logic is handled by the 'switch' node, so just assume it's a misplaced 'default'
  throw exceptionFromLocation(context, "The 'default' statement may only be used within switch statements", *this);
}

std::shared_ptr<egg::yolk::IEggProgramNode> egg::yolk::EggSyntaxNode_Do::promote(egg::yolk::IEggParserContext& context) const {
  auto condition = context.promote(*this->child[0]);
  EggParserContextNested nested(context, EggParserAllowed::Break|EggParserAllowed::Continue, EggParserAllowed::Rethrow|EggParserAllowed::Return|EggParserAllowed::Yield);
  auto block = context.promote(*this->child[1]);
  return std::make_shared<EggParserNode_Do>(condition, block);
}

std::shared_ptr<egg::yolk::IEggProgramNode> egg::yolk::EggSyntaxNode_If::promote(egg::yolk::IEggParserContext& context) const {
  assert((this->child.size() == 2) || (this->child.size() == 3));
  auto condition = context.promote(*this->child[0]);
  auto trueBlock = context.promote(*this->child[1]);
  std::shared_ptr<egg::yolk::IEggProgramNode> falseBlock = nullptr;
  if (this->child.size() == 3) {
    falseBlock = context.promote(*this->child[2]);
  }
  return std::make_shared<EggParserNode_If>(condition, trueBlock, falseBlock);
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
  return std::make_shared<EggParserNode_For>(pre, cond, post, block);
}

std::shared_ptr<egg::yolk::IEggProgramNode> egg::yolk::EggSyntaxNode_Foreach::promote(egg::yolk::IEggParserContext& context) const {
  auto target = context.promote(*this->child[0]);
  auto expr = context.promote(*this->child[1]);
  EggParserContextNested nested(context, EggParserAllowed::Break|EggParserAllowed::Continue, EggParserAllowed::Rethrow|EggParserAllowed::Return|EggParserAllowed::Yield);
  auto block = nested.promote(*this->child[2]);
  return std::make_shared<EggParserNode_Foreach>(target, expr, block);
}

std::shared_ptr<egg::yolk::IEggProgramNode> egg::yolk::EggSyntaxNode_FunctionDefinition::promote(egg::yolk::IEggParserContext& context) const {
  // The children are: <type> <parameter>* <block>
  assert(this->child.size() >= 2);
  size_t parameters = this->child.size() - 2;
  auto function = std::make_shared<EggParserTypeFunction>(context.promote(*this->child[0])->getType());
  egg::lang::String parameter_name;
  std::shared_ptr<IEggProgramType> parameter_type;
  for (size_t i = 1; i <= parameters; ++i) {
    // We promote the parameter, extract the name/type/optional information and then discard it
    auto parameter = context.promote(*this->child[i]);
    auto parameter_optional = parameter->symbol(parameter_name, parameter_type);
    function->addParameter(parameter_name, parameter_type, parameter_optional);
  }
  EggParserContextNested nested(context, EggParserAllowed::Return|EggParserAllowed::Yield);
  auto block = nested.promote(*this->child[parameters + 1]);
  return std::make_shared<EggParserNode_FunctionDefinition>(this->name, function, block);
}

std::shared_ptr<egg::yolk::IEggProgramNode> egg::yolk::EggSyntaxNode_Parameter::promote(egg::yolk::IEggParserContext& context) const {
  auto type = context.promote(*this->child)->getType();
  return std::make_shared<EggParserNode_FunctionParameter>(this->name, type, this->optional);
}

std::shared_ptr<egg::yolk::IEggProgramNode> egg::yolk::EggSyntaxNode_Return::promote(egg::yolk::IEggParserContext& context) const {
  if (!context.isAllowed(EggParserAllowed::Return)) {
    throw exceptionFromLocation(context, "The 'return' statement may only be used within functions", *this);
  }
  if (this->child.empty()) {
    return std::make_shared<EggParserNode_Return>();
  }
  assert(this->child.size() == 1);
  return std::make_shared<EggParserNode_Return>(context.promote(*this->child[0]));
}

std::shared_ptr<egg::yolk::IEggProgramNode> egg::yolk::EggSyntaxNode_Switch::promote(egg::yolk::IEggParserContext& context) const {
  EggParserContextSwitch nested(context, *this->child[0]);
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
  return std::make_shared<EggParserNode_Throw>(expr);
}

std::shared_ptr<egg::yolk::IEggProgramNode> egg::yolk::EggSyntaxNode_Try::promote(egg::yolk::IEggParserContext& context) const {
  // TODO There's some ambiguity amongst C++/Java/C# as to whether 'break' inside here breaks out of any enclosing loop
  // The 'switch' pattern is even more confusing, so we just disallow 'break'/'continue' inside 'try'/'catch'/'finally' for now
  EggParserContextNested nested1(context, EggParserAllowed::None, EggParserAllowed::Rethrow|EggParserAllowed::Return|EggParserAllowed::Yield);
  auto clauses = this->child.size();
  assert(clauses >= 2);
  auto block = nested1.promote(*this->child[0]);
  auto result = std::make_shared<EggParserNode_Try>(block);
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

std::shared_ptr<egg::yolk::IEggProgramNode> egg::yolk::EggSyntaxNode_Using::promote(egg::yolk::IEggParserContext& context) const {
  auto expr = context.promote(*this->child[0]);
  auto block = context.promote(*this->child[1]);
  return std::make_shared<EggParserNode_Using>(expr, block);
}

std::shared_ptr<egg::yolk::IEggProgramNode> egg::yolk::EggSyntaxNode_While::promote(egg::yolk::IEggParserContext& context) const {
  auto condition = context.promote(*this->child[0]);
  EggParserContextNested nested(context, EggParserAllowed::Break|EggParserAllowed::Continue, EggParserAllowed::Rethrow|EggParserAllowed::Return|EggParserAllowed::Yield);
  auto block = nested.promote(*this->child[1]);
  return std::make_shared<EggParserNode_While>(condition, block);
}

std::shared_ptr<egg::yolk::IEggProgramNode> egg::yolk::EggSyntaxNode_Yield::promote(egg::yolk::IEggParserContext& context) const {
  if (!context.isAllowed(EggParserAllowed::Yield)) {
    throw exceptionFromLocation(context, "The 'yield' statement may only be used within generator functions", *this);
  }
  return std::make_shared<EggParserNode_Yield>(context.promote(*this->child));
}

std::shared_ptr<egg::yolk::IEggProgramNode> egg::yolk::EggSyntaxNode_UnaryOperator::promote(egg::yolk::IEggParserContext& context) const {
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

std::shared_ptr<egg::yolk::IEggProgramNode> egg::yolk::EggSyntaxNode_BinaryOperator::promote(egg::yolk::IEggParserContext& context) const {
  switch (this->op) {
  case EggTokenizerOperator::BangEqual:
    return this->promoteBinary<EggParserNode_BinaryUnequal>(context);
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

std::shared_ptr<egg::yolk::IEggProgramNode> egg::yolk::EggSyntaxNode_TernaryOperator::promote(egg::yolk::IEggParserContext& context) const {
  auto condition = context.promote(*this->child[0]);
  auto whenTrue = context.promote(*this->child[1]);
  auto whenFalse = context.promote(*this->child[2]);
  return std::make_shared<EggParserNode_Ternary>(condition, whenTrue, whenFalse);
}

std::shared_ptr<egg::yolk::IEggProgramNode> egg::yolk::EggSyntaxNode_Call::promote(egg::yolk::IEggParserContext& context) const {
  auto children = this->child.size();
  assert(children >= 1);
  auto callee = context.promote(*this->child[0]);
  auto result = std::make_shared<EggParserNode_Call>(callee);
  for (size_t i = 1; i < children; ++i) {
    result->addParameter(context.promote(*this->child[i]));
  }
  return result;
}

std::shared_ptr<egg::yolk::IEggProgramNode> egg::yolk::EggSyntaxNode_Named::promote(egg::yolk::IEggParserContext& context) const {
  return std::make_shared<EggParserNode_Named>(this->name, context.promote(*this->child));
}

std::shared_ptr<egg::yolk::IEggProgramNode> egg::yolk::EggSyntaxNode_Identifier::promote(egg::yolk::IEggParserContext&) const {
  return std::make_shared<EggParserNode_Identifier>(this->name);
}

std::shared_ptr<egg::yolk::IEggProgramNode> egg::yolk::EggSyntaxNode_Literal::promote(egg::yolk::IEggParserContext& context) const {
  if (this->kind == EggTokenizerKind::Integer) {
    return std::make_shared<EggParserNode_LiteralInteger>(this->value.i);
  }
  if (this->kind == EggTokenizerKind::Float) {
    return std::make_shared<EggParserNode_LiteralFloat>(this->value.f);
  }
  if (this->kind == EggTokenizerKind::String) {
    return std::make_shared<EggParserNode_LiteralString>(this->value.s);
  }
  if (this->kind != EggTokenizerKind::Keyword) {
    throw exceptionFromToken(context, "Unknown literal value type", *this);
  }
  if (this->value.k == EggTokenizerKeyword::Null) {
    return constNull;
  }
  if (this->value.k == EggTokenizerKeyword::False) {
    return constFalse;
  }
  if (this->value.k == EggTokenizerKeyword::True) {
    return constTrue;
  }
  throw exceptionFromToken(context, "Unknown literal value keyword", *this);
}

std::shared_ptr<egg::yolk::IEggProgramType> EggParserNode_UnaryLogicalNot::getType() const {
  return typeBool;
}

std::shared_ptr<egg::yolk::IEggProgramType> EggParserNode_UnaryRef::getType() const {
  auto underlying = this->expr->getType();
  assert(underlying != nullptr);
  return std::make_shared<EggParserTypeReference>(underlying, false);
}

std::shared_ptr<egg::yolk::IEggProgramType> EggParserNode_UnaryDeref::getType() const {
  auto underlying = this->expr->getType();
  assert(underlying != nullptr);
  return underlying->dereferencedType();
}

std::shared_ptr<egg::yolk::IEggProgramType> EggParserNode_UnaryNegate::getType() const {
  auto arithmetic = getArithmeticTypes(this->expr);
  return makeArithmeticType(arithmetic);
}

std::shared_ptr<egg::yolk::IEggProgramType> EggParserNode_UnaryEllipsis::getType() const {
  EGG_THROW(__FUNCTION__ " TODO"); // TODO
}

std::shared_ptr<egg::yolk::IEggProgramType> EggParserNode_UnaryBitwiseNot::getType() const {
  return typeInt;
}

std::shared_ptr<egg::yolk::IEggProgramType> EggParserNode_BinaryUnequal::getType() const {
  return typeBool;
}

std::shared_ptr<egg::yolk::IEggProgramType> EggParserNode_BinaryRemainder::getType() const {
  // See http://mindprod.com/jgloss/modulus.html
  // Turn out this equates to the rules for binary-multiply too
  return binaryArithmeticTypes(this->lhs, this->rhs);
}

std::shared_ptr<egg::yolk::IEggProgramType> EggParserNode_BinaryBitwiseAnd::getType() const {
  return typeInt;
}

std::shared_ptr<egg::yolk::IEggProgramType> EggParserNode_BinaryLogicalAnd::getType() const {
  return typeBool;
}

std::shared_ptr<egg::yolk::IEggProgramType> EggParserNode_BinaryMultiply::getType() const {
  return binaryArithmeticTypes(this->lhs, this->rhs);
}

std::shared_ptr<egg::yolk::IEggProgramType> EggParserNode_BinaryPlus::getType() const {
  return binaryArithmeticTypes(this->lhs, this->rhs);
}

std::shared_ptr<egg::yolk::IEggProgramType> EggParserNode_BinaryMinus::getType() const {
  return binaryArithmeticTypes(this->lhs, this->rhs);
}

std::shared_ptr<egg::yolk::IEggProgramType> EggParserNode_BinaryLambda::getType() const {
  EGG_THROW(__FUNCTION__ " TODO"); // TODO
}

std::shared_ptr<egg::yolk::IEggProgramType> EggParserNode_BinaryDot::getType() const {
  EGG_THROW(__FUNCTION__ " TODO"); // TODO
}

std::shared_ptr<egg::yolk::IEggProgramType> EggParserNode_BinaryDivide::getType() const {
  return binaryArithmeticTypes(this->lhs, this->rhs);
}

std::shared_ptr<egg::yolk::IEggProgramType> EggParserNode_BinaryLess::getType() const {
  return typeBool;
}

std::shared_ptr<egg::yolk::IEggProgramType> EggParserNode_BinaryShiftLeft::getType() const {
  return typeInt;
}

std::shared_ptr<egg::yolk::IEggProgramType> EggParserNode_BinaryLessEqual::getType() const {
  return typeBool;
}

std::shared_ptr<egg::yolk::IEggProgramType> EggParserNode_BinaryEqual::getType() const {
  return typeBool;
}

std::shared_ptr<egg::yolk::IEggProgramType> EggParserNode_BinaryGreater::getType() const {
  return typeBool;
}

std::shared_ptr<egg::yolk::IEggProgramType> EggParserNode_BinaryGreaterEqual::getType() const {
  return typeBool;
}

std::shared_ptr<egg::yolk::IEggProgramType> EggParserNode_BinaryShiftRight::getType() const {
  return typeInt;
}

std::shared_ptr<egg::yolk::IEggProgramType> EggParserNode_BinaryShiftRightUnsigned::getType() const {
  return typeInt;
}

std::shared_ptr<egg::yolk::IEggProgramType> EggParserNode_BinaryNullCoalescing::getType() const {
  auto type1 = this->lhs->getType();
  assert(type1 != nullptr);
  if (!type1->hasSimpleType(egg::lang::Discriminator::Null)) {
    // The left-hand-side cannot be null, so the right side is irrelevant
    return type1;
  }
  auto type2 = this->rhs->getType();
  assert(type2 != nullptr);
  return type1->nullableType(false)->unionWith(*type2);
}

std::shared_ptr<egg::yolk::IEggProgramType> EggParserNode_BinaryBrackets::getType() const {
  EGG_THROW(__FUNCTION__ " TODO"); // TODO
}

std::shared_ptr<egg::yolk::IEggProgramType> EggParserNode_BinaryBitwiseXor::getType() const {
  return typeInt;
}

std::shared_ptr<egg::yolk::IEggProgramType> EggParserNode_BinaryBitwiseOr::getType() const {
  return typeInt;
}

std::shared_ptr<egg::yolk::IEggProgramType> EggParserNode_BinaryLogicalOr::getType() const {
  return typeBool;
}

std::shared_ptr<egg::yolk::IEggProgramType> EggParserNode_Ternary::getType() const {
  auto type1 = this->condition->getType();
  assert(type1 != nullptr);
  if (!type1->hasSimpleType(egg::lang::Discriminator::Bool)) {
    // The condition is not a bool, so the other values are irrelevant
    return typeVoid;
  }
  auto type2 = this->whenTrue->getType();
  assert(type2 != nullptr);
  auto type3 = this->whenFalse->getType();
  assert(type3 != nullptr);
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
