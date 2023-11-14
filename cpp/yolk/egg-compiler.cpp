#include "yolk/yolk.h"
#include "yolk/lexers.h"
#include "yolk/egg-tokenizer.h"
#include "yolk/egg-parser.h"
#include "yolk/egg-compiler.h"

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
    sb.add(" : ");
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
  public:
    ModuleCompiler(egg::ovum::IVM& vm, const egg::ovum::String& resource, egg::ovum::IVMModuleBuilder& mbuilder)
      : vm(vm),
        resource(resource),
        mbuilder(mbuilder) {
    }
    egg::ovum::HardPtr<egg::ovum::IVMModule> compile(ParserNode& root);
  private:
    struct StmtContext {
      bool root : 1 = false;
    };
    ModuleNode* compileStmt(ParserNode& pnode, const StmtContext& context);
    ModuleNode* compileStmtCall(ParserNode& pnode);
    ModuleNode* compileExpr(ParserNode& pnode);
    ModuleNode* compileExprVar(ParserNode& pnode);
    ModuleNode* compileExprCall(ParserNodes& pnodes);
    ModuleNode* compileLiteral(ParserNode& pnode);
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
  if (root.kind != ParserNode::Kind::ModuleRoot) {
    this->unexpected(root, "module root node");
    return nullptr;
  }
  StmtContext context;
  context.root = true;
  for (const auto& child : root.children) {
    auto* stmt = this->compileStmt(*child, context);
    if (stmt == nullptr) {
      return nullptr;
    }
    this->mbuilder.addStatement(*stmt);
  }
  return this->mbuilder.build();
}

ModuleNode* ModuleCompiler::compileStmt(ParserNode& pnode, const StmtContext& context) {
  switch (pnode.kind) {
  case ParserNode::Kind::StmtCall:
    EXPECT(pnode, pnode.children.size() == 1);
    return this->compileStmtCall(*pnode.children.front());
  case ParserNode::Kind::ModuleRoot:
  case ParserNode::Kind::ExprVar:
  case ParserNode::Kind::ExprCall:
  case ParserNode::Kind::Literal:
  default:
    break;
  }
  if (context.root) {
    return this->unexpected(pnode, "statement root child");
  }
  return this->unexpected(pnode, "statement");
}

ModuleNode* ModuleCompiler::compileStmtCall(ParserNode& pnode) {
  EXPECT(pnode, pnode.kind == ParserNode::Kind::ExprCall);
  ModuleNode* stmt = nullptr;
  auto pchild = pnode.children.begin();
  auto* expr = this->compileExpr(**pchild);
  if (expr != nullptr) {
    stmt = &this->mbuilder.stmtFunctionCall(*expr, pnode.begin.line, pnode.begin.column);
  }
  while (++pchild != pnode.children.end()) {
    expr = this->compileExpr(**pchild);
    if (expr == nullptr) {
      stmt = nullptr;
    } else if (stmt != nullptr) {
      this->mbuilder.appendChild(*stmt, *expr);
    }
  }
  return stmt;
}

ModuleNode* ModuleCompiler::compileExpr(ParserNode& pnode) {
  switch (pnode.kind) {
  case ParserNode::Kind::ExprVar:
    return this->compileExprVar(pnode);
  case ParserNode::Kind::ExprCall:
    EXPECT(pnode, pnode.children.size() > 0);
    return this->compileExprCall(pnode.children);
  case ParserNode::Kind::Literal:
    return this->compileLiteral(pnode);
  case ParserNode::Kind::ModuleRoot:
  case ParserNode::Kind::StmtCall:
  default:
    break;
  }
  return this->unexpected(pnode, "expression");
}

ModuleNode* ModuleCompiler::compileExprVar(ParserNode& pnode) {
  EXPECT(pnode, pnode.children.size() == 0);
  egg::ovum::String symbol;
  EXPECT(pnode, pnode.value->getString(symbol));
  return &this->mbuilder.exprVariable(symbol, pnode.begin.line, pnode.begin.column);
}

ModuleNode* ModuleCompiler::compileExprCall(ParserNodes& pnodes) {
  auto pnode = pnodes.begin();
  assert(pnode != pnodes.end());
  ModuleNode* call = nullptr;
  auto* expr = this->compileExpr(**pnode);
  if (expr != nullptr) {
    call = &this->mbuilder.exprFunctionCall(*expr, (*pnode)->begin.line, (*pnode)->begin.column);
  }
  while (++pnode != pnodes.end()) {
    expr = this->compileExpr(**pnode);
    if (expr == nullptr) {
      call = nullptr;
    } else if (call != nullptr) {
      this->mbuilder.appendChild(*call, *expr);
    }
  }
  return call;
}

ModuleNode* ModuleCompiler::compileLiteral(ParserNode& pnode) {
  EXPECT(pnode, pnode.children.size() == 0);
  return &this->mbuilder.exprLiteral(pnode.value, pnode.begin.line, pnode.begin.column);
}

std::string ModuleCompiler::toString(const ParserNode& pnode) {
  switch (pnode.kind) {
  case ParserNode::Kind::ModuleRoot:
    return "call statement";
  case ParserNode::Kind::StmtCall:
    return "module route";
  case ParserNode::Kind::ExprVar:
    return "variable expression";
  case ParserNode::Kind::ExprCall:
    return "call expression";
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
