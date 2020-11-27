#include "yolk/yolk.h"
#include "yolk/lexers.h"
#include "yolk/egg-tokenizer.h"
#include "yolk/egg-syntax.h"
#include "yolk/egg-parser.h"
#include "yolk/egg-engine.h"
#include "yolk/egg-program.h"

namespace {
  using namespace egg::yolk;

#define EGG_PROGRAM_OPERATOR_STRING(name, text) text,

  std::string unaryToString(EggProgramUnary op) {
    static const char* const table[] = {
      EGG_PROGRAM_UNARY_OPERATORS(EGG_PROGRAM_OPERATOR_STRING)
      "" // Stops GCC 7.3 complaining with error: array subscript is above array bounds [-Werror=array-bounds]
    };
    auto index = static_cast<size_t>(op);
    assert(index < EGG_NELEMS(table));
    return table[index];
  }

  std::string binaryToString(EggProgramBinary op) {
    static const char* const table[] = {
      EGG_PROGRAM_BINARY_OPERATORS(EGG_PROGRAM_OPERATOR_STRING)
      "" // Stops GCC 7.3 complaining with error: array subscript is above array bounds [-Werror=array-bounds]
    };
    auto index = static_cast<size_t>(op);
    assert(index < EGG_NELEMS(table));
    return table[index];
  }

  std::string assignToString(EggProgramAssign op) {
    static const char* const table[] = {
      EGG_PROGRAM_ASSIGN_OPERATORS(EGG_PROGRAM_OPERATOR_STRING)
      "" // Stops GCC 7.3 complaining with error: array subscript is above array bounds [-Werror=array-bounds]
    };
    auto index = static_cast<size_t>(op);
    assert(index < EGG_NELEMS(table));
    return table[index];
  }

  std::string mutateToString(EggProgramMutate op) {
    static const char* const table[] = {
      EGG_PROGRAM_MUTATE_OPERATORS(EGG_PROGRAM_OPERATOR_STRING)
      "" // Stops GCC 7.3 complaining with error: array subscript is above array bounds [-Werror=array-bounds]
    };
    auto index = static_cast<size_t>(op);
    assert(index < EGG_NELEMS(table));
    return table[index];
  }

  inline EggParserAllowed operator|(EggParserAllowed lhs, EggParserAllowed rhs) {
    return egg::ovum::Bits::set(lhs, rhs);
  }

  template<typename T, typename... ARGS>
  std::shared_ptr<T> makeParserNode(const IEggParserContext& context, const IEggSyntaxNode& node, ARGS&&... args) {
    // Fetch the syntax node's location and create a new 'T' based on it
    egg::ovum::LocationSource location(context.getResourceName(), node.location().begin.line, node.location().begin.column);
    return std::make_shared<T>(context.getAllocator(), location, std::forward<ARGS>(args)...);
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

  egg::ovum::Type unaryArithmeticType(const std::shared_ptr<IEggProgramNode>& expr) {
    switch (EggProgram::arithmeticTypes(expr->getType())) {
    case EggProgram::ArithmeticTypes::None:
      break;
    case EggProgram::ArithmeticTypes::Int:
      return egg::ovum::Type::Int;
    case EggProgram::ArithmeticTypes::Float:
      return egg::ovum::Type::Float;
    case EggProgram::ArithmeticTypes::Both:
      return egg::ovum::Type::Arithmetic;
    }
    return egg::ovum::Type::Void;
  }

  egg::ovum::Type binaryArithmeticType(const std::shared_ptr<IEggProgramNode>& lhs, const std::shared_ptr<IEggProgramNode>& rhs) {
    auto lhsa = EggProgram::arithmeticTypes(lhs->getType());
    auto rhsa = EggProgram::arithmeticTypes(rhs->getType());
    if ((lhsa == EggProgram::ArithmeticTypes::None) || (rhsa == EggProgram::ArithmeticTypes::None)) {
      return egg::ovum::Type::Void;
    }
    if ((lhsa == EggProgram::ArithmeticTypes::Int) && (rhsa == EggProgram::ArithmeticTypes::Int)) {
      return egg::ovum::Type::Int;
    }
    if ((lhsa == EggProgram::ArithmeticTypes::Float) || (rhsa == EggProgram::ArithmeticTypes::Float)) {
      return egg::ovum::Type::Float;
    }
    return egg::ovum::Type::Arithmetic;
  }

  egg::ovum::Type binaryBitwiseType(egg::ovum::IAllocator& allocator, const std::shared_ptr<IEggProgramNode>& lhs, const std::shared_ptr<IEggProgramNode>& rhs) {
    auto common = egg::ovum::Bits::mask(lhs->getType()->getFlags(), rhs->getType()->getFlags());
    common = egg::ovum::Bits::mask(common, egg::ovum::ValueFlags::Bool | egg::ovum::ValueFlags::Int);
    if (common == egg::ovum::ValueFlags::None) {
      return egg::ovum::Type::Void;
    }
    return egg::ovum::TypeFactory::createSimple(allocator, common);
  }

  SyntaxException exceptionFromLocation(const IEggParserContext& context, const std::string& reason, const EggSyntaxNodeLocation& location) {
    return SyntaxException(reason, context.getResourceName().toUTF8(), location);
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
      : os(&os) {
      *this->os << '(' << text;
    }
    ~ParserDump() {
      *this->os << ')';
    }
    ParserDump& add(const std::string& text) {
      *this->os << ' ' << '\'' << text << '\'';
      return *this;
    }
    ParserDump& add(const egg::ovum::String& text) {
      *this->os << ' ' << '\'' << text.toUTF8() << '\'';
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
    static void simple(std::ostream& os, const char* text) {
      os << '(' << text << ')';
    }

  };

  class EggParserNodeBase : public IEggProgramNode {
  protected:
    egg::ovum::LocationSource locationSource;
  public:
    explicit EggParserNodeBase(const egg::ovum::LocationSource& locationSource)
      : locationSource(locationSource) {
    }
    virtual egg::ovum::Type getType() const override {
      // By default, nodes are statements (i.e. void return type)
      return egg::ovum::Type::Void;
    }
    virtual egg::ovum::LocationSource location() const override {
      // Just return the source location
      return this->locationSource;
    }
    virtual void empredicate(EggProgramContext&, std::shared_ptr<IEggProgramNode>&) override {
      // By default, nodes do not change when they're used as predicates in function call parameters
    }
    virtual bool symbol(egg::ovum::String&, egg::ovum::Type&) const override {
      // By default, nodes do not declare symbols
      return false;
    }
  };

  class EggParserNode_Module : public EggParserNodeBase {
  private:
    std::vector<std::shared_ptr<IEggProgramNode>> child;
  public:
    EggParserNode_Module(egg::ovum::IAllocator&, const egg::ovum::LocationSource& locationSource)
      : EggParserNodeBase(locationSource) {
    }
    virtual EggProgramNodeFlags prepare(EggProgramContext& context) override {
      return context.prepareModule(this->child);
    }
    virtual void dump(std::ostream& os) const override {
      ParserDump(os, "module").add(this->child);
    }
    virtual egg::ovum::Node compile(EggProgramCompiler& compiler) const override {
      auto block = compiler.statement(this->locationSource, egg::ovum::Opcode::BLOCK, this->child);
      if (block == nullptr) {
        return nullptr;
      }
      return compiler.statement(this->locationSource, egg::ovum::Opcode::MODULE, block);
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
    EggParserNode_Block(egg::ovum::IAllocator&, const egg::ovum::LocationSource& locationSource)
      : EggParserNodeBase(locationSource) {
    }
    virtual EggProgramNodeFlags prepare(EggProgramContext& context) override {
      return context.prepareBlock(this->child);
    }
    virtual void dump(std::ostream& os) const override {
      ParserDump(os, "block").add(this->child);
    }
    virtual egg::ovum::Node compile(EggProgramCompiler& compiler) const override {
      return compiler.statement(this->locationSource, egg::ovum::Opcode::BLOCK, this->child);
    }
    void addChild(const std::shared_ptr<IEggProgramNode>& statement) {
      assert(statement != nullptr);
      this->child.push_back(statement);
    }
  };

  class EggParserNode_Type : public EggParserNodeBase {
  private:
    egg::ovum::Type type;
  public:
    EggParserNode_Type(egg::ovum::IAllocator&, const egg::ovum::LocationSource& locationSource, const egg::ovum::Type& type)
      : EggParserNodeBase(locationSource), type(type) {
    }
    virtual egg::ovum::Type getType() const override {
      return this->type;
    }
    virtual EggProgramNodeFlags prepare(EggProgramContext&) override {
      // Nothing to do: we're just a holder for the type
      return EggProgramNodeFlags::None;
    }
    virtual void dump(std::ostream& os) const override {
      ParserDump(os, "type").add(this->type.toString());
    }
    virtual egg::ovum::Node compile(EggProgramCompiler& compiler) const override {
      // This is used, for example, in catch clauses
      return compiler.type(this->locationSource, this->type);
    }
  };

  class EggParserNode_Declare : public EggParserNodeBase {
  private:
    egg::ovum::String name;
    egg::ovum::Type type;
    std::shared_ptr<IEggProgramNode> init;
  public:
    EggParserNode_Declare(egg::ovum::IAllocator&, const egg::ovum::LocationSource& locationSource, const egg::ovum::String& name, const egg::ovum::Type& type, const std::shared_ptr<IEggProgramNode>& init = nullptr)
      : EggParserNodeBase(locationSource), name(name), type(type), init(init) {
    }
    virtual bool symbol(egg::ovum::String& nameOut, egg::ovum::Type& typeOut) const override {
      // The symbol is obviously the variable being declared
      nameOut = this->name;
      typeOut = this->type;
      return true;
    }
    virtual EggProgramNodeFlags prepare(EggProgramContext& context) override {
      return context.prepareDeclare(this->locationSource, this->name, this->type, this->init.get());
    }
    virtual void dump(std::ostream& os) const override {
      auto tname = (this->type == nullptr) ? "var" : this->type.toString();
      ParserDump(os, "declare").add(this->name).add(tname).add(this->init);
    }
    virtual egg::ovum::Node compile(EggProgramCompiler& compiler) const override {
      if (this->init == nullptr) {
        // No initializer
        return compiler.statement(this->locationSource, egg::ovum::Opcode::DECLARE, compiler.type(this->locationSource, this->type), compiler.identifier(this->locationSource, this->name));
      }
      return compiler.statement(this->locationSource, egg::ovum::Opcode::DECLARE, compiler.type(this->locationSource, this->type), compiler.identifier(this->locationSource, this->name), *this->init);
    }
  };

  class EggParserNode_Guard : public EggParserNodeBase {
  private:
    egg::ovum::String name;
    egg::ovum::Type type;
    std::shared_ptr<IEggProgramNode> expr;
  public:
    EggParserNode_Guard(egg::ovum::IAllocator&, const egg::ovum::LocationSource& locationSource, const egg::ovum::String& name, const egg::ovum::Type& type, const std::shared_ptr<IEggProgramNode>& expr)
      : EggParserNodeBase(locationSource), name(name), type(type), expr(expr) {
    }
    virtual bool symbol(egg::ovum::String& nameOut, egg::ovum::Type& typeOut) const override {
      // The symbol is obviously the variable being declared
      nameOut = this->name;
      typeOut = this->type;
      return true;
    }
    virtual EggProgramNodeFlags prepare(EggProgramContext& context) override {
      return context.prepareGuard(this->locationSource, this->name, this->type, *this->expr);
    }
    virtual void dump(std::ostream& os) const override {
      ParserDump(os, "guard").add(this->name).add(this->type.toString()).add(this->expr);
    }
    virtual egg::ovum::Node compile(EggProgramCompiler& compiler) const override {
      return compiler.statement(this->locationSource, egg::ovum::Opcode::GUARD, compiler.type(this->locationSource, this->type), compiler.identifier(this->locationSource, this->name), this->expr);
    }
  };

  class EggParserNode_Mutate : public EggParserNodeBase {
  private:
    EggProgramMutate op;
    std::shared_ptr<IEggProgramNode> lvalue;
  public:
    EggParserNode_Mutate(egg::ovum::IAllocator&, const egg::ovum::LocationSource& locationSource, EggProgramMutate op, const std::shared_ptr<IEggProgramNode>& lvalue)
      : EggParserNodeBase(locationSource), op(op), lvalue(lvalue) {
      assert(lvalue != nullptr);
    }
    virtual EggProgramNodeFlags prepare(EggProgramContext& context) override {
      return context.prepareMutate(this->locationSource, this->op, *this->lvalue);
    }
    virtual void dump(std::ostream& os) const override {
      ParserDump(os, "mutate").add(mutateToString(this->op)).add(this->lvalue);
    }
    virtual egg::ovum::Node compile(EggProgramCompiler& compiler) const override {
      return compiler.mutate(this->locationSource, this->op, *this->lvalue);
    }
  };

  class EggParserNode_Break : public EggParserNodeBase {
  public:
    EggParserNode_Break(egg::ovum::IAllocator&, const egg::ovum::LocationSource& locationSource)
      : EggParserNodeBase(locationSource) {
    }
    virtual EggProgramNodeFlags prepare(EggProgramContext&) override {
      // Nothing to prepare, but we fallthrough
      return EggProgramNodeFlags::Fallthrough;
    }
    virtual void dump(std::ostream& os) const override {
      ParserDump::simple(os, "break");
    }
    virtual egg::ovum::Node compile(EggProgramCompiler& compiler) const override {
      return compiler.opcode(this->locationSource, egg::ovum::Opcode::BREAK);
    }
  };

  class EggParserNode_Catch : public EggParserNodeBase {
  private:
    egg::ovum::String name;
    std::shared_ptr<IEggProgramNode> type;
    std::shared_ptr<IEggProgramNode> block;
  public:
    EggParserNode_Catch(egg::ovum::IAllocator&, const egg::ovum::LocationSource& locationSource, const egg::ovum::String& name, const std::shared_ptr<IEggProgramNode>& type, const std::shared_ptr<IEggProgramNode>& block)
      : EggParserNodeBase(locationSource), name(name), type(type), block(block) {
      assert(type != nullptr);
      assert(block != nullptr);
    }
    virtual EggProgramNodeFlags prepare(EggProgramContext& context) override {
      return context.prepareCatch(this->name, *this->type, *this->block);
    }
    virtual void dump(std::ostream& os) const override {
      ParserDump(os, "catch").add(this->name).add(this->type).add(this->block);
    }
    virtual egg::ovum::Node compile(EggProgramCompiler& compiler) const override {
      return compiler.statement(this->locationSource, egg::ovum::Opcode::CATCH, this->type, compiler.identifier(this->locationSource, this->name), this->block);
    }
  };

  class EggParserNode_Continue : public EggParserNodeBase {
  public:
    EggParserNode_Continue(egg::ovum::IAllocator&, const egg::ovum::LocationSource& locationSource)
      : EggParserNodeBase(locationSource) {
    }
    virtual EggProgramNodeFlags prepare(EggProgramContext&) override {
      // Nothing to prepare, but no fallthrough
      return EggProgramNodeFlags::None;
    }
    virtual void dump(std::ostream& os) const override {
      ParserDump::simple(os, "continue");
    }
    virtual egg::ovum::Node compile(EggProgramCompiler& compiler) const override {
      return compiler.opcode(this->locationSource, egg::ovum::Opcode::CONTINUE);
    }
  };

  class EggParserNode_Do : public EggParserNodeBase {
  private:
    std::shared_ptr<IEggProgramNode> condition;
    std::shared_ptr<IEggProgramNode> block;
  public:
    EggParserNode_Do(egg::ovum::IAllocator&, const egg::ovum::LocationSource& locationSource, const std::shared_ptr<IEggProgramNode>& condition, const std::shared_ptr<IEggProgramNode>& block)
      : EggParserNodeBase(locationSource), condition(condition), block(block) {
      assert(condition != nullptr);
      assert(block != nullptr);
    }
    virtual EggProgramNodeFlags prepare(EggProgramContext& context) override {
      return context.prepareDo(*this->condition, *this->block);
    }
    virtual void dump(std::ostream& os) const override {
      ParserDump(os, "do").add(this->condition).add(this->block);
    }
    virtual egg::ovum::Node compile(EggProgramCompiler& compiler) const override {
      return compiler.statement(this->locationSource, egg::ovum::Opcode::DO, this->condition, this->block);
    }
  };

  class EggParserNode_If : public EggParserNodeBase {
  private:
    std::shared_ptr<IEggProgramNode> condition;
    std::shared_ptr<IEggProgramNode> trueBlock;
    std::shared_ptr<IEggProgramNode> falseBlock;
  public:
    EggParserNode_If(egg::ovum::IAllocator&, const egg::ovum::LocationSource& locationSource, const std::shared_ptr<IEggProgramNode>& condition, const std::shared_ptr<IEggProgramNode>& trueBlock, const std::shared_ptr<IEggProgramNode>& falseBlock)
      : EggParserNodeBase(locationSource), condition(condition), trueBlock(trueBlock), falseBlock(falseBlock) {
      assert(condition != nullptr);
      assert(trueBlock != nullptr);
      // falseBlock may be null if the 'else' clause is missing
    }
    virtual EggProgramNodeFlags prepare(EggProgramContext& context) override {
      return context.prepareIf(*this->condition, *this->trueBlock, this->falseBlock.get());
    }
    virtual void dump(std::ostream& os) const override {
      ParserDump(os, "if").add(this->condition).add(this->trueBlock).add(this->falseBlock);
    }
    virtual egg::ovum::Node compile(EggProgramCompiler& compiler) const override {
      if (this->falseBlock == nullptr) {
        // No else clause
        return compiler.statement(this->locationSource, egg::ovum::Opcode::IF, this->condition, this->trueBlock);
      }
      return compiler.statement(this->locationSource, egg::ovum::Opcode::IF, this->condition, this->trueBlock, this->falseBlock);
    }
  };

  class EggParserNode_For : public EggParserNodeBase {
  private:
    std::shared_ptr<IEggProgramNode> pre;
    std::shared_ptr<IEggProgramNode> cond;
    std::shared_ptr<IEggProgramNode> post;
    std::shared_ptr<IEggProgramNode> block;
  public:
    EggParserNode_For(egg::ovum::IAllocator&, const egg::ovum::LocationSource& locationSource, const std::shared_ptr<IEggProgramNode>& pre, const std::shared_ptr<IEggProgramNode>& cond, const std::shared_ptr<IEggProgramNode>& post, const std::shared_ptr<IEggProgramNode>& block)
      : EggParserNodeBase(locationSource), pre(pre), cond(cond), post(post), block(block) {
      // pre/cond/post may be null
      assert(block != nullptr);
    }
    virtual EggProgramNodeFlags prepare(EggProgramContext& context) override {
      return context.prepareFor(this->pre.get(), this->cond.get(), this->post.get(), *this->block);
    }
    virtual void dump(std::ostream& os) const override {
      ParserDump(os, "for").add(this->pre).add(this->cond).add(this->post).add(this->block);
    }
    virtual egg::ovum::Node compile(EggProgramCompiler& compiler) const override {
      return compiler.statement(this->locationSource, egg::ovum::Opcode::FOR, compiler.noop(this->locationSource, this->pre.get()), compiler.noop(this->locationSource, this->cond.get()), compiler.noop(this->locationSource, this->post.get()), this->block);
    }
  };

  class EggParserNode_Foreach : public EggParserNodeBase {
  private:
    std::shared_ptr<IEggProgramNode> target;
    std::shared_ptr<IEggProgramNode> expr;
    std::shared_ptr<IEggProgramNode> block;
  public:
    EggParserNode_Foreach(egg::ovum::IAllocator&, const egg::ovum::LocationSource& locationSource, const std::shared_ptr<IEggProgramNode>& target, const std::shared_ptr<IEggProgramNode>& expr, const std::shared_ptr<IEggProgramNode>& block)
      : EggParserNodeBase(locationSource), target(target), expr(expr), block(block) {
      assert(target != nullptr);
      assert(expr != nullptr);
      assert(block != nullptr);
    }
    virtual EggProgramNodeFlags prepare(EggProgramContext& context) override {
      return context.prepareForeach(*this->target, *this->expr, *this->block);
    }
    virtual void dump(std::ostream& os) const override {
      ParserDump(os, "foreach").add(this->target).add(this->expr).add(this->block);
    }
    virtual egg::ovum::Node compile(EggProgramCompiler& compiler) const override {
      return compiler.statement(this->locationSource, egg::ovum::Opcode::FOREACH, this->target, this->expr, this->block);
    }
  };

  class EggParserNode_FunctionDefinition : public EggParserNodeBase {
  private:
    egg::ovum::String name;
    egg::ovum::Type type;
    std::shared_ptr<IEggProgramNode> block;
  public:
    EggParserNode_FunctionDefinition(egg::ovum::IAllocator&, const egg::ovum::LocationSource& locationSource, const egg::ovum::String& name, const egg::ovum::Type& type, const std::shared_ptr<IEggProgramNode>& block)
      : EggParserNodeBase(locationSource), name(name), type(type), block(block) {
      assert(type != nullptr);
      assert(block != nullptr);
    }
    virtual bool symbol(egg::ovum::String& nameOut, egg::ovum::Type& typeOut) const override {
      // The symbol is obviously the identifier being defined
      nameOut = this->name;
      typeOut = this->type;
      return true;
    }
    virtual EggProgramNodeFlags prepare(EggProgramContext& context) override {
      return context.prepareFunctionDefinition(this->name, this->type, this->block);
    }
    virtual void dump(std::ostream& os) const override {
      ParserDump(os, "function").add(this->name).add(this->type.toString()).add(this->block);
    }
    virtual egg::ovum::Node compile(EggProgramCompiler& compiler) const override {
      if (this->name.empty()) {
        return compiler.statement(this->locationSource, egg::ovum::Opcode::FUNCTION, compiler.type(this->locationSource, this->type), this->block);
      }
      return compiler.statement(this->locationSource, egg::ovum::Opcode::FUNCTION, compiler.type(this->locationSource, this->type), this->block, compiler.identifier(this->locationSource, this->name));
    }
  };

  class EggParserNode_FunctionParameter : public EggParserNodeBase {
  private:
    egg::ovum::String name;
    egg::ovum::Type type;
    bool optional;
  public:
    EggParserNode_FunctionParameter(egg::ovum::IAllocator&, const egg::ovum::LocationSource& locationSource, const egg::ovum::String& name, const egg::ovum::Type& type, bool optional)
      : EggParserNodeBase(locationSource), name(name), type(type), optional(optional) {
    }
    virtual bool symbol(egg::ovum::String& nameOut, egg::ovum::Type& typeOut) const override {
      // Beware: the return value is the optionality flag!
      nameOut = this->name;
      typeOut = this->type;
      return this->optional;
    }
    virtual EggProgramNodeFlags prepare(EggProgramContext& context) override {
      return context.compilerError(this->locationSource, "Internal parser error: Inappropriate 'prepare' call for function parameter");
    }
    virtual void dump(std::ostream& os) const override {
      ParserDump(os, this->optional ? "parameter?" : "parameter").add(this->name).add(this->type.toString());
    }
    virtual egg::ovum::Node compile(EggProgramCompiler& compiler) const override {
      return compiler.raise("Internal compile error: Inappropriate 'compile' call for function parameter");
    }
  };

  class EggParserNode_GeneratorDefinition : public EggParserNodeBase {
  private:
    egg::ovum::String name;
    egg::ovum::Type gentype; // e.g. 'int...(string, bool)'
    egg::ovum::Type rettype; // e.g. 'int'
    std::shared_ptr<IEggProgramNode> block;
  public:
    EggParserNode_GeneratorDefinition(egg::ovum::IAllocator&, const egg::ovum::LocationSource& locationSource, const egg::ovum::String& name, const egg::ovum::Type& gentype, const egg::ovum::Type& rettype, const std::shared_ptr<IEggProgramNode>& block)
      : EggParserNodeBase(locationSource), name(name), gentype(gentype), rettype(rettype), block(block) {
      assert(block != nullptr);
    }
    virtual EggProgramNodeFlags prepare(EggProgramContext& context) override {
      return context.prepareGeneratorDefinition(this->rettype, this->block);
    }
    virtual void dump(std::ostream& os) const override {
      ParserDump(os, "generator").add(this->name).add(this->gentype.toString()).add(this->block);
    }
    virtual egg::ovum::Node compile(EggProgramCompiler& compiler) const override {
      if (this->name.empty()) {
        return compiler.statement(this->locationSource, egg::ovum::Opcode::GENERATOR, compiler.type(this->locationSource, this->gentype), this->block);
      }
      return compiler.statement(this->locationSource, egg::ovum::Opcode::GENERATOR, compiler.type(this->locationSource, this->gentype), this->block, compiler.identifier(this->locationSource, this->name));
    }
  };

  class EggParserNode_Return : public EggParserNodeBase {
  private:
    std::shared_ptr<IEggProgramNode> expr;
  public:
    EggParserNode_Return(egg::ovum::IAllocator&, const egg::ovum::LocationSource& locationSource, const std::shared_ptr<IEggProgramNode>& expr = nullptr)
      : EggParserNodeBase(locationSource), expr(expr) {
    }
    virtual EggProgramNodeFlags prepare(EggProgramContext& context) override {
      return context.prepareReturn(this->locationSource, this->expr.get());
    }
    virtual void dump(std::ostream& os) const override {
      ParserDump(os, "return").add(this->expr);
    }
    virtual egg::ovum::Node compile(EggProgramCompiler& compiler) const override {
      if (this->expr == nullptr) {
        return compiler.opcode(this->locationSource, egg::ovum::Opcode::RETURN);
      }
      return compiler.statement(this->locationSource, egg::ovum::Opcode::RETURN, this->expr);
    }
  };

  class EggParserNode_Case : public EggParserNodeBase {
  private:
    std::vector<std::shared_ptr<IEggProgramNode>> child;
    std::shared_ptr<EggParserNode_Block> block;
    bool defclause;
  public:
    EggParserNode_Case(egg::ovum::IAllocator& allocator, const egg::ovum::LocationSource& locationSource)
      : EggParserNodeBase(locationSource), block(std::make_shared<EggParserNode_Block>(allocator, locationSource)), defclause(false) {
      assert(this->block != nullptr);
    }
    virtual EggProgramNodeFlags prepare(EggProgramContext& context) override {
      return context.prepareCase(this->child, *this->block);
    }
    virtual void dump(std::ostream& os) const override {
      ParserDump(os, "case").add(this->child).add(this->block);
    }
    virtual egg::ovum::Node compile(EggProgramCompiler& compiler) const override {
      const IEggProgramNode& clause = *this->block;
      return compiler.statement(this->locationSource, this->defclause ? egg::ovum::Opcode::DEFAULT : egg::ovum::Opcode::CASE, clause, this->child);
    }
    void addValue(const std::shared_ptr<IEggProgramNode>& value) {
      assert(value != nullptr);
      this->child.push_back(value);
    }
    void addStatement(const std::shared_ptr<IEggProgramNode>& statement) {
      assert(statement != nullptr);
      this->block->addChild(statement);
    }
    void setDefault() {
      assert(!this->defclause);
      this->defclause = true;
    }
  };

  class EggParserNode_Switch : public EggParserNodeBase {
  private:
    std::shared_ptr<IEggProgramNode> expr;
    int64_t defaultIndex;
    std::vector<std::shared_ptr<IEggProgramNode>> child;
    std::shared_ptr<EggParserNode_Case> latest;
  public:
    EggParserNode_Switch(egg::ovum::IAllocator&, const egg::ovum::LocationSource& locationSource, const std::shared_ptr<IEggProgramNode>& expr)
      : EggParserNodeBase(locationSource), expr(expr), defaultIndex(-1) {
      assert(expr != nullptr);
    }
    virtual EggProgramNodeFlags prepare(EggProgramContext& context) override {
      return context.prepareSwitch(*this->expr, this->defaultIndex, this->child);
    }
    virtual void dump(std::ostream& os) const override {
      ParserDump(os, "switch").add(this->expr).raw(this->defaultIndex).add(this->child);
    }
    virtual egg::ovum::Node compile(EggProgramCompiler& compiler) const override {
      return compiler.statement(this->locationSource, egg::ovum::Opcode::SWITCH, this->expr, this->child);
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
      if (this->defaultIndex >= 0) {
        return false;
      }
      if (this->latest == nullptr) {
        return false;
      }
      this->defaultIndex = static_cast<int64_t>(this->child.size()) - 1;
      this->latest->setDefault();
      return true;
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
    EggParserNode_Throw(egg::ovum::IAllocator&, const egg::ovum::LocationSource& locationSource, const std::shared_ptr<IEggProgramNode>& expr = nullptr)
      : EggParserNodeBase(locationSource), expr(expr) {
    }
    virtual EggProgramNodeFlags prepare(EggProgramContext& context) override {
      return context.prepareThrow(this->expr.get());
    }
    virtual void dump(std::ostream& os) const override {
      ParserDump(os, "throw").add(this->expr);
    }
    virtual egg::ovum::Node compile(EggProgramCompiler& compiler) const override {
      if (this->expr == nullptr) {
        // Rethrow
        return compiler.opcode(this->locationSource, egg::ovum::Opcode::THROW);
      }
      return compiler.statement(this->locationSource, egg::ovum::Opcode::THROW, this->expr);
    }
  };

  class EggParserNode_Try : public EggParserNodeBase {
  private:
    std::shared_ptr<IEggProgramNode> block;
    std::vector<std::shared_ptr<IEggProgramNode>> catches;
    std::shared_ptr<IEggProgramNode> final;
  public:
    EggParserNode_Try(egg::ovum::IAllocator&, const egg::ovum::LocationSource& locationSource, const std::shared_ptr<IEggProgramNode>& block)
      : EggParserNodeBase(locationSource), block(block) {
      assert(block != nullptr);
    }
    virtual EggProgramNodeFlags prepare(EggProgramContext& context) override {
      return context.prepareTry(*this->block, this->catches, this->final.get());
    }
    virtual void dump(std::ostream& os) const override {
      ParserDump(os, "try").add(this->block).add(this->catches).add(this->final);
    }
    virtual egg::ovum::Node compile(EggProgramCompiler& compiler) const override {
      return compiler.statement(this->locationSource, egg::ovum::Opcode::TRY, this->block, compiler.noop(this->locationSource, this->final.get()), this->catches);
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
    EggParserNode_While(egg::ovum::IAllocator&, const egg::ovum::LocationSource& locationSource, const std::shared_ptr<IEggProgramNode>& condition, const std::shared_ptr<IEggProgramNode>& block)
      : EggParserNodeBase(locationSource), condition(condition), block(block) {
      assert(condition != nullptr);
      assert(block != nullptr);
    }
    virtual EggProgramNodeFlags prepare(EggProgramContext& context) override {
      return context.prepareWhile(*this->condition, *this->block);
    }
    virtual void dump(std::ostream& os) const override {
      ParserDump(os, "while").add(this->condition).add(this->block);
    }
    virtual egg::ovum::Node compile(EggProgramCompiler& compiler) const override {
      return compiler.statement(this->locationSource, egg::ovum::Opcode::WHILE, this->condition, this->block);
    }
  };

  class EggParserNode_Yield : public EggParserNodeBase {
  private:
    std::shared_ptr<IEggProgramNode> expr;
  public:
    EggParserNode_Yield(egg::ovum::IAllocator&, const egg::ovum::LocationSource& locationSource, const std::shared_ptr<IEggProgramNode>& expr)
      : EggParserNodeBase(locationSource), expr(expr) {
      assert(expr != nullptr);
    }
    virtual EggProgramNodeFlags prepare(EggProgramContext& context) override {
      return context.prepareYield(this->locationSource, *this->expr);
    }
    virtual void dump(std::ostream& os) const override {
      ParserDump(os, "yield").add(this->expr);
    }
    virtual egg::ovum::Node compile(EggProgramCompiler& compiler) const override {
      if (this->expr == nullptr) {
        return compiler.opcode(this->locationSource, egg::ovum::Opcode::YIELD);
      }
      return compiler.statement(this->locationSource, egg::ovum::Opcode::YIELD, *this->expr);
    }
  };

  class EggParserNode_Named : public EggParserNodeBase {
  private:
    egg::ovum::String name;
    std::shared_ptr<IEggProgramNode> expr;
  public:
    EggParserNode_Named(egg::ovum::IAllocator&, const egg::ovum::LocationSource& locationSource, const egg::ovum::String& name, const std::shared_ptr<IEggProgramNode>& expr)
      : EggParserNodeBase(locationSource), name(name), expr(expr) {
      assert(expr != nullptr);
    }
    virtual bool symbol(egg::ovum::String& nameOut, egg::ovum::Type& typeOut) const override {
      // Use the symbol to extract the parameter name
      nameOut = this->name;
      typeOut = this->expr->getType();
      return true;
    }
    virtual EggProgramNodeFlags prepare(EggProgramContext& context) override {
      return this->expr->prepare(context);
    }
    virtual void dump(std::ostream& os) const override {
      ParserDump(os, "named").add(this->name).add(this->expr);
    }
    virtual egg::ovum::Node compile(EggProgramCompiler& compiler) const override {
      return compiler.expression(this->locationSource, egg::ovum::Opcode::NAMED, compiler.identifier(this->locationSource, this->name), this->expr);
    }
  };

  class EggParserNode_Array : public EggParserNodeBase {
  private:
    std::vector<std::shared_ptr<IEggProgramNode>> child;
  public:
    EggParserNode_Array(egg::ovum::IAllocator&, const egg::ovum::LocationSource& locationSource)
      : EggParserNodeBase(locationSource) {
    }
    virtual egg::ovum::Type getType() const override {
      return egg::ovum::Vanilla::getArrayType();
    }
    virtual EggProgramNodeFlags prepare(EggProgramContext& context) override {
      return context.prepareArray(this->child);
    }
    virtual void dump(std::ostream& os) const override {
      ParserDump(os, "array").add(this->child);
    }
    virtual egg::ovum::Node compile(EggProgramCompiler& compiler) const override {
      return compiler.expression(this->locationSource, egg::ovum::Opcode::AVALUE, this->child);
    }
    void addValue(const std::shared_ptr<IEggProgramNode>& value) {
      this->child.emplace_back(value);
    }
  };

  class EggParserNode_Object : public EggParserNodeBase {
  private:
    std::vector<std::shared_ptr<IEggProgramNode>> child; // All EggParserNode_Named instances in order
  public:
    EggParserNode_Object(egg::ovum::IAllocator&, const egg::ovum::LocationSource& locationSource)
      : EggParserNodeBase(locationSource) {
    }
    virtual egg::ovum::Type getType() const override {
      return egg::ovum::Vanilla::getDictionaryType();
    }
    virtual EggProgramNodeFlags prepare(EggProgramContext& context) override {
      return context.prepareObject(this->child);
    }
    virtual void dump(std::ostream& os) const override {
      ParserDump(os, "object").add(this->child);
    }
    virtual egg::ovum::Node compile(EggProgramCompiler& compiler) const override {
      return compiler.expression(this->locationSource, egg::ovum::Opcode::OVALUE, this->child);
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
    EggParserNode_Call(egg::ovum::IAllocator&, const egg::ovum::LocationSource& locationSource, const std::shared_ptr<IEggProgramNode>& callee)
      : EggParserNodeBase(locationSource), callee(callee) {
      assert(callee != nullptr);
    }
    virtual egg::ovum::Type getType() const override {
      // Get this from the function signature, if possible
      auto* signature = this->callee->getType().queryCallable();
      if (signature == nullptr) {
        return egg::ovum::Type::Void;
      }
      return signature->getReturnType();
    }
    virtual EggProgramNodeFlags prepare(EggProgramContext& context) override {
      return context.prepareCall(*this->callee, this->child);
    }
    virtual void dump(std::ostream& os) const override {
      ParserDump(os, "call").add(this->callee).add(this->child);
    }
    virtual egg::ovum::Node compile(EggProgramCompiler& compiler) const override {
      return compiler.expression(this->locationSource, egg::ovum::Opcode::CALL, this->callee, this->child);
    }
    void addParameter(const std::shared_ptr<IEggProgramNode>& parameter) {
      this->child.emplace_back(parameter);
    }
  };

  class EggParserNode_Identifier : public EggParserNodeBase {
  private:
    egg::ovum::String name;
    egg::ovum::Type type; // Initially 'Void' because we don't know until we're prepared
  public:
    EggParserNode_Identifier(egg::ovum::IAllocator&, const egg::ovum::LocationSource& locationSource, const egg::ovum::String& name)
      : EggParserNodeBase(locationSource), name(name), type(egg::ovum::Type::Void) {
    }
    virtual egg::ovum::Type getType() const override {
      return this->type;
    }
    virtual EggProgramNodeFlags prepare(EggProgramContext& context) override {
      return context.prepareIdentifier(this->locationSource, this->name, this->type);
    }
    virtual void dump(std::ostream& os) const override {
      ParserDump(os, "identifier").add(this->name);
    }
    virtual egg::ovum::Node compile(EggProgramCompiler& compiler) const override {
      return compiler.identifier(this->locationSource, this->name); // TODO byref
    }
  };

  class EggParserNode_LiteralNull : public EggParserNodeBase {
  public:
    EggParserNode_LiteralNull(egg::ovum::IAllocator&, const egg::ovum::LocationSource& locationSource)
      : EggParserNodeBase(locationSource) {
    }
    virtual egg::ovum::Type getType() const override {
      return egg::ovum::Type::Null;
    }
    virtual EggProgramNodeFlags prepare(EggProgramContext&) override {
      return EggProgramNodeFlags::Constant;
    }
    virtual void dump(std::ostream& os) const override {
      ParserDump::simple(os, "literal null");
    }
    virtual egg::ovum::Node compile(EggProgramCompiler& compiler) const override {
      return compiler.opcode(this->locationSource, egg::ovum::Opcode::NULL_);
    }
  };

  class EggParserNode_LiteralBool : public EggParserNodeBase {
  private:
    bool value;
  public:
    EggParserNode_LiteralBool(egg::ovum::IAllocator&, const egg::ovum::LocationSource& locationSource, bool value)
      : EggParserNodeBase(locationSource), value(value) {
    }
    virtual egg::ovum::Type getType() const override {
      return egg::ovum::Type::Bool;
    }
    virtual EggProgramNodeFlags prepare(EggProgramContext&) override {
      return EggProgramNodeFlags::Constant;
    }
    virtual void dump(std::ostream& os) const override {
      ParserDump(os, "literal bool").raw(this->value);
    }
    virtual egg::ovum::Node compile(EggProgramCompiler& compiler) const override {
      return compiler.opcode(this->locationSource, this->value ? egg::ovum::Opcode::TRUE : egg::ovum::Opcode::FALSE);
    }
  };

  class EggParserNode_LiteralInteger : public EggParserNodeBase {
  private:
    int64_t value;
  public:
    EggParserNode_LiteralInteger(egg::ovum::IAllocator&, const egg::ovum::LocationSource& locationSource, int64_t value)
      : EggParserNodeBase(locationSource), value(value) {
    }
    virtual egg::ovum::Type getType() const override {
      return egg::ovum::Type::Int;
    }
    virtual EggProgramNodeFlags prepare(EggProgramContext&) override {
      return EggProgramNodeFlags::Constant;
    }
    virtual void dump(std::ostream& os) const override {
      ParserDump(os, "literal int").raw(this->value);
    }
    virtual egg::ovum::Node compile(EggProgramCompiler& compiler) const override {
      return compiler.ivalue(this->locationSource, this->value);
    }
  };

  class EggParserNode_LiteralFloat : public EggParserNodeBase {
  private:
    double value;
  public:
    EggParserNode_LiteralFloat(egg::ovum::IAllocator&, const egg::ovum::LocationSource& locationSource, double value)
      : EggParserNodeBase(locationSource), value(value) {
    }
    virtual egg::ovum::Type getType() const override {
      return egg::ovum::Type::Float;
    }
    virtual EggProgramNodeFlags prepare(EggProgramContext&) override {
      return EggProgramNodeFlags::Constant;
    }
    virtual void dump(std::ostream& os) const override {
      ParserDump(os, "literal float").raw(this->value);
    }
    virtual egg::ovum::Node compile(EggProgramCompiler& compiler) const override {
      return compiler.fvalue(this->locationSource, this->value);
    }
  };

  class EggParserNode_LiteralString : public EggParserNodeBase {
  private:
    egg::ovum::String value;
  public:
    EggParserNode_LiteralString(egg::ovum::IAllocator&, const egg::ovum::LocationSource& locationSource, const egg::ovum::String& value)
      : EggParserNodeBase(locationSource), value(value) {
    }
    virtual egg::ovum::Type getType() const override {
      return egg::ovum::Type::String;
    }
    virtual EggProgramNodeFlags prepare(EggProgramContext&) override {
      return EggProgramNodeFlags::Constant;
    }
    virtual void dump(std::ostream& os) const override {
      ParserDump(os, "literal string").add(this->value);
    }
    virtual egg::ovum::Node compile(EggProgramCompiler& compiler) const override {
      return compiler.svalue(this->locationSource, this->value);
    }
  };

  class EggParserNode_Brackets : public EggParserNodeBase {
  protected:
    std::shared_ptr<IEggProgramNode> lhs;
    std::shared_ptr<IEggProgramNode> rhs;
  public:
    EggParserNode_Brackets(egg::ovum::IAllocator&, const egg::ovum::LocationSource& locationSource, const std::shared_ptr<IEggProgramNode>& lhs, const std::shared_ptr<IEggProgramNode>& rhs)
      : EggParserNodeBase(locationSource), lhs(lhs), rhs(rhs) {
      assert(lhs != nullptr);
      assert(rhs != nullptr);
    }
    virtual egg::ovum::Type getType() const override;
    virtual EggProgramNodeFlags prepare(EggProgramContext& context) override {
      return context.prepareBrackets(this->locationSource, *this->lhs, *this->rhs);
    }
    virtual void dump(std::ostream& os) const override {
      ParserDump(os, "brackets").add(this->lhs).add(this->rhs);
    }
    virtual egg::ovum::Node compile(EggProgramCompiler& compiler) const override {
      return compiler.expression(this->locationSource, egg::ovum::Opcode::INDEX, this->lhs, this->rhs);
    }
  };

  class EggParserNode_Dot : public EggParserNodeBase {
  protected:
    std::shared_ptr<IEggProgramNode> lhs;
    egg::ovum::String rhs;
  public:
    EggParserNode_Dot(egg::ovum::IAllocator&, const egg::ovum::LocationSource& locationSource, const std::shared_ptr<IEggProgramNode>& lhs, const egg::ovum::String& rhs)
      : EggParserNodeBase(locationSource), lhs(lhs), rhs(rhs) {
      assert(lhs != nullptr);
    }
    virtual EggProgramNodeFlags prepare(EggProgramContext& context) override {
      return context.prepareDot(this->locationSource, *this->lhs, this->rhs);
    }
    virtual egg::ovum::Type getType() const override;
    virtual void dump(std::ostream& os) const override {
      ParserDump(os, "dot").add(this->lhs).add(this->rhs);
    }
    virtual egg::ovum::Node compile(EggProgramCompiler& compiler) const override {
      return compiler.expression(this->locationSource, egg::ovum::Opcode::PROPERTY, this->lhs, compiler.identifier(this->locationSource, this->rhs));
    }
  };

  class EggParserNode_Unary : public EggParserNodeBase {
  protected:
    egg::ovum::IAllocator* allocator;
    EggProgramUnary op;
    std::shared_ptr<IEggProgramNode> expr;
    EggParserNode_Unary(egg::ovum::IAllocator& allocator, const egg::ovum::LocationSource& locationSource, EggProgramUnary op, const std::shared_ptr<IEggProgramNode>& expr)
      : EggParserNodeBase(locationSource), allocator(&allocator), op(op), expr(expr) {
      assert(expr != nullptr);
    }
  public:
    virtual EggProgramNodeFlags prepare(EggProgramContext& context) override {
      return context.prepareUnary(this->locationSource, this->op, *this->expr);
    }
    virtual void dump(std::ostream& os) const override {
      ParserDump(os, "unary").add(unaryToString(this->op)).add(this->expr);
    }
    virtual egg::ovum::Node compile(EggProgramCompiler& compiler) const override {
      return compiler.unary(this->locationSource, op, *this->expr);
    }
  };

#define EGG_PARSER_UNARY_OPERATOR_DEFINE(name, text) \
  class EggParserNode_Unary##name : public EggParserNode_Unary { \
  public: \
    EggParserNode_Unary##name(egg::ovum::IAllocator& allocator, const egg::ovum::LocationSource& locationSource, const std::shared_ptr<IEggProgramNode>& expr) \
      : EggParserNode_Unary(allocator, locationSource, EggProgramUnary::name, expr) { \
    } \
    virtual egg::ovum::Type getType() const override; \
  };
  EGG_PROGRAM_UNARY_OPERATORS(EGG_PARSER_UNARY_OPERATOR_DEFINE)

  class EggParserNode_Binary : public EggParserNodeBase {
  protected:
    egg::ovum::IAllocator* allocator;
    EggProgramBinary op;
    std::shared_ptr<IEggProgramNode> lhs;
    std::shared_ptr<IEggProgramNode> rhs;
    EggParserNode_Binary(egg::ovum::IAllocator& allocator, const egg::ovum::LocationSource& locationSource, EggProgramBinary op, const std::shared_ptr<IEggProgramNode>& lhs, const std::shared_ptr<IEggProgramNode>& rhs)
      : EggParserNodeBase(locationSource), allocator(&allocator), op(op), lhs(lhs), rhs(rhs) {
      assert(lhs != nullptr);
      assert(rhs != nullptr);
    }
  public:
    virtual void empredicate(EggProgramContext& context, std::shared_ptr<IEggProgramNode>& node) override {
      node = context.empredicateBinary(node, this->op, this->lhs, this->rhs);
    }
    virtual void dump(std::ostream& os) const override {
      ParserDump(os, "binary").add(binaryToString(this->op)).add(this->lhs).add(this->rhs);
    }
    virtual EggProgramNodeFlags prepare(EggProgramContext& context) override {
      return context.prepareBinary(this->locationSource, this->op, *this->lhs, *this->rhs);
    }
    virtual egg::ovum::Node compile(EggProgramCompiler& compiler) const override {
      return compiler.binary(this->locationSource, this->op, *this->lhs, *this->rhs);
    }
  };

#define EGG_PARSER_BINARY_OPERATOR_DEFINE(name, text) \
  class EggParserNode_Binary##name : public EggParserNode_Binary { \
  public: \
    EggParserNode_Binary##name(egg::ovum::IAllocator& allocator, const egg::ovum::LocationSource& locationSource, const std::shared_ptr<IEggProgramNode>& lhs, const std::shared_ptr<IEggProgramNode>& rhs) \
      : EggParserNode_Binary(allocator, locationSource, EggProgramBinary::name, lhs, rhs) { \
    } \
    virtual egg::ovum::Type getType() const override; \
  };
  EGG_PROGRAM_BINARY_OPERATORS(EGG_PARSER_BINARY_OPERATOR_DEFINE)

  class EggParserNode_Ternary : public EggParserNodeBase {
  private:
    egg::ovum::IAllocator* allocator;
    std::shared_ptr<IEggProgramNode> condition;
    std::shared_ptr<IEggProgramNode> whenTrue;
    std::shared_ptr<IEggProgramNode> whenFalse;
  public:
    EggParserNode_Ternary(egg::ovum::IAllocator& allocator, const egg::ovum::LocationSource& locationSource, const std::shared_ptr<IEggProgramNode>& condition, const std::shared_ptr<IEggProgramNode>& whenTrue, const std::shared_ptr<IEggProgramNode>& whenFalse)
      : EggParserNodeBase(locationSource), allocator(&allocator), condition(condition), whenTrue(whenTrue), whenFalse(whenFalse) {
      assert(condition != nullptr);
      assert(whenTrue != nullptr);
      assert(whenFalse != nullptr);
    }
    virtual void dump(std::ostream& os) const override {
      ParserDump(os, "ternary").add(this->condition).add(this->whenTrue).add(this->whenFalse);
    }
    virtual egg::ovum::Type getType() const override;
    virtual EggProgramNodeFlags prepare(EggProgramContext& context) override {
      return context.prepareTernary(this->locationSource, *this->condition, *this->whenTrue, *this->whenFalse);
    }
    virtual egg::ovum::Node compile(EggProgramCompiler& compiler) const override {
      return compiler.ternary(this->locationSource, egg::yolk::EggProgramTernary::Ternary, *this->condition, *this->whenTrue, *this->whenFalse);
    }
  };

  class EggParserNode_Assign : public EggParserNodeBase {
  protected:
    EggProgramAssign op;
    std::shared_ptr<IEggProgramNode> lhs;
    std::shared_ptr<IEggProgramNode> rhs;
    EggParserNode_Assign(egg::ovum::IAllocator&, const egg::ovum::LocationSource& locationSource, EggProgramAssign op, const std::shared_ptr<IEggProgramNode>& lhs, const std::shared_ptr<IEggProgramNode>& rhs)
      : EggParserNodeBase(locationSource), op(op), lhs(lhs), rhs(rhs) {
      assert(lhs != nullptr);
      assert(rhs != nullptr);
    }
  public:
    virtual EggProgramNodeFlags prepare(EggProgramContext& context) override {
      return context.prepareAssign(this->locationSource, this->op, *this->lhs, *this->rhs);
    }
    virtual void dump(std::ostream& os) const override {
      ParserDump(os, "assign").add(assignToString(this->op)).add(this->lhs).add(this->rhs);
    }
    virtual egg::ovum::Node compile(EggProgramCompiler& compiler) const override {
      return compiler.assign(this->locationSource, this->op, *this->lhs, *this->rhs);
    }
  };

#define EGG_PARSER_ASSIGN_OPERATOR_DEFINE(name, text) \
  class EggParserNode_Assign##name : public EggParserNode_Assign { \
  public: \
    EggParserNode_Assign##name(egg::ovum::IAllocator& allocator, const egg::ovum::LocationSource& locationSource, const std::shared_ptr<IEggProgramNode>& lhs, const std::shared_ptr<IEggProgramNode>& rhs) \
      : EggParserNode_Assign(allocator, locationSource, EggProgramAssign::name, lhs, rhs) { \
    } \
  };
  EGG_PROGRAM_ASSIGN_OPERATORS(EGG_PARSER_ASSIGN_OPERATOR_DEFINE)

    class EggParserNode_Predicate : public EggParserNodeBase {
    private:
      EggProgramBinary op;
      std::shared_ptr<IEggProgramNode> lhs;
      std::shared_ptr<IEggProgramNode> rhs;
    public:
      EggParserNode_Predicate(egg::ovum::IAllocator&, const egg::ovum::LocationSource& locationSource, EggProgramBinary op, const std::shared_ptr<IEggProgramNode>& lhs, const std::shared_ptr<IEggProgramNode>& rhs)
        : EggParserNodeBase(locationSource), op(op), lhs(lhs), rhs(rhs) {
        assert(lhs != nullptr);
        assert(rhs != nullptr);
      }
    public:
    virtual EggProgramNodeFlags prepare(EggProgramContext& context) override {
      return context.preparePredicate(this->locationSource, this->op, *this->lhs, *this->rhs);
    }      virtual void dump(std::ostream& os) const override {
        ParserDump(os, "predicate").add(binaryToString(this->op)).add(this->lhs).add(this->rhs);
      }
    virtual egg::ovum::Node compile(EggProgramCompiler& compiler) const override {
      return compiler.predicate(this->locationSource, this->op, *this->lhs, *this->rhs);
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
      return egg::ovum::Bits::hasAnySet(this->allowed, bit);
    }
    virtual EggParserAllowed inheritAllowed(EggParserAllowed allow, EggParserAllowed inherit) const override {
      return egg::ovum::Bits::mask(this->allowed, inherit) | allow;
    }
    virtual std::shared_ptr<IEggProgramNode> promote(const IEggSyntaxNode& node) override {
      return node.promote(*this);
    }
  };

  class EggParserContext : public EggParserContextBase {
  private:
    egg::ovum::IAllocator* mallocator;
    egg::ovum::String resource;
  public:
    EggParserContext(egg::ovum::IAllocator& allocator, const egg::ovum::String& resource, EggParserAllowed allowed = EggParserAllowed::None)
      : EggParserContextBase(allowed), mallocator(&allocator), resource(resource) {
    }
    virtual egg::ovum::IAllocator& getAllocator() const override {
      return *this->mallocator;
    }
    virtual egg::ovum::String getResourceName() const override {
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
    virtual egg::ovum::IAllocator& getAllocator() const override {
      return this->parent->getAllocator();
    }
    virtual egg::ovum::String getResourceName() const override {
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
      : nested(parent, EggParserAllowed::Break | EggParserAllowed::Case | EggParserAllowed::Continue, EggParserAllowed::Rethrow | EggParserAllowed::Return | EggParserAllowed::Yield),
        promoted(makeParserNode<EggParserNode_Switch>(parent, switchStatement, parent.promote(switchExpression))),
        previous(EggTokenizerKeyword::Null) {
      assert(this->promoted != nullptr);
    }
    virtual egg::ovum::IAllocator& getAllocator() const override {
      return this->nested.getAllocator();
    }
    virtual egg::ovum::String getResourceName() const override {
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
    virtual std::shared_ptr<IEggProgramNode> parse(egg::ovum::IAllocator& allocator, IEggTokenizer& tokenizer) override {
      auto syntax = EggParserFactory::createModuleSyntaxParser(allocator);
      auto ast = syntax->parse(tokenizer);
      EggParserContext context(allocator, tokenizer.resource());
      return context.promote(*ast);
    }
  };

  class EggParserExpression : public IEggParser {
  public:
    virtual std::shared_ptr<IEggProgramNode> parse(egg::ovum::IAllocator& allocator, IEggTokenizer& tokenizer) override {
      auto syntax = EggParserFactory::createExpressionSyntaxParser(allocator);
      auto ast = syntax->parse(tokenizer);
      EggParserContext context(allocator, tokenizer.resource());
      return context.promote(*ast);
    }
  };
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
  case EggTokenizerOperator::AmpersandAmpersandEqual:
    return promoteBinary<EggParserNode_AssignLogicalAnd>(context, *this, this->child);
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
  case EggTokenizerOperator::QueryQueryEqual:
    return promoteBinary<EggParserNode_AssignNullCoalescing>(context, *this, this->child);
  case EggTokenizerOperator::CaretEqual:
    return promoteBinary<EggParserNode_AssignBitwiseXor>(context, *this, this->child);
  case EggTokenizerOperator::BarEqual:
    return promoteBinary<EggParserNode_AssignBitwiseOr>(context, *this, this->child);
  case EggTokenizerOperator::BarBarEqual:
    return promoteBinary<EggParserNode_AssignLogicalOr>(context, *this, this->child);
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
  EggParserContextNested nested(context, EggParserAllowed::Rethrow | EggParserAllowed::Return | EggParserAllowed::Yield);
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
  EggParserContextNested nested(context, EggParserAllowed::Break | EggParserAllowed::Continue, EggParserAllowed::Rethrow | EggParserAllowed::Return | EggParserAllowed::Yield);
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
  // The main logic is handled by the 'try' node
  return context.promote(*this->child);
}

std::shared_ptr<egg::yolk::IEggProgramNode> egg::yolk::EggSyntaxNode_For::promote(egg::yolk::IEggParserContext& context) const {
  // We allow empty statements but not flow control in the three 'for' clauses
  EggParserContextNested nested1(context, EggParserAllowed::Empty);
  std::shared_ptr<IEggProgramNode> promoted[3];
  for (size_t i = 0; i < 3; ++i) {
    if (this->child[i] != nullptr) {
      promoted[i] = nested1.promote(*this->child[i]);
    }
  }
  EggParserContextNested nested2(context, EggParserAllowed::Break | EggParserAllowed::Continue, EggParserAllowed::Rethrow | EggParserAllowed::Return | EggParserAllowed::Yield);
  auto block = nested2.promote(*this->child[3]);
  return makeParserNode<EggParserNode_For>(context, *this, promoted[0], promoted[1], promoted[2], block);
}

std::shared_ptr<egg::yolk::IEggProgramNode> egg::yolk::EggSyntaxNode_Foreach::promote(egg::yolk::IEggParserContext& context) const {
  auto target = context.promote(*this->child[0]);
  auto expr = context.promote(*this->child[1]);
  EggParserContextNested nested(context, EggParserAllowed::Break | EggParserAllowed::Continue, EggParserAllowed::Rethrow | EggParserAllowed::Return | EggParserAllowed::Yield);
  auto block = nested.promote(*this->child[2]);
  return makeParserNode<EggParserNode_Foreach>(context, *this, target, expr, block);
}

std::shared_ptr<egg::yolk::IEggProgramNode> egg::yolk::EggSyntaxNode_FunctionDefinition::promote(egg::yolk::IEggParserContext& context) const {
  // The children are: <type> <parameter>* <block>
  assert(this->child.size() >= 2);
  size_t parameters = this->child.size() - 2;
  auto rettype = context.promote(*this->child[0])->getType();
  egg::ovum::TypeBuilder builder;
  if (this->generator) {
    // Generators cannot explicitly return voids
    if (rettype.hasAnyFlags(egg::ovum::ValueFlags::Void)) {
      throw exceptionFromLocation(context, "The return value of a generator may not include 'void'", *this);
    }
    builder = egg::ovum::TypeFactory::createGeneratorBuilder(context.getAllocator(), rettype, this->name);
  } else {
    builder = egg::ovum::TypeFactory::createFunctionBuilder(context.getAllocator(), rettype, this->name);
  }
  assert(builder != nullptr);
  egg::ovum::String parameter_name;
  auto parameter_type = egg::ovum::Type::Void;
  for (size_t i = 1; i <= parameters; ++i) {
    // We promote the parameter, extract the name/type/optional information and then discard it
    auto parameter = context.promote(*this->child[i]);
    auto parameter_optional = parameter->symbol(parameter_name, parameter_type);
    auto parameter_flags = parameter_optional ? egg::ovum::IFunctionSignatureParameter::Flags::None : egg::ovum::IFunctionSignatureParameter::Flags::Required;
    builder->addPositionalParameter(parameter_type, parameter_name, parameter_flags);
  }
  auto type = builder->build();
  auto allowed = this->generator ? (EggParserAllowed::Return | EggParserAllowed::Yield) : EggParserAllowed::Return;
  EggParserContextNested nested(context, allowed);
  auto block = nested.promote(*this->child[parameters + 1]);
  if (this->generator) {
    block = makeParserNode<EggParserNode_GeneratorDefinition>(context, *this, this->name, type, rettype, block);
  }
  return makeParserNode<EggParserNode_FunctionDefinition>(context, *this, this->name, type, block);
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
  EggParserContextNested nested1(context, EggParserAllowed::None, EggParserAllowed::Rethrow | EggParserAllowed::Return | EggParserAllowed::Yield);
  auto clauses = this->child.size();
  assert(clauses >= 2);
  auto block = nested1.promote(*this->child[0]);
  auto result = makeParserNode<EggParserNode_Try>(context, *this, block);
  EggParserContextNested nested2(context, EggParserAllowed::Rethrow, EggParserAllowed::Return | EggParserAllowed::Yield);
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
  EggParserContextNested nested(context, EggParserAllowed::Break | EggParserAllowed::Continue, EggParserAllowed::Rethrow | EggParserAllowed::Return | EggParserAllowed::Yield);
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
  case EggTokenizerOperator::AmpersandAmpersandEqual:
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
  case EggTokenizerOperator::QueryQueryEqual:
  case EggTokenizerOperator::BracketRight:
  case EggTokenizerOperator::CaretEqual:
  case EggTokenizerOperator::CurlyLeft:
  case EggTokenizerOperator::BarEqual:
  case EggTokenizerOperator::BarBarEqual:
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

egg::ovum::Type EggParserNode_Brackets::getType() const {
  return egg::ovum::Type::AnyQ; // TODO
}

egg::ovum::Type EggParserNode_Dot::getType() const {
  auto ltype = this->lhs->getType();
  auto dotable = ltype.queryDotable();
  assert(dotable != nullptr);
  return dotable->getType(this->rhs);
}

egg::ovum::Type EggParserNode_UnaryLogicalNot::getType() const {
  return egg::ovum::Type::Bool;
}

egg::ovum::Type EggParserNode_UnaryRef::getType() const {
  auto pointee = this->expr->getType();
  return egg::ovum::TypeFactory::createPointer(*this->allocator, *pointee);
}

egg::ovum::Type EggParserNode_UnaryDeref::getType() const {
  return egg::ovum::Type::AnyQ; // TODO
}

egg::ovum::Type EggParserNode_UnaryNegate::getType() const {
  return unaryArithmeticType(this->expr);
}

egg::ovum::Type EggParserNode_UnaryEllipsis::getType() const {
  EGG_THROW("TODO"); // TODO
}

egg::ovum::Type EggParserNode_UnaryBitwiseNot::getType() const {
  return egg::ovum::Type::Int;
}

egg::ovum::Type EggParserNode_BinaryUnequal::getType() const {
  return egg::ovum::Type::Bool;
}

egg::ovum::Type EggParserNode_BinaryRemainder::getType() const {
  // See http://mindprod.com/jgloss/modulus.html
  // Turn out this equates to the rules for binary-multiply too
  return binaryArithmeticType(this->lhs, this->rhs);
}

egg::ovum::Type EggParserNode_BinaryBitwiseAnd::getType() const {
  return binaryBitwiseType(*this->allocator, this->lhs, this->rhs);
}

egg::ovum::Type EggParserNode_BinaryLogicalAnd::getType() const {
  return egg::ovum::Type::Bool;
}

egg::ovum::Type EggParserNode_BinaryMultiply::getType() const {
  return binaryArithmeticType(this->lhs, this->rhs);
}

egg::ovum::Type EggParserNode_BinaryPlus::getType() const {
  return binaryArithmeticType(this->lhs, this->rhs);
}

egg::ovum::Type EggParserNode_BinaryMinus::getType() const {
  return binaryArithmeticType(this->lhs, this->rhs);
}

egg::ovum::Type EggParserNode_BinaryLambda::getType() const {
  EGG_THROW("TODO"); // TODO
}

egg::ovum::Type EggParserNode_BinaryDivide::getType() const {
  return binaryArithmeticType(this->lhs, this->rhs);
}

egg::ovum::Type EggParserNode_BinaryLess::getType() const {
  return egg::ovum::Type::Bool;
}

egg::ovum::Type EggParserNode_BinaryShiftLeft::getType() const {
  return egg::ovum::Type::Int;
}

egg::ovum::Type EggParserNode_BinaryLessEqual::getType() const {
  return egg::ovum::Type::Bool;
}

egg::ovum::Type EggParserNode_BinaryEqual::getType() const {
  return egg::ovum::Type::Bool;
}

egg::ovum::Type EggParserNode_BinaryGreater::getType() const {
  return egg::ovum::Type::Bool;
}

egg::ovum::Type EggParserNode_BinaryGreaterEqual::getType() const {
  return egg::ovum::Type::Bool;
}

egg::ovum::Type EggParserNode_BinaryShiftRight::getType() const {
  return egg::ovum::Type::Int;
}

egg::ovum::Type EggParserNode_BinaryShiftRightUnsigned::getType() const {
  return egg::ovum::Type::Int;
}

egg::ovum::Type EggParserNode_BinaryNullCoalescing::getType() const {
  return egg::ovum::Type::Void; // WIBBLE
  /*
  auto type1 = this->lhs->getType();
  if (!type1->hasBasalType(egg::ovum::BasalBits::Null)) {
    // The left-hand-side cannot be null, so the right side is irrelevant
    return type1;
  }
  auto type2 = this->rhs->getType();
  type1 = type1->denulledType();
  if (type1 == nullptr) {
    // The left-hand-side is only ever null, so only the right side is relevant
    return type2;
  }
  return egg::ovum::TypeFactory::createUnion(*this->allocator, *type1, *type2);
  */
}

egg::ovum::Type EggParserNode_BinaryBitwiseXor::getType() const {
  return binaryBitwiseType(*this->allocator, this->lhs, this->rhs);
}

egg::ovum::Type EggParserNode_BinaryBitwiseOr::getType() const {
  return binaryBitwiseType(*this->allocator, this->lhs, this->rhs);
}

egg::ovum::Type EggParserNode_BinaryLogicalOr::getType() const {
  return egg::ovum::Type::Bool;
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
    return std::make_shared<EggParserNode_Predicate>(this->allocator, node->location(), op, lhs, rhs);
  }
  return node;
}

egg::ovum::Type EggParserNode_Ternary::getType() const {
  auto type1 = this->condition->getType();
  if (!type1.hasAnyFlags(egg::ovum::ValueFlags::Bool)) {
    // The condition is not a bool, so the other values are irrelevant
    return egg::ovum::Type::Void;
  }
  auto type2 = this->whenTrue->getType();
  auto type3 = this->whenFalse->getType();
  return egg::ovum::TypeFactory::createUnion(*this->allocator, *type2, *type3);
}

std::shared_ptr<egg::yolk::IEggProgramNode> egg::yolk::EggParserFactory::parseModule(egg::ovum::IAllocator& allocator, TextStream& stream) {
  auto lexer = LexerFactory::createFromTextStream(stream);
  auto tokenizer = EggTokenizerFactory::createFromLexer(lexer);
  EggParserModule parser;
  return parser.parse(allocator, *tokenizer);
}

std::shared_ptr<egg::yolk::IEggParser> egg::yolk::EggParserFactory::createModuleParser() {
  return std::make_shared<EggParserModule>();
}

std::shared_ptr<egg::yolk::IEggParser> egg::yolk::EggParserFactory::createExpressionParser() {
  return std::make_shared<EggParserExpression>();
}
