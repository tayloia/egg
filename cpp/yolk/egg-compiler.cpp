#include "yolk/yolk.h"
#include "yolk/lexers.h"
#include "yolk/egg-tokenizer.h"
#include "yolk/egg-parser.h"
#include "yolk/egg-compiler.h"

#include <stack>

// TODO remove and replace with better messages
#define EXPECT(node, condition) \
  if (condition) {} else return this->error(node, "Expection failure in egg-compiler.cpp line ", __LINE__, ": " #condition);

using namespace egg::yolk;

namespace {
  using ModuleNode = egg::ovum::IVMModule::Node;
  using ParserNode = egg::yolk::IEggParser::Node;
  using ParserNodes = std::vector<std::unique_ptr<ParserNode>>;

  class ModuleCompiler final : public egg::ovum::IVMModuleBuilder::Reporter {
    ModuleCompiler(const ModuleCompiler&) = delete;
    ModuleCompiler& operator=(const ModuleCompiler&) = delete;
  private:
    egg::ovum::IVM& vm;
    egg::ovum::String resource;
    egg::ovum::IVMModuleBuilder& mbuilder;
  public:
    ModuleCompiler(egg::ovum::IVM& vm, const egg::ovum::String& resource, egg::ovum::IVMModuleBuilder& mbuilder)
      : vm(vm),
        resource(resource),
        mbuilder(mbuilder) {
    }
    class StmtContext;
    egg::ovum::HardPtr<egg::ovum::IVMModule> compile(ParserNode& root, StmtContext& context);
    virtual void report(const egg::ovum::SourceRange& range, const egg::ovum::String& problem) override {
      this->log(egg::ovum::ILogger::Severity::Error, this->resource, range, ": ", problem);
    }
    class ExprContext {
    public:
      struct Entry {
        enum class Kind {
          Builtin,
          Parameter,
          Variable,
          Function,
          Type
        };
        Kind kind;
        egg::ovum::Type type;
        egg::ovum::SourceRange range;
      };
    protected:
      std::map<egg::ovum::String, Entry> entries;
      const StmtContext* chain;
      explicit ExprContext(const StmtContext* parent)
        : entries(),
          chain(parent) {
      }
    public:
      // Symbol table
      const Entry* findSymbol(const egg::ovum::String& name) const {
        // Searches the entire chain from front to back
        for (auto* table = this; table != nullptr; table = table->chain) {
          auto iterator = table->entries.find(name);
          if (iterator != table->entries.end()) {
            return &iterator->second;
          }
        }
        return nullptr;
      }
    };
    struct StmtContextData {
      bool canBreak : 1 = false;
      bool canContinue : 1 = false;
      bool canYield : 1 = false;
      bool canRethrow : 1 = false;
      egg::ovum::Type canReturn = nullptr;
      ModuleNode* target = nullptr;
    };
    class StmtContext : public ExprContext, public StmtContextData {
      StmtContext() = delete;
      StmtContext(const StmtContext& parent) = delete;
    public:
      explicit StmtContext(const StmtContext* parent)
        : ExprContext(parent),
          StmtContextData() {
        if (parent != nullptr) {
          *static_cast<StmtContextData*>(this) = *parent;
        }
      }
      bool isModuleRoot() const {
        return this->chain == nullptr;
      }
      // Symbol table
      Entry* addSymbol(Entry::Kind kind, const egg::ovum::String& name, const egg::ovum::Type& type, const egg::ovum::SourceRange& range) {
        // We should only add builtins to the base of the chain
        assert((kind != Entry::Kind::Builtin) || (this->chain == nullptr));
        assert(!name.empty());
        assert(type != nullptr);
        auto iterator = this->entries.emplace(name, Entry{ kind, type, range });
        return iterator.second ? nullptr : &iterator.first->second;
      }
      bool removeSymbol(const egg::ovum::String& name) {
        // Only removes from the base of the chain
        assert(!name.empty());
        auto count = this->entries.erase(name);
        assert(count <= 1);
        return count > 0;
      }
    };
  private:
    ModuleNode* compileStmt(ParserNode& pnode, StmtContext& context);
    ModuleNode* compileStmtInto(ParserNode& pnode, StmtContext& context, ModuleNode& parent);
    ModuleNode* compileStmtVoid(ParserNode& pnode);
    ModuleNode* compileStmtBlock(ParserNode& pnodes, StmtContext& context);
    ModuleNode* compileStmtBlockInto(ParserNode& pnodes, StmtContext& context, ModuleNode& parent);
    ModuleNode* compileStmtDeclareVariable(ParserNode& pnode, StmtContext& context);
    ModuleNode* compileStmtDefineVariable(ParserNode& pnode, StmtContext& context);
    ModuleNode* compileStmtDefineFunction(ParserNode& pnode, StmtContext& context);
    ModuleNode* compileStmtMutate(ParserNode& pnode, StmtContext& context);
    ModuleNode* compileStmtForEach(ParserNode& pnode, StmtContext& context);
    ModuleNode* compileStmtForLoop(ParserNode& pnode, StmtContext& context);
    ModuleNode* compileStmtIf(ParserNode& pnode, StmtContext& context);
    ModuleNode* compileStmtIfGuarded(ParserNode& pnode, StmtContext& context);
    ModuleNode* compileStmtIfUnguarded(ParserNode& pnode, StmtContext& context);
    ModuleNode* compileStmtReturn(ParserNode& pnode, StmtContext& context);
    ModuleNode* compileStmtTry(ParserNode& pnode, StmtContext& context);
    ModuleNode* compileStmtCatch(ParserNode& pnode, StmtContext& context);
    ModuleNode* compileStmtWhile(ParserNode& pnode, StmtContext& context);
    ModuleNode* compileStmtWhileGuarded(ParserNode& pnode, StmtContext& context);
    ModuleNode* compileStmtWhileUnguarded(ParserNode& pnode, StmtContext& context);
    ModuleNode* compileValueExpr(ParserNode& pnode, const ExprContext& context);
    ModuleNode* compileValueExprVariable(ParserNode& pnode, const ExprContext& context);
    ModuleNode* compileValueExprUnary(ParserNode& op, ParserNode& rhs, const ExprContext& context);
    ModuleNode* compileValueExprBinary(ParserNode& op, ParserNode& lhs, ParserNode& rhs, const ExprContext& context);
    ModuleNode* compileValueExprTernary(ParserNode& op, ParserNode& lhs, ParserNode& mid, ParserNode& rhs, const ExprContext& context);
    ModuleNode* compileValueExprPredicate(ParserNode& pnode, const ExprContext& context);
    ModuleNode* compileValueExprCall(ParserNodes& pnodes, const ExprContext& context);
    ModuleNode* compileValueExprCallAssert(ParserNode& function, ParserNode& predicate, const ExprContext& context);
    ModuleNode* compileValueExprIndex(ParserNode& bracket, ParserNode& lhs, ParserNode& rhs, const ExprContext& context);
    ModuleNode* compileValueExprProperty(ParserNode& dot, ParserNode& lhs, ParserNode& rhs, const ExprContext& context);
    ModuleNode* compileValueExprArray(ParserNode& pnode, const ExprContext& context);
    ModuleNode* compileValueExprArrayElement(ParserNode& pnode, const ExprContext& context);
    ModuleNode* compileValueExprObject(ParserNode& pnode, const ExprContext& context);
    ModuleNode* compileValueExprObjectElement(ParserNode& pnode, const ExprContext& context);
    ModuleNode* compileValueExprGuard(ParserNode& pnode, ParserNode& ptype, ParserNode& pexpr, const ExprContext& context);
    ModuleNode* compileValueExprManifestation(ParserNode& pnode, egg::ovum::ValueFlags flags);
    ModuleNode* compileTypeExpr(ParserNode& pnode, const ExprContext& context, egg::ovum::Type& type);
    ModuleNode* compileTypeGuard(ParserNode& pnode, const ExprContext& context, egg::ovum::Type& type, ModuleNode*& mcond);
    ModuleNode* compileTypeInfer(ParserNode& pnode, ParserNode& ptype, ParserNode& pexpr, const ExprContext& context, egg::ovum::Type& type, ModuleNode*& mexpr);
    ModuleNode* compileTypeFunctionSignature(ParserNode& pnode, const egg::ovum::IFunctionSignature*& signature, egg::ovum::Type& type);
    ModuleNode* compileLiteral(ParserNode& pnode);
    ModuleNode* checkCompilation(ModuleNode& mnode);
    bool checkValueExprOperand(const char* expected, ModuleNode& mnode, ParserNode& pnode, egg::ovum::ValueFlags required);
    bool checkValueExprOperand2(const char* expected, ModuleNode& lhs, ModuleNode& rhs, ParserNode& pnode, egg::ovum::ValueFlags required);
    bool checkValueExprUnary(egg::ovum::ValueUnaryOp op, ModuleNode& rhs, ParserNode& pnode);
    bool checkValueExprBinary(egg::ovum::ValueBinaryOp op, ModuleNode& lhs, ModuleNode& rhs, ParserNode& pnode);
    bool checkValueExprTernary(egg::ovum::ValueTernaryOp op, ModuleNode& lhs, ModuleNode& mid, ModuleNode& rhs, ParserNode& pnode);
    egg::ovum::Type literalType(ParserNode& pnode);
    egg::ovum::Type forgeType(ParserNode& pnode);
    egg::ovum::Type deduceType(ModuleNode& mnode) {
      return this->mbuilder.deduce(mnode, this);
    }
    bool deduceIterationType(egg::ovum::Type& type) {
      auto itype = this->vm.getTypeForge().forgeIterationType(type);
      if (itype == nullptr) {
        return false;
      }
      type = itype;
      return true;
    }
    void forgeNullability(egg::ovum::Type& type, bool nullable) {
      assert(type != nullptr);
      type = this->vm.getTypeForge().forgeNullableType(type, nullable);
      assert(type != nullptr);
    }
    egg::ovum::Assignability isAssignable(const egg::ovum::Type& dst, const egg::ovum::Type& src) {
      assert(dst != nullptr);
      assert(src != nullptr);
      return this->vm.getTypeForge().isTypeAssignable(dst, src);
    }
    bool addSymbol(StmtContext& context, ParserNode& pnode, StmtContext::Entry::Kind kind, const egg::ovum::String& symbol, const egg::ovum::Type& type) {
      auto* extant = context.addSymbol(kind, symbol, type, pnode.range);
      if (extant != nullptr) {
        const char* already = "";
        switch (extant->kind) {
        case StmtContext::Entry::Kind::Builtin:
          already = " as a builtin";
          break;
        case StmtContext::Entry::Kind::Function:
          already = " as a function";
          break;
        case StmtContext::Entry::Kind::Parameter:
          already = " as a function parameter";
          break;
        case StmtContext::Entry::Kind::Variable:
          already = " as a variable";
          break;
        case StmtContext::Entry::Kind::Type:
          already = " as a type";
          break;
        }
        this->error(pnode, "Identifier '", symbol, "' already used", already);
        return false;
      }
      return true;
    }
    template<typename... ARGS>
    egg::ovum::String concat(ARGS&&... args) const {
      return egg::ovum::StringBuilder::concat(this->vm.getAllocator(), std::forward<ARGS>(args)...);
    }
    template<typename... ARGS>
    void log(egg::ovum::ILogger::Severity severity, ARGS&&... args) const {
      auto message = this->concat(std::forward<ARGS>(args)...);
      this->vm.getLogger().log(egg::ovum::ILogger::Source::Compiler, severity, message);
    }
    template<typename... ARGS>
    ModuleNode* error(ParserNode& pnode, ARGS&&... args) const {
      this->log(egg::ovum::ILogger::Severity::Error, this->resource, pnode.range, ": ", std::forward<ARGS>(args)...);
      return nullptr;
    }
    ModuleNode* expected(ParserNode& pnode, const char* expected) const {
      return this->error(pnode, "Expected ", expected, ", but instead got ", ModuleCompiler::toString(pnode));
    }
    static std::string toString(const ParserNode& pnode);
  };

  class ModuleCompilerReporter final : public ModuleCompiler::Reporter {
    ModuleCompilerReporter(const ModuleCompilerReporter&) = delete;
    ModuleCompilerReporter& operator=(const ModuleCompilerReporter&) = delete;
  private:
    ModuleCompiler& compiler;
  public:
    explicit ModuleCompilerReporter(ModuleCompiler& compiler)
      : compiler(compiler) {
    }
  };

  class EggCompiler : public IEggCompiler {
    EggCompiler(const EggCompiler&) = delete;
    EggCompiler& operator=(const EggCompiler&) = delete;
  private:
    egg::ovum::HardPtr<egg::ovum::IVMProgramBuilder> pbuilder;
  public:
    explicit EggCompiler(const egg::ovum::HardPtr<egg::ovum::IVMProgramBuilder>& pbuilder)
      : pbuilder(pbuilder) {
    }
    virtual egg::ovum::HardPtr<egg::ovum::IVMModule> compile(IEggParser& parser) override {
      // TODO warnings as errors?
      auto resource = parser.resource();
      auto parsed = parser.parse();
      this->logIssues(resource, parsed.issues);
      if (parsed.root != nullptr) {
        auto mbuilder = this->pbuilder->createModuleBuilder(resource);
        assert(mbuilder != nullptr);
        ModuleCompiler compiler(this->pbuilder->getVM(), resource, *mbuilder);
        ModuleCompiler::StmtContext context{ nullptr };
        this->pbuilder->visitBuiltins([&context](const egg::ovum::String& symbol, const egg::ovum::Type& type) {
          context.addSymbol(ModuleCompiler::StmtContext::Entry::Kind::Builtin, symbol, type, {});
        });
        context.target = &mbuilder->getRoot();
        return compiler.compile(*parsed.root, context);
      }
      return nullptr;
    }
  private:
    egg::ovum::ILogger::Severity parse(IEggParser& parser, IEggParser::Result& result) {
      result = parser.parse();
      return this->logIssues(parser.resource(), result.issues);
    }
    egg::ovum::ILogger::Severity logIssues(const egg::ovum::String& resource, const std::vector<IEggParser::Issue>& issues) {
      auto& logger = this->pbuilder->getVM().getLogger();
      auto worst = egg::ovum::ILogger::Severity::None;
      for (const auto& issue : issues) {
        egg::ovum::ILogger::Severity severity = egg::ovum::ILogger::Severity::Error;
        switch (issue.severity) {
        case IEggParser::Issue::Severity::Information:
          severity = egg::ovum::ILogger::Severity::Information;
          if (worst == egg::ovum::ILogger::Severity::None) {
            worst = egg::ovum::ILogger::Severity::Information;
          }
          break;
        case IEggParser::Issue::Severity::Warning:
          severity = egg::ovum::ILogger::Severity::Warning;
          if (worst != egg::ovum::ILogger::Severity::Error) {
            worst = egg::ovum::ILogger::Severity::Warning;
          }
          break;
        case IEggParser::Issue::Severity::Error:
          severity = egg::ovum::ILogger::Severity::Error;
          worst = egg::ovum::ILogger::Severity::Error;
          break;
        }
        auto message = egg::ovum::StringBuilder::concat(this->pbuilder->getAllocator(), resource, issue.range, ": ", issue.message);
        logger.log(egg::ovum::ILogger::Source::Compiler, severity, message);
      }
      return worst;
    }
  };
}

egg::ovum::HardPtr<egg::ovum::IVMModule> ModuleCompiler::compile(ParserNode& root, StmtContext& context) {
  assert(context.isModuleRoot());
  assert(context.target != nullptr);
  if (root.kind != ParserNode::Kind::ModuleRoot) {
    this->expected(root, "module root node");
    return nullptr;
  }
  for (const auto& child : root.children) {
    // Make sure we append to the target as it is now
    auto* target = context.target;
    assert(target != nullptr);
    auto* stmt = this->compileStmt(*child, context);
    if (stmt == nullptr) {
      return nullptr;
    }
    this->mbuilder.appendChild(*target, *stmt);
  }
  context.target = nullptr;
  return this->mbuilder.build();
}

ModuleNode* ModuleCompiler::compileStmt(ParserNode& pnode, StmtContext& context) {
  switch (pnode.kind) {
  case ParserNode::Kind::StmtBlock:
    return this->compileStmtBlock(pnode, context);
  case ParserNode::Kind::StmtDeclareVariable:
    EXPECT(pnode, pnode.children.size() == 1);
    return this->compileStmtDeclareVariable(pnode, context);
  case ParserNode::Kind::StmtDefineVariable:
    EXPECT(pnode, pnode.children.size() == 2);
    return this->compileStmtDefineVariable(pnode, context);
  case ParserNode::Kind::StmtDefineFunction:
    EXPECT(pnode, pnode.children.size() == 2);
    return this->compileStmtDefineFunction(pnode, context);
  case ParserNode::Kind::StmtForEach:
    EXPECT(pnode, pnode.children.size() == 3);
    return this->compileStmtForEach(pnode, context);
  case ParserNode::Kind::StmtForLoop:
    EXPECT(pnode, pnode.children.size() == 4);
    return this->compileStmtForLoop(pnode, context);
  case ParserNode::Kind::StmtIf:
    EXPECT(pnode, (pnode.children.size() == 2) || (pnode.children.size() == 3));
    return this->compileStmtIf(pnode, context);
  case ParserNode::Kind::StmtReturn:
    EXPECT(pnode, pnode.children.size() <= 2);
    return this->compileStmtReturn(pnode, context);
  case ParserNode::Kind::StmtTry:
    EXPECT(pnode, pnode.children.size() >= 2);
    return this->compileStmtTry(pnode, context);
  case ParserNode::Kind::StmtWhile:
    EXPECT(pnode, pnode.children.size() == 2);
    return this->compileStmtWhile(pnode, context);
  case ParserNode::Kind::StmtMutate:
    return this->compileStmtMutate(pnode, context);
  case ParserNode::Kind::ExprCall:
    EXPECT(pnode, !pnode.children.empty());
    return this->compileValueExprCall(pnode.children, context);
  case ParserNode::Kind::ModuleRoot:
  case ParserNode::Kind::StmtCatch:
  case ParserNode::Kind::StmtFinally:
  case ParserNode::Kind::ExprVariable:
  case ParserNode::Kind::ExprUnary:
  case ParserNode::Kind::ExprBinary:
  case ParserNode::Kind::ExprTernary:
  case ParserNode::Kind::ExprIndex:
  case ParserNode::Kind::ExprProperty:
  case ParserNode::Kind::ExprArray:
  case ParserNode::Kind::ExprObject:
  case ParserNode::Kind::ExprGuard:
  case ParserNode::Kind::TypeInfer:
  case ParserNode::Kind::TypeInferQ:
  case ParserNode::Kind::TypeVoid:
  case ParserNode::Kind::TypeBool:
  case ParserNode::Kind::TypeInt:
  case ParserNode::Kind::TypeFloat:
  case ParserNode::Kind::TypeString:
  case ParserNode::Kind::TypeObject:
  case ParserNode::Kind::TypeAny:
  case ParserNode::Kind::TypeType:
  case ParserNode::Kind::TypeUnary:
  case ParserNode::Kind::TypeBinary:
  case ParserNode::Kind::TypeFunctionSignature:
  case ParserNode::Kind::TypeFunctionSignatureParameter:
  case ParserNode::Kind::Literal:
  case ParserNode::Kind::Named:
  default:
    break;
  }
  if (context.isModuleRoot()) {
    return this->expected(pnode, "statement root child");
  }
  return this->expected(pnode, "statement");
}

ModuleNode* ModuleCompiler::compileStmtInto(ParserNode& pnode, StmtContext& context, ModuleNode& parent) {
  if (pnode.kind == ParserNode::Kind::StmtBlock) {
    return this->compileStmtBlockInto(pnode, context, parent);
  }
  auto child = this->compileStmt(pnode, context);
  if (child != nullptr) {
    this->mbuilder.appendChild(parent, *child);
    return &parent;
  }
  return nullptr;
}

ModuleNode* ModuleCompiler::compileStmtVoid(ParserNode& pnode) {
  return &this->mbuilder.exprLiteral(egg::ovum::HardValue::Void, pnode.range);
}

ModuleNode* ModuleCompiler::compileStmtBlock(ParserNode& pnode, StmtContext& context) {
  return this->compileStmtBlockInto(pnode, context, this->mbuilder.stmtBlock(pnode.range));
}

ModuleNode* ModuleCompiler::compileStmtBlockInto(ParserNode& pnode, StmtContext& context, ModuleNode& parent) {
  auto* block = &parent;
  for (const auto& child : pnode.children) {
    auto* stmt = this->compileStmt(*child, context);
    if (stmt == nullptr) {
      block = nullptr;
    } else if (block != nullptr) {
      this->mbuilder.appendChild(*block, *stmt);
    }
  }
  return block;
}

ModuleNode* ModuleCompiler::compileStmtDeclareVariable(ParserNode& pnode, StmtContext& context) {
  assert(pnode.kind == ParserNode::Kind::StmtDeclareVariable);
  egg::ovum::String symbol;
  EXPECT(pnode, pnode.value->getString(symbol));
  assert(pnode.children.size() == 1);
  auto& ptype = *pnode.children.front();
  egg::ovum::Type type;
  auto* mtype = this->compileTypeExpr(ptype, context, type);
  if (mtype == nullptr) {
    return nullptr;
  }
  assert(type != nullptr);
  if (!this->addSymbol(context, pnode, StmtContext::Entry::Kind::Variable, symbol, type)) {
    return nullptr;
  }
  auto* stmt = &this->mbuilder.stmtVariableDeclare(symbol, *mtype, pnode.range);
  context.target = stmt;
  return stmt;
}

ModuleNode* ModuleCompiler::compileStmtDefineVariable(ParserNode& pnode, StmtContext& context) {
  assert(pnode.kind == ParserNode::Kind::StmtDefineVariable);
  assert(pnode.children.size() == 2);
  egg::ovum::String symbol;
  EXPECT(pnode, pnode.value->getString(symbol));
  egg::ovum::Type ltype;
  ModuleNode* rnode;
  auto lnode = this->compileTypeInfer(pnode, *pnode.children.front(), *pnode.children.back(), context, ltype, rnode);
  if (lnode == nullptr) {
    return nullptr;
  }
  assert(rnode != nullptr);
  auto rtype = this->deduceType(*rnode);
  assert(rtype != nullptr);
  auto assignable = this->isAssignable(ltype, rtype);
  if (assignable == egg::ovum::Assignability::Never) {
    return this->error(*pnode.children.back(), "Cannot initialize '", symbol, "' of type '", ltype, "' with a value of type '", rtype, "'");
  }
  assert(ltype != nullptr);
  if (!this->addSymbol(context, pnode, StmtContext::Entry::Kind::Variable, symbol, ltype)) {
    return nullptr;
  }
  auto* stmt = &this->mbuilder.stmtVariableDefine(symbol, *lnode, *rnode, pnode.range);
  context.target = stmt;
  return stmt;
}

ModuleNode* ModuleCompiler::compileStmtDefineFunction(ParserNode& pnode, StmtContext& context) {
  assert(pnode.kind == ParserNode::Kind::StmtDefineFunction);
  assert(pnode.children.size() == 2);
  egg::ovum::String symbol;
  EXPECT(pnode, pnode.value->getString(symbol));
  auto& phead = *pnode.children.front();
  if (phead.kind != ParserNode::Kind::TypeFunctionSignature) {
    return this->expected(phead, "function signature in function definition");
  }
  const egg::ovum::IFunctionSignature* signature = nullptr;
  egg::ovum::Type type = nullptr;
  auto mtype = this->compileTypeFunctionSignature(phead, signature, type); // TODO add symbols directly here with better locations
  if (mtype == nullptr) {
    return nullptr;
  }
  assert(type != nullptr);
  auto okay = this->addSymbol(context, phead, StmtContext::Entry::Kind::Function, symbol, type);
  StmtContext inner{ &context };
  inner.canReturn = signature->getReturnType();
  assert(inner.canReturn != nullptr);
  assert(signature != nullptr);
  size_t pcount = signature->getParameterCount();
  for (size_t pindex = 0; pindex < pcount; ++pindex) {
    auto& parameter = signature->getParameter(pindex);
    auto pname = parameter.getName();
    if (!pname.empty()) {
      okay &= this->addSymbol(inner, pnode, StmtContext::Entry::Kind::Parameter, pname, parameter.getType());
    }
  }
  if (!okay) {
    return nullptr;
  }
  auto& ptail = *pnode.children.back();
  auto block = this->compileStmtBlockInto(ptail, inner, this->mbuilder.stmtFunctionInvoke(pnode.range));
  if (block == nullptr) {
    return nullptr;
  }
  auto* stmt = &this->mbuilder.stmtFunctionDefine(symbol, *mtype, pnode.range);
  this->mbuilder.appendChild(*stmt, *block);
  context.target = stmt;
  return stmt;
}

ModuleNode* ModuleCompiler::compileStmtMutate(ParserNode& pnode, StmtContext& context) {
  assert(pnode.kind == ParserNode::Kind::StmtMutate);
  auto nudge = (pnode.op.valueMutationOp == egg::ovum::ValueMutationOp::Decrement) || (pnode.op.valueMutationOp == egg::ovum::ValueMutationOp::Increment);
  if (nudge) {
    EXPECT(pnode, pnode.children.size() == 1);
  } else {
    EXPECT(pnode, pnode.children.size() == 2);
  }
  auto& plhs = *pnode.children.front();
  if (plhs.kind == ParserNode::Kind::ExprVariable) {
    EXPECT(plhs, plhs.children.size() == 0);
    egg::ovum::String symbol;
    EXPECT(plhs, plhs.value->getString(symbol));
    ModuleNode* rhs;
    if (nudge) {
      rhs = this->compileStmtVoid(pnode);
    } else {
      rhs = this->compileValueExpr(*pnode.children.back(), context);
    }
    return &this->mbuilder.stmtVariableMutate(symbol, pnode.op.valueMutationOp, *rhs, pnode.range);
  }
  if (plhs.kind == ParserNode::Kind::ExprProperty) {
    EXPECT(plhs, plhs.children.size() == 2);
    auto* instance = this->compileValueExpr(*plhs.children.front(), context);
    if (instance == nullptr) {
      return nullptr;
    }
    auto* property = this->compileValueExpr(*plhs.children.back(), context);
    if (property == nullptr) {
      return nullptr;
    }
    ModuleNode* rhs;
    if (nudge) {
      rhs = this->compileStmtVoid(pnode);
    } else {
      rhs = this->compileValueExpr(*pnode.children.back(), context);
    }
    return &this->mbuilder.stmtPropertyMutate(*instance, *property, pnode.op.valueMutationOp, *rhs, pnode.range);
  }
  if (plhs.kind == ParserNode::Kind::ExprIndex) {
    EXPECT(plhs, plhs.children.size() == 2);
    auto* instance = this->compileValueExpr(*plhs.children.front(), context);
    if (instance == nullptr) {
      return nullptr;
    }
    auto* property = this->compileValueExpr(*plhs.children.back(), context);
    if (property == nullptr) {
      return nullptr;
    }
    ModuleNode* rhs;
    if (nudge) {
      rhs = this->compileStmtVoid(pnode);
    } else {
      rhs = this->compileValueExpr(*pnode.children.back(), context);
    }
    return &this->mbuilder.stmtIndexMutate(*instance, *property, pnode.op.valueMutationOp, *rhs, pnode.range);
  }
  return this->expected(plhs, "variable in mutation statement");
}

ModuleNode* ModuleCompiler::compileStmtForEach(ParserNode& pnode, StmtContext& context) {
  assert(pnode.kind == ParserNode::Kind::StmtForEach);
  assert(pnode.children.size() == 3);
  egg::ovum::String symbol;
  EXPECT(pnode, pnode.value->getString(symbol));
  egg::ovum::Type type;
  ModuleNode* iter;
  auto mtype = this->compileTypeInfer(pnode, *pnode.children[0], *pnode.children[1], context, type, iter);
  if (mtype == nullptr) {
    return nullptr;
  }
  assert(type != nullptr);
  assert(iter != nullptr);
  StmtContext inner{ &context };
  inner.canBreak = true;
  inner.canContinue = true;
  assert(type != nullptr);
  if (!this->addSymbol(inner, pnode, StmtContext::Entry::Kind::Variable, symbol, type)) {
    return nullptr;
  }
  auto* bloc = this->compileStmt(*pnode.children[2], inner);
  if (bloc == nullptr) {
    return nullptr;
  }
  return &this->mbuilder.stmtForEach(symbol, *mtype, *iter, *bloc, pnode.range);
}

ModuleNode* ModuleCompiler::compileStmtForLoop(ParserNode& pnode, StmtContext& context) {
  assert(pnode.kind == ParserNode::Kind::StmtForLoop);
  assert(pnode.children.size() == 4);
  ModuleNode* scope;
  ModuleNode* init;
  StmtContext inner{ &context };
  auto& phead = *pnode.children.front();
  if (phead.kind == ParserNode::Kind::StmtDeclareVariable) {
    // Hoist the declaration
    scope = this->compileStmtDeclareVariable(phead, inner);
    if (scope == nullptr) {
      return nullptr;
    }
    init = this->compileStmtVoid(phead);
  } else if (phead.kind == ParserNode::Kind::StmtDefineVariable) {
    // Hoist the definition
    scope = this->compileStmtDefineVariable(phead, inner);
    if (scope == nullptr) {
      return nullptr;
    }
    init = this->compileStmtVoid(phead);
  } else {
    // No outer scope
    scope = nullptr;
    init = this->compileStmt(phead, context);
    if (init == nullptr) {
      return nullptr;
    }
  }
  assert(init != nullptr);
  auto* cond = this->compileValueExpr(*pnode.children[1], inner);
  if (cond == nullptr) {
    return nullptr;
  }
  auto* adva = this->compileStmt(*pnode.children[2], inner);
  if (adva == nullptr) {
    return nullptr;
  }
  inner.canBreak = true;
  inner.canContinue = true;
  auto* bloc = this->compileStmt(*pnode.children[3], inner);
  if (bloc == nullptr) {
    return nullptr;
  }
  auto* stmt = &this->mbuilder.stmtForLoop(*init, *cond, *adva, *bloc, pnode.range);
  if (scope == nullptr) {
    return stmt;
  }
  this->mbuilder.appendChild(*scope, *stmt);
  return scope;
}

ModuleNode* ModuleCompiler::compileStmtIf(ParserNode& pnode, StmtContext& context) {
  assert(pnode.kind == ParserNode::Kind::StmtIf);
  assert((pnode.children.size() == 2) || (pnode.children.size() == 3));
  if (pnode.children.front()->kind == ParserNode::Kind::ExprGuard) {
    return this->compileStmtIfGuarded(pnode, context);
  }
  return this->compileStmtIfUnguarded(pnode, context);
}

ModuleNode* ModuleCompiler::compileStmtIfGuarded(ParserNode& pnode, StmtContext& context) {
  assert(pnode.kind == ParserNode::Kind::StmtIf);
  assert((pnode.children.size() == 2) || (pnode.children.size() == 3));
  auto& pguard = *pnode.children.front();
  assert(pguard.kind == ParserNode::Kind::ExprGuard);
  assert(pguard.children.size() == 2);
  egg::ovum::String symbol;
  EXPECT(pguard, pguard.value->getString(symbol));
  egg::ovum::Type type;
  ModuleNode* mcond;
  auto* mtype = this->compileTypeGuard(pguard, context, type, mcond);
  if (mtype == nullptr) {
    return nullptr;
  }
  assert(mcond != nullptr);
  StmtContext inner{ &context };
  if (!this->addSymbol(inner, pguard, StmtContext::Entry::Kind::Variable, symbol, type)) {
    return nullptr;
  }
  auto* truthy = this->compileStmt(*pnode.children[1], inner);
  if (truthy == nullptr) {
    return nullptr;
  }
  ModuleNode* falsy = nullptr;
  if (pnode.children.size() == 3) {
    // There is an 'else' clause, so undeclare the guard variable at the beginning
    auto& undeclare = this->mbuilder.stmtVariableUndeclare(symbol, pguard.range);
    falsy = &this->mbuilder.stmtBlock(pnode.range);
    this->mbuilder.appendChild(*falsy, undeclare);
    falsy = this->compileStmtInto(*pnode.children.back(), context, *falsy);
    if (falsy == nullptr) {
      return nullptr;
    }
  }
  auto& stmt = this->mbuilder.stmtIf(*mcond, pnode.range);
  this->mbuilder.appendChild(stmt, *truthy);
  if (falsy != nullptr) {
    this->mbuilder.appendChild(stmt, *falsy);
  }
  auto& guarded = this->mbuilder.stmtVariableDeclare(symbol, *mtype, pguard.range);
  this->mbuilder.appendChild(guarded, stmt);
  return &guarded;
}

ModuleNode* ModuleCompiler::compileStmtIfUnguarded(ParserNode& pnode, StmtContext& context) {
  assert(pnode.kind == ParserNode::Kind::StmtIf);
  assert((pnode.children.size() == 2) || (pnode.children.size() == 3));
  auto* condition = this->compileValueExpr(*pnode.children.front(), context);
  if (condition == nullptr) {
    return nullptr;
  }
  auto* truthy = this->compileStmt(*pnode.children[1], context);
  if (truthy == nullptr) {
    return nullptr;
  }
  ModuleNode* falsy = nullptr;
  if (pnode.children.size() == 3) {
    // There is an 'else' clause
    falsy = this->compileStmt(*pnode.children.back(), context);
    if (falsy == nullptr) {
      return nullptr;
    }
  }
  auto* stmt = &this->mbuilder.stmtIf(*condition, pnode.range);
  this->mbuilder.appendChild(*stmt, *truthy);
  if (falsy != nullptr) {
    this->mbuilder.appendChild(*stmt, *falsy);
  }
  return stmt;
}

ModuleNode* ModuleCompiler::compileStmtReturn(ParserNode& pnode, StmtContext& context) {
  assert(pnode.kind == ParserNode::Kind::StmtReturn);
  assert(pnode.children.size() <= 1);
  if (context.canReturn == nullptr) {
    return this->error(pnode, "'return' statements are only valid within function definitions");
  }
  auto* stmt = &this->mbuilder.stmtReturn(pnode.range);
  if (pnode.children.empty()) {
    // return ;
    if (context.canReturn != egg::ovum::Type::Void) {
      return this->error(pnode, "Expected 'return' statement with a value of type '", *context.canReturn, "'");
    }
  } else {
    // return <expr> ;
    auto& pchild = *pnode.children.back();
    if (context.canReturn == egg::ovum::Type::Void) {
      return this->error(pchild, "Expected 'return' statement with no value");
    }
    auto* expr = this->compileValueExpr(pchild, context);
    if (expr == nullptr) {
      return nullptr;
    }
    auto type = this->deduceType(*expr);
    assert(type != nullptr);
    auto assignable = this->isAssignable(context.canReturn, type);
    if (assignable == egg::ovum::Assignability::Never) {
      return this->error(pchild, "Expected 'return' statement with a value of type '", *context.canReturn, "', but instead got a value of type '", *type, "'");
    }
    this->mbuilder.appendChild(*stmt, *expr);
  }
  return stmt;
}

ModuleNode* ModuleCompiler::compileStmtTry(ParserNode& pnode, StmtContext& context) {
  assert(pnode.kind == ParserNode::Kind::StmtTry);
  assert(pnode.children.size() >= 2);
  ModuleNode* stmt = nullptr;
  size_t index = 0;
  auto seenFinally = false;
  for (const auto& pchild : pnode.children) {
    StmtContext inner{ &context };
    ModuleNode* child;
    if (index == 0) {
      // The initial try block
      inner.canRethrow = false;
      child = this->compileStmt(*pchild, inner);
    } else if (pchild->kind == ParserNode::Kind::StmtCatch) {
      // A catch clause
      if (seenFinally) {
        return this->error(pnode, "Unexpected 'catch' clause after 'finally' clause in 'try' statement");
      }
      inner.canRethrow = true;
      child = this->compileStmtCatch(*pchild, inner);
    } else if (pchild->kind == ParserNode::Kind::StmtFinally) {
      // The finally clause
      if (seenFinally) {
        return this->error(pnode, "Unexpected second 'finally' clause in 'try' statement");
      }
      seenFinally = true;
      inner.canRethrow = false;
      child = this->compileStmt(*pchild, inner);
    } else {
      return this->expected(pnode, "'catch' or 'finally' after 'try'");
    }
    if (child == nullptr) {
      stmt = nullptr;
    } else if (index == 0) {
      assert(stmt == nullptr);
      stmt = &this->mbuilder.stmtTry(*child, pnode.range);
    } else if (stmt != nullptr) {
      this->mbuilder.appendChild(*stmt, *child);
    }
    index++;
  }
  return stmt;
}

ModuleNode* ModuleCompiler::compileStmtCatch(ParserNode& pnode, StmtContext& context) {
  assert(pnode.kind == ParserNode::Kind::StmtCatch);
  assert(pnode.children.size() >= 1);
  assert(context.canRethrow);
  egg::ovum::String symbol;
  EXPECT(pnode, pnode.value->getString(symbol));
  ModuleNode* stmt = nullptr;
  size_t index = 0;
  for (const auto& pchild : pnode.children) {
    if (index == 0) {
      // The catch type
      egg::ovum::Type type;
      auto mtype = this->compileTypeExpr(*pchild, context, type);
      if (mtype != nullptr) {
        if (this->addSymbol(context, *pchild, StmtContext::Entry::Kind::Variable, symbol, type)) {
          stmt = &this->mbuilder.stmtCatch(symbol, *mtype, pnode.range);
        }
      }
    } else {
      // Statements in the catch block
      auto child = this->compileStmt(*pchild, context);
      if (child == nullptr) {
        stmt = nullptr;
      } if (stmt != nullptr) {
        this->mbuilder.appendChild(*stmt, *child);
      }
    }
    index++;
  }
  return stmt;
}

ModuleNode* ModuleCompiler::compileStmtWhile(ParserNode& pnode, StmtContext& context) {
  assert(pnode.kind == ParserNode::Kind::StmtWhile);
  assert(pnode.children.size() == 2);
  if (pnode.children.front()->kind == ParserNode::Kind::ExprGuard) {
    return this->compileStmtWhileGuarded(pnode, context);
  }
  return this->compileStmtWhileUnguarded(pnode, context);
}

ModuleNode* ModuleCompiler::compileStmtWhileGuarded(ParserNode& pnode, StmtContext& context) {
  assert(pnode.kind == ParserNode::Kind::StmtWhile);
  assert(pnode.children.size() == 2);
  auto& pguard = *pnode.children.front();
  assert(pguard.kind == ParserNode::Kind::ExprGuard);
  assert(pguard.children.size() == 2);
  egg::ovum::String symbol;
  EXPECT(pguard, pguard.value->getString(symbol));
  egg::ovum::Type type;
  ModuleNode* mcond;
  auto* mtype = this->compileTypeGuard(pguard, context, type, mcond);
  if (mtype == nullptr) {
    return nullptr;
  }
  assert(mcond != nullptr);
  StmtContext inner{ &context };
  if (!this->addSymbol(inner, pguard, StmtContext::Entry::Kind::Variable, symbol, type)) {
    return nullptr;
  }
  auto* block = this->compileStmt(*pnode.children.back(), inner);
  if (block == nullptr) {
    return nullptr;
  }
  auto& stmt = this->mbuilder.stmtWhile(*mcond, *block, pnode.range);
  auto& guarded = this->mbuilder.stmtVariableDeclare(symbol, *mtype, pguard.range);
  this->mbuilder.appendChild(guarded, stmt);
  return &guarded;
}

ModuleNode* ModuleCompiler::compileStmtWhileUnguarded(ParserNode& pnode, StmtContext& context) {
  assert(pnode.kind == ParserNode::Kind::StmtWhile);
  assert(pnode.children.size() == 2);
  auto* condition = this->compileValueExpr(*pnode.children.front(), context);
  if (condition == nullptr) {
    return nullptr;
  }
  auto* block = this->compileStmt(*pnode.children.back(), context);
  if (block == nullptr) {
    return nullptr;
  }
  auto* stmt = &this->mbuilder.stmtWhile(*condition, *block, pnode.range);
  return stmt;
}

ModuleNode* ModuleCompiler::compileValueExpr(ParserNode& pnode, const ExprContext& context) {
  switch (pnode.kind) {
  case ParserNode::Kind::ExprVariable:
    return this->compileValueExprVariable(pnode, context);
  case ParserNode::Kind::ExprUnary:
    EXPECT(pnode, pnode.children.size() == 1);
    EXPECT(pnode, pnode.children[0] != nullptr);
    return this->compileValueExprUnary(pnode, *pnode.children[0], context);
  case ParserNode::Kind::ExprBinary:
    EXPECT(pnode, pnode.children.size() == 2);
    EXPECT(pnode, pnode.children[0] != nullptr);
    EXPECT(pnode, pnode.children[1] != nullptr);
    return this->compileValueExprBinary(pnode, *pnode.children[0], *pnode.children[1], context);
  case ParserNode::Kind::ExprTernary:
    EXPECT(pnode, pnode.children.size() == 3);
    EXPECT(pnode, pnode.children[0] != nullptr);
    EXPECT(pnode, pnode.children[1] != nullptr);
    EXPECT(pnode, pnode.children[2] != nullptr);
    return this->compileValueExprTernary(pnode, *pnode.children[0], *pnode.children[1], *pnode.children[2], context);
  case ParserNode::Kind::ExprCall:
    EXPECT(pnode, pnode.children.size() > 0);
    return this->compileValueExprCall(pnode.children, context);
  case ParserNode::Kind::ExprIndex:
    EXPECT(pnode, pnode.children.size() == 2);
    EXPECT(pnode, pnode.children[0] != nullptr);
    EXPECT(pnode, pnode.children[1] != nullptr);
    return this->compileValueExprIndex(pnode, *pnode.children[0], *pnode.children[1], context);
  case ParserNode::Kind::ExprProperty:
    EXPECT(pnode, pnode.children.size() == 2);
    EXPECT(pnode, pnode.children[0] != nullptr);
    EXPECT(pnode, pnode.children[1] != nullptr);
    return this->compileValueExprProperty(pnode, *pnode.children[0], *pnode.children[1], context);
  case ParserNode::Kind::ExprArray:
    return this->compileValueExprArray(pnode, context);
  case ParserNode::Kind::ExprObject:
    return this->compileValueExprObject(pnode, context);
  case ParserNode::Kind::ExprGuard:
    EXPECT(pnode, pnode.children.size() == 2);
    EXPECT(pnode, pnode.children[0] != nullptr);
    EXPECT(pnode, pnode.children[1] != nullptr);
    return this->compileValueExprGuard(pnode, *pnode.children[0], *pnode.children[1], context);
  case ParserNode::Kind::Literal:
    return this->compileLiteral(pnode);
  case ParserNode::Kind::TypeVoid:
    EXPECT(pnode, pnode.children.empty());
    return this->compileValueExprManifestation(pnode, egg::ovum::ValueFlags::Void);
  case ParserNode::Kind::TypeBool:
    EXPECT(pnode, pnode.children.empty());
    return this->compileValueExprManifestation(pnode, egg::ovum::ValueFlags::Bool);
  case ParserNode::Kind::TypeInt:
    EXPECT(pnode, pnode.children.empty());
    return this->compileValueExprManifestation(pnode, egg::ovum::ValueFlags::Int);
  case ParserNode::Kind::TypeFloat:
    EXPECT(pnode, pnode.children.empty());
    return this->compileValueExprManifestation(pnode, egg::ovum::ValueFlags::Float);
  case ParserNode::Kind::TypeString:
    EXPECT(pnode, pnode.children.empty());
    return this->compileValueExprManifestation(pnode, egg::ovum::ValueFlags::String);
  case ParserNode::Kind::TypeObject:
    EXPECT(pnode, pnode.children.empty());
    return this->compileValueExprManifestation(pnode, egg::ovum::ValueFlags::Object);
  case ParserNode::Kind::TypeAny:
    EXPECT(pnode, pnode.children.empty());
    return this->compileValueExprManifestation(pnode, egg::ovum::ValueFlags::Any);
  case ParserNode::Kind::TypeType:
    EXPECT(pnode, pnode.children.empty());
    return this->compileValueExprManifestation(pnode, egg::ovum::ValueFlags::Type);
  case ParserNode::Kind::ModuleRoot:
  case ParserNode::Kind::TypeInfer:
  case ParserNode::Kind::TypeInferQ:
  case ParserNode::Kind::TypeUnary:
  case ParserNode::Kind::TypeBinary:
  case ParserNode::Kind::TypeFunctionSignature:
  case ParserNode::Kind::TypeFunctionSignatureParameter:
  case ParserNode::Kind::StmtBlock:
  case ParserNode::Kind::StmtDeclareVariable:
  case ParserNode::Kind::StmtDefineVariable:
  case ParserNode::Kind::StmtDefineFunction:
  case ParserNode::Kind::StmtForEach:
  case ParserNode::Kind::StmtForLoop:
  case ParserNode::Kind::StmtIf:
  case ParserNode::Kind::StmtReturn:
  case ParserNode::Kind::StmtTry:
  case ParserNode::Kind::StmtCatch:
  case ParserNode::Kind::StmtFinally:
  case ParserNode::Kind::StmtWhile:
  case ParserNode::Kind::StmtMutate:
  case ParserNode::Kind::Named:
  default:
    break;
  }
  return this->expected(pnode, "value expression");
}

ModuleNode* ModuleCompiler::compileValueExprVariable(ParserNode& pnode, const ExprContext& context) {
  EXPECT(pnode, pnode.children.size() == 0);
  egg::ovum::String symbol;
  EXPECT(pnode, pnode.value->getString(symbol));
  if (context.findSymbol(symbol) == nullptr) {
    return this->error(pnode, "Unknown identifier: '", symbol, "'");
  }
  return &this->mbuilder.exprVariable(symbol, pnode.range);
}

ModuleNode* ModuleCompiler::compileValueExprUnary(ParserNode& op, ParserNode& rhs, const ExprContext& context) {
  auto* expr = this->compileValueExpr(rhs, context);
  if (expr != nullptr) {
    if (!this->checkValueExprUnary(op.op.valueUnaryOp, *expr, op)) {
      return nullptr;
    }
    return &this->mbuilder.exprValueUnaryOp(op.op.valueUnaryOp, *expr, op.range);
  }
  return nullptr;
}

ModuleNode* ModuleCompiler::compileValueExprBinary(ParserNode& op, ParserNode& lhs, ParserNode& rhs, const ExprContext& context) {
  auto* lexpr = this->compileValueExpr(lhs, context);
  if (lexpr != nullptr) {
    auto* rexpr = this->compileValueExpr(rhs, context);
    if (rexpr != nullptr) {
      if (!this->checkValueExprBinary(op.op.valueBinaryOp, *lexpr, *rexpr, op)) {
        return nullptr;
      }
      return &this->mbuilder.exprValueBinaryOp(op.op.valueBinaryOp, *lexpr, *rexpr, op.range);
    }
  }
  return nullptr;
}

ModuleNode* ModuleCompiler::compileValueExprTernary(ParserNode& op, ParserNode& lhs, ParserNode& mid, ParserNode& rhs, const ExprContext& context) {
  auto* lexpr = this->compileValueExpr(lhs, context);
  if (lexpr != nullptr) {
    auto* mexpr = this->compileValueExpr(mid, context);
    if (mexpr != nullptr) {
      auto* rexpr = this->compileValueExpr(rhs, context);
      if (rexpr != nullptr) {
        if (!this->checkValueExprTernary(op.op.valueTernaryOp, *lexpr, *mexpr, *rexpr, lhs)) {
          return nullptr;
        }
        return &this->mbuilder.exprValueTernaryOp(op.op.valueTernaryOp, *lexpr, *mexpr, *rexpr, op.range);
      }
    }
  }
  return nullptr;
}

ModuleNode* ModuleCompiler::compileValueExprPredicate(ParserNode& pnode, const ExprContext& context) {
  auto op = egg::ovum::ValuePredicateOp::None;
  ModuleNode* first;
  ModuleNode* second;
  if ((pnode.kind == ParserNode::Kind::ExprUnary) && (pnode.children.size() == 1)) {
    switch (pnode.op.valueUnaryOp) {
    case egg::ovum::ValueUnaryOp::LogicalNot:
      op = egg::ovum::ValuePredicateOp::LogicalNot;
      break;
    case egg::ovum::ValueUnaryOp::Negate:
    case egg::ovum::ValueUnaryOp::BitwiseNot:
      break;
    }
    if (op == egg::ovum::ValuePredicateOp::None) {
      first = this->compileValueExpr(pnode, context);
    } else {
      first = this->compileValueExpr(*pnode.children.front(), context);
    }
    second = nullptr;
  } else if ((pnode.kind == ParserNode::Kind::ExprBinary) && (pnode.children.size() == 2)) {
    switch (pnode.op.valueBinaryOp) {
    case egg::ovum::ValueBinaryOp::LessThan:
      op = egg::ovum::ValuePredicateOp::LessThan;
      break;
    case egg::ovum::ValueBinaryOp::LessThanOrEqual:
      op = egg::ovum::ValuePredicateOp::LessThanOrEqual;
      break;
    case egg::ovum::ValueBinaryOp::Equal:
      op = egg::ovum::ValuePredicateOp::Equal;
      break;
    case egg::ovum::ValueBinaryOp::NotEqual:
      op = egg::ovum::ValuePredicateOp::NotEqual;
      break;
    case egg::ovum::ValueBinaryOp::GreaterThanOrEqual:
      op = egg::ovum::ValuePredicateOp::GreaterThanOrEqual;
      break;
    case egg::ovum::ValueBinaryOp::GreaterThan:
      op = egg::ovum::ValuePredicateOp::GreaterThan;
      break;
    case egg::ovum::ValueBinaryOp::Add:
    case egg::ovum::ValueBinaryOp::Subtract:
    case egg::ovum::ValueBinaryOp::Multiply:
    case egg::ovum::ValueBinaryOp::Divide:
    case egg::ovum::ValueBinaryOp::Remainder:
    case egg::ovum::ValueBinaryOp::BitwiseAnd:
    case egg::ovum::ValueBinaryOp::BitwiseOr:
    case egg::ovum::ValueBinaryOp::BitwiseXor:
    case egg::ovum::ValueBinaryOp::ShiftLeft:
    case egg::ovum::ValueBinaryOp::ShiftRight:
    case egg::ovum::ValueBinaryOp::ShiftRightUnsigned:
    case egg::ovum::ValueBinaryOp::IfNull:
    case egg::ovum::ValueBinaryOp::IfFalse:
    case egg::ovum::ValueBinaryOp::IfTrue:
    default:
      break;
    }
    if (op == egg::ovum::ValuePredicateOp::None) {
      first = this->compileValueExpr(pnode, context);
      second = nullptr;
    } else {
      first = this->compileValueExpr(*pnode.children.front(), context);
      second = this->compileValueExpr(*pnode.children.back(), context);
      if (second == nullptr) {
        return nullptr;
      }
    }
  } else {
    first = this->compileValueExpr(pnode, context);
    second = nullptr;
  }
  if (first == nullptr) {
    return nullptr;
  }
  auto& expr = this->mbuilder.exprValuePredicateOp(op, pnode.range);
  this->mbuilder.appendChild(expr, *first);
  if (second != nullptr) {
    this->mbuilder.appendChild(expr, *second);
  }
  return &expr;
}

ModuleNode* ModuleCompiler::compileValueExprCall(ParserNodes& pnodes, const ExprContext& context) {
  auto pnode = pnodes.begin();
  assert(pnode != pnodes.end());
  if (pnodes.size() == 2) {
    // Possible special case for 'assert(predicate)'
    // TODO: Replace with predicate argument hint
    egg::ovum::String symbol;
    if ((*pnode)->value->getString(symbol) && symbol.equals("assert")) {
      auto& predicate = *pnodes.back();
      return this->compileValueExprCallAssert(**pnode, predicate, context);
    }
  }
  ModuleNode* call = nullptr;
  auto type = this->literalType(**pnode);
  if (type != nullptr) {
    call = &this->mbuilder.typeLiteral(type, (*pnode)->range);
  } else {
    call = this->compileValueExpr(**pnode, context);
  }
  if (call != nullptr) {
    call = &this->mbuilder.exprFunctionCall(*call, (*pnode)->range);
  }
  while (++pnode != pnodes.end()) {
    auto* expr = this->compileValueExpr(**pnode, context);
    if (expr == nullptr) {
      call = nullptr;
    } else if (call != nullptr) {
      this->mbuilder.appendChild(*call, *expr);
    }
  }
  return call;
}

ModuleNode* ModuleCompiler::compileValueExprCallAssert(ParserNode& function, ParserNode& predicate, const ExprContext& context) {
  // Specialization for 'assert(predicate)'
  auto* expr = this->compileValueExpr(function, context);
  if (expr == nullptr) {
    return nullptr;
  }
  auto& stmt = this->mbuilder.exprFunctionCall(*expr, function.range);
  expr = this->compileValueExprPredicate(predicate, context);
  if (expr == nullptr) {
    return nullptr;
  }
  this->mbuilder.appendChild(stmt, *expr);
  return &stmt;
}

ModuleNode* ModuleCompiler::compileValueExprIndex(ParserNode& bracket, ParserNode& lhs, ParserNode& rhs, const ExprContext& context) {
  auto* lexpr = this->compileValueExpr(lhs, context);
  if (lexpr != nullptr) {
    auto* rexpr = this->compileValueExpr(rhs, context);
    if (rexpr != nullptr) {
      return &this->mbuilder.exprIndexGet(*lexpr, *rexpr, bracket.range);
    }
  }
  return nullptr;
}

ModuleNode* ModuleCompiler::compileValueExprProperty(ParserNode& dot, ParserNode& lhs, ParserNode& rhs, const ExprContext& context) {
  ModuleNode* lexpr;
  auto type = this->literalType(lhs);
  if (type != nullptr) {
    // Something like 'string.from'
    lexpr = &this->mbuilder.typeLiteral(type, lhs.range);
  } else {
    lexpr = this->compileValueExpr(lhs, context);
  }
  if (lexpr != nullptr) {
    auto* rexpr = this->compileValueExpr(rhs, context);
    if (rexpr != nullptr) {
      return this->checkCompilation(this->mbuilder.exprPropertyGet(*lexpr, *rexpr, dot.range));
    }
  }
  return nullptr;
}

ModuleNode* ModuleCompiler::compileValueExprArray(ParserNode& pnode, const ExprContext& context) {
  auto* array = &this->mbuilder.exprArray(pnode.range);
  for (auto& child : pnode.children) {
    auto* element = this->compileValueExprArrayElement(*child, context);
    if (element == nullptr) {
      array = nullptr;
    } else if (array != nullptr) {
      this->mbuilder.appendChild(*array, *element);
    }
  }
  return array;
}

ModuleNode* ModuleCompiler::compileValueExprArrayElement(ParserNode& pnode, const ExprContext& context) {
  // TODO: handle ellipsis '...'
  return this->compileValueExpr(pnode, context);
}

ModuleNode* ModuleCompiler::compileValueExprObject(ParserNode& pnode, const ExprContext& context) {
  auto* object = &this->mbuilder.exprObject(pnode.range);
  for (auto& child : pnode.children) {
    auto* element = this->compileValueExprObjectElement(*child, context);
    if (element == nullptr) {
      object = nullptr;
    } else if (object != nullptr) {
      this->mbuilder.appendChild(*object, *element);
    }
  }
  return object;
}

ModuleNode* ModuleCompiler::compileValueExprObjectElement(ParserNode& pnode, const ExprContext& context) {
  // TODO: handle ellipsis '...'
  if (pnode.kind == ParserNode::Kind::Named) {
    EXPECT(pnode, pnode.children.size() == 1);
    auto* value = this->compileValueExpr(*pnode.children.front(), context);
    if (value == nullptr) {
      return nullptr;
    }
    return &this->mbuilder.exprNamed(pnode.value, *value, pnode.range);
  }
  return this->expected(pnode, "object expression element");
}

ModuleNode* ModuleCompiler::compileValueExprGuard(ParserNode& pnode, ParserNode&, ParserNode& pexpr, const ExprContext& context) {
  // The variable has been declared with the appropriate type already
  egg::ovum::String symbol;
  EXPECT(pnode, pnode.value->getString(symbol));
  auto* mexpr = this->compileValueExpr(pexpr, context);
  if (mexpr != nullptr) {
    return &this->mbuilder.exprGuard(symbol, *mexpr, pnode.range);
  }
  return nullptr;
}

ModuleNode* ModuleCompiler::compileValueExprManifestation(ParserNode& pnode, egg::ovum::ValueFlags flags) {
  return &this->mbuilder.typeManifestation(flags, pnode.range);
}

ModuleNode* ModuleCompiler::compileTypeExpr(ParserNode& pnode, const ExprContext&, egg::ovum::Type& type) {
  type = this->forgeType(pnode);
  if (type == nullptr) {
    return this->expected(pnode, "type expression");
  }
  return &this->mbuilder.typeLiteral(type, pnode.range);
}

ModuleNode* ModuleCompiler::compileTypeGuard(ParserNode& pnode, const ExprContext& context, egg::ovum::Type& type, ModuleNode*& mcond) {
  assert(pnode.kind == ParserNode::Kind::ExprGuard);
  assert(pnode.children.size() == 2);
  auto* mtype = this->compileTypeInfer(pnode, *pnode.children.front(), *pnode.children.back(), context, type, mcond);
  if (mcond != nullptr) {
    egg::ovum::String symbol;
    EXPECT(pnode, pnode.value->getString(symbol));
    mcond = &this->mbuilder.exprGuard(symbol, *mcond, pnode.range);
  }
  return mtype;
}

ModuleNode* ModuleCompiler::compileTypeInfer(ParserNode& pnode, ParserNode& ptype, ParserNode& pexpr, const ExprContext& context, egg::ovum::Type& type, ModuleNode*& mexpr) {
  assert((pnode.kind == ParserNode::Kind::StmtDefineVariable) || (pnode.kind == ParserNode::Kind::StmtForEach) || (pnode.kind == ParserNode::Kind::ExprGuard));
  if (ptype.kind == ParserNode::Kind::TypeInfer) {
    mexpr = this->compileValueExpr(pexpr, context);
    if (mexpr == nullptr) {
      return nullptr;
    }
    type = this->deduceType(*mexpr);
    assert(type != nullptr);
    if (pnode.kind == ParserNode::Kind::StmtForEach) {
      // We now have the type of 'iterable' in 'for (var <iterator> : <iterable>)'
      if (!this->deduceIterationType(type)) {
        return this->error(pexpr, "Value of type '", *type, "' is not iterable");
      }
      assert(type != nullptr);
    }
    this->forgeNullability(type, false);
    assert(type != nullptr);
    return &this->mbuilder.typeLiteral(type, ptype.range);
  }
  if (ptype.kind == ParserNode::Kind::TypeInferQ) {
    mexpr = this->compileValueExpr(pexpr, context);
    if (mexpr == nullptr) {
      return nullptr;
    }
    type = this->deduceType(*mexpr);
    assert(type != nullptr);
    if (pnode.kind == ParserNode::Kind::StmtForEach) {
      // We now have the type of 'iterable' in 'for (var? <iterator> : <iterable>)'
      if (!this->deduceIterationType(type)) {
        return this->error(pexpr, "Value of type '", *type, "' is not iterable");
      }
      assert(type != nullptr);
    }
    this->forgeNullability(type, true);
    assert(type != nullptr);
    return &this->mbuilder.typeLiteral(type, ptype.range);
  }
  auto* mtype = this->compileTypeExpr(ptype, context, type);
  if (mtype == nullptr) {
    return nullptr;
  }
  mexpr = this->compileValueExpr(pexpr, context);
  if (mexpr == nullptr) {
    return nullptr;
  }
  if (pnode.kind == ParserNode::Kind::StmtForEach) {
    // We need to check the validity of 'for (<type> <iterator> : <iterable>)'
    auto actual = this->deduceType(*mexpr);
    assert(actual != nullptr);
    if (!this->deduceIterationType(actual)) {
      return this->error(pexpr, "Value of type '", *actual, "' is not iterable");
    }
    // TODO check 'actual' against 'type'
  }
  return mtype;
}

ModuleNode* ModuleCompiler::compileTypeFunctionSignature(ParserNode& pnode, const egg::ovum::IFunctionSignature*& signature, egg::ovum::Type& type) {
  assert(pnode.kind == ParserNode::Kind::TypeFunctionSignature);
  egg::ovum::String symbol;
  EXPECT(pnode, pnode.value->getString(symbol));
  auto rtype = this->forgeType(*pnode.children.front());
  if (rtype == nullptr) {
    return this->expected(*pnode.children.front(), "function definition return type");
  }
  auto& forge = this->mbuilder.getTypeForge();
  auto fb = forge.createFunctionBuilder();
  assert(fb != nullptr);
  fb->setFunctionName(symbol);
  fb->setReturnType(rtype);
  for (size_t index = 1; index < pnode.children.size(); ++index) {
    auto& pchild = *pnode.children[index];
    assert(pchild.kind == ParserNode::Kind::TypeFunctionSignatureParameter);
    assert(pchild.children.size() == 1);
    egg::ovum::String pname;
    EXPECT(pchild, pchild.value->getString(pname));
    auto ptype = this->forgeType(*pchild.children.front());
    if (ptype == nullptr) {
      return nullptr;
    }
    switch (pchild.op.parameterOp) {
    case ParserNode::ParameterOp::Required:
      fb->addRequiredParameter(ptype, pname);
      break;
    case ParserNode::ParameterOp::Optional:
    default:
      fb->addOptionalParameter(ptype, pname);
      break;
    }
  }
  signature = &fb->build();
  type = forge.forgeFunctionType(*signature);
  return &this->mbuilder.typeLiteral(type, pnode.range);
}

ModuleNode* ModuleCompiler::compileLiteral(ParserNode& pnode) {
  EXPECT(pnode, pnode.children.size() == 0);
  return &this->mbuilder.exprLiteral(pnode.value, pnode.range);
}

egg::ovum::Type ModuleCompiler::literalType(ParserNode& pnode) {
  // Note that 'null' is not included on purpose
  EGG_WARNING_SUPPRESS_SWITCH_BEGIN
  switch (pnode.kind) {
  case ParserNode::Kind::TypeVoid:
    return egg::ovum::Type::Void;
  case ParserNode::Kind::TypeBool:
    return egg::ovum::Type::Bool;
  case ParserNode::Kind::TypeInt:
    return egg::ovum::Type::Int;
  case ParserNode::Kind::TypeFloat:
    return egg::ovum::Type::Float;
  case ParserNode::Kind::TypeString:
    return egg::ovum::Type::String;
  case ParserNode::Kind::TypeObject:
    return egg::ovum::Type::Object;
  case ParserNode::Kind::TypeAny:
    return egg::ovum::Type::Any;
  }
  EGG_WARNING_SUPPRESS_SWITCH_END
  return nullptr;
}

ModuleNode* ModuleCompiler::checkCompilation(ModuleNode& mnode) {
  auto type = this->mbuilder.deduce(mnode, this);
  if (type == nullptr) {
    return nullptr;
  }
  return &mnode;
}

bool ModuleCompiler::checkValueExprOperand(const char* expected, ModuleNode& mnode, ParserNode& pnode, egg::ovum::ValueFlags required) {
  auto type = this->deduceType(mnode);
  assert(type != nullptr);
  if (!egg::ovum::Bits::hasAnySet(type->getPrimitiveFlags(), required)) {
    this->error(pnode, "Expected ", expected, ", but instead got a value of type '", *type, "'");
    return false;
  }
  return true;
}

bool ModuleCompiler::checkValueExprOperand2(const char* expected, ModuleNode& lhs, ModuleNode& rhs, ParserNode& pnode, egg::ovum::ValueFlags required) {
  auto type = this->deduceType(lhs);
  assert(type != nullptr);
  if (!egg::ovum::Bits::hasAnySet(type->getPrimitiveFlags(), required)) {
    this->error(pnode, "Expected left-hand side of ", expected, ", but instead got a value of type '", *type, "'");
    return false;
  }
  type = this->deduceType(rhs);
  assert(type != nullptr);
  if (!egg::ovum::Bits::hasAnySet(type->getPrimitiveFlags(), required)) {
    this->error(pnode, "Expected right-hand side of ", expected, ", but instead got a value of type '", *type, "'");
    return false;
  }
  return true;
}

bool ModuleCompiler::checkValueExprUnary(egg::ovum::ValueUnaryOp op, ModuleNode& rhs, ParserNode& pnode) {
  switch (op) {
  case egg::ovum::ValueUnaryOp::Negate: // -a
    return this->checkValueExprOperand("expression after negation operator '-' to be an 'int' or 'float'", rhs, pnode, egg::ovum::ValueFlags::Arithmetic);
  case egg::ovum::ValueUnaryOp::BitwiseNot: // ~a
    return this->checkValueExprOperand("expression after bitwise-not operator '~' to be an 'int'", rhs, pnode, egg::ovum::ValueFlags::Int);
  case egg::ovum::ValueUnaryOp::LogicalNot: // !a
    return this->checkValueExprOperand("expression after logical-not operator '!' to be an 'int'", rhs, pnode, egg::ovum::ValueFlags::Bool);
  }
  return false;
}

bool ModuleCompiler::checkValueExprBinary(egg::ovum::ValueBinaryOp op, ModuleNode& lhs, ModuleNode& rhs, ParserNode& pnode) {
  const auto arithmetic = egg::ovum::ValueFlags::Arithmetic;
  const auto bitwise = egg::ovum::ValueFlags::Bool | egg::ovum::ValueFlags::Int;
  const auto integer = egg::ovum::ValueFlags::Bool | egg::ovum::ValueFlags::Int;
  switch (op) {
    case egg::ovum::ValueBinaryOp::Add: // a + b
      return this->checkValueExprOperand2("addition operator '+' to be an 'int' or 'float'", lhs, rhs, pnode, arithmetic);
    case egg::ovum::ValueBinaryOp::Subtract: // a - b
      return this->checkValueExprOperand2("subtraction operator '-' to be an 'int' or 'float'", lhs, rhs, pnode, arithmetic);
    case egg::ovum::ValueBinaryOp::Multiply: // a * b
      return this->checkValueExprOperand2("multiplication operator '*' to be an 'int' or 'float'", lhs, rhs, pnode, arithmetic);
    case egg::ovum::ValueBinaryOp::Divide: // a / b
      return this->checkValueExprOperand2("division operator '/' to be an 'int' or 'float'", lhs, rhs, pnode, arithmetic);
    case egg::ovum::ValueBinaryOp::Remainder: // a % b
      return this->checkValueExprOperand2("remainder operator '%' to be an 'int' or 'float'", lhs, rhs, pnode, arithmetic);
    case egg::ovum::ValueBinaryOp::LessThan: // a < b
      return this->checkValueExprOperand2("comparison operator '<' to be an 'int' or 'float'", lhs, rhs, pnode, arithmetic);
    case egg::ovum::ValueBinaryOp::LessThanOrEqual: // a <= b
      return this->checkValueExprOperand2("comparison operator '<=' to be an 'int' or 'float'", lhs, rhs, pnode, arithmetic);
    case egg::ovum::ValueBinaryOp::Equal: // a == b
      // TODO
      break;
    case egg::ovum::ValueBinaryOp::NotEqual: // a != b
      // TODO
      break;
    case egg::ovum::ValueBinaryOp::GreaterThanOrEqual: // a >= b
      return this->checkValueExprOperand2("comparison operator '>=' to be an 'int' or 'float'", lhs, rhs, pnode, arithmetic);
    case egg::ovum::ValueBinaryOp::GreaterThan: // a > b
      return this->checkValueExprOperand2("comparison operator '>' to be an 'int' or 'float'", lhs, rhs, pnode, arithmetic);
    case egg::ovum::ValueBinaryOp::BitwiseAnd: // a & b
      return this->checkValueExprOperand2("bitwise-and operator '&' to be a 'bool' or 'int'", lhs, rhs, pnode, bitwise);
    case egg::ovum::ValueBinaryOp::BitwiseOr: // a | b
      return this->checkValueExprOperand2("bitwise-or operator '|' to be a 'bool' or 'int'", lhs, rhs, pnode, bitwise);
    case egg::ovum::ValueBinaryOp::BitwiseXor: // a ^ b
      return this->checkValueExprOperand2("bitwise-xor operator '^' to be a 'bool' or 'int'", lhs, rhs, pnode, bitwise);
    case egg::ovum::ValueBinaryOp::ShiftLeft: // a << b
      return this->checkValueExprOperand2("left-shift operator '<<' to be an 'int'", lhs, rhs, pnode, integer);
    case egg::ovum::ValueBinaryOp::ShiftRight: // a >> b
      return this->checkValueExprOperand2("right-shift operator '>>' to be an 'int'", lhs, rhs, pnode, integer);
    case egg::ovum::ValueBinaryOp::ShiftRightUnsigned: // a >>> b
      return this->checkValueExprOperand2("unsigned-shift operator '>>>' to be an 'int'", lhs, rhs, pnode, integer);
    case egg::ovum::ValueBinaryOp::IfNull: // a ?? b
      // TODO
      break;
    case egg::ovum::ValueBinaryOp::IfFalse: // a || b
      // TODO
      break;
    case egg::ovum::ValueBinaryOp::IfTrue: // a && b
      // TODO
      break;
  }
  return true;
}

bool ModuleCompiler::checkValueExprTernary(egg::ovum::ValueTernaryOp, ModuleNode& lhs, ModuleNode& mid, ModuleNode& rhs, ParserNode& pnode) {
  if (!this->checkValueExprOperand("condition of ternary operator '?:' to be a 'bool'", lhs, pnode, egg::ovum::ValueFlags::Bool)) {
    return false;
  }
  if (this->checkCompilation(mid) == nullptr) {
    return false;
  }
  if (this->checkCompilation(rhs) == nullptr) {
    return false;
  }
  return true;
}

egg::ovum::Type ModuleCompiler::forgeType(ParserNode& pnode) {
  auto type = this->literalType(pnode);
  if (type != nullptr) {
    return type;
  }
  if (pnode.kind == ParserNode::Kind::TypeUnary) {
    assert(pnode.children.size() == 1);
    auto inner = this->forgeType(*pnode.children.front());
    if (inner == nullptr) {
      return nullptr;
    }
    switch (pnode.op.typeUnaryOp) {
    case egg::ovum::TypeUnaryOp::Pointer:
    case egg::ovum::TypeUnaryOp::Iterator:
    case egg::ovum::TypeUnaryOp::Array:
      // TODO
      break;
    case egg::ovum::TypeUnaryOp::Nullable:
      return this->vm.getTypeForge().forgeNullableType(inner, true);
    }
  }
  // TODO
  assert(false);
  return nullptr;
}

std::string ModuleCompiler::toString(const ParserNode& pnode) {
  switch (pnode.kind) {
  case ParserNode::Kind::ModuleRoot:
    return "module root";
  case ParserNode::Kind::StmtBlock:
    return "statement block";
  case ParserNode::Kind::StmtDeclareVariable:
    return "variable declaration statement";
  case ParserNode::Kind::StmtDefineVariable:
    return "variable definition statement";
  case ParserNode::Kind::StmtDefineFunction:
    return "function definition statement";
  case ParserNode::Kind::StmtForEach:
    return "for each statement";
  case ParserNode::Kind::StmtForLoop:
    return "for loop statement";
  case ParserNode::Kind::StmtIf:
    return "if statement";
  case ParserNode::Kind::StmtReturn:
    return "return statement";
  case ParserNode::Kind::StmtTry:
    return "try statement";
  case ParserNode::Kind::StmtCatch:
    return "catch statement";
  case ParserNode::Kind::StmtFinally:
    return "finally statement";
  case ParserNode::Kind::StmtWhile:
    return "while statement";
  case ParserNode::Kind::StmtMutate:
    return "mutate statement";
  case ParserNode::Kind::ExprVariable:
    return "variable expression";
  case ParserNode::Kind::ExprUnary:
    return "unary operator";
  case ParserNode::Kind::ExprBinary:
    return "binary operator";
  case ParserNode::Kind::ExprTernary:
    return "ternary operator";
  case ParserNode::Kind::ExprCall:
    return "call expression";
  case ParserNode::Kind::ExprIndex:
    return "index access";
  case ParserNode::Kind::ExprProperty:
    return "property access";
  case ParserNode::Kind::ExprArray:
    return "array expression";
  case ParserNode::Kind::ExprObject:
    return "object expression";
  case ParserNode::Kind::ExprGuard:
    return "guard expression";
  case ParserNode::Kind::TypeInfer:
    return "type infer";
  case ParserNode::Kind::TypeInferQ:
    return "type infer?";
  case ParserNode::Kind::TypeBool:
    return "type bool";
  case ParserNode::Kind::TypeVoid:
    return "type void";
  case ParserNode::Kind::TypeInt:
    return "type int";
  case ParserNode::Kind::TypeFloat:
    return "type float";
  case ParserNode::Kind::TypeString:
    return "type string";
  case ParserNode::Kind::TypeObject:
    return "type object";
  case ParserNode::Kind::TypeAny:
    return "type any";
  case ParserNode::Kind::TypeType:
    return "type type";
  case ParserNode::Kind::TypeUnary:
    return "type unary operator";
  case ParserNode::Kind::TypeBinary:
    return "type binary operator";
  case ParserNode::Kind::TypeFunctionSignature:
    return "type function signature";
  case ParserNode::Kind::TypeFunctionSignatureParameter:
    return "type function signature parameter";
  case ParserNode::Kind::Literal:
    return "literal";
  case ParserNode::Kind::Named:
    return "named expression";
  }
  return "unknown node kind";
}

egg::ovum::HardPtr<egg::ovum::IVMProgram> egg::yolk::EggCompilerFactory::compileFromStream(egg::ovum::IVM& vm, egg::yolk::TextStream& stream) {
  auto lexer = LexerFactory::createFromTextStream(stream);
  auto tokenizer = EggTokenizerFactory::createFromLexer(vm.getAllocator(), lexer);
  auto parser = EggParserFactory::createFromTokenizer(vm.getAllocator(), tokenizer);
  auto pbuilder = vm.createProgramBuilder();
  pbuilder->addBuiltin(vm.createString("assert"), egg::ovum::Type::Object); // TODO
  pbuilder->addBuiltin(vm.createString("print"), egg::ovum::Type::Object); // TODO
  auto compiler = EggCompilerFactory::createFromProgramBuilder(pbuilder);
  auto module = compiler->compile(*parser);
  if (module != nullptr) {
    return pbuilder->build();
  }
  return nullptr;
}

egg::ovum::HardPtr<egg::ovum::IVMProgram> egg::yolk::EggCompilerFactory::compileFromPath(egg::ovum::IVM& vm, const std::string& path, bool swallowBOM) {
  FileTextStream stream{ path, swallowBOM };
  return EggCompilerFactory::compileFromStream(vm, stream);
}

egg::ovum::HardPtr<egg::ovum::IVMProgram> egg::yolk::EggCompilerFactory::compileFromText(egg::ovum::IVM& vm, const std::string& text, const std::string& resource) {
  StringTextStream stream{ text, resource };
  return EggCompilerFactory::compileFromStream(vm, stream);
}

std::shared_ptr<IEggCompiler> EggCompilerFactory::createFromProgramBuilder(const egg::ovum::HardPtr<egg::ovum::IVMProgramBuilder>& builder) {
  auto compiler = std::make_shared<EggCompiler>(builder);
  return compiler;
}
