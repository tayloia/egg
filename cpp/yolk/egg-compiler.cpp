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
    std::stack<ModuleNode*> targets;
  public:
    ModuleCompiler(egg::ovum::IVM& vm, const egg::ovum::String& resource, egg::ovum::IVMModuleBuilder& mbuilder)
      : vm(vm),
        resource(resource),
        mbuilder(mbuilder) {
    }
    egg::ovum::HardPtr<egg::ovum::IVMModule> compile(ParserNode& root);
  private:
    class StmtContext {
    private:
      StmtContext() = default;
    public:
      bool moduleRoot : 1 = false;
      bool canBreak : 1 = false;
      bool canContinue : 1 = false;
      bool canReturn : 1 = false;
      bool canYield : 1 = false;
      bool canRethrow : 1 = false;
      static StmtContext root() {
        StmtContext context;
        context.moduleRoot = true;
        return context;
      }
      static StmtContext function() {
        StmtContext context;
        context.canReturn = true;
        return context;
      }
    };
    virtual void report(const egg::ovum::SourceRange& range, const egg::ovum::String& problem) override {
      auto message = this->concat(this->resource, range, ": ", problem);
      this->log(egg::ovum::ILogger::Source::Compiler, egg::ovum::ILogger::Severity::Error, message);
    }
    ModuleNode* compileStmt(ParserNode& pnode, const StmtContext& context);
    ModuleNode* compileStmtVoid(ParserNode& pnode);
    ModuleNode* compileStmtBlock(ParserNode& pnodes, const StmtContext& context);
    ModuleNode* compileStmtBlockInto(ParserNode& pnodes, const StmtContext& context, ModuleNode& parent);
    ModuleNode* compileStmtDeclareVariable(ParserNode& pnode);
    ModuleNode* compileStmtDeclareVariableScope(ParserNode& pnode);
    ModuleNode* compileStmtDefineVariable(ParserNode& pnode);
    ModuleNode* compileStmtDefineVariableScope(ParserNode& pnode);
    ModuleNode* compileStmtDefineFunction(ParserNode& pnode);
    ModuleNode* compileStmtDefineFunctionScope(ParserNode& pnode);
    ModuleNode* compileStmtMutate(ParserNode& pnode);
    ModuleNode* compileStmtForEach(ParserNode& pnode, const StmtContext& context);
    ModuleNode* compileStmtForLoop(ParserNode& pnode, const StmtContext& context);
    ModuleNode* compileStmtIf(ParserNode& pnode, const StmtContext& context);
    ModuleNode* compileStmtReturn(ParserNode& pnode, const StmtContext& context);
    ModuleNode* compileStmtTry(ParserNode& pnode, const StmtContext& context);
    ModuleNode* compileStmtCatch(ParserNode& pnode, const StmtContext& context);
    ModuleNode* compileValueExpr(ParserNode& pnode);
    ModuleNode* compileValueExprVariable(ParserNode& pnode);
    ModuleNode* compileValueExprUnary(ParserNode& op, ParserNode& rhs);
    ModuleNode* compileValueExprBinary(ParserNode& op, ParserNode& lhs, ParserNode& rhs);
    ModuleNode* compileValueExprTernary(ParserNode& op, ParserNode& lhs, ParserNode& mid, ParserNode& rhs);
    ModuleNode* compileValueExprPredicate(ParserNode& pnode);
    ModuleNode* compileValueExprCall(ParserNodes& pnodes);
    ModuleNode* compileValueExprCallAssert(ParserNode& function, ParserNode& predicate);
    ModuleNode* compileValueExprIndex(ParserNode& bracket, ParserNode& lhs, ParserNode& rhs);
    ModuleNode* compileValueExprProperty(ParserNode& dot, ParserNode& lhs, ParserNode& rhs);
    ModuleNode* compileValueExprArray(ParserNode& pnode);
    ModuleNode* compileValueExprArrayElement(ParserNode& pnode);
    ModuleNode* compileValueExprObject(ParserNode& pnode);
    ModuleNode* compileValueExprObjectElement(ParserNode& pnode);
    ModuleNode* compileTypeExpr(ParserNode& pnode);
    ModuleNode* compileTypeInfer(ParserNode& pnode, ParserNode& ptype, ParserNode& pexpr, ModuleNode*& mexpr);
    ModuleNode* compileTypeFunctionSignature(ParserNode& pnode);
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
    void log(egg::ovum::ILogger::Source source, egg::ovum::ILogger::Severity severity, const egg::ovum::String& message) const {
      this->vm.getLogger().log(source, severity, message);
    }
    template<typename... ARGS>
    egg::ovum::String concat(ARGS&&... args) const {
      return egg::ovum::StringBuilder::concat(this->vm.getAllocator(), std::forward<ARGS>(args)...);
    }
    template<typename... ARGS>
    ModuleNode* error(ParserNode& pnode, ARGS&&... args) const {
      auto message = this->concat(this->resource, pnode.range, ": ", std::forward<ARGS>(args)...);
      this->log(egg::ovum::ILogger::Source::Compiler, egg::ovum::ILogger::Severity::Error, message);
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
        auto& vm = this->pbuilder->getVM();
        auto mbuilder = this->pbuilder->createModuleBuilder(parser.resource());
        assert(mbuilder != nullptr);
        ModuleCompiler mcompiler(vm, resource, *mbuilder);
        return mcompiler.compile(*parsed.root);
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

egg::ovum::HardPtr<egg::ovum::IVMModule> ModuleCompiler::compile(ParserNode& root) {
  assert(this->targets.empty());
  if (root.kind != ParserNode::Kind::ModuleRoot) {
    this->expected(root, "module root node");
    return nullptr;
  }
  auto context = StmtContext::root();
  this->targets.push(&this->mbuilder.getRoot());
  for (const auto& child : root.children) {
    // Make sure we append to the target as it is now
    assert(this->targets.size() == 1);
    auto* target = this->targets.top();
    assert(target != nullptr);
    auto* stmt = this->compileStmt(*child, context);
    assert(this->targets.size() == 1);
    if (stmt == nullptr) {
      this->targets.pop();
      return nullptr;
    }
    this->mbuilder.appendChild(*target, *stmt);
  }
  this->targets.pop();
  assert(this->targets.empty());
  return this->mbuilder.build();
}

ModuleNode* ModuleCompiler::compileStmt(ParserNode& pnode, const StmtContext& context) {
  StmtContext inner{ context };
  inner.moduleRoot = false;
  switch (pnode.kind) {
  case ParserNode::Kind::StmtBlock:
    return this->compileStmtBlock(pnode, inner);
  case ParserNode::Kind::StmtDeclareVariable:
    EXPECT(pnode, pnode.children.size() == 1);
    return this->compileStmtDeclareVariableScope(pnode);
  case ParserNode::Kind::StmtDefineVariable:
    EXPECT(pnode, pnode.children.size() == 2);
    return this->compileStmtDefineVariableScope(pnode);
  case ParserNode::Kind::StmtDefineFunction:
    EXPECT(pnode, pnode.children.size() == 2);
    return this->compileStmtDefineFunctionScope(pnode);
  case ParserNode::Kind::StmtForEach:
    EXPECT(pnode, pnode.children.size() == 3);
    return this->compileStmtForEach(pnode, inner);
  case ParserNode::Kind::StmtForLoop:
    EXPECT(pnode, pnode.children.size() == 4);
    return this->compileStmtForLoop(pnode, inner);
  case ParserNode::Kind::StmtIf:
    EXPECT(pnode, (pnode.children.size() == 2) || (pnode.children.size() == 3));
    return this->compileStmtIf(pnode, inner);
  case ParserNode::Kind::StmtReturn:
    EXPECT(pnode, pnode.children.size() <= 2);
    return this->compileStmtReturn(pnode, inner);
  case ParserNode::Kind::StmtTry:
    EXPECT(pnode, pnode.children.size() >= 2);
    return this->compileStmtTry(pnode, inner);
  case ParserNode::Kind::StmtMutate:
    return this->compileStmtMutate(pnode);
  case ParserNode::Kind::ExprCall:
    EXPECT(pnode, !pnode.children.empty());
    return this->compileValueExprCall(pnode.children);
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
  case ParserNode::Kind::TypeInfer:
  case ParserNode::Kind::TypeInferQ:
  case ParserNode::Kind::TypeVoid:
  case ParserNode::Kind::TypeBool:
  case ParserNode::Kind::TypeInt:
  case ParserNode::Kind::TypeFloat:
  case ParserNode::Kind::TypeString:
  case ParserNode::Kind::TypeObject:
  case ParserNode::Kind::TypeAny:
  case ParserNode::Kind::TypeUnary:
  case ParserNode::Kind::TypeBinary:
  case ParserNode::Kind::TypeFunctionSignature:
  case ParserNode::Kind::TypeFunctionSignatureParameter:
  case ParserNode::Kind::Literal:
  case ParserNode::Kind::Named:
  default:
    break;
  }
  if (context.moduleRoot) {
    return this->expected(pnode, "statement root child");
  }
  return this->expected(pnode, "statement");
}

ModuleNode* ModuleCompiler::compileStmtVoid(ParserNode& pnode) {
  return &this->mbuilder.exprLiteral(egg::ovum::HardValue::Void, pnode.range);
}

ModuleNode* ModuleCompiler::compileStmtBlock(ParserNode& pnode, const StmtContext& context) {
  return this->compileStmtBlockInto(pnode, context, this->mbuilder.stmtBlock(pnode.range));
}

ModuleNode* ModuleCompiler::compileStmtBlockInto(ParserNode& pnode, const StmtContext& context, ModuleNode& parent) {
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

ModuleNode* ModuleCompiler::compileStmtDeclareVariable(ParserNode& pnode) {
  assert(pnode.kind == ParserNode::Kind::StmtDeclareVariable);
  egg::ovum::String symbol;
  EXPECT(pnode, pnode.value->getString(symbol));
  assert(pnode.children.size() == 1);
  auto& ptype = *pnode.children.front();
  auto* type = this->compileTypeExpr(ptype);
  if (type == nullptr) {
    return nullptr;
  }
  return &this->mbuilder.stmtVariableDeclare(symbol, *type, pnode.range);
}

ModuleNode* ModuleCompiler::compileStmtDeclareVariableScope(ParserNode& pnode) {
  auto* stmt = this->compileStmtDeclareVariable(pnode);
  this->targets.top() = stmt;
  return stmt;
}

ModuleNode* ModuleCompiler::compileStmtDefineVariable(ParserNode& pnode) {
  assert(pnode.kind == ParserNode::Kind::StmtDefineVariable);
  assert(pnode.children.size() == 2);
  egg::ovum::String symbol;
  EXPECT(pnode, pnode.value->getString(symbol));
  ModuleNode* rnode;
  auto lnode = this->compileTypeInfer(pnode, *pnode.children.front(), *pnode.children.back(), rnode);
  if (lnode == nullptr) {
    return nullptr;
  }
  assert(rnode != nullptr);
  auto ltype = this->deduceType(*lnode);
  assert(ltype != nullptr);
  auto rtype = this->deduceType(*rnode);
  assert(rtype != nullptr);
  auto assignable = this->isAssignable(ltype, rtype);
  if (assignable == egg::ovum::Assignability::Never) {
    return this->error(*pnode.children.back(), "Cannot initialize '", symbol, "' of type '", ltype, "' with a value of type '", rtype, "'");
  }
  return &this->mbuilder.stmtVariableDefine(symbol, *lnode, *rnode, pnode.range);
}

ModuleNode* ModuleCompiler::compileStmtDefineVariableScope(ParserNode& pnode) {
  auto* stmt = this->compileStmtDefineVariable(pnode);
  this->targets.top() = stmt;
  return stmt;
}

ModuleNode* ModuleCompiler::compileStmtDefineFunction(ParserNode& pnode) {
  assert(pnode.kind == ParserNode::Kind::StmtDefineFunction);
  assert(pnode.children.size() == 2);
  egg::ovum::String symbol;
  EXPECT(pnode, pnode.value->getString(symbol));
  auto& phead = *pnode.children.front();
  if (phead.kind != ParserNode::Kind::TypeFunctionSignature) {
    return this->expected(phead, "function signature in function definition");
  }
  auto mtype = this->compileTypeFunctionSignature(phead);
  if (mtype == nullptr) {
    return nullptr;
  }
  auto& ptail = *pnode.children.back();
  auto block = this->compileStmtBlockInto(ptail, StmtContext::function(), this->mbuilder.stmtFunctionInvoke(pnode.range));
  if (block == nullptr) {
    return nullptr;
  }
  auto& stmt = this->mbuilder.stmtFunctionDefine(symbol, *mtype, pnode.range);
  this->mbuilder.appendChild(stmt, *block);
  return &stmt;
}

ModuleNode* ModuleCompiler::compileStmtDefineFunctionScope(ParserNode& pnode) {
  auto* stmt = this->compileStmtDefineFunction(pnode);
  this->targets.top() = stmt;
  return stmt;
}

ModuleNode* ModuleCompiler::compileStmtMutate(ParserNode& pnode) {
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
      rhs = this->compileValueExpr(*pnode.children.back());
    }
    return &this->mbuilder.stmtVariableMutate(symbol, pnode.op.valueMutationOp, *rhs, pnode.range);
  }
  if (plhs.kind == ParserNode::Kind::ExprProperty) {
    EXPECT(plhs, plhs.children.size() == 2);
    auto* instance = this->compileValueExpr(*plhs.children.front());
    if (instance == nullptr) {
      return nullptr;
    }
    auto* property = this->compileValueExpr(*plhs.children.back());
    if (property == nullptr) {
      return nullptr;
    }
    ModuleNode* rhs;
    if (nudge) {
      rhs = this->compileStmtVoid(pnode);
    } else {
      rhs = this->compileValueExpr(*pnode.children.back());
    }
    return &this->mbuilder.stmtPropertyMutate(*instance, *property, pnode.op.valueMutationOp, *rhs, pnode.range);
  }
  if (plhs.kind == ParserNode::Kind::ExprIndex) {
    EXPECT(plhs, plhs.children.size() == 2);
    auto* instance = this->compileValueExpr(*plhs.children.front());
    if (instance == nullptr) {
      return nullptr;
    }
    auto* property = this->compileValueExpr(*plhs.children.back());
    if (property == nullptr) {
      return nullptr;
    }
    ModuleNode* rhs;
    if (nudge) {
      rhs = this->compileStmtVoid(pnode);
    } else {
      rhs = this->compileValueExpr(*pnode.children.back());
    }
    return &this->mbuilder.stmtIndexMutate(*instance, *property, pnode.op.valueMutationOp, *rhs, pnode.range);
  }
  return this->expected(plhs, "variable in mutation statement");
}

ModuleNode* ModuleCompiler::compileStmtForEach(ParserNode& pnode, const StmtContext& context) {
  assert(pnode.kind == ParserNode::Kind::StmtForEach);
  assert(pnode.children.size() == 3);
  egg::ovum::String symbol;
  EXPECT(pnode, pnode.value->getString(symbol));
  ModuleNode* iter;
  auto type = this->compileTypeInfer(pnode, *pnode.children[0], *pnode.children[1], iter);
  if (type == nullptr) {
    return nullptr;
  }
  assert(iter != nullptr);
  StmtContext inner{ context };
  inner.canBreak = true;
  inner.canContinue = true;
  auto* bloc = this->compileStmt(*pnode.children[2], inner);
  if (bloc == nullptr) {
    return nullptr;
  }
  return &this->mbuilder.stmtForEach(symbol, *type, *iter, *bloc, pnode.range);
}

ModuleNode* ModuleCompiler::compileStmtForLoop(ParserNode& pnode, const StmtContext& context) {
  assert(pnode.kind == ParserNode::Kind::StmtForLoop);
  assert(pnode.children.size() == 4);
  ModuleNode* scope;
  ModuleNode* init;
  StmtContext inner{ context };
  inner.canBreak = true;
  inner.canContinue = true;
  auto& phead = *pnode.children.front();
  if (phead.kind == ParserNode::Kind::StmtDeclareVariable) {
    // Hoist the declaration
    scope = this->compileStmtDeclareVariable(phead);
    if (scope == nullptr) {
      return nullptr;
    }
    init = this->compileStmtVoid(phead);
  } else if (phead.kind == ParserNode::Kind::StmtDefineVariable) {
    // Hoist the definition
    scope = this->compileStmtDefineVariable(phead);
    if (scope == nullptr) {
      return nullptr;
    }
    init = this->compileStmtVoid(phead);
  } else {
    // No outer scope
    scope = nullptr;
    init = this->compileStmt(phead, inner);
    if (init == nullptr) {
      return nullptr;
    }
  }
  assert(init != nullptr);
  auto* cond = this->compileValueExpr(*pnode.children[1]);
  if (cond == nullptr) {
    return nullptr;
  }
  auto* adva = this->compileStmt(*pnode.children[2], inner);
  if (adva == nullptr) {
    return nullptr;
  }
  auto* bloc = this->compileStmt(*pnode.children[3], inner);
  if (bloc == nullptr) {
    return nullptr;
  }
  auto* loop = &this->mbuilder.stmtForLoop(*init, *cond, *adva, *bloc, pnode.range);
  if (scope == nullptr) {
    return loop;
  }
  this->mbuilder.appendChild(*scope, *loop);
  return scope;
}

ModuleNode* ModuleCompiler::compileStmtIf(ParserNode& pnode, const StmtContext& context) {
  assert(pnode.kind == ParserNode::Kind::StmtIf);
  assert((pnode.children.size() == 2) || (pnode.children.size() == 3));
  ModuleNode* stmt;
  auto* condition = this->compileValueExpr(*pnode.children.front());
  if (condition == nullptr) {
    return nullptr;
  }
  auto* truthy = this->compileStmt(*pnode.children[1], context);
  if (truthy == nullptr) {
    return nullptr;
  }
  if (pnode.children.size() == 3) {
    // There is an 'else' clause
    auto* falsy = this->compileStmt(*pnode.children.back(), context);
    if (falsy == nullptr) {
      return nullptr;
    }
    stmt = &this->mbuilder.stmtIf(*condition, pnode.range);
    this->mbuilder.appendChild(*stmt, *truthy);
    this->mbuilder.appendChild(*stmt, *falsy);
  } else {
    // There is no 'else' clause
    stmt = &this->mbuilder.stmtIf(*condition, pnode.range);
    this->mbuilder.appendChild(*stmt, *truthy);
  }
  return stmt;
}

ModuleNode* ModuleCompiler::compileStmtReturn(ParserNode& pnode, const StmtContext& context) {
  assert(pnode.kind == ParserNode::Kind::StmtReturn);
  assert(pnode.children.size() <= 1);
  if (!context.canReturn) {
    return this->error(pnode, "'return' statements are only valid within function definitions");
  }
  auto* stmt = &this->mbuilder.stmtReturn(pnode.range);
  if (!pnode.children.empty()) {
    auto* expr = this->compileValueExpr(*pnode.children.back());
    if (expr == nullptr) {
      return nullptr;
    }
    this->mbuilder.appendChild(*stmt, *expr);
  }
  return stmt;
}

ModuleNode* ModuleCompiler::compileStmtTry(ParserNode& pnode, const StmtContext& context) {
  assert(pnode.kind == ParserNode::Kind::StmtTry);
  assert(pnode.children.size() >= 2);
  StmtContext inner{ context };
  ModuleNode* stmt = nullptr;
  size_t index = 0;
  auto seenFinally = false;
  for (const auto& pchild : pnode.children) {
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

ModuleNode* ModuleCompiler::compileStmtCatch(ParserNode& pnode, const StmtContext& context) {
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
      auto type = this->compileTypeExpr(*pchild);
      if (type != nullptr) {
        stmt = &this->mbuilder.stmtCatch(symbol, *type, pnode.range);
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

ModuleNode* ModuleCompiler::compileValueExpr(ParserNode& pnode) {
  switch (pnode.kind) {
  case ParserNode::Kind::ExprVariable:
    return this->compileValueExprVariable(pnode);
  case ParserNode::Kind::ExprUnary:
    EXPECT(pnode, pnode.children.size() == 1);
    EXPECT(pnode, pnode.children[0] != nullptr);
    return this->compileValueExprUnary(pnode, *pnode.children[0]);
  case ParserNode::Kind::ExprBinary:
    EXPECT(pnode, pnode.children.size() == 2);
    EXPECT(pnode, pnode.children[0] != nullptr);
    EXPECT(pnode, pnode.children[1] != nullptr);
    return this->compileValueExprBinary(pnode, *pnode.children[0], *pnode.children[1]);
  case ParserNode::Kind::ExprTernary:
    EXPECT(pnode, pnode.children.size() == 3);
    EXPECT(pnode, pnode.children[0] != nullptr);
    EXPECT(pnode, pnode.children[1] != nullptr);
    EXPECT(pnode, pnode.children[2] != nullptr);
    return this->compileValueExprTernary(pnode, *pnode.children[0], *pnode.children[1], *pnode.children[2]);
  case ParserNode::Kind::ExprCall:
    EXPECT(pnode, pnode.children.size() > 0);
    return this->compileValueExprCall(pnode.children);
  case ParserNode::Kind::ExprIndex:
    EXPECT(pnode, pnode.children.size() == 2);
    EXPECT(pnode, pnode.children[0] != nullptr);
    EXPECT(pnode, pnode.children[1] != nullptr);
    return this->compileValueExprIndex(pnode, *pnode.children[0], *pnode.children[1]);
  case ParserNode::Kind::ExprProperty:
    EXPECT(pnode, pnode.children.size() == 2);
    EXPECT(pnode, pnode.children[0] != nullptr);
    EXPECT(pnode, pnode.children[1] != nullptr);
    return this->compileValueExprProperty(pnode, *pnode.children[0], *pnode.children[1]);
  case ParserNode::Kind::ExprArray:
    return this->compileValueExprArray(pnode);
  case ParserNode::Kind::ExprObject:
    return this->compileValueExprObject(pnode);
  case ParserNode::Kind::Literal:
    return this->compileLiteral(pnode);
  case ParserNode::Kind::ModuleRoot:
  case ParserNode::Kind::TypeVoid:
  case ParserNode::Kind::TypeBool:
  case ParserNode::Kind::TypeInt:
  case ParserNode::Kind::TypeFloat:
  case ParserNode::Kind::TypeString:
  case ParserNode::Kind::TypeObject:
  case ParserNode::Kind::TypeAny:
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
  case ParserNode::Kind::StmtMutate:
  case ParserNode::Kind::Named:
  default:
    break;
  }
  return this->expected(pnode, "value expression");
}

ModuleNode* ModuleCompiler::compileValueExprVariable(ParserNode& pnode) {
  EXPECT(pnode, pnode.children.size() == 0);
  egg::ovum::String symbol;
  EXPECT(pnode, pnode.value->getString(symbol));
  return &this->mbuilder.exprVariable(symbol, pnode.range);
}

ModuleNode* ModuleCompiler::compileValueExprUnary(ParserNode& op, ParserNode& rhs) {
  auto* expr = this->compileValueExpr(rhs);
  if (expr != nullptr) {
    if (!this->checkValueExprUnary(op.op.valueUnaryOp, *expr, op)) {
      return nullptr;
    }
    return &this->mbuilder.exprValueUnaryOp(op.op.valueUnaryOp, *expr, op.range);
  }
  return nullptr;
}

ModuleNode* ModuleCompiler::compileValueExprBinary(ParserNode& op, ParserNode& lhs, ParserNode& rhs) {
  auto* lexpr = this->compileValueExpr(lhs);
  if (lexpr != nullptr) {
    auto* rexpr = this->compileValueExpr(rhs);
    if (rexpr != nullptr) {
      if (!this->checkValueExprBinary(op.op.valueBinaryOp, *lexpr, *rexpr, op)) {
        return nullptr;
      }
      return &this->mbuilder.exprValueBinaryOp(op.op.valueBinaryOp, *lexpr, *rexpr, op.range);
    }
  }
  return nullptr;
}

ModuleNode* ModuleCompiler::compileValueExprTernary(ParserNode& op, ParserNode& lhs, ParserNode& mid, ParserNode& rhs) {
  auto* lexpr = this->compileValueExpr(lhs);
  if (lexpr != nullptr) {
    auto* mexpr = this->compileValueExpr(mid);
    if (mexpr != nullptr) {
      auto* rexpr = this->compileValueExpr(rhs);
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

ModuleNode* ModuleCompiler::compileValueExprPredicate(ParserNode& pnode) {
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
      first = this->compileValueExpr(pnode);
    } else {
      first = this->compileValueExpr(*pnode.children.front());
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
      first = this->compileValueExpr(pnode);
      second = nullptr;
    } else {
      first = this->compileValueExpr(*pnode.children.front());
      second = this->compileValueExpr(*pnode.children.back());
      if (second == nullptr) {
        return nullptr;
      }
    }
  } else {
    first = this->compileValueExpr(pnode);
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

ModuleNode* ModuleCompiler::compileValueExprCall(ParserNodes& pnodes) {
  auto pnode = pnodes.begin();
  assert(pnode != pnodes.end());
  if (pnodes.size() == 2) {
    // Possible special case for 'assert(predicate)'
    // TODO: Replace with predicate argument hint
    egg::ovum::String symbol;
    if ((*pnode)->value->getString(symbol) && symbol.equals("assert")) {
      auto& predicate = *pnodes.back();
      return this->compileValueExprCallAssert(**pnode, predicate);
    }
  }
  ModuleNode* call = nullptr;
  auto type = this->literalType(**pnode);
  if (type != nullptr) {
    call = &this->mbuilder.typeLiteral(type, (*pnode)->range);
  } else {
    call = this->compileValueExpr(**pnode);
  }
  if (call != nullptr) {
    call = &this->mbuilder.exprFunctionCall(*call, (*pnode)->range);
  }
  while (++pnode != pnodes.end()) {
    auto* expr = this->compileValueExpr(**pnode);
    if (expr == nullptr) {
      call = nullptr;
    } else if (call != nullptr) {
      this->mbuilder.appendChild(*call, *expr);
    }
  }
  return call;
}

ModuleNode* ModuleCompiler::compileValueExprCallAssert(ParserNode& function, ParserNode& predicate) {
  // Specialization for 'assert(predicate)'
  auto* expr = this->compileValueExpr(function);
  if (expr == nullptr) {
    return nullptr;
  }
  auto& stmt = this->mbuilder.exprFunctionCall(*expr, function.range);
  expr = this->compileValueExprPredicate(predicate);
  if (expr == nullptr) {
    return nullptr;
  }
  this->mbuilder.appendChild(stmt, *expr);
  return &stmt;
}

ModuleNode* ModuleCompiler::compileValueExprIndex(ParserNode& bracket, ParserNode& lhs, ParserNode& rhs) {
  auto* lexpr = this->compileValueExpr(lhs);
  if (lexpr != nullptr) {
    auto* rexpr = this->compileValueExpr(rhs);
    if (rexpr != nullptr) {
      return &this->mbuilder.exprIndexGet(*lexpr, *rexpr, bracket.range);
    }
  }
  return nullptr;
}

ModuleNode* ModuleCompiler::compileValueExprProperty(ParserNode& dot, ParserNode& lhs, ParserNode& rhs) {
  ModuleNode* lexpr;
  auto type = this->literalType(lhs);
  if (type != nullptr) {
    // Something like 'string.from'
    lexpr = &this->mbuilder.typeLiteral(type, lhs.range);
  } else {
    lexpr = this->compileValueExpr(lhs);
  }
  if (lexpr != nullptr) {
    auto* rexpr = this->compileValueExpr(rhs);
    if (rexpr != nullptr) {
      return this->checkCompilation(this->mbuilder.exprPropertyGet(*lexpr, *rexpr, dot.range));
    }
  }
  return nullptr;
}

ModuleNode* ModuleCompiler::compileValueExprArray(ParserNode& pnode) {
  auto* array = &this->mbuilder.exprArray(pnode.range);
  for (auto& child : pnode.children) {
    auto* element = this->compileValueExprArrayElement(*child);
    if (element == nullptr) {
      array = nullptr;
    } else if (array != nullptr) {
      this->mbuilder.appendChild(*array, *element);
    }
  }
  return array;
}

ModuleNode* ModuleCompiler::compileValueExprArrayElement(ParserNode& pnode) {
  // TODO: handle ellipsis '...'
  return this->compileValueExpr(pnode);
}

ModuleNode* ModuleCompiler::compileValueExprObject(ParserNode& pnode) {
  auto* object = &this->mbuilder.exprObject(pnode.range);
  for (auto& child : pnode.children) {
    auto* element = this->compileValueExprObjectElement(*child);
    if (element == nullptr) {
      object = nullptr;
    } else if (object != nullptr) {
      this->mbuilder.appendChild(*object, *element);
    }
  }
  return object;
}

ModuleNode* ModuleCompiler::compileValueExprObjectElement(ParserNode& pnode) {
  // TODO: handle ellipsis '...'
  if (pnode.kind == ParserNode::Kind::Named) {
    EXPECT(pnode, pnode.children.size() == 1);
    auto* value = this->compileValueExpr(*pnode.children.front());
    if (value == nullptr) {
      return nullptr;
    }
    return &this->mbuilder.exprNamed(pnode.value, *value, pnode.range);
  }
  return this->expected(pnode, "object expression element");
}

ModuleNode* ModuleCompiler::compileTypeExpr(ParserNode& pnode) {
  auto type = this->forgeType(pnode);
  if (type == nullptr) {
    return this->expected(pnode, "type expression");
  }
  return &this->mbuilder.typeLiteral(type, pnode.range);
}

ModuleNode* ModuleCompiler::compileTypeInfer(ParserNode& pnode, ParserNode& ptype, ParserNode& pexpr, ModuleNode*& mexpr) {
  assert((pnode.kind == ParserNode::Kind::StmtDefineVariable) || (pnode.kind == ParserNode::Kind::StmtForEach));
  if (ptype.kind == ParserNode::Kind::TypeInfer) {
    mexpr = this->compileValueExpr(pexpr);
    if (mexpr == nullptr) {
      return nullptr;
    }
    auto type = this->deduceType(*mexpr);
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
    mexpr = this->compileValueExpr(pexpr);
    if (mexpr == nullptr) {
      return nullptr;
    }
    auto type = this->deduceType(*mexpr);
    assert(type != nullptr);
    if (pnode.kind == ParserNode::Kind::StmtForEach) {
      // We now have the type of 'iterable' in 'for (var <iterator> : <iterable>)'
      if (!this->deduceIterationType(type)) {
        return this->error(pexpr, "Value of type '", *type, "' is not iterable");
      }
      assert(type != nullptr);
    }
    this->forgeNullability(type, true);
    assert(type != nullptr);
    return &this->mbuilder.typeLiteral(type, ptype.range);
  }
  auto* mtype = this->compileTypeExpr(ptype);
  if (mtype == nullptr) {
    return nullptr;
  }
  mexpr = this->compileValueExpr(pexpr);
  if (mexpr == nullptr) {
    return nullptr;
  }
  if (pnode.kind == ParserNode::Kind::StmtForEach) {
    // We need to check the validity of 'for (<type> <iterator> : <iterable>)'
    auto type = this->deduceType(*mexpr);
    assert(type != nullptr);
    if (!this->deduceIterationType(type)) {
      return this->error(pexpr, "Value of type '", *type, "' is not iterable");
    }
  }
  return mtype;
}

ModuleNode* ModuleCompiler::compileTypeFunctionSignature(ParserNode& pnode) {
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
  auto ftype = forge.forgeFunctionType(fb->build());
  return &this->mbuilder.typeLiteral(ftype, pnode.range);
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
  // TODO
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
  return std::make_shared<EggCompiler>(builder);
}
