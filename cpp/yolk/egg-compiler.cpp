#include "yolk/yolk.h"
#include "yolk/lexers.h"
#include "yolk/egg-tokenizer.h"
#include "yolk/egg-parser.h"
#include "yolk/egg-compiler.h"

#include <stack>

// TODO remove and replace with better messages
#define EXPECT(node, condition) \
  if (condition) {} else return this->error(node, "Expection failure in egg-compiler line ", __LINE__, ": " #condition);

using namespace egg::yolk;

namespace {
  using ModuleNode = egg::ovum::IVMModule::Node;
  using ParserNode = egg::yolk::IEggParser::Node;
  using ParserNodes = std::vector<std::unique_ptr<ParserNode>>;

  void printIssueRange(egg::ovum::StringBuilder& sb, const egg::ovum::String& resource, const IEggParser::Location& begin, const IEggParser::Location& end) {
    // See https://learn.microsoft.com/en-us/visualstudio/msbuild/msbuild-diagnostic-format-for-tasks
    // Also https://sarifweb.azurewebsites.net/
    sb.add(resource);
    if (begin.line > 0) {
      // resource(line
      sb.add('(', begin.line);
      if (begin.column > 0) {
        // resource(line,column
        sb.add(',', begin.column);
        if ((end.line > 0) && (end.column > 0)) {
          if (end.line > begin.line) {
            // resource(line,column,line,column
            sb.add(',', end.line, ',', end.column);
          } else if (end.column > begin.column) {
            // resource(line,column-column
            sb.add('-', end.column);
          }
        }
      }
      sb.add(')');
    }
    sb.add(": ");
  }

  egg::ovum::String formatIssue(egg::ovum::IAllocator& allocator, const egg::ovum::String& resource, const IEggParser::Issue& issue) {
    egg::ovum::StringBuilder sb;
    printIssueRange(sb, resource, issue.begin, issue.end);
    sb.add(issue.message);
    return sb.build(allocator);
  }

  class ModuleCompiler final {
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
    struct StmtContext {
      bool inRoot : 1 = false;
      bool inFor : 1 = false;
    };
    ModuleNode* compileStmt(ParserNode& pnode, const StmtContext& context);
    ModuleNode* compileStmtBlock(ParserNodes& pnodes, const StmtContext& context, const IEggParser::Location& location);
    ModuleNode* compileStmtDeclareVariable(ParserNode& pnode);
    ModuleNode* compileStmtDefineVariable(ParserNode& pnode);
    ModuleNode* compileStmtMutate(ParserNode& pnode);
    ModuleNode* compileStmtCall(ParserNode& pnode);
    ModuleNode* compileStmtForEach(ParserNode& pnode, const StmtContext& context);
    ModuleNode* compileStmtForLoop(ParserNode& pnode, const StmtContext& context);
    ModuleNode* compileValueExpr(ParserNode& pnode);
    ModuleNode* compileValueExprVariable(ParserNode& pnode);
    ModuleNode* compileValueExprUnary(ParserNode& op, ParserNode& rhs);
    ModuleNode* compileValueExprBinary(ParserNode& op, ParserNode& lhs, ParserNode& rhs);
    ModuleNode* compileValueExprTernary(ParserNode& op, ParserNode& lhs, ParserNode& mid, ParserNode& rhs);
    ModuleNode* compileValueExprCall(ParserNodes& pnodes);
    ModuleNode* compileValueExprIndex(ParserNode& bracket, ParserNode& lhs, ParserNode& rhs);
    ModuleNode* compileValueExprProperty(ParserNode& dot, ParserNode& lhs, ParserNode& rhs);
    ModuleNode* compileTypeExpr(ParserNode& pnode);
    ModuleNode* compileTypeInfer(ParserNode& ptype, ParserNode& pexpr, ModuleNode*& mexpr);
    ModuleNode* compileLiteral(ParserNode& pnode);
    egg::ovum::Type deduceType(ModuleNode& mnode) {
      return this->mbuilder.deduceType(mnode);
    }
    void setNullability(egg::ovum::Type& type, bool nullable) {
      type = &this->vm.getTypeForge().setNullability(*type, nullable);
    }
    egg::ovum::ITypeForge::Assignability isAssignable(const egg::ovum::Type& dst, const egg::ovum::Type& src) {
      assert(dst != nullptr);
      assert(src != nullptr);
      return this->vm.getTypeForge().isAssignable(*dst, *src);
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
      egg::ovum::StringBuilder sb;
      printIssueRange(sb, this->resource, pnode.begin, pnode.end);
      sb.add(std::forward<ARGS>(args)...);
      auto message = sb.build(this->vm.getAllocator());
      this->log(egg::ovum::ILogger::Source::Compiler, egg::ovum::ILogger::Severity::Error, message);
      return nullptr;
    }
    ModuleNode* unexpected(ParserNode& pnode, const char* expected) const {
      return this->error(pnode, "Expected ", expected, ", but instead got ", ModuleCompiler::toString(pnode));
    }
    static std::string toString(const ParserNode& pnode);
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
      egg::ovum::String message;
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
        logger.log(egg::ovum::ILogger::Source::Compiler, severity, formatIssue(this->pbuilder->getAllocator(), resource, issue));
      }
      return worst;
    }
  };
}

egg::ovum::HardPtr<egg::ovum::IVMModule> ModuleCompiler::compile(ParserNode& root) {
  assert(this->targets.empty());
  if (root.kind != ParserNode::Kind::ModuleRoot) {
    this->unexpected(root, "module root node");
    return nullptr;
  }
  this->targets.push(&this->mbuilder.getRoot());
  StmtContext context;
  context.inRoot = true;
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
  StmtContext inner = context;
  inner.inRoot = false;
  switch (pnode.kind) {
  case ParserNode::Kind::StmtBlock:
    return this->compileStmtBlock(pnode.children, context, pnode.begin);
  case ParserNode::Kind::StmtDeclareVariable:
    EXPECT(pnode, pnode.children.size() == 1);
    return this->compileStmtDeclareVariable(pnode);
  case ParserNode::Kind::StmtDefineVariable:
    EXPECT(pnode, pnode.children.size() == 2);
    return this->compileStmtDefineVariable(pnode);
  case ParserNode::Kind::StmtCall:
    EXPECT(pnode, pnode.children.size() == 1);
    return this->compileStmtCall(*pnode.children.front());
  case ParserNode::Kind::StmtForEach:
    EXPECT(pnode, pnode.children.size() == 3);
    return this->compileStmtForEach(pnode, context);
  case ParserNode::Kind::StmtForLoop:
    EXPECT(pnode, pnode.children.size() == 4);
    return this->compileStmtForLoop(pnode, context);
  case ParserNode::Kind::StmtMutate:
    return this->compileStmtMutate(pnode);
  case ParserNode::Kind::ModuleRoot:
  case ParserNode::Kind::ExprVariable:
  case ParserNode::Kind::ExprUnary:
  case ParserNode::Kind::ExprBinary:
  case ParserNode::Kind::ExprTernary:
  case ParserNode::Kind::ExprCall:
  case ParserNode::Kind::ExprIndex:
  case ParserNode::Kind::ExprProperty:
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
  case ParserNode::Kind::Literal:
  default:
    break;
  }
  if (context.inRoot) {
    return this->unexpected(pnode, "statement root child");
  }
  return this->unexpected(pnode, "statement");
}

ModuleNode* ModuleCompiler::compileStmtBlock(ParserNodes& pnodes, const StmtContext& context, const IEggParser::Location& location) {
  auto& block = this->mbuilder.stmtBlock(location.line, location.column);
  for (const auto& pnode : pnodes) {
    auto* mnode = this->compileStmt(*pnode, context);
    if (mnode == nullptr) {
      return nullptr;
    }
    this->mbuilder.appendChild(block, *mnode);
  }
  return &block;
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
  auto& stmt = this->mbuilder.stmtVariableDeclare(symbol, *type, pnode.begin.line, pnode.begin.column);
  this->targets.top() = &stmt;
  return &stmt;
}

ModuleNode* ModuleCompiler::compileStmtDefineVariable(ParserNode& pnode) {
  assert(pnode.kind == ParserNode::Kind::StmtDefineVariable);
  assert(pnode.children.size() == 2);
  egg::ovum::String symbol;
  EXPECT(pnode, pnode.value->getString(symbol));
  ModuleNode* rnode;
  auto lnode = this->compileTypeInfer(*pnode.children.front(), *pnode.children.back(), rnode);
  if (lnode == nullptr) {
    return nullptr;
  }
  assert(rnode != nullptr);
  auto ltype = this->deduceType(*lnode);
  assert(ltype != nullptr);
  auto rtype = this->deduceType(*rnode);
  assert(rtype != nullptr);
  auto assignable = this->isAssignable(ltype, rtype);
  if (assignable == egg::ovum::ITypeForge::Assignability::Never) {
    return this->error(*pnode.children.back(), "Cannot initialize '", symbol, "' of type '", ltype, "' with a value of type '", rtype, "'");
  }
  auto& stmt = this->mbuilder.stmtVariableDefine(symbol, *lnode, *rnode, pnode.begin.line, pnode.begin.column);
  this->targets.top() = &stmt;
  return &stmt;
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
      rhs = &this->mbuilder.exprLiteral(egg::ovum::HardValue::Void, pnode.begin.line, pnode.begin.column);
    } else {
      rhs = this->compileValueExpr(*pnode.children.back());
    }
    return &this->mbuilder.stmtVariableMutate(symbol, pnode.op.valueMutationOp, *rhs, pnode.begin.line, pnode.begin.column);
  }
  return this->unexpected(plhs, "variable in mutation statement");
}

ModuleNode* ModuleCompiler::compileStmtCall(ParserNode& pnode) {
  assert(pnode.kind == ParserNode::Kind::ExprCall);
  ModuleNode* stmt = nullptr;
  auto pchild = pnode.children.begin();
  auto* expr = this->compileValueExpr(**pchild);
  if (expr != nullptr) {
    stmt = &this->mbuilder.stmtFunctionCall(*expr, pnode.begin.line, pnode.begin.column);
  }
  while (++pchild != pnode.children.end()) {
    expr = this->compileValueExpr(**pchild);
    if (expr == nullptr) {
      stmt = nullptr;
    } else if (stmt != nullptr) {
      this->mbuilder.appendChild(*stmt, *expr);
    }
  }
  return stmt;
}

ModuleNode* ModuleCompiler::compileStmtForEach(ParserNode& pnode, const StmtContext& context) {
  assert(pnode.kind == ParserNode::Kind::StmtForEach);
  assert(pnode.children.size() == 3);
  egg::ovum::String symbol;
  EXPECT(pnode, pnode.value->getString(symbol));
  ModuleNode* iter;
  auto type = this->compileTypeInfer(*pnode.children[0], *pnode.children[1], iter);
  if (type == nullptr) {
    return nullptr;
  }
  assert(iter != nullptr);
  StmtContext inner = context;
  inner.inFor = true;
  auto* bloc = this->compileStmt(*pnode.children[2], inner);
  if (bloc == nullptr) {
    return nullptr;
  }
  return &this->mbuilder.stmtForEach(symbol, *type, *iter, *bloc, pnode.begin.line, pnode.begin.column);
}

ModuleNode* ModuleCompiler::compileStmtForLoop(ParserNode& pnode, const StmtContext& context) {
  assert(pnode.kind == ParserNode::Kind::StmtForLoop);
  assert(pnode.children.size() == 4);
  StmtContext inner = context;
  inner.inFor = true;
  auto* init = this->compileStmt(*pnode.children[0], inner);
  if (init == nullptr) {
    return nullptr;
  }
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
  return &this->mbuilder.stmtForLoop(*init, *cond, *adva, *bloc, pnode.begin.line, pnode.begin.column);
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
  case ParserNode::Kind::Literal:
    return this->compileLiteral(pnode);
  case ParserNode::Kind::TypeString:
    EXPECT(pnode, pnode.children.size() == 0);
    return &this->mbuilder.exprStringCall(pnode.begin.line, pnode.begin.column);
  case ParserNode::Kind::TypeVoid:
  case ParserNode::Kind::TypeBool:
  case ParserNode::Kind::TypeInt:
  case ParserNode::Kind::TypeFloat:
  case ParserNode::Kind::TypeObject:
  case ParserNode::Kind::TypeAny:
    // TODO constructors above
  case ParserNode::Kind::ModuleRoot:
  case ParserNode::Kind::TypeInfer:
  case ParserNode::Kind::TypeInferQ:
  case ParserNode::Kind::TypeUnary:
  case ParserNode::Kind::TypeBinary:
  case ParserNode::Kind::StmtBlock:
  case ParserNode::Kind::StmtCall:
  case ParserNode::Kind::StmtDeclareVariable:
  case ParserNode::Kind::StmtDefineVariable:
  case ParserNode::Kind::StmtForEach:
  case ParserNode::Kind::StmtForLoop:
  case ParserNode::Kind::StmtMutate:
  default:
    break;
  }
  return this->unexpected(pnode, "value expression");
}

ModuleNode* ModuleCompiler::compileValueExprVariable(ParserNode& pnode) {
  EXPECT(pnode, pnode.children.size() == 0);
  egg::ovum::String symbol;
  EXPECT(pnode, pnode.value->getString(symbol));
  return &this->mbuilder.exprVariable(symbol, pnode.begin.line, pnode.begin.column);
}

ModuleNode* ModuleCompiler::compileValueExprUnary(ParserNode& op, ParserNode& rhs) {
  auto* expr = this->compileValueExpr(rhs);
  if (expr != nullptr) {
    return &this->mbuilder.exprValueUnaryOp(op.op.valueUnaryOp, *expr, op.begin.line, op.begin.column);
  }
  return nullptr;
}

ModuleNode* ModuleCompiler::compileValueExprBinary(ParserNode& op, ParserNode& lhs, ParserNode& rhs) {
  auto* lexpr = this->compileValueExpr(lhs);
  if (lexpr != nullptr) {
    auto* rexpr = this->compileValueExpr(rhs);
    if (rexpr != nullptr) {
      return &this->mbuilder.exprValueBinaryOp(op.op.valueBinaryOp, *lexpr, *rexpr, op.begin.line, op.begin.column);
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
        return &this->mbuilder.exprValueTernaryOp(op.op.valueTernaryOp, *lexpr, *mexpr, *rexpr, op.begin.line, op.begin.column);
      }
    }
  }
  return nullptr;
}

ModuleNode* ModuleCompiler::compileValueExprCall(ParserNodes& pnodes) {
  ModuleNode* call = nullptr;
  auto pnode = pnodes.begin();
  assert(pnode != pnodes.end());
  auto* expr = this->compileValueExpr(**pnode);
  if (expr != nullptr) {
    call = &this->mbuilder.exprFunctionCall(*expr, (*pnode)->begin.line, (*pnode)->begin.column);
  }
  while (++pnode != pnodes.end()) {
    expr = this->compileValueExpr(**pnode);
    if (expr == nullptr) {
      call = nullptr;
    } else if (call != nullptr) {
      this->mbuilder.appendChild(*call, *expr);
    }
  }
  return call;
}

ModuleNode* ModuleCompiler::compileValueExprIndex(ParserNode& bracket, ParserNode& lhs, ParserNode& rhs) {
  auto* lexpr = this->compileValueExpr(lhs);
  if (lexpr != nullptr) {
    auto* rexpr = this->compileValueExpr(rhs);
    if (rexpr != nullptr) {
      return &this->mbuilder.exprIndexGet(*lexpr, *rexpr, bracket.begin.line, bracket.begin.column);
    }
  }
  return nullptr;
}

ModuleNode* ModuleCompiler::compileValueExprProperty(ParserNode& dot, ParserNode& lhs, ParserNode& rhs) {
  auto* lexpr = this->compileValueExpr(lhs);
  if (lexpr != nullptr) {
    auto* rexpr = this->compileValueExpr(rhs);
    if (rexpr != nullptr) {
      return &this->mbuilder.exprPropertyGet(*lexpr, *rexpr, dot.begin.line, dot.begin.column);
    }
  }
  return nullptr;
}

ModuleNode* ModuleCompiler::compileTypeExpr(ParserNode& pnode) {
  switch (pnode.kind) {
  case ParserNode::Kind::TypeVoid:
    EXPECT(pnode, pnode.children.size() == 0);
    return &this->mbuilder.typeLiteral(egg::ovum::Type::Void, pnode.begin.line, pnode.end.column);
  case ParserNode::Kind::TypeBool:
    EXPECT(pnode, pnode.children.size() == 0);
    return &this->mbuilder.typeLiteral(egg::ovum::Type::Bool, pnode.begin.line, pnode.end.column);
  case ParserNode::Kind::TypeInt:
    EXPECT(pnode, pnode.children.size() == 0);
    return &this->mbuilder.typeLiteral(egg::ovum::Type::Int, pnode.begin.line, pnode.end.column);
  case ParserNode::Kind::TypeFloat:
    EXPECT(pnode, pnode.children.size() == 0);
    return &this->mbuilder.typeLiteral(egg::ovum::Type::Float, pnode.begin.line, pnode.end.column);
  case ParserNode::Kind::TypeString:
    EXPECT(pnode, pnode.children.size() == 0);
    return &this->mbuilder.typeLiteral(egg::ovum::Type::String, pnode.begin.line, pnode.end.column);
  case ParserNode::Kind::TypeObject:
    EXPECT(pnode, pnode.children.size() == 0);
    return &this->mbuilder.typeLiteral(egg::ovum::Type::Object, pnode.begin.line, pnode.end.column);
  case ParserNode::Kind::TypeAny:
    EXPECT(pnode, pnode.children.size() == 0);
    return &this->mbuilder.typeLiteral(egg::ovum::Type::Any, pnode.begin.line, pnode.end.column);
  case ParserNode::Kind::TypeUnary:
  case ParserNode::Kind::TypeBinary:
    // WIBBLE
    break;
  case ParserNode::Kind::ModuleRoot:
  case ParserNode::Kind::StmtBlock:
  case ParserNode::Kind::StmtDeclareVariable:
  case ParserNode::Kind::StmtDefineVariable:
  case ParserNode::Kind::StmtCall:
  case ParserNode::Kind::StmtForEach:
  case ParserNode::Kind::StmtForLoop:
  case ParserNode::Kind::StmtMutate:
  case ParserNode::Kind::ExprVariable:
  case ParserNode::Kind::ExprUnary:
  case ParserNode::Kind::ExprBinary:
  case ParserNode::Kind::ExprTernary:
  case ParserNode::Kind::ExprCall:
  case ParserNode::Kind::ExprIndex:
  case ParserNode::Kind::ExprProperty:
  case ParserNode::Kind::TypeInfer:
  case ParserNode::Kind::TypeInferQ:
  case ParserNode::Kind::Literal:
  default:
    break;
  }
  return this->unexpected(pnode, "type expression");
}

ModuleNode* ModuleCompiler::compileTypeInfer(ParserNode& ptype, ParserNode& pexpr, ModuleNode*& mexpr) {
  if (ptype.kind == ParserNode::Kind::TypeInfer) {
    mexpr = this->compileValueExpr(pexpr);
    if (mexpr == nullptr) {
      return nullptr;
    }
    auto type = this->deduceType(*mexpr);
    assert(type != nullptr);
    this->setNullability(type, false);
    assert(type != nullptr);
    return &this->mbuilder.typeLiteral(type, ptype.begin.line, ptype.begin.column);
  }
  if (ptype.kind == ParserNode::Kind::TypeInferQ) {
    mexpr = this->compileValueExpr(pexpr);
    if (mexpr == nullptr) {
      return nullptr;
    }
    auto type = this->deduceType(*mexpr);
    assert(type != nullptr);
    this->setNullability(type, true);
    assert(type != nullptr);
    return &this->mbuilder.typeLiteral(type, ptype.begin.line, ptype.begin.column);
  }
  auto* mtype = this->compileTypeExpr(ptype);
  if (mtype == nullptr) {
    return nullptr;
  }
  mexpr = this->compileValueExpr(pexpr);
  if (mexpr == nullptr) {
    return nullptr;
  }
  return mtype;
}

ModuleNode* ModuleCompiler::compileLiteral(ParserNode& pnode) {
  EXPECT(pnode, pnode.children.size() == 0);
  return &this->mbuilder.exprLiteral(pnode.value, pnode.begin.line, pnode.begin.column);
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
  case ParserNode::Kind::StmtCall:
    return "call statement";
  case ParserNode::Kind::StmtForEach:
    return "for each statement";
  case ParserNode::Kind::StmtForLoop:
    return "for loop statement";
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
  case ParserNode::Kind::Literal:
    return "literal";
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
