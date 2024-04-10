#include "ovum/ovum.h"
#include "ovum/file.h"
#include "ovum/lexer.h"
#include "ovum/stream.h"
#include "ovum/egg-tokenizer.h"
#include "ovum/egg-parser.h"
#include "ovum/egg-compiler.h"

// TODO remove and replace with better messages
#define EXPECT(node, condition) \
  if (condition) {} else return this->error(node, "Expection failure in egg-compiler.cpp line ", __LINE__, ": " #condition);

namespace {
  using namespace egg::ovum;

  using ModuleNode = IVMModule::Node;
  using Parser = IEggParser;
  using ParserNode = Parser::Node;
  using ParserNodes = std::vector<std::unique_ptr<ParserNode>>;

  enum class Ambiguous {
    Value,
    Type
  };

  Accessability getAccessabilityUnion(const IPropertySignature& dotable) {
    // Fix all accessibility bits for all properties (including unknowns for open sets)
    auto bits = dotable.getAccessability({});
    auto index = dotable.getNameCount();
    while (index-- > 0) {
      bits = Bits::set(bits, dotable.getAccessability(dotable.getName(index)));
    }
    return bits;
  }

  class ModuleCompiler final : public IVMModuleBuilder::Reporter {
    ModuleCompiler(const ModuleCompiler&) = delete;
    ModuleCompiler& operator=(const ModuleCompiler&) = delete;
  private:
    IVM& vm;
    String resource;
    IVMModuleBuilder& mbuilder;
  public:
    ModuleCompiler(IVM& vm, const String& resource, IVMModuleBuilder& mbuilder)
      : vm(vm),
        resource(resource),
        mbuilder(mbuilder) {
    }
    class StmtContext;
    HardPtr<IVMModule> compile(ParserNode& root, StmtContext& context);
    virtual void report(const SourceRange& range, const String& problem) override {
      this->log(ILogger::Severity::Error, this->resource, range, ": ", problem);
    }
    struct ExprContextData {
      Type arrayElementType = nullptr;
    };
    class ExprContext : public ExprContextData {
      ExprContext() = delete;
      ExprContext(const ExprContext& parent) = delete;
    public:
      struct Symbol {
        enum class Kind {
          Builtin,
          Parameter,
          Variable,
          Function,
          Type
        };
        Kind kind;
        Type type;
        SourceRange range;
      };
    protected:
      std::map<String, Symbol> symbols;
      std::set<String>* captures;
      const ExprContext* chain;
    public:
      explicit ExprContext(const ExprContext* parent, std::set<String>* captures = nullptr)
        : symbols(),
          captures(captures),
          chain(parent) {
      }
      // Symbol table
      const Symbol* findSymbol(const String& name) const {
        // Searches the entire chain from front to back
        // TODO optimize
        auto seenCaptures = false;
        for (auto* table = this; table != nullptr; table = table->chain) {
          seenCaptures = seenCaptures || (table->captures != nullptr);
          auto iterator = table->symbols.find(name);
          if (iterator != table->symbols.end()) {
            // Found it, so now go back and update any capture lists, if necessary
            if (seenCaptures) {
              for (auto* again = this; again != table; again = again->chain) {
                if (again->captures != nullptr) {
                  again->captures->emplace(name);
                }
              }
            }
            return &iterator->second;
          }
        }
        return nullptr;
      }
    };
    struct StmtContextData {
      struct Count {
        Type type = nullptr;
        size_t count = 0;
      };
      bool canBreak : 1 = false;
      bool canContinue : 1 = false;
      bool canRethrow : 1 = false;
      Count* canReturn = nullptr;
      Count* canYield = nullptr;
      ModuleNode* target = nullptr;
    };
    class StmtContext : public ExprContext, public StmtContextData {
      StmtContext() = delete;
      StmtContext(const StmtContext& parent) = delete;
    public:
      explicit StmtContext(const StmtContext* parent, std::set<String>* captures = nullptr)
        : ExprContext(parent, captures),
          StmtContextData() {
        if (parent != nullptr) {
          *static_cast<StmtContextData*>(this) = *parent;
        }
      }
      StmtContext(const ExprContext& parent, std::set<String>* captures, StmtContextData&& data)
        : ExprContext(&parent, captures),
          StmtContextData(std::move(data)) {
      }
      bool isModuleRoot() const {
        return this->chain == nullptr;
      }
      // Symbol table
      const Symbol* addSymbol(Symbol::Kind kind, const String& name, const Type& type, const SourceRange& range) {
        // Return a pointer to the extant symbol else null if added
        assert(!name.empty());
        assert(type != nullptr);
        // We should only add builtins to the base of the chain
        assert((kind != Symbol::Kind::Builtin) || (this->chain == nullptr));
        auto iterator = this->symbols.emplace(name, Symbol{ kind, type, range });
        return iterator.second ? nullptr : &iterator.first->second;
      }
      bool removeSymbol(const String& name) {
        // Only removes from the base of the chain
        assert(!name.empty());
        auto count = this->symbols.erase(name);
        assert(count <= 1);
        return count > 0;
      }
    };
  private:
    ModuleNode* compileStmt(ParserNode& pnode, StmtContext& context);
    ModuleNode* compileStmtInto(ParserNode& pnode, StmtContext& context, ModuleNode& parent);
    ModuleNode* compileStmtVoid(ParserNode& pnode);
    ModuleNode* compileStmtBlock(ParserNode& pnodes, StmtContext& context);
    ModuleNode* compileStmtBlockInto(ParserNodes& pnodes, StmtContext& context, ModuleNode& parent);
    ModuleNode* compileStmtDeclareVariable(ParserNode& pnode, StmtContext& context);
    ModuleNode* compileStmtDefineVariable(ParserNode& pnode, StmtContext& context);
    ModuleNode* compileStmtDefineFunction(ParserNode& pnode, StmtContext& context);
    ModuleNode* compileStmtDefineType(ParserNode& pnode, StmtContext& context);
    ModuleNode* compileStmtMutate(ParserNode& pnode, StmtContext& context);
    ModuleNode* compileStmtForEach(ParserNode& pnode, StmtContext& context);
    ModuleNode* compileStmtForLoop(ParserNode& pnode, StmtContext& context);
    ModuleNode* compileStmtIf(ParserNode& pnode, StmtContext& context);
    ModuleNode* compileStmtIfGuarded(ParserNode& pnode, StmtContext& context);
    ModuleNode* compileStmtIfUnguarded(ParserNode& pnode, StmtContext& context);
    ModuleNode* compileStmtReturn(ParserNode& pnode, StmtContext& context);
    ModuleNode* compileStmtYield(ParserNode& pnode, StmtContext& context);
    ModuleNode* compileStmtThrow(ParserNode& pnode, StmtContext& context);
    ModuleNode* compileStmtTry(ParserNode& pnode, StmtContext& context);
    ModuleNode* compileStmtCatch(ParserNode& pnode, StmtContext& context);
    ModuleNode* compileStmtWhile(ParserNode& pnode, StmtContext& context);
    ModuleNode* compileStmtWhileGuarded(ParserNode& pnode, StmtContext& context);
    ModuleNode* compileStmtWhileUnguarded(ParserNode& pnode, StmtContext& context);
    ModuleNode* compileStmtDo(ParserNode& pnode, StmtContext& context);
    ModuleNode* compileStmtSwitch(ParserNode& pnode, StmtContext& context);
    ModuleNode* compileStmtBreak(ParserNode& pnode, StmtContext& context);
    ModuleNode* compileStmtContinue(ParserNode& pnode, StmtContext& context);
    ModuleNode* compileStmtMissing(ParserNode& pnode, StmtContext& context);
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
    ModuleNode* compileValueExprReference(ParserNode& ampersand, ParserNode& pexpr, const ExprContext& context);
    ModuleNode* compileValueExprDereference(ParserNode& star, ParserNode& pexpr, const ExprContext& context);
    ModuleNode* compileValueExprArray(ParserNode& pnode, const ExprContext& context);
    ModuleNode* compileValueExprArrayHinted(ParserNode& pnode, const ExprContext& context, const Type& elementType);
    ModuleNode* compileValueExprArrayHintedElement(ParserNode& pnode, const ExprContext& context, const Type& elementType);
    ModuleNode* compileValueExprArrayUnhinted(ParserNode& pnode, const ExprContext& context);
    ModuleNode* compileValueExprArrayUnhintedElement(ParserNode& pnode, const ExprContext& context);
    ModuleNode* compileValueExprEon(ParserNode& pnode, const ExprContext& context);
    ModuleNode* compileValueExprEonElement(ParserNode& pnode, const ExprContext& context);
    ModuleNode* compileValueExprObject(ParserNode& pnode, const ExprContext& context);
    ModuleNode* compileValueExprObjectElement(ParserNode& pnode, const ExprContext& context);
    ModuleNode* compileValueExprGuard(ParserNode& pnode, ParserNode& ptype, ParserNode& pexpr, const ExprContext& context);
    ModuleNode* compileValueExprManifestation(ParserNode& pnode, const Type& type, const ExprContext& context);
    ModuleNode* compileValueExprMissing(ParserNode& pnode, const ExprContext& context);
    ModuleNode* compileTypeExpr(ParserNode& pnode, const ExprContext& context);
    ModuleNode* compileTypeExprVariable(ParserNode& pnode, const ExprContext& context);
    ModuleNode* compileTypeExprProperty(ParserNode& dot, ParserNode& lhs, ParserNode& rhs, const ExprContext& context);
    ModuleNode* compileTypeExprUnary(ParserNode& op, ParserNode& lhs, const ExprContext& context);
    ModuleNode* compileTypeExprBinary(ParserNode& op, ParserNode& lhs, ParserNode& rhs, const ExprContext& context);
    ModuleNode* compileTypeExprFunctionSignature(ParserNode& pnode, const ExprContext& context);
    ModuleNode* compileTypeSpecification(ParserNode& pnode, const ExprContext& context);
    ModuleNode* compileTypeSpecificationStaticData(ParserNode& pnode, const ExprContext& context, ModuleNode*& inode);
    ModuleNode* compileTypeSpecificationStaticFunction(ParserNode& pnode, const ExprContext& context, ModuleNode*& inode);
    ModuleNode* compileTypeSpecificationInstanceData(ParserNode& pnode, const ExprContext& context);
    ModuleNode* compileTypeSpecificationInstanceFunction(ParserNode& pnode, const ExprContext& context);
    ModuleNode* compileTypeGuard(ParserNode& pnode, const ExprContext& context, Type& type, ModuleNode*& mcond);
    ModuleNode* compileTypeInfer(ParserNode& pnode, ParserNode& ptype, ParserNode& pexpr, const ExprContext& context, Type& type, ModuleNode*& mexpr);
    ModuleNode* compileTypeInferVar(ParserNode& pnode, ParserNode& ptype, ParserNode& pexpr, const ExprContext& context, Type& type, ModuleNode*& mexpr, bool nullable);
    ModuleNode* compileAmbiguousExpr(ParserNode& pnode, const ExprContext& context, Ambiguous& ambiguous);
    ModuleNode* compileAmbiguousVariable(ParserNode& pnode, const ExprContext& context, Ambiguous& ambiguous);
    ModuleNode* compileLiteral(ParserNode& pnode);
    ModuleNode* checkValueExpr(ModuleNode& mnode, const ExprContext& context);
    bool checkValueExprOperand(const char* expected, ModuleNode& mnode, ParserNode& pnode, ValueFlags required, const ExprContext& context);
    bool checkValueExprOperand2(const char* expected, ModuleNode& lhs, ModuleNode& rhs, ParserNode& pnode, ValueFlags required, const ExprContext& context);
    bool checkValueExprUnary(ValueUnaryOp op, ModuleNode& rhs, ParserNode& pnode, const ExprContext& context);
    bool checkValueExprBinary(ValueBinaryOp op, ModuleNode& lhs, ModuleNode& rhs, ParserNode& pnode, const ExprContext& context);
    bool checkValueExprTernary(ValueTernaryOp op, ModuleNode& lhs, ModuleNode& mid, ModuleNode& rhs, ParserNode& pnode, const ExprContext& context);
    bool checkStmtVariableMutate(const String& symbol, ValueMutationOp op, ModuleNode& value, ParserNode& pnode, const StmtContext& context);
    bool checkStmtPropertyMutate(ModuleNode& instance, ModuleNode& property, ValueMutationOp op, ModuleNode& value, ParserNode& pnode, const StmtContext& context);
    bool checkStmtIndexMutate(ModuleNode& instance, ModuleNode& index, ValueMutationOp op, ModuleNode& value, ParserNode& pnode, const StmtContext& context);
    bool checkStmtPointeeMutate(ModuleNode& instance, ValueMutationOp op, ModuleNode& value, ParserNode& pnode, const StmtContext& context);
    enum class MutateCheck { Success, Unnecessary, Failure };
    MutateCheck checkTargetMutate(const Type& target, ValueMutationOp op, const Type& value, String& problem);
    Type literalType(ParserNode & pnode);
    String deduceString(ModuleNode& mnode, const ExprContext& context);
    Type deduceType(ModuleNode& mnode, const ExprContext& context, IVMTypeResolver::Kind& deduced) {
      class Resolver : public IVMTypeResolver {
        Resolver(const Resolver&) = delete;
        Resolver& operator=(const Resolver&) = delete;
      private:
        IVMModuleBuilder& mbuilder;
        const ExprContext& context;
        IVMModuleBuilder::Reporter& reporter;
      public:
        Resolver(IVMModuleBuilder& mbuilder, const ExprContext& context, IVMModuleBuilder::Reporter& reporter)
          : mbuilder(mbuilder),
            context(context),
            reporter(reporter) {
        }
        virtual Type resolveSymbol(const String& symbol, IVMTypeResolver::Kind& kind) override {
          auto* entry = this->context.findSymbol(symbol);
          if (entry != nullptr) {
            switch (entry->kind) {
            case ExprContext::Symbol::Kind::Type:
              kind = IVMTypeResolver::Kind::Type;
              break;
            case ExprContext::Symbol::Kind::Builtin:
            case ExprContext::Symbol::Kind::Parameter:
            case ExprContext::Symbol::Kind::Variable:
            case ExprContext::Symbol::Kind::Function:
            default:
              kind = IVMTypeResolver::Kind::Value;
              break;
            }
            return entry->type;
          }
          return nullptr;
        }
        virtual IVMTypeSpecification* resolveTypeSpecification(const IVMModule::Node& spec) override {
          return this->mbuilder.registerTypeSpecification(spec, *this, this->reporter);
        }
      };
      Resolver resolver{ this->mbuilder, context, *this };
      return this->mbuilder.deduceType(mnode, resolver, this, deduced);
    }
    Type deduceTypeExpr(ModuleNode& mnode, const ExprContext& context) {
      // TODO: The split between 'deduceTypeExpr' and 'deduceExprType' needs revisiting
      auto kind = IVMTypeResolver::Kind::Type;
      auto type = this->deduceType(mnode, context, kind);
      if ((type != nullptr) && (kind != IVMTypeResolver::Kind::Type)) {
        throw InternalException("Cannot deduce type expression");
      }
      return type;
    }
    Type deduceExprType(ModuleNode& mnode, const ExprContext& context) {
      auto kind = IVMTypeResolver::Kind::Value;
      auto type = this->deduceType(mnode, context, kind);
      if ((type != nullptr) && (kind != IVMTypeResolver::Kind::Value)) {
        throw InternalException("Cannot deduce type of expression");
      }
      return type;
    }
    void forgeNullability(Type& type, bool nullable) {
      assert(type != nullptr);
      type = this->vm.getTypeForge().forgeNullableType(type, nullable);
      assert(type != nullptr);
    }
    Type forgeYieldability(const Type& type) {
      if (type->getShapeCount() != 1) {
        return nullptr;
      }
      auto* shape = type->getShape(0);
      if ((shape == nullptr) || (shape->iterable == nullptr)) {
        return nullptr;
      }
      return shape->iterable->getIterationType();
    }
    Assignability isAssignable(const Type& dst, const Type& src) {
      assert(dst != nullptr);
      assert(src != nullptr);
      return this->vm.getTypeForge().isTypeAssignable(dst, src);
    }
    bool addSymbol(StmtContext& context, ParserNode& pnode, StmtContext::Symbol::Kind kind, const String& name, const Type& type) {
      auto* extant = context.addSymbol(kind, name, type, pnode.range);
      if (extant != nullptr) {
        const char* already = "";
        switch (extant->kind) {
        case StmtContext::Symbol::Kind::Builtin:
          already = " as a builtin";
          break;
        case StmtContext::Symbol::Kind::Function:
          already = " as a function";
          break;
        case StmtContext::Symbol::Kind::Parameter:
          already = " as a function parameter";
          break;
        case StmtContext::Symbol::Kind::Variable:
          already = " as a variable";
          break;
        case StmtContext::Symbol::Kind::Type:
          already = " as a type";
          break;
        }
        this->error(pnode, "Identifier '", name, "' already used", already);
        return false;
      }
      return true;
    }
    template<typename... ARGS>
    String concat(ARGS&&... args) const {
      return StringBuilder::concat(this->vm.getAllocator(), std::forward<ARGS>(args)...);
    }
    template<typename... ARGS>
    void log(ILogger::Severity severity, ARGS&&... args) const {
      auto message = this->concat(std::forward<ARGS>(args)...);
      this->vm.getLogger().log(ILogger::Source::Compiler, severity, message);
    }
    template<typename... ARGS>
    void warning(ParserNode& pnode, ARGS&&... args) const {
      this->log(ILogger::Severity::Warning, this->resource, pnode.range, ": ", std::forward<ARGS>(args)...);
    }
    template<typename... ARGS>
    ModuleNode* error(ParserNode& pnode, ARGS&&... args) const {
      this->log(ILogger::Severity::Error, this->resource, pnode.range, ": ", std::forward<ARGS>(args)...);
      return nullptr;
    }
    template<typename... ARGS>
    ModuleNode* expected(ParserNode& pnode, ARGS&&... args) const {
      return this->error(pnode, "Expected ", std::forward<ARGS>(args)..., ", but instead got ", ModuleCompiler::toString(pnode));
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
    HardPtr<IVMProgramBuilder> pbuilder;
  public:
    explicit EggCompiler(const HardPtr<IVMProgramBuilder>& pbuilder)
      : pbuilder(pbuilder) {
    }
    virtual HardPtr<IVMModule> compile(Parser& parser) override {
      // TODO warnings as errors?
      auto resource = parser.resource();
      auto parsed = parser.parse();
      this->logIssues(resource, parsed.issues);
      if (parsed.root != nullptr) {
        auto mbuilder = this->pbuilder->createModuleBuilder(resource);
        assert(mbuilder != nullptr);
        ModuleCompiler compiler(this->pbuilder->getVM(), resource, *mbuilder);
        ModuleCompiler::StmtContext context{ nullptr };
        this->pbuilder->visitBuiltins([&context](const String& symbol, const Type& type) {
          context.addSymbol(ModuleCompiler::StmtContext::Symbol::Kind::Builtin, symbol, type, {});
        });
        context.target = &mbuilder->getRoot();
        return compiler.compile(*parsed.root, context);
      }
      return nullptr;
    }
  private:
    ILogger::Severity parse(Parser& parser, Parser::Result& result) {
      result = parser.parse();
      return this->logIssues(parser.resource(), result.issues);
    }
    ILogger::Severity logIssues(const String& resource, const std::vector<Parser::Issue>& issues) {
      auto& logger = this->pbuilder->getVM().getLogger();
      auto worst = ILogger::Severity::None;
      for (const auto& issue : issues) {
        ILogger::Severity severity = ILogger::Severity::Error;
        switch (issue.severity) {
        case Parser::Issue::Severity::Information:
          severity = ILogger::Severity::Information;
          if (worst == ILogger::Severity::None) {
            worst = ILogger::Severity::Information;
          }
          break;
        case Parser::Issue::Severity::Warning:
          severity = ILogger::Severity::Warning;
          if (worst != ILogger::Severity::Error) {
            worst = ILogger::Severity::Warning;
          }
          break;
        case Parser::Issue::Severity::Error:
          severity = ILogger::Severity::Error;
          worst = ILogger::Severity::Error;
          break;
        }
        auto message = StringBuilder::concat(this->pbuilder->getAllocator(), resource, issue.range, ": ", issue.message);
        logger.log(ILogger::Source::Compiler, severity, message);
      }
      return worst;
    }
  };
}

HardPtr<IVMModule> ModuleCompiler::compile(ParserNode& root, StmtContext& context) {
  assert(context.isModuleRoot());
  assert(context.target != nullptr);
  if (root.kind != ParserNode::Kind::ModuleRoot) {
    this->expected(root, "module root node");
    return nullptr;
  }
  auto block = this->compileStmtBlockInto(root.children, context, *context.target);
  if (block == nullptr) {
    return nullptr;
  }
  assert(block == context.target);
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
  case ParserNode::Kind::StmtDefineType:
    EXPECT(pnode, pnode.children.size() == 1);
    return this->compileStmtDefineType(pnode, context);
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
    EXPECT(pnode, pnode.children.size() <= 1);
    return this->compileStmtReturn(pnode, context);
  case ParserNode::Kind::StmtYield:
    EXPECT(pnode, pnode.children.size() == 1);
    return this->compileStmtYield(pnode, context);
  case ParserNode::Kind::StmtThrow:
    EXPECT(pnode, pnode.children.size() <= 2);
    return this->compileStmtThrow(pnode, context);
  case ParserNode::Kind::StmtTry:
    EXPECT(pnode, pnode.children.size() >= 2);
    return this->compileStmtTry(pnode, context);
  case ParserNode::Kind::StmtWhile:
    EXPECT(pnode, pnode.children.size() == 2);
    return this->compileStmtWhile(pnode, context);
  case ParserNode::Kind::StmtDo:
    EXPECT(pnode, pnode.children.size() == 2);
    return this->compileStmtDo(pnode, context);
  case ParserNode::Kind::StmtSwitch:
    EXPECT(pnode, pnode.children.size() >= 1);
    return this->compileStmtSwitch(pnode, context);
  case ParserNode::Kind::StmtBreak:
    EXPECT(pnode, pnode.children.empty());
    return this->compileStmtBreak(pnode, context);
  case ParserNode::Kind::StmtContinue:
    EXPECT(pnode, pnode.children.empty());
    return this->compileStmtContinue(pnode, context);
  case ParserNode::Kind::StmtMutate:
    return this->compileStmtMutate(pnode, context);
  case ParserNode::Kind::ExprCall:
    EXPECT(pnode, !pnode.children.empty());
    return this->compileValueExprCall(pnode.children, context);
  case ParserNode::Kind::Missing:
    EXPECT(pnode, pnode.children.empty());
    return this->compileStmtMissing(pnode, context);
  case ParserNode::Kind::ModuleRoot:
  case ParserNode::Kind::StmtCase:
  case ParserNode::Kind::StmtDefault:
  case ParserNode::Kind::StmtCatch:
  case ParserNode::Kind::StmtFinally:
  case ParserNode::Kind::ExprUnary:
  case ParserNode::Kind::ExprBinary:
  case ParserNode::Kind::ExprTernary:
  case ParserNode::Kind::ExprIndex:
  case ParserNode::Kind::ExprProperty:
  case ParserNode::Kind::ExprReference:
  case ParserNode::Kind::ExprDereference:
  case ParserNode::Kind::ExprArray:
  case ParserNode::Kind::ExprEon:
  case ParserNode::Kind::ExprObject:
  case ParserNode::Kind::ExprEllipsis:
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
  case ParserNode::Kind::TypeSpecification:
  case ParserNode::Kind::TypeSpecificationStaticData:
  case ParserNode::Kind::TypeSpecificationStaticFunction:
  case ParserNode::Kind::TypeSpecificationInstanceData:
  case ParserNode::Kind::TypeSpecificationInstanceFunction:
  case ParserNode::Kind::TypeSpecificationAccess:
  case ParserNode::Kind::ObjectSpecification:
  case ParserNode::Kind::ObjectSpecificationData:
  case ParserNode::Kind::ObjectSpecificationFunction:
  case ParserNode::Kind::Literal:
  case ParserNode::Kind::Variable:
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
    return this->compileStmtBlockInto(pnode.children, context, parent);
  }
  auto* before = context.target;
  context.target = &parent;
  auto child = this->compileStmt(pnode, context);
  context.target = before;
  if (child == nullptr) {
    return nullptr;
  }
  this->mbuilder.appendChild(parent, *child);
  return &parent;
}

ModuleNode* ModuleCompiler::compileStmtVoid(ParserNode& pnode) {
  return &this->mbuilder.exprLiteral(HardValue::Void, pnode.range);
}

ModuleNode* ModuleCompiler::compileStmtBlock(ParserNode& pnode, StmtContext& context) {
  EXPECT(pnode, pnode.kind == ParserNode::Kind::StmtBlock);
  StmtContext inner{ &context };
  return this->compileStmtBlockInto(pnode.children, inner, this->mbuilder.stmtBlock(pnode.range));
}

ModuleNode* ModuleCompiler::compileStmtBlockInto(ParserNodes& pnodes, StmtContext& context, ModuleNode& parent) {
  auto* before = context.target;
  context.target = &parent;
  for (const auto& pnode : pnodes) {
    // Make sure we append to the target as it is now
    auto* target = context.target;
    assert(target != nullptr);
    auto* stmt = this->compileStmt(*pnode, context);
    if (stmt == nullptr) {
      context.target = before;
      return nullptr;
    }
    this->mbuilder.appendChild(*target, *stmt);
  }
  context.target = before;
  return &parent;
}

ModuleNode* ModuleCompiler::compileStmtDeclareVariable(ParserNode& pnode, StmtContext& context) {
  assert(pnode.kind == ParserNode::Kind::StmtDeclareVariable);
  String symbol;
  EXPECT(pnode, pnode.value->getString(symbol));
  EXPECT(pnode, pnode.children.size() == 1);
  auto& ptype = *pnode.children.front();
  auto* mtype = this->compileTypeExpr(ptype, context);
  if (mtype == nullptr) {
    return nullptr;
  }
  auto type = this->deduceTypeExpr(*mtype, context);
  if (type == nullptr) {
    return this->error(pnode, "Unable to deduce type of variable '", symbol, "' at compile time");
  }
  assert(type != nullptr);
  if (!this->addSymbol(context, pnode, StmtContext::Symbol::Kind::Variable, symbol, type)) {
    return nullptr;
  }
  auto* stmt = &this->mbuilder.stmtVariableDeclare(symbol, *mtype, pnode.range);
  context.target = stmt;
  return stmt;
}

ModuleNode* ModuleCompiler::compileStmtDefineVariable(ParserNode& pnode, StmtContext& context) {
  assert(pnode.kind == ParserNode::Kind::StmtDefineVariable);
  EXPECT(pnode, pnode.children.size() == 2);
  String symbol;
  EXPECT(pnode, pnode.value->getString(symbol));
  Type ltype;
  ModuleNode* rnode;
  auto lnode = this->compileTypeInfer(pnode, *pnode.children.front(), *pnode.children.back(), context, ltype, rnode);
  if (lnode == nullptr) {
    return nullptr;
  }
  assert(rnode != nullptr);
  auto rtype = this->deduceExprType(*rnode, context);
  assert(rtype != nullptr);
  auto assignable = this->isAssignable(ltype, rtype);
  if (assignable == Assignability::Never) {
    return this->error(*pnode.children.back(), "Cannot initialize '", symbol, "' of type '", ltype, "' with a value of type '", rtype, "'");
  }
  assert(ltype != nullptr);
  if (!this->addSymbol(context, pnode, StmtContext::Symbol::Kind::Variable, symbol, ltype)) {
    return nullptr;
  }
  auto* stmt = &this->mbuilder.stmtVariableDefine(symbol, *lnode, *rnode, pnode.range);
  context.target = stmt;
  return stmt;
}

ModuleNode* ModuleCompiler::compileStmtDefineType(ParserNode& pnode, StmtContext& context) {
  assert(pnode.kind == ParserNode::Kind::StmtDefineType);
  EXPECT(pnode, pnode.children.size() == 1);
  String symbol;
  EXPECT(pnode, pnode.value->getString(symbol));
  auto mtype = this->compileTypeExpr(*pnode.children.front(), context);
  if (mtype == nullptr) {
    return nullptr;
  }
  auto type = this->deduceTypeExpr(*mtype, context);
  if (type == nullptr) {
    // TODO this is the second error message generated for this problem; the other is issued by 'deduceTypeExpr()'
    return this->error(pnode, "Unable to deduce type '", symbol, "' at compile time");
  }
  if (!this->addSymbol(context, pnode, StmtContext::Symbol::Kind::Type, symbol, type)) {
    return nullptr;
  }
  auto* stmt = &this->mbuilder.stmtTypeDefine(symbol, *mtype, pnode.range);
  context.target = stmt;
  return stmt;
}

ModuleNode* ModuleCompiler::compileStmtDefineFunction(ParserNode& pnode, StmtContext& context) {
  assert(pnode.kind == ParserNode::Kind::StmtDefineFunction);
  EXPECT(pnode, pnode.children.size() == 2);
  String symbol;
  EXPECT(pnode, pnode.value->getString(symbol));
  auto& phead = *pnode.children.front();
  if (phead.kind != ParserNode::Kind::TypeFunctionSignature) {
    return this->expected(phead, "function signature in function definition");
  }
  auto mtype = this->compileTypeExprFunctionSignature(phead, context); // TODO add symbols directly here with better locations
  if (mtype == nullptr) {
    return nullptr;
  }
  auto type = this->deduceTypeExpr(*mtype, context);
  if (type == nullptr) {
    return this->error(pnode, "Unable to deduce type of function '", symbol, "' at compile time");
  }
  auto* signature = type.getOnlyFunctionSignature();
  if (signature == nullptr) {
    return this->error(pnode, "Unable to deduce signature of function '", symbol, "' with type '", type, "' at compile time");
  }
  auto okay = this->addSymbol(context, phead, StmtContext::Symbol::Kind::Function, symbol, type);
  std::set<String> captures;
  StmtContextData::Count canReturn{ signature->getReturnType() };
  assert(canReturn.type != nullptr);
  StmtContextData::Count canYield{ this->forgeYieldability(canReturn.type) };
  StmtContext inner{ &context, &captures };
  inner.canReturn = &canReturn;
  inner.canYield = (canYield.type == nullptr) ? nullptr : &canYield;
  size_t pcount = signature->getParameterCount();
  for (size_t pindex = 0; pindex < pcount; ++pindex) {
    auto& parameter = signature->getParameter(pindex);
    auto pname = parameter.getName();
    if (!pname.empty()) {
      okay &= this->addSymbol(inner, pnode, StmtContext::Symbol::Kind::Parameter, pname, parameter.getType());
    }
  }
  if (!okay) {
    return nullptr;
  }
  auto& ptail = *pnode.children.back();
  assert(ptail.kind == ParserNode::Kind::StmtBlock);
  auto mblock = this->compileStmtBlockInto(ptail.children, inner, this->mbuilder.stmtFunctionInvoke(pnode.range));
  if (mblock == nullptr) {
    return nullptr;
  }
  if (canYield.count > 0) {
    // Promote the function invocation to a generator
    mblock = &this->mbuilder.stmtGeneratorInvoke(*mblock, pnode.range);
  }
  auto& mvalue = this->mbuilder.exprFunctionConstruct(*mtype, *mblock, pnode.range);
  for (const auto& capture : captures) {
    this->mbuilder.appendChild(mvalue, this->mbuilder.exprFunctionCapture(capture, pnode.range));
  }
  auto* stmt = &this->mbuilder.stmtVariableDefine(symbol, *mtype, mvalue, pnode.range);
  context.target = stmt;
  return stmt;
}

ModuleNode* ModuleCompiler::compileStmtMutate(ParserNode& pnode, StmtContext& context) {
  assert(pnode.kind == ParserNode::Kind::StmtMutate);
  auto nudge = (pnode.op.valueMutationOp == ValueMutationOp::Decrement) || (pnode.op.valueMutationOp == ValueMutationOp::Increment);
  if (nudge) {
    EXPECT(pnode, pnode.children.size() == 1);
  } else {
    EXPECT(pnode, pnode.children.size() == 2);
  }
  auto& plhs = *pnode.children.front();
  if (plhs.kind == ParserNode::Kind::Variable) {
    // 'variable'
    EXPECT(plhs, plhs.children.empty());
    String symbol;
    EXPECT(plhs, plhs.value->getString(symbol));
    ModuleNode* rhs;
    if (nudge) {
      rhs = this->compileStmtVoid(pnode);
    } else {
      rhs = this->compileValueExpr(*pnode.children.back(), context);
    }
    if (!this->checkStmtVariableMutate(symbol, pnode.op.valueMutationOp, *rhs, pnode, context)) {
      return nullptr;
    }
    return &this->mbuilder.stmtVariableMutate(symbol, pnode.op.valueMutationOp, *rhs, pnode.range);
  }
  if (plhs.kind == ParserNode::Kind::ExprProperty) {
    // 'instance.property'
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
    if (!this->checkStmtPropertyMutate(*instance, *property, pnode.op.valueMutationOp, *rhs, pnode, context)) {
      return nullptr;
    }
    return &this->mbuilder.stmtPropertyMutate(*instance, *property, pnode.op.valueMutationOp, *rhs, pnode.range);
  }
  if (plhs.kind == ParserNode::Kind::ExprIndex) {
    // 'instance[index]'
    EXPECT(plhs, plhs.children.size() == 2);
    auto* instance = this->compileValueExpr(*plhs.children.front(), context);
    if (instance == nullptr) {
      return nullptr;
    }
    auto* index = this->compileValueExpr(*plhs.children.back(), context);
    if (index == nullptr) {
      return nullptr;
    }
    ModuleNode* rhs;
    if (nudge) {
      rhs = this->compileStmtVoid(pnode);
    } else {
      rhs = this->compileValueExpr(*pnode.children.back(), context);
    }
    if (!this->checkStmtIndexMutate(*instance, *index, pnode.op.valueMutationOp, *rhs, pnode, context)) {
      return nullptr;
    }
    return &this->mbuilder.stmtIndexMutate(*instance, *index, pnode.op.valueMutationOp, *rhs, pnode.range);
  }
  if (plhs.kind == ParserNode::Kind::ExprDereference) {
    // '*pointer'
    EXPECT(plhs, plhs.children.size() == 1);
    auto* instance = this->compileValueExpr(*plhs.children.front(), context);
    if (instance == nullptr) {
      return nullptr;
    }
    ModuleNode* rhs;
    if (nudge) {
      rhs = this->compileStmtVoid(pnode);
    } else {
      rhs = this->compileValueExpr(*pnode.children.back(), context);
    }
    if (!this->checkStmtPointeeMutate(*instance, pnode.op.valueMutationOp, *rhs, pnode, context)) {
      return nullptr;
    }
    return &this->mbuilder.stmtPointeeMutate(*instance, pnode.op.valueMutationOp, *rhs, pnode.range);
  }
  return this->expected(plhs, "variable in mutation statement");
}

ModuleNode* ModuleCompiler::compileStmtForEach(ParserNode& pnode, StmtContext& context) {
  assert(pnode.kind == ParserNode::Kind::StmtForEach);
  EXPECT(pnode, pnode.children.size() == 3);
  String symbol;
  EXPECT(pnode, pnode.value->getString(symbol));
  Type type;
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
  if (!this->addSymbol(inner, pnode, StmtContext::Symbol::Kind::Variable, symbol, type)) {
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
  EXPECT(pnode, pnode.children.size() == 4);
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
  EXPECT(pnode, (pnode.children.size() == 2) || (pnode.children.size() == 3));
  if (pnode.children.front()->kind == ParserNode::Kind::ExprGuard) {
    return this->compileStmtIfGuarded(pnode, context);
  }
  return this->compileStmtIfUnguarded(pnode, context);
}

ModuleNode* ModuleCompiler::compileStmtIfGuarded(ParserNode& pnode, StmtContext& context) {
  assert(pnode.kind == ParserNode::Kind::StmtIf);
  EXPECT(pnode, (pnode.children.size() == 2) || (pnode.children.size() == 3));
  auto& pguard = *pnode.children.front();
  assert(pguard.kind == ParserNode::Kind::ExprGuard);
  assert(pguard.children.size() == 2);
  String symbol;
  EXPECT(pguard, pguard.value->getString(symbol));
  Type type;
  ModuleNode* mcond;
  auto* mtype = this->compileTypeGuard(pguard, context, type, mcond);
  if (mtype == nullptr) {
    return nullptr;
  }
  assert(mcond != nullptr);
  StmtContext inner{ &context };
  if (!this->addSymbol(inner, pguard, StmtContext::Symbol::Kind::Variable, symbol, type)) {
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
  EXPECT(pnode, (pnode.children.size() == 2) || (pnode.children.size() == 3));
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
  EXPECT(pnode, pnode.children.size() <= 1);
  if (context.canReturn == nullptr) {
    return this->error(pnode, "'return' statements are only valid within function definitions");
  }
  if ((context.canYield != nullptr) && (context.canYield->count > 0)) {
    return this->error(pnode, "Cannot mix 'return' and 'yield statements within generator definitions");
  }
  Type expected{ context.canReturn->type };
  auto* stmt = &this->mbuilder.stmtReturn(pnode.range);
  if (pnode.children.empty()) {
    // return ;
    if (expected != Type::Void) {
      return this->error(pnode, "Expected 'return' statement with a value of type '", *expected, "'");
    }
  } else {
    // return <expr> ;
    auto& pchild = *pnode.children.back();
    if (expected == Type::Void) {
      return this->error(pchild, "Expected 'return' statement with no value");
    }
    auto* expr = this->compileValueExpr(pchild, context);
    if (expr == nullptr) {
      return nullptr;
    }
    auto type = this->deduceExprType(*expr, context);
    assert(type != nullptr);
    auto assignable = this->isAssignable(expected, type);
    if (assignable == Assignability::Never) {
      return this->error(pchild, "Expected 'return' statement with a value of type '", *expected, "', but instead got a value of type '", *type, "'");
    }
    this->mbuilder.appendChild(*stmt, *expr);
  }
  context.canReturn->count++;
  return stmt;
}

ModuleNode* ModuleCompiler::compileStmtYield(ParserNode& pnode, StmtContext& context) {
  assert(pnode.kind == ParserNode::Kind::StmtYield);
  EXPECT(pnode, pnode.children.size() == 1);
  if (context.canYield == nullptr) {
    return this->error(pnode, "'yield' statements are only valid within generator definitions");
  }
  if ((context.canReturn != nullptr) && (context.canReturn->count > 0)) {
    return this->error(pnode, "Cannot mix 'return' and 'yield statements within function definitions");
  }
  context.canYield->count++;
  auto& pchild = *pnode.children.front();
  if (pchild.kind == ParserNode::Kind::StmtBreak) {
    // yield break ;
    return &this->mbuilder.stmtYieldBreak(pchild.range);
  }
  if (pchild.kind == ParserNode::Kind::StmtContinue) {
    // yield continue ;
    return &this->mbuilder.stmtYieldContinue(pchild.range);
  }
  if (pchild.kind == ParserNode::Kind::ExprEllipsis) {
    // yield ... <expr> ;
    assert(pchild.children.size() == 1);
    auto& pgrandchild = *pchild.children.front();
    ExprContext inner{ &context };
    inner.arrayElementType = context.canYield->type;
    auto* expr = this->compileValueExpr(pgrandchild, inner);
    if (expr == nullptr) {
      return nullptr;
    }
    auto& forge = this->vm.getTypeForge();
    auto xtype = this->deduceExprType(*expr, context);
    assert(xtype != nullptr);
    auto itype = forge.forgeIterationType(xtype);
    if (itype == nullptr) {
      return this->error(pgrandchild, "Value of type '", *xtype, "' is not iterable in 'yield ...' statement");
    }
    auto assignable = this->isAssignable(context.canYield->type, itype);
    if (assignable == Assignability::Never) {
      return this->error(pchild, "Expected 'yield ...' statement with values of type '", *context.canYield->type, "', but instead got values of type '", *itype, "'");
    }
    return &this->mbuilder.stmtYieldAll(*expr, pnode.range);
  }
  // yield <expr> ;
  auto* expr = this->compileValueExpr(pchild, context);
  if (expr == nullptr) {
    return nullptr;
  }
  auto type = this->deduceExprType(*expr, context);
  assert(type != nullptr);
  auto assignable = this->isAssignable(context.canYield->type, type);
  if (assignable == Assignability::Never) {
    return this->error(pchild, "Expected 'yield' statement with a value of type '", *context.canYield->type, "', but instead got a value of type '", *type, "'");
  }
  return &this->mbuilder.stmtYield(*expr, pnode.range);
}

ModuleNode* ModuleCompiler::compileStmtThrow(ParserNode& pnode, StmtContext& context) {
  assert(pnode.kind == ParserNode::Kind::StmtThrow);
  EXPECT(pnode, pnode.children.size() <= 1);
  if (pnode.children.empty()) {
    // throw ;
    if (!context.canRethrow) {
      return this->error(pnode, "Rethrow 'throw' statements are only valid within 'catch' clauses");
    }
    return &this->mbuilder.stmtRethrow(pnode.range);
  }
  // throw <expr> ;
  auto* expr = this->compileValueExpr(*pnode.children.back(), context);
  if (expr == nullptr) {
    return nullptr;
  }
  return &this->mbuilder.stmtThrow(*expr, pnode.range);
}

ModuleNode* ModuleCompiler::compileStmtTry(ParserNode& pnode, StmtContext& context) {
  assert(pnode.kind == ParserNode::Kind::StmtTry);
  EXPECT(pnode, pnode.children.size() >= 2);
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
      child = this->compileStmtBlockInto(pchild->children, inner, this->mbuilder.stmtBlock(pchild->range));
    } else {
      return this->expected(pnode, "'catch' or 'finally' clause in 'try' statement");
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
  EXPECT(pnode, pnode.children.size() >= 1);
  assert(context.canRethrow);
  String symbol;
  EXPECT(pnode, pnode.value->getString(symbol));
  ModuleNode* stmt = nullptr;
  size_t index = 0;
  for (const auto& pchild : pnode.children) {
    if (index == 0) {
      // The catch type
      auto mtype = this->compileTypeExpr(*pchild, context);
      if (mtype != nullptr) {
        auto type = this->deduceTypeExpr(*mtype, context);
        if (type == nullptr) {
          return this->error(pnode, "Unable to deduce type of '", symbol, "' at compile time");
        }
        if (this->addSymbol(context, *pchild, StmtContext::Symbol::Kind::Variable, symbol, type)) {
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
  EXPECT(pnode, pnode.children.size() == 2);
  if (pnode.children.front()->kind == ParserNode::Kind::ExprGuard) {
    return this->compileStmtWhileGuarded(pnode, context);
  }
  return this->compileStmtWhileUnguarded(pnode, context);
}

ModuleNode* ModuleCompiler::compileStmtWhileGuarded(ParserNode& pnode, StmtContext& context) {
  assert(pnode.kind == ParserNode::Kind::StmtWhile);
  EXPECT(pnode, pnode.children.size() == 2);
  auto& pguard = *pnode.children.front();
  assert(pguard.kind == ParserNode::Kind::ExprGuard);
  assert(pguard.children.size() == 2);
  String symbol;
  EXPECT(pguard, pguard.value->getString(symbol));
  Type type;
  ModuleNode* mcond;
  auto* mtype = this->compileTypeGuard(pguard, context, type, mcond);
  if (mtype == nullptr) {
    return nullptr;
  }
  assert(mcond != nullptr);
  StmtContext inner{ &context };
  if (!this->addSymbol(inner, pguard, StmtContext::Symbol::Kind::Variable, symbol, type)) {
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
  EXPECT(pnode, pnode.children.size() == 2);
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

ModuleNode* ModuleCompiler::compileStmtDo(ParserNode& pnode, StmtContext& context) {
  assert(pnode.kind == ParserNode::Kind::StmtDo);
  EXPECT(pnode, pnode.children.size() == 2);
  auto* block = this->compileStmt(*pnode.children.front(), context);
  if (block == nullptr) {
    return nullptr;
  }
  auto* condition = this->compileValueExpr(*pnode.children.back(), context);
  if (condition == nullptr) {
    return nullptr;
  }
  auto* stmt = &this->mbuilder.stmtDo(*block, *condition, pnode.range);
  return stmt;
}

ModuleNode* ModuleCompiler::compileStmtSwitch(ParserNode& pnode, StmtContext& context) {
  assert(pnode.kind == ParserNode::Kind::StmtSwitch);
  EXPECT(pnode, pnode.children.size() == 2);
  auto* expr = this->compileValueExpr(*pnode.children.front(), context);
  if (expr == nullptr) {
    return nullptr;
  }
  struct Clause {
    std::vector<ParserNode*> statements;
    std::vector<ParserNode*> values;
  };
  std::vector<Clause> pclauses;
  enum { Start, Labels, Statements } state = Start;
  StmtContext inner{ &context };
  inner.canBreak = true;
  inner.canContinue = true;
  inner.target = nullptr;
  size_t defaultIndex = 0;
  const auto& pchildren = pnode.children.back()->children;
  auto count = pchildren.size();
  for (size_t index = 0; index < count; ++index) {
    assert(pchildren.at(index) != nullptr);
    auto& pchild = *pchildren[index];
    if (pchild.kind == ParserNode::Kind::StmtCase) {
      // case <value> :
      EXPECT(pchild, pchild.children.size() == 1);
      if ((index + 1) >= count) {
        return this->error(*pchildren[index], "Expected at least one statement within final 'case' clause of 'switch' statement block");
      }
      if (state != Labels) {
        pclauses.push_back({});
        state = Labels;
      }
      pclauses.back().values.push_back(pchild.children.front().get());
    } else if (pchild.kind == ParserNode::Kind::StmtDefault) {
      // default :
      EXPECT(pchild, pchild.children.empty());
      if (defaultIndex > 0) {
        return this->error(*pchildren[index], "Unexpected second 'default' clause in 'switch' statement");
      }
      if ((index + 1) >= count) {
        return this->error(*pchildren[index], "Expected at least one statement within final 'default' clause of 'switch' statement");
      }
      if (state != Labels) {
        pclauses.push_back({});
        state = Labels;
      }
      defaultIndex = pclauses.size();
    } else {
      // Any other statement
      if (state == Start) {
        return this->error(pchild, "Expected 'case' or 'default' clause to start 'switch' statement block, but instead got ", ModuleCompiler::toString(pchild));
      }
      if (state != Statements) {
        assert(pclauses.back().statements.empty());
        state = Statements;
      }
      pclauses.back().statements.push_back(&pchild);
    }
  }
  if (pclauses.empty()) {
    return this->error(pnode, "Expected at least one 'case' or 'default' clause within 'switch' statement");
  }
  auto& mswitch = this->mbuilder.stmtSwitch(*expr, defaultIndex, pnode.range);
  for (auto& pclause : pclauses) {
    auto& range = pclause.statements.front()->range;
    auto& mblock = this->mbuilder.stmtBlock(range);
    for (auto& pstmt : pclause.statements) {
      auto* mstmt = this->compileStmt(*pstmt, inner);
      if (mstmt == nullptr) {
        return nullptr;
      }
      this->mbuilder.appendChild(mblock, *mstmt);
    }
    auto& mcase = this->mbuilder.stmtCase(mblock, range);
    for (auto& pvalue : pclause.values) {
      auto* mvalue = this->compileValueExpr(*pvalue, inner);
      if (mvalue == nullptr) {
        return nullptr;
      }
      this->mbuilder.appendChild(mcase, *mvalue);
    }
    this->mbuilder.appendChild(mswitch, mcase);
  }
  return &mswitch;
}

ModuleNode* ModuleCompiler::compileStmtBreak(ParserNode& pnode, StmtContext& context) {
  assert(pnode.kind == ParserNode::Kind::StmtBreak);
  EXPECT(pnode, pnode.children.empty());
  if (!context.canBreak) {
    return this->error(pnode, "'break' statements are only valid within loops");
  }
  auto* stmt = &this->mbuilder.stmtBreak(pnode.range);
  return stmt;
}

ModuleNode* ModuleCompiler::compileStmtContinue(ParserNode& pnode, StmtContext& context) {
  assert(pnode.kind == ParserNode::Kind::StmtContinue);
  EXPECT(pnode, pnode.children.empty());
  if (!context.canContinue) {
    return this->error(pnode, "'continue' statements are only valid within loops");
  }
  auto* stmt = &this->mbuilder.stmtContinue(pnode.range);
  return stmt;
}

ModuleNode* ModuleCompiler::compileStmtMissing(ParserNode& pnode, StmtContext&) {
  // This is a missing statement; replace with an empty block
  assert(pnode.kind == ParserNode::Kind::Missing);
  EXPECT(pnode, pnode.children.empty());
  auto* stmt = &this->mbuilder.stmtBlock(pnode.range);
  return stmt;
}

ModuleNode* ModuleCompiler::compileValueExpr(ParserNode& pnode, const ExprContext& context) {
  switch (pnode.kind) {
  case ParserNode::Kind::Variable:
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
  case ParserNode::Kind::ExprReference:
    EXPECT(pnode, pnode.children.size() == 1);
    EXPECT(pnode, pnode.children[0] != nullptr);
    return this->compileValueExprReference(pnode, *pnode.children.front(), context);
  case ParserNode::Kind::ExprDereference:
    EXPECT(pnode, pnode.children.size() == 1);
    EXPECT(pnode, pnode.children[0] != nullptr);
    return this->compileValueExprDereference(pnode, *pnode.children.front(), context);
  case ParserNode::Kind::ExprArray:
    return this->compileValueExprArray(pnode, context);
  case ParserNode::Kind::ExprEon:
    return this->compileValueExprEon(pnode, context);
  case ParserNode::Kind::ExprObject:
    return this->compileValueExprObject(pnode, context);
  case ParserNode::Kind::ExprGuard:
    EXPECT(pnode, pnode.children.size() == 2);
    EXPECT(pnode, pnode.children[0] != nullptr);
    EXPECT(pnode, pnode.children[1] != nullptr);
    return this->compileValueExprGuard(pnode, *pnode.children[0], *pnode.children[1], context);
  case ParserNode::Kind::Literal:
    EXPECT(pnode, pnode.children.empty());
    return this->compileLiteral(pnode);
  case ParserNode::Kind::Missing:
    EXPECT(pnode, pnode.children.empty());
    return this->compileValueExprMissing(pnode, context);
  case ParserNode::Kind::TypeVoid:
    EXPECT(pnode, pnode.children.empty());
    return this->compileValueExprManifestation(pnode, Type::Void, context);
  case ParserNode::Kind::TypeBool:
    EXPECT(pnode, pnode.children.empty());
    return this->compileValueExprManifestation(pnode, Type::Bool, context);
  case ParserNode::Kind::TypeInt:
    EXPECT(pnode, pnode.children.empty());
    return this->compileValueExprManifestation(pnode, Type::Int, context);
  case ParserNode::Kind::TypeFloat:
    EXPECT(pnode, pnode.children.empty());
    return this->compileValueExprManifestation(pnode, Type::Float, context);
  case ParserNode::Kind::TypeString:
    EXPECT(pnode, pnode.children.empty());
    return this->compileValueExprManifestation(pnode, Type::String, context);
  case ParserNode::Kind::TypeObject:
    EXPECT(pnode, pnode.children.empty());
    return this->compileValueExprManifestation(pnode, Type::Object, context);
  case ParserNode::Kind::TypeAny:
    EXPECT(pnode, pnode.children.empty());
    return this->compileValueExprManifestation(pnode, Type::Any, context);
  case ParserNode::Kind::TypeType:
    EXPECT(pnode, pnode.children.empty());
    return this->compileValueExprManifestation(pnode, Type::Type_, context);
  case ParserNode::Kind::ModuleRoot:
  case ParserNode::Kind::ExprEllipsis:
  case ParserNode::Kind::TypeInfer:
  case ParserNode::Kind::TypeInferQ:
  case ParserNode::Kind::TypeUnary:
  case ParserNode::Kind::TypeBinary:
  case ParserNode::Kind::TypeFunctionSignature:
  case ParserNode::Kind::TypeFunctionSignatureParameter:
  case ParserNode::Kind::TypeSpecification:
  case ParserNode::Kind::TypeSpecificationStaticData:
  case ParserNode::Kind::TypeSpecificationStaticFunction:
  case ParserNode::Kind::TypeSpecificationInstanceData:
  case ParserNode::Kind::TypeSpecificationInstanceFunction:
  case ParserNode::Kind::TypeSpecificationAccess:
  case ParserNode::Kind::ObjectSpecification:
  case ParserNode::Kind::ObjectSpecificationData:
  case ParserNode::Kind::ObjectSpecificationFunction:
  case ParserNode::Kind::StmtBlock:
  case ParserNode::Kind::StmtDeclareVariable:
  case ParserNode::Kind::StmtDefineVariable:
  case ParserNode::Kind::StmtDefineFunction:
  case ParserNode::Kind::StmtDefineType:
  case ParserNode::Kind::StmtForEach:
  case ParserNode::Kind::StmtForLoop:
  case ParserNode::Kind::StmtIf:
  case ParserNode::Kind::StmtReturn:
  case ParserNode::Kind::StmtYield:
  case ParserNode::Kind::StmtThrow:
  case ParserNode::Kind::StmtTry:
  case ParserNode::Kind::StmtCatch:
  case ParserNode::Kind::StmtFinally:
  case ParserNode::Kind::StmtWhile:
  case ParserNode::Kind::StmtDo:
  case ParserNode::Kind::StmtSwitch:
  case ParserNode::Kind::StmtCase:
  case ParserNode::Kind::StmtDefault:
  case ParserNode::Kind::StmtBreak:
  case ParserNode::Kind::StmtContinue:
  case ParserNode::Kind::StmtMutate:
  case ParserNode::Kind::Named:
  default:
    break;
  }
  return this->expected(pnode, "value expression");
}

ModuleNode* ModuleCompiler::compileValueExprVariable(ParserNode& pnode, const ExprContext& context) {
  EXPECT(pnode, pnode.children.empty());
  String symbol;
  EXPECT(pnode, pnode.value->getString(symbol));
  auto extant = context.findSymbol(symbol);
  if (extant == nullptr) {
    return this->error(pnode, "Unknown identifier: '", symbol, "'");
  }
  if (extant->kind == ExprContext::Symbol::Kind::Type) {
    return &this->mbuilder.typeVariableGet(symbol, pnode.range);
  }
  return &this->mbuilder.exprVariableGet(symbol, pnode.range);
}

ModuleNode* ModuleCompiler::compileValueExprUnary(ParserNode& op, ParserNode& rhs, const ExprContext& context) {
  auto* expr = this->compileValueExpr(rhs, context);
  if (expr != nullptr) {
    if (!this->checkValueExprUnary(op.op.valueUnaryOp, *expr, op, context)) {
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
      if (!this->checkValueExprBinary(op.op.valueBinaryOp, *lexpr, *rexpr, op, context)) {
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
        if (!this->checkValueExprTernary(op.op.valueTernaryOp, *lexpr, *mexpr, *rexpr, lhs, context)) {
          return nullptr;
        }
        return &this->mbuilder.exprValueTernaryOp(op.op.valueTernaryOp, *lexpr, *mexpr, *rexpr, op.range);
      }
    }
  }
  return nullptr;
}

ModuleNode* ModuleCompiler::compileValueExprPredicate(ParserNode& pnode, const ExprContext& context) {
  auto op = ValuePredicateOp::None;
  ModuleNode* first;
  ModuleNode* second;
  if ((pnode.kind == ParserNode::Kind::ExprUnary) && (pnode.children.size() == 1)) {
    switch (pnode.op.valueUnaryOp) {
    case ValueUnaryOp::LogicalNot:
      op = ValuePredicateOp::LogicalNot;
      break;
    case ValueUnaryOp::Negate:
    case ValueUnaryOp::BitwiseNot:
      break;
    }
    if (op == ValuePredicateOp::None) {
      first = this->compileValueExpr(pnode, context);
    } else {
      first = this->compileValueExpr(*pnode.children.front(), context);
    }
    second = nullptr;
  } else if ((pnode.kind == ParserNode::Kind::ExprBinary) && (pnode.children.size() == 2)) {
    switch (pnode.op.valueBinaryOp) {
    case ValueBinaryOp::LessThan:
      op = ValuePredicateOp::LessThan;
      break;
    case ValueBinaryOp::LessThanOrEqual:
      op = ValuePredicateOp::LessThanOrEqual;
      break;
    case ValueBinaryOp::Equal:
      op = ValuePredicateOp::Equal;
      break;
    case ValueBinaryOp::NotEqual:
      op = ValuePredicateOp::NotEqual;
      break;
    case ValueBinaryOp::GreaterThanOrEqual:
      op = ValuePredicateOp::GreaterThanOrEqual;
      break;
    case ValueBinaryOp::GreaterThan:
      op = ValuePredicateOp::GreaterThan;
      break;
    case ValueBinaryOp::Add:
    case ValueBinaryOp::Subtract:
    case ValueBinaryOp::Multiply:
    case ValueBinaryOp::Divide:
    case ValueBinaryOp::Remainder:
    case ValueBinaryOp::BitwiseAnd:
    case ValueBinaryOp::BitwiseOr:
    case ValueBinaryOp::BitwiseXor:
    case ValueBinaryOp::ShiftLeft:
    case ValueBinaryOp::ShiftRight:
    case ValueBinaryOp::ShiftRightUnsigned:
    case ValueBinaryOp::Minimum:
    case ValueBinaryOp::Maximum:
    case ValueBinaryOp::IfVoid:
    case ValueBinaryOp::IfNull:
    case ValueBinaryOp::IfFalse:
    case ValueBinaryOp::IfTrue:
    default:
      break;
    }
    if (op == ValuePredicateOp::None) {
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
    String symbol;
    if ((*pnode)->value->getString(symbol) && symbol.equals("assert")) {
      auto& predicate = *pnodes.back();
      return this->compileValueExprCallAssert(**pnode, predicate, context);
    }
  }
  ModuleNode* call = nullptr;
  auto type = this->literalType(**pnode);
  if (type != nullptr) {
    call = this->compileValueExprManifestation(**pnode, type, context);
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
  Ambiguous ambiguous;
  auto* lexpr = this->compileAmbiguousExpr(lhs, context, ambiguous);
  if (lexpr == nullptr) {
    return nullptr;
  }
  auto* rexpr = this->compileValueExpr(rhs, context);
  if (rexpr == nullptr) {
    return nullptr;
  }
  ModuleNode* mnode;
  if (ambiguous == Ambiguous::Type) {
    lexpr = &this->mbuilder.typeManifestation(*lexpr, lhs.range);
    mnode = &this->mbuilder.exprPropertyGet(*lexpr, *rexpr, dot.range);
    auto type = this->deduceTypeExpr(*mnode, context);
    if (type == nullptr) {
      return nullptr;
    }
  } else {
    mnode = &this->mbuilder.exprPropertyGet(*lexpr, *rexpr, dot.range);
    auto type = this->deduceExprType(*mnode, context);
    if (type == nullptr) {
      return nullptr;
    }
  }
  return mnode;
}

ModuleNode* ModuleCompiler::compileValueExprReference(ParserNode& ampersand, ParserNode& pexpr, const ExprContext& context) {
  if (pexpr.kind == ParserNode::Kind::Variable) {
    // '&variable'
    EXPECT(pexpr, pexpr.children.empty());
    String symbol;
    EXPECT(pexpr, pexpr.value->getString(symbol));
    return this->checkValueExpr(this->mbuilder.exprVariableRef(symbol, ampersand.range), context);
  }
  if (pexpr.kind == ParserNode::Kind::ExprIndex) {
    // '&instance[index]'
    EXPECT(pexpr, pexpr.children.size() == 2);
    auto* instance = this->compileValueExpr(*pexpr.children.front(), context);
    if (instance == nullptr) {
      return nullptr;
    }
    auto* index = this->compileValueExpr(*pexpr.children.back(), context);
    if (index == nullptr) {
      return nullptr;
    }
    return this->checkValueExpr(this->mbuilder.exprIndexRef(*instance, *index, ampersand.range), context);
  }
  if (pexpr.kind == ParserNode::Kind::ExprProperty) {
    // '&instance.property'
    EXPECT(pexpr, pexpr.children.size() == 2);
    auto* instance = this->compileValueExpr(*pexpr.children.front(), context);
    if (instance == nullptr) {
      return nullptr;
    }
    auto* property = this->compileValueExpr(*pexpr.children.back(), context);
    if (property == nullptr) {
      return nullptr;
    }
    return this->checkValueExpr(this->mbuilder.exprPropertyRef(*instance, *property, ampersand.range), context);
  }
  return this->expected(pexpr, "addressable expression");
}

ModuleNode* ModuleCompiler::compileValueExprDereference(ParserNode& star, ParserNode& pexpr, const ExprContext& context) {
  // '*expression'
  auto* mexpr = this->compileValueExpr(pexpr, context);
  if (mexpr != nullptr) {
    return this->checkValueExpr(this->mbuilder.exprPointeeGet(*mexpr, star.range), context);
  }
  return nullptr;
}

ModuleNode* ModuleCompiler::compileValueExprArray(ParserNode& pnode, const ExprContext& context) {
  if (context.arrayElementType != nullptr) {
    // There's a hint as to what each element should be
    ExprContext inner{ &context };
    assert(inner.arrayElementType == nullptr);
    return this->compileValueExprArrayHinted(pnode, inner, context.arrayElementType);
  }
  return this->compileValueExprArrayUnhinted(pnode, context);
}

ModuleNode* ModuleCompiler::compileValueExprArrayHinted(ParserNode& pnode, const ExprContext& context, const Type& elementType) {
  assert(elementType != nullptr);
  auto* marray = &this->mbuilder.exprArrayConstruct(elementType, pnode.range);
  for (auto& pchild : pnode.children) {
    auto* mchild = this->compileValueExprArrayHintedElement(*pchild, context, elementType);
    if (mchild == nullptr) {
      marray = nullptr;
    } else if (marray != nullptr) {
      this->mbuilder.appendChild(*marray, *mchild);
    }
  }
  return marray;
}

ModuleNode* ModuleCompiler::compileValueExprArrayHintedElement(ParserNode& pnode, const ExprContext& context, const Type&) {
  // TODO: handle ellipsis '...'
  // TODO: check assignability with elementType
  return this->compileValueExpr(pnode, context);
}

ModuleNode* ModuleCompiler::compileValueExprArrayUnhinted(ParserNode& pnode, const ExprContext& context) {
  auto& forge = this->vm.getTypeForge();
  auto unionType = Type::None;
  std::vector<ModuleNode*> mchildren;
  mchildren.reserve(pnode.children.size());
  for (auto& pchild : pnode.children) {
    auto* mchild = this->compileValueExprArrayUnhintedElement(*pchild, context);
    if (mchild == nullptr) {
      unionType = nullptr;
    } else if (unionType != nullptr) {
      auto elementType = this->deduceExprType(*mchild, context);
      assert(elementType != nullptr);
      unionType = forge.forgeUnionType(unionType, elementType);
      assert(unionType != nullptr);
      mchildren.push_back(mchild);
    }
  }
  if (unionType == nullptr) {
    return nullptr;
  }
  if (unionType == Type::None) {
    unionType = Type::AnyQ;
  }
  auto* marray = &this->mbuilder.exprArrayConstruct(unionType, pnode.range);
  for (auto& mchild : mchildren) {
    this->mbuilder.appendChild(*marray, *mchild);
  }
  return marray;
}

ModuleNode* ModuleCompiler::compileValueExprArrayUnhintedElement(ParserNode& pnode, const ExprContext& context) {
  // TODO: handle ellipsis '...'
  return this->compileValueExpr(pnode, context);
}

ModuleNode* ModuleCompiler::compileValueExprEon(ParserNode& pnode, const ExprContext& context) {
  auto* object = &this->mbuilder.exprEonConstruct(pnode.range);
  for (auto& child : pnode.children) {
    auto* element = this->compileValueExprEonElement(*child, context);
    if (element == nullptr) {
      object = nullptr;
    } else if (object != nullptr) {
      this->mbuilder.appendChild(*object, *element);
    }
  }
  return object;
}

ModuleNode* ModuleCompiler::compileValueExprEonElement(ParserNode& pnode, const ExprContext& context) {
  // TODO: handle ellipsis '...'
  if (pnode.kind == ParserNode::Kind::Named) {
    EXPECT(pnode, pnode.children.size() == 1);
    auto* value = this->compileValueExpr(*pnode.children.front(), context);
    if (value == nullptr) {
      return nullptr;
    }
    return &this->mbuilder.exprNamed(pnode.value, *value, pnode.range);
  }
  return this->expected(pnode, "EON expression element");
}

ModuleNode* ModuleCompiler::compileValueExprObject(ParserNode& pnode, const ExprContext& context) {
  EXPECT(pnode, pnode.children.size() > 0);
  auto child = pnode.children.begin();
  auto* type = this->compileTypeExpr(**child, context);
  if (type == nullptr) {
    return nullptr;
  }
  auto* object = &this->mbuilder.exprObjectConstruct(*type, pnode.range);
  while (++child != pnode.children.end()) {
    auto* element = this->compileValueExprObjectElement(**child, context);
    if (element == nullptr) {
      object = nullptr;
    } else if (object != nullptr) {
      this->mbuilder.appendChild(*object, *element);
    }
  }
  return object;
}

ModuleNode* ModuleCompiler::compileValueExprObjectElement(ParserNode& pnode, const ExprContext& context) {
  if (pnode.kind == ParserNode::Kind::ObjectSpecificationData) {
    EXPECT(pnode, pnode.children.size() == 2);
    String symbol;
    EXPECT(pnode, pnode.value->getString(symbol));
    auto mtype = this->compileTypeExpr(*pnode.children.front(), context);
    if (mtype == nullptr) {
      return nullptr;
    }
    auto mvalue = this->compileValueExpr(*pnode.children.back(), context);
    if (mvalue == nullptr) {
      return nullptr;
    }
    return &this->mbuilder.exprObjectConstructProperty(symbol, *mtype, *mvalue, Accessability::All, pnode.range);
  }
  if (pnode.kind == ParserNode::Kind::ObjectSpecificationFunction) {
    EXPECT(pnode, pnode.children.size() == 2);
    String symbol;
    EXPECT(pnode, pnode.value->getString(symbol));
    auto& phead = *pnode.children.front();
    if (phead.kind != ParserNode::Kind::TypeFunctionSignature) {
      return this->expected(phead, "function signature in static function definition");
    }
    auto mtype = this->compileTypeExprFunctionSignature(phead, context); // TODO add symbols directly here with better locations
    if (mtype == nullptr) {
      return nullptr;
    }
    auto type = this->deduceTypeExpr(*mtype, context);
    if (type == nullptr) {
      // TODO double-reported?
      return this->error(pnode, "Unable to deduce type of object function '", symbol, "' at compile time");
    }
    auto* signature = type.getOnlyFunctionSignature();
    assert(signature != nullptr);
    std::set<String> captures;
    StmtContextData::Count canReturn{};
    canReturn.type = signature->getReturnType();
    StmtContextData data{};
    data.canReturn = &canReturn;
    StmtContext inner{ context, &captures, std::move(data) };
    assert(inner.canReturn != nullptr);
    auto okay = this->addSymbol(inner, phead, StmtContext::Symbol::Kind::Function, symbol, type);
    size_t pcount = signature->getParameterCount();
    for (size_t pindex = 0; pindex < pcount; ++pindex) {
      auto& parameter = signature->getParameter(pindex);
      auto pname = parameter.getName();
      if (!pname.empty()) {
        okay &= this->addSymbol(inner, pnode, StmtContext::Symbol::Kind::Parameter, pname, parameter.getType());
      }
    }
    if (!okay) {
      return nullptr;
    }
    auto& ptail = *pnode.children.back();
    assert(ptail.kind == ParserNode::Kind::StmtBlock);
    auto& invoke = this->mbuilder.stmtFunctionInvoke(pnode.range);
    auto block = this->compileStmtBlockInto(ptail.children, inner, invoke);
    if (block == nullptr) {
      return nullptr;
    }
    assert(block == &invoke);
    auto& mvalue = this->mbuilder.exprFunctionConstruct(*mtype, invoke, pnode.range);
    for (const auto& capture : captures) {
      this->mbuilder.appendChild(mvalue, this->mbuilder.exprFunctionCapture(capture, pnode.range));
    }
    return &this->mbuilder.exprObjectConstructProperty(symbol, *mtype, mvalue, Accessability::Get, pnode.range);
  }
  return this->expected(pnode, "object expression element");
}

ModuleNode* ModuleCompiler::compileValueExprGuard(ParserNode& pnode, ParserNode&, ParserNode& pexpr, const ExprContext& context) {
  // The variable has been declared with the appropriate type already
  String symbol;
  EXPECT(pnode, pnode.value->getString(symbol));
  auto* mexpr = this->compileValueExpr(pexpr, context);
  if (mexpr == nullptr) {
    return nullptr;
  }
  return &this->mbuilder.exprGuard(symbol, *mexpr, pnode.range);
}

ModuleNode* ModuleCompiler::compileValueExprManifestation(ParserNode& pnode, const Type& type, const ExprContext&) {
  return &this->mbuilder.typeManifestation(this->mbuilder.typeLiteral(type, pnode.range), pnode.range);
}

ModuleNode* ModuleCompiler::compileValueExprMissing(ParserNode& pnode, const ExprContext&) {
  // This is a missing condition (e.g. 'for(;;){}'); replace with 'true'
  assert(pnode.kind == ParserNode::Kind::Missing);
  EXPECT(pnode, pnode.children.empty());
  return &this->mbuilder.exprLiteral(HardValue::True, pnode.range);
}

ModuleNode* ModuleCompiler::compileTypeExpr(ParserNode& pnode, const ExprContext& context) {
  switch (pnode.kind) {
  case ParserNode::Kind::Variable:
    return this->compileTypeExprVariable(pnode, context);
  case ParserNode::Kind::ExprProperty:
    EXPECT(pnode, pnode.children.size() == 2);
    EXPECT(pnode, pnode.children[0] != nullptr);
    EXPECT(pnode, pnode.children[1] != nullptr);
    return this->compileTypeExprProperty(pnode, *pnode.children.front(), *pnode.children.back(), context);
  case ParserNode::Kind::TypeVoid:
    return &this->mbuilder.typeLiteral(Type::Void, pnode.range);
  case ParserNode::Kind::TypeBool:
    return &this->mbuilder.typeLiteral(Type::Bool, pnode.range);
  case ParserNode::Kind::TypeInt:
    return &this->mbuilder.typeLiteral(Type::Int, pnode.range);
  case ParserNode::Kind::TypeFloat:
    return &this->mbuilder.typeLiteral(Type::Float, pnode.range);
  case ParserNode::Kind::TypeString:
    return &this->mbuilder.typeLiteral(Type::String, pnode.range);
  case ParserNode::Kind::TypeObject:
    return &this->mbuilder.typeLiteral(Type::Object, pnode.range);
  case ParserNode::Kind::TypeAny:
    return &this->mbuilder.typeLiteral(Type::Any, pnode.range);
  case ParserNode::Kind::TypeType:
    return &this->mbuilder.typeLiteral(nullptr, pnode.range);
  case ParserNode::Kind::TypeUnary:
    EXPECT(pnode, pnode.children.size() == 1);
    EXPECT(pnode, pnode.children[0] != nullptr);
    return this->compileTypeExprUnary(pnode, *pnode.children.front(), context);
  case ParserNode::Kind::TypeBinary:
    EXPECT(pnode, pnode.children.size() == 2);
    EXPECT(pnode, pnode.children[0] != nullptr);
    EXPECT(pnode, pnode.children[1] != nullptr);
    return this->compileTypeExprBinary(pnode, *pnode.children.front(), *pnode.children.back(), context);
  case ParserNode::Kind::TypeFunctionSignature:
    EXPECT(pnode, pnode.children.size() >= 1);
    return this->compileTypeExprFunctionSignature(pnode, context);
  case ParserNode::Kind::TypeSpecification:
    return this->compileTypeSpecification(pnode, context);
  case ParserNode::Kind::TypeInfer:
  case ParserNode::Kind::TypeInferQ:
  case ParserNode::Kind::TypeFunctionSignatureParameter:
  case ParserNode::Kind::TypeSpecificationStaticData:
  case ParserNode::Kind::TypeSpecificationStaticFunction:
  case ParserNode::Kind::TypeSpecificationInstanceData:
  case ParserNode::Kind::TypeSpecificationInstanceFunction:
  case ParserNode::Kind::TypeSpecificationAccess:
    // Should not be compiled directly
    break;
  case ParserNode::Kind::ModuleRoot:
  case ParserNode::Kind::StmtBlock:
  case ParserNode::Kind::StmtDeclareVariable:
  case ParserNode::Kind::StmtDefineVariable:
  case ParserNode::Kind::StmtDefineFunction:
  case ParserNode::Kind::StmtDefineType:
  case ParserNode::Kind::StmtForEach:
  case ParserNode::Kind::StmtForLoop:
  case ParserNode::Kind::StmtIf:
  case ParserNode::Kind::StmtReturn:
  case ParserNode::Kind::StmtYield:
  case ParserNode::Kind::StmtThrow:
  case ParserNode::Kind::StmtTry:
  case ParserNode::Kind::StmtCatch:
  case ParserNode::Kind::StmtFinally:
  case ParserNode::Kind::StmtWhile:
  case ParserNode::Kind::StmtDo:
  case ParserNode::Kind::StmtSwitch:
  case ParserNode::Kind::StmtCase:
  case ParserNode::Kind::StmtDefault:
  case ParserNode::Kind::StmtBreak:
  case ParserNode::Kind::StmtContinue:
  case ParserNode::Kind::StmtMutate:
  case ParserNode::Kind::ExprUnary:
  case ParserNode::Kind::ExprBinary:
  case ParserNode::Kind::ExprTernary:
  case ParserNode::Kind::ExprCall:
  case ParserNode::Kind::ExprIndex:
  case ParserNode::Kind::ExprReference:
  case ParserNode::Kind::ExprDereference:
  case ParserNode::Kind::ExprArray:
  case ParserNode::Kind::ExprEon:
  case ParserNode::Kind::ExprObject:
  case ParserNode::Kind::ExprEllipsis:
  case ParserNode::Kind::ExprGuard:
  case ParserNode::Kind::ObjectSpecification:
  case ParserNode::Kind::ObjectSpecificationData:
  case ParserNode::Kind::ObjectSpecificationFunction:
  case ParserNode::Kind::Literal:
  case ParserNode::Kind::Named:
  case ParserNode::Kind::Missing:
    break;
  }
  return this->expected(pnode, "type expression");
}

ModuleNode* ModuleCompiler::compileTypeExprVariable(ParserNode& pnode, const ExprContext& context) {
  EXPECT(pnode, pnode.children.empty());
  String symbol;
  EXPECT(pnode, pnode.value->getString(symbol));
  auto extant = context.findSymbol(symbol);
  if (extant == nullptr) {
    return this->error(pnode, "Unknown type identifier: '", symbol, "'");
  }
  if (extant->kind != ExprContext::Symbol::Kind::Type) {
    return this->error(pnode, "Identifier '", symbol, "' is not a type");
  }
  return &this->mbuilder.typeVariableGet(symbol, pnode.range);
}

ModuleNode* ModuleCompiler::compileTypeExprProperty(ParserNode& dot, ParserNode& lhs, ParserNode& rhs, const ExprContext& context) {
  auto* lexpr = this->compileTypeExpr(lhs, context);
  if (lexpr == nullptr) {
    return nullptr;
  }
  auto* rexpr = this->compileValueExpr(rhs, context);
  if (rexpr == nullptr) {
    return nullptr;
  }
  auto* mnode = &this->mbuilder.typePropertyGet(*lexpr, *rexpr, dot.range);
  auto type = this->deduceTypeExpr(*mnode, context);
  if (type == nullptr) {
    return nullptr;
  }
  return mnode;
}

ModuleNode* ModuleCompiler::compileTypeExprUnary(ParserNode& op, ParserNode& lhs, const ExprContext& context) {
  auto* mtype = this->compileTypeExpr(lhs, context);
  if (mtype == nullptr) {
    return nullptr;
  }
  return &this->mbuilder.typeUnaryOp(op.op.typeUnaryOp, *mtype, op.range);
}

ModuleNode* ModuleCompiler::compileTypeExprBinary(ParserNode& op, ParserNode& lhs, ParserNode& rhs, const ExprContext& context) {
  auto* ltype = this->compileTypeExpr(lhs, context);
  if (ltype == nullptr) {
    return nullptr;
  }
  auto* rtype = this->compileTypeExpr(rhs, context);
  if (rtype == nullptr) {
    return nullptr;
  }
  return &this->mbuilder.typeBinaryOp(op.op.typeBinaryOp, *ltype, *rtype, op.range);
}

ModuleNode* ModuleCompiler::compileTypeExprFunctionSignature(ParserNode& pnode, const ExprContext& context) {
  assert(pnode.kind == ParserNode::Kind::TypeFunctionSignature);
  String fname;
  (void)pnode.value->getString(fname); // may be anonymous
  ModuleNode* mtype;
  auto& ptype = *pnode.children.front();
  mtype = this->compileTypeExpr(ptype, context);
  if (mtype == nullptr) {
    return nullptr;
  }
  auto* mnode = &this->mbuilder.typeFunctionSignature(fname, *mtype, pnode.range);
  for (size_t index = 1; index < pnode.children.size(); ++index) {
    auto& pchild = *pnode.children[index];
    assert(pchild.kind == ParserNode::Kind::TypeFunctionSignatureParameter);
    assert(pchild.children.size() == 1);
    String pname;
    EXPECT(pchild, pchild.value->getString(pname));
    mtype = this->compileTypeExpr(*pchild.children.front(), context);
    if (mtype == nullptr) {
      mnode = nullptr;
    } else if (mnode != nullptr) {
      ModuleNode* mchild;
      switch (pchild.op.parameterOp) {
      case ParserNode::ParameterOp::Required:
        mchild = &this->mbuilder.typeFunctionSignatureParameter(pname, IFunctionSignatureParameter::Flags::Required, *mtype, pnode.range);
        break;
      case ParserNode::ParameterOp::Optional:
      default:
        mchild = &this->mbuilder.typeFunctionSignatureParameter(pname, IFunctionSignatureParameter::Flags::None, *mtype, pnode.range);
        break;
      }
      this->mbuilder.appendChild(*mnode, *mchild);
    }
  }
  return mnode;
}

ModuleNode* ModuleCompiler::compileTypeSpecification(ParserNode& pnode, const ExprContext& context) {
  assert(pnode.kind == ParserNode::Kind::TypeSpecification);
  auto* mnode = &this->mbuilder.typeSpecification(pnode.range);
  ModuleNode* inode = nullptr;
  String description;
  if (pnode.value->getString(description)) {
    this->mbuilder.appendChild(*mnode, this->mbuilder.typeSpecificationDescription(description, pnode.range));
  }
  for (auto& pchild : pnode.children) {
    assert(pchild != nullptr);
    ModuleNode* mchild;
    ModuleNode* ichild = nullptr;
    EGG_WARNING_SUPPRESS_SWITCH_BEGIN
    switch (pchild->kind) {
    case ParserNode::Kind::TypeSpecificationStaticData:
      mchild = this->compileTypeSpecificationStaticData(*pchild, context, ichild);
      break;
    case ParserNode::Kind::TypeSpecificationStaticFunction:
      mchild = this->compileTypeSpecificationStaticFunction(*pchild, context, ichild);
      break;
    case ParserNode::Kind::TypeSpecificationInstanceData:
      mchild = this->compileTypeSpecificationInstanceData(*pchild, context);
      break;
    case ParserNode::Kind::TypeSpecificationInstanceFunction:
      mchild = this->compileTypeSpecificationInstanceFunction(*pchild, context);
      break;
    default:
      return this->expected(*pchild, "type specification clause");
    }
    EGG_WARNING_SUPPRESS_SWITCH_END
    if (ichild != nullptr) {
      if (inode == nullptr) {
        inode = &this->mbuilder.stmtManifestationInvoke(pnode.range);
      }
      this->mbuilder.appendChild(*inode, *ichild);
    }
    if (mchild == nullptr) {
      mnode = nullptr;
    } else if (mnode != nullptr) {
      this->mbuilder.appendChild(*mnode, *mchild);
    }
  }
  if ((mnode != nullptr) && (inode != nullptr)) {
    // Add the invoke node to the end, if it exists
    this->mbuilder.appendChild(*mnode, *inode);
  }
  return mnode;
}

ModuleNode* ModuleCompiler::compileTypeSpecificationStaticData(ParserNode& pnode, const ExprContext& context, ModuleNode*& inode) {
  assert(pnode.kind == ParserNode::Kind::TypeSpecificationStaticData);
  EXPECT(pnode, pnode.children.size() == 2);
  String symbol;
  EXPECT(pnode, pnode.value->getString(symbol));
  auto mtype = this->compileTypeExpr(*pnode.children.front(), context);
  if (mtype == nullptr) {
    return nullptr;
  }
  auto mvalue = this->compileValueExpr(*pnode.children.back(), context);
  if (mvalue == nullptr) {
    return nullptr;
  }
  auto* mnode = &this->mbuilder.typeSpecificationStaticMember(symbol, *mtype, pnode.range);
  inode = &this->mbuilder.stmtManifestationProperty(symbol, *mtype, *mvalue, Accessability::Get, pnode.range);
  return mnode;
}

ModuleNode* ModuleCompiler::compileTypeSpecificationStaticFunction(ParserNode& pnode, const ExprContext& context, ModuleNode*& inode) {
  assert(pnode.kind == ParserNode::Kind::TypeSpecificationStaticFunction);
  EXPECT(pnode, pnode.children.size() == 2);
  String symbol;
  EXPECT(pnode, pnode.value->getString(symbol));
  auto& phead = *pnode.children.front();
  if (phead.kind != ParserNode::Kind::TypeFunctionSignature) {
    return this->expected(phead, "function signature in static function definition");
  }
  auto mtype = this->compileTypeExprFunctionSignature(phead, context); // TODO add symbols directly here with better locations
  if (mtype == nullptr) {
    return nullptr;
  }
  auto type = this->deduceTypeExpr(*mtype, context);
  if (type == nullptr) {
    // TODO double-reported?
    return this->error(pnode, "Unable to deduce type of static function '", symbol, "' at compile time");
  }
  auto* signature = type.getOnlyFunctionSignature();
  assert(signature != nullptr);
  std::set<String> captures;
  StmtContextData::Count canReturn{};
  canReturn.type = signature->getReturnType();
  StmtContextData data{};
  data.canReturn = &canReturn;
  StmtContext inner{ context, &captures, std::move(data) };
  assert(inner.canReturn != nullptr);
  auto okay = this->addSymbol(inner, phead, StmtContext::Symbol::Kind::Function, symbol, type);
  size_t pcount = signature->getParameterCount();
  for (size_t pindex = 0; pindex < pcount; ++pindex) {
    auto& parameter = signature->getParameter(pindex);
    auto pname = parameter.getName();
    if (!pname.empty()) {
      okay &= this->addSymbol(inner, pnode, StmtContext::Symbol::Kind::Parameter, pname, parameter.getType());
    }
  }
  if (!okay) {
    return nullptr;
  }
  auto& ptail = *pnode.children.back();
  assert(ptail.kind == ParserNode::Kind::StmtBlock);
  auto& invoke = this->mbuilder.stmtFunctionInvoke(pnode.range);
  auto block = this->compileStmtBlockInto(ptail.children, inner, invoke);
  if (block == nullptr) {
    return nullptr;
  }
  assert(block == &invoke);
  auto& mvalue = this->mbuilder.exprFunctionConstruct(*mtype, invoke, pnode.range);
  for (const auto& capture : captures) {
    this->mbuilder.appendChild(mvalue, this->mbuilder.exprFunctionCapture(capture, pnode.range));
  }
  auto* mnode = &this->mbuilder.typeSpecificationStaticMember(symbol, *mtype, pnode.range);
  inode = &this->mbuilder.stmtManifestationProperty(symbol, *mtype, mvalue, Accessability::Get, pnode.range);
  return mnode;
}

ModuleNode* ModuleCompiler::compileTypeSpecificationInstanceData(ParserNode& pnode, const ExprContext& context) {
  assert(pnode.kind == ParserNode::Kind::TypeSpecificationInstanceData);
  EXPECT(pnode, pnode.children.size() >= 1);
  String symbol;
  EXPECT(pnode, pnode.value->getString(symbol));
  auto pchild = pnode.children.begin();
  auto mtype = this->compileTypeExpr(**pchild, context);
  if (mtype == nullptr) {
    return nullptr;
  }
  auto accessability = Accessability::None;
  while (++pchild != pnode.children.end()) {
    auto& child = **pchild;
    if (child.kind != ParserNode::Kind::TypeSpecificationAccess) {
      return this->expected(child, "type specification access node in instance data declaration");
    }
    if (Bits::hasAnySet(accessability, child.op.accessability)) {
      String keyword;
      if (child.value->getString(keyword)) {
        this->warning(child, "Duplicate '", keyword, "' access clause in instance data declaration of '", symbol, "'");
      } else {
        this->warning(child, "Duplicate access clause in instance data declaration of '", symbol, "'");
      }
    }
    accessability = accessability | child.op.accessability;
  }
  if (accessability == Accessability::None) {
    accessability = Accessability::All;
  }
  assert(Bits::clear(accessability, Accessability::All) == Accessability::None);
  auto* mnode = &this->mbuilder.typeSpecificationInstanceMember(symbol, *mtype, accessability, pnode.range);
  return mnode;
}

ModuleNode* ModuleCompiler::compileTypeSpecificationInstanceFunction(ParserNode& pnode, const ExprContext& context) {
  assert(pnode.kind == ParserNode::Kind::TypeSpecificationInstanceFunction);
  EXPECT(pnode, pnode.children.size() == 1);
  String symbol;
  EXPECT(pnode, pnode.value->getString(symbol));
  auto& phead = *pnode.children.front();
  if (phead.kind != ParserNode::Kind::TypeFunctionSignature) {
    return this->expected(phead, "function signature in instance function definition");
  }
  auto mtype = this->compileTypeExprFunctionSignature(phead, context);
  if (mtype == nullptr) {
    return nullptr;
  }
  auto accessability = Accessability::Get;
  auto* mnode = &this->mbuilder.typeSpecificationInstanceMember(symbol, *mtype, accessability, pnode.range);
  return mnode;
}

ModuleNode* ModuleCompiler::compileTypeGuard(ParserNode& pnode, const ExprContext& context, Type& type, ModuleNode*& mcond) {
  assert(pnode.kind == ParserNode::Kind::ExprGuard);
  EXPECT(pnode, pnode.children.size() == 2);
  ModuleNode* mexpr;
  auto* mtype = this->compileTypeInfer(pnode, *pnode.children.front(), *pnode.children.back(), context, type, mexpr);
  if (mexpr != nullptr) {
    String symbol;
    EXPECT(pnode, pnode.value->getString(symbol));
    auto actual = this->deduceExprType(*mexpr, context);
    assert(actual != nullptr);
    switch (this->isAssignable(type, actual)) {
    case Assignability::Never:
      this->log(ILogger::Severity::Warning, this->resource, pnode.range, ": Guarded assignment to '", symbol, "' of type '", type, "' will always fail");
      break;
    case Assignability::Sometimes:
      break;
    case Assignability::Always:
      this->log(ILogger::Severity::Warning, this->resource, pnode.range, ": Guarded assignment to '", symbol, "' of type '", type, "' will always succeed");
      break;
    }
    mcond = &this->mbuilder.exprGuard(symbol, *mexpr, pnode.range);
  }
  return mtype;
}

ModuleNode* ModuleCompiler::compileTypeInfer(ParserNode& pnode, ParserNode& ptype, ParserNode& pexpr, const ExprContext& context, Type& type, ModuleNode*& mexpr) {
  assert((pnode.kind == ParserNode::Kind::StmtDefineVariable) || (pnode.kind == ParserNode::Kind::StmtForEach) || (pnode.kind == ParserNode::Kind::ExprGuard));
  if (ptype.kind == ParserNode::Kind::TypeInfer) {
    return this->compileTypeInferVar(pnode, ptype, pexpr, context, type, mexpr, false);
  }
  if (ptype.kind == ParserNode::Kind::TypeInferQ) {
    return this->compileTypeInferVar(pnode, ptype, pexpr, context, type, mexpr, true);
  }
  auto* mtype = this->compileTypeExpr(ptype, context);
  if (mtype == nullptr) {
    return nullptr;
  }
  type = this->deduceTypeExpr(*mtype, context);
  if (type == nullptr) {
    return this->error(pnode, "Unable to infer type at compile time"); // TODO
  }
  mexpr = this->compileValueExpr(pexpr, context);
  if (mexpr == nullptr) {
    return nullptr;
  }
  if (pnode.kind == ParserNode::Kind::StmtForEach) {
    // We need to check the validity of 'for (<type> <iterator> : <iterable>)'
    auto actual = this->deduceTypeExpr(*mexpr, context);
    assert(actual != nullptr);
    auto& forge = this->vm.getTypeForge();
    auto itype = forge.forgeIterationType(type);
    if (itype == nullptr) {
      return this->error(pexpr, "Value of type '", *actual, "' is not iterable in 'for' statement");
    }
    // TODO check 'actual' against 'type'
    type = forge.forgeVoidableType(itype, false);
  }
  return mtype;
}

ModuleNode* ModuleCompiler::compileTypeInferVar(ParserNode& pnode, ParserNode& ptype, ParserNode& pexpr, const ExprContext& context, Type& type, ModuleNode*& mexpr, bool nullable) {
  assert((pnode.kind == ParserNode::Kind::StmtDefineVariable) || (pnode.kind == ParserNode::Kind::StmtForEach) || (pnode.kind == ParserNode::Kind::ExprGuard));
  assert((ptype.kind == ParserNode::Kind::TypeInfer) || (ptype.kind == ParserNode::Kind::TypeInferQ));
  mexpr = this->compileValueExpr(pexpr, context);
  if (mexpr == nullptr) {
    return nullptr;
  }
  type = this->deduceExprType(*mexpr, context);
  assert(type != nullptr);
  auto& forge = this->vm.getTypeForge();
  if (pnode.kind == ParserNode::Kind::StmtForEach) {
    // We now have the type of 'iterable' in 'for (var[?] <iterator> : <iterable>)'
    auto itype = forge.forgeIterationType(type);
    if (itype == nullptr) {
      return this->error(pexpr, "Value of type '", *type, "' is not iterable in 'for' statement");
    }
    type = itype;
  }
  assert(type != nullptr);
  type = forge.forgeNullableType(type, nullable);
  assert(type != nullptr);
  type = forge.forgeVoidableType(type, false);
  assert(type != nullptr);
  return &this->mbuilder.typeLiteral(type, ptype.range);
}

ModuleNode* ModuleCompiler::compileAmbiguousExpr(ParserNode& pnode, const ExprContext& context, Ambiguous& ambiguous) {
  EGG_WARNING_SUPPRESS_SWITCH_BEGIN
  switch (pnode.kind) {
  case ParserNode::Kind::Variable:
    return this->compileAmbiguousVariable(pnode, context, ambiguous);
  case ParserNode::Kind::TypeVoid:
    ambiguous = Ambiguous::Type;
    return &this->mbuilder.typeLiteral(Type::Void, pnode.range);
  case ParserNode::Kind::TypeBool:
    ambiguous = Ambiguous::Type;
    return &this->mbuilder.typeLiteral(Type::Bool, pnode.range);
  case ParserNode::Kind::TypeInt:
    ambiguous = Ambiguous::Type;
    return &this->mbuilder.typeLiteral(Type::Int, pnode.range);
  case ParserNode::Kind::TypeFloat:
    ambiguous = Ambiguous::Type;
    return &this->mbuilder.typeLiteral(Type::Float, pnode.range);
  case ParserNode::Kind::TypeString:
    ambiguous = Ambiguous::Type;
    return &this->mbuilder.typeLiteral(Type::String, pnode.range);
  case ParserNode::Kind::TypeObject:
    ambiguous = Ambiguous::Type;
    return &this->mbuilder.typeLiteral(Type::Object, pnode.range);
  case ParserNode::Kind::TypeAny:
    ambiguous = Ambiguous::Type;
    return &this->mbuilder.typeLiteral(Type::Any, pnode.range);
  }
  EGG_WARNING_SUPPRESS_SWITCH_END
  auto* mnode = this->compileValueExpr(pnode, context);
  if (mnode != nullptr) {
    ambiguous = Ambiguous::Value;
  }
  return mnode;
}

ModuleNode* ModuleCompiler::compileAmbiguousVariable(ParserNode& pnode, const ExprContext& context, Ambiguous& ambiguous) {
  EXPECT(pnode, pnode.children.empty());
  String symbol;
  EXPECT(pnode, pnode.value->getString(symbol));
  auto extant = context.findSymbol(symbol);
  if (extant == nullptr) {
    return this->error(pnode, "Unknown identifier: '", symbol, "'");
  }
  if (extant->kind == ExprContext::Symbol::Kind::Type) {
    ambiguous = Ambiguous::Type;
    return &this->mbuilder.typeVariableGet(symbol, pnode.range);
  }
  ambiguous = Ambiguous::Value;
  return &this->mbuilder.exprVariableGet(symbol, pnode.range);
}

ModuleNode* ModuleCompiler::compileLiteral(ParserNode& pnode) {
  EXPECT(pnode, pnode.children.empty());
  return &this->mbuilder.exprLiteral(pnode.value, pnode.range);
}

Type ModuleCompiler::literalType(ParserNode& pnode) {
  // Note that 'null' is not included on purpose
  EGG_WARNING_SUPPRESS_SWITCH_BEGIN
  switch (pnode.kind) {
  case ParserNode::Kind::TypeVoid:
    return Type::Void;
  case ParserNode::Kind::TypeBool:
    return Type::Bool;
  case ParserNode::Kind::TypeInt:
    return Type::Int;
  case ParserNode::Kind::TypeFloat:
    return Type::Float;
  case ParserNode::Kind::TypeString:
    return Type::String;
  case ParserNode::Kind::TypeObject:
    return Type::Object;
  case ParserNode::Kind::TypeAny:
    return Type::Any;
  }
  EGG_WARNING_SUPPRESS_SWITCH_END
  return nullptr;
}

String ModuleCompiler::deduceString(ModuleNode& mnode, const ExprContext&) {
  auto value = this->mbuilder.deduceConstant(mnode);
  String svalue;
  if (value->getString(svalue)) {
    return svalue;
  }
  return {};
}

ModuleNode* ModuleCompiler::checkValueExpr(ModuleNode& mnode, const ExprContext& context) {
  auto type = this->deduceExprType(mnode, context);
  if (type == nullptr) {
    return nullptr;
  }
  return &mnode;
}

bool ModuleCompiler::checkValueExprOperand(const char* expected, ModuleNode& mnode, ParserNode& pnode, ValueFlags required, const ExprContext& context) {
  auto type = this->deduceExprType(mnode, context);
  assert(type != nullptr);
  if (!Bits::hasAnySet(type->getPrimitiveFlags(), required)) {
    this->error(pnode, "Expected ", expected, ", but instead got a value of type '", *type, "'");
    return false;
  }
  return true;
}

bool ModuleCompiler::checkValueExprOperand2(const char* expected, ModuleNode& lhs, ModuleNode& rhs, ParserNode& pnode, ValueFlags required, const ExprContext& context) {
  auto type = this->deduceExprType(lhs, context);
  assert(type != nullptr);
  if (!Bits::hasAnySet(type->getPrimitiveFlags(), required)) {
    this->error(pnode, "Expected left-hand side of ", expected, ", but instead got a value of type '", *type, "'");
    return false;
  }
  type = this->deduceExprType(rhs, context);
  assert(type != nullptr);
  if (!Bits::hasAnySet(type->getPrimitiveFlags(), required)) {
    this->error(pnode, "Expected right-hand side of ", expected, ", but instead got a value of type '", *type, "'");
    return false;
  }
  return true;
}

bool ModuleCompiler::checkValueExprUnary(ValueUnaryOp op, ModuleNode& rhs, ParserNode& pnode, const ExprContext& context) {
  switch (op) {
  case ValueUnaryOp::Negate: // -a
    return this->checkValueExprOperand("expression after negation operator '-' to be an 'int' or 'float'", rhs, pnode, ValueFlags::Arithmetic, context);
  case ValueUnaryOp::BitwiseNot: // ~a
    return this->checkValueExprOperand("expression after bitwise-not operator '~' to be an 'int'", rhs, pnode, ValueFlags::Int, context);
  case ValueUnaryOp::LogicalNot: // !a
    return this->checkValueExprOperand("expression after logical-not operator '!' to be an 'int'", rhs, pnode, ValueFlags::Bool, context);
  }
  return false;
}

bool ModuleCompiler::checkValueExprBinary(ValueBinaryOp op, ModuleNode& lhs, ModuleNode& rhs, ParserNode& pnode, const ExprContext& context) {
  const auto arithmetic = ValueFlags::Arithmetic;
  const auto bitwise = ValueFlags::Bool | ValueFlags::Int;
  const auto integer = ValueFlags::Bool | ValueFlags::Int;
  switch (op) {
    case ValueBinaryOp::Add: // a + b
      return this->checkValueExprOperand2("addition operator '+' to be an 'int' or 'float'", lhs, rhs, pnode, arithmetic, context);
    case ValueBinaryOp::Subtract: // a - b
      return this->checkValueExprOperand2("subtraction operator '-' to be an 'int' or 'float'", lhs, rhs, pnode, arithmetic, context);
    case ValueBinaryOp::Multiply: // a * b
      return this->checkValueExprOperand2("multiplication operator '*' to be an 'int' or 'float'", lhs, rhs, pnode, arithmetic, context);
    case ValueBinaryOp::Divide: // a / b
      return this->checkValueExprOperand2("division operator '/' to be an 'int' or 'float'", lhs, rhs, pnode, arithmetic, context);
    case ValueBinaryOp::Remainder: // a % b
      return this->checkValueExprOperand2("remainder operator '%' to be an 'int' or 'float'", lhs, rhs, pnode, arithmetic, context);
    case ValueBinaryOp::LessThan: // a < b
      return this->checkValueExprOperand2("comparison operator '<' to be an 'int' or 'float'", lhs, rhs, pnode, arithmetic, context);
    case ValueBinaryOp::LessThanOrEqual: // a <= b
      return this->checkValueExprOperand2("comparison operator '<=' to be an 'int' or 'float'", lhs, rhs, pnode, arithmetic, context);
    case ValueBinaryOp::Equal: // a == b
      // TODO
      break;
    case ValueBinaryOp::NotEqual: // a != b
      // TODO
      break;
    case ValueBinaryOp::GreaterThanOrEqual: // a >= b
      return this->checkValueExprOperand2("comparison operator '>=' to be an 'int' or 'float'", lhs, rhs, pnode, arithmetic, context);
    case ValueBinaryOp::GreaterThan: // a > b
      return this->checkValueExprOperand2("comparison operator '>' to be an 'int' or 'float'", lhs, rhs, pnode, arithmetic, context);
    case ValueBinaryOp::BitwiseAnd: // a & b
      return this->checkValueExprOperand2("bitwise-and operator '&' to be a 'bool' or 'int'", lhs, rhs, pnode, bitwise, context);
    case ValueBinaryOp::BitwiseOr: // a | b
      return this->checkValueExprOperand2("bitwise-or operator '|' to be a 'bool' or 'int'", lhs, rhs, pnode, bitwise, context);
    case ValueBinaryOp::BitwiseXor: // a ^ b
      return this->checkValueExprOperand2("bitwise-xor operator '^' to be a 'bool' or 'int'", lhs, rhs, pnode, bitwise, context);
    case ValueBinaryOp::ShiftLeft: // a << b
      return this->checkValueExprOperand2("left-shift operator '<<' to be an 'int'", lhs, rhs, pnode, integer, context);
    case ValueBinaryOp::ShiftRight: // a >> b
      return this->checkValueExprOperand2("right-shift operator '>>' to be an 'int'", lhs, rhs, pnode, integer, context);
    case ValueBinaryOp::ShiftRightUnsigned: // a >>> b
      return this->checkValueExprOperand2("unsigned-shift operator '>>>' to be an 'int'", lhs, rhs, pnode, integer, context);
    case ValueBinaryOp::Minimum: // a <| b
      return this->checkValueExprOperand2("minimum operator '<|' to be an 'int' or 'float'", lhs, rhs, pnode, arithmetic, context);
    case ValueBinaryOp::Maximum: // a >| b
      return this->checkValueExprOperand2("maximum operator '>|' to be an 'int' or 'float'", lhs, rhs, pnode, arithmetic, context);
    case ValueBinaryOp::IfVoid: // a !! b
      // TODO
      break;
    case ValueBinaryOp::IfNull: // a ?? b
      // TODO
      break;
    case ValueBinaryOp::IfFalse: // a || b
      // TODO
      break;
    case ValueBinaryOp::IfTrue: // a && b
      // TODO
      break;
  }
  return true;
}

bool ModuleCompiler::checkValueExprTernary(ValueTernaryOp, ModuleNode& lhs, ModuleNode& mid, ModuleNode& rhs, ParserNode& pnode, const ExprContext& context) {
  if (!this->checkValueExprOperand("condition of ternary operator '?:' to be a 'bool'", lhs, pnode, ValueFlags::Bool, context)) {
    return false;
  }
  if (this->checkValueExpr(mid, context) == nullptr) {
    return false;
  }
  if (this->checkValueExpr(rhs, context) == nullptr) {
    return false;
  }
  return true;
}

bool ModuleCompiler::checkStmtVariableMutate(const String& symbol, ValueMutationOp op, ModuleNode& value, ParserNode& pnode, const StmtContext& context) {
  auto extant = context.findSymbol(symbol);
  if (extant == nullptr) {
    this->error(pnode, "Unknown identifier: '", symbol, "'");
    return false;
  }
  if (extant->kind == ExprContext::Symbol::Kind::Type) {
    this->error(pnode, "Type identifier '", symbol, "' cannot be modified");
    return false;
  }
  auto vtype = this->deduceExprType(value, context);
  if (vtype == nullptr) {
    return false;
  }
  String problem;
  switch (this->checkTargetMutate(extant->type, op, vtype, problem)) {
  case MutateCheck::Failure:
    this->error(pnode, "Variable '", symbol, "' (declared as '", *extant->type, "') ", problem);
    return false;
  case MutateCheck::Unnecessary:
    this->warning(pnode, problem, " when applied to variable '", symbol, "' (declared as '", *extant->type, "')");
    break;
  case MutateCheck::Success:
  default:
    break;
  }
  return true;
}

bool ModuleCompiler::checkStmtPropertyMutate(ModuleNode& instance, ModuleNode& property, ValueMutationOp op, ModuleNode& value, ParserNode& pnode, const StmtContext& context) {
  // Careful: The property may belong to a type (e.g. 'int.max') or an instance (e.g. 'o.p')
  auto kind = IVMTypeResolver::Kind::Type;
  auto ctype = this->deduceType(instance, context, kind);
  if (ctype == nullptr) {
    return false;
  }
  auto vtype = this->deduceExprType(value, context);
  if (vtype == nullptr) {
    return false;
  }
  auto pname = this->deduceString(property, context);
  auto& forge = this->vm.getTypeForge();
  if (kind == IVMTypeResolver::Kind::Type) {
    auto* metashape = forge.getMetashape(ctype);
    if (metashape == nullptr) {
      this->error(pnode, "TODO: Cannot find metashape for type '", *ctype, "'");
      return false;
    }
    if (metashape->dotable == nullptr) {
      this->error(pnode, "Type '", *ctype, "' does not support properties");
      return false;
    }
    if (pname.empty()) {
      // Unknown or runtime-only property name
      auto paccessability = getAccessabilityUnion(*metashape->dotable);
      if (Bits::hasNoneSet(paccessability, Accessability::Mut)) {
        this->error(pnode, "Type '", *ctype, "' does not support property modification");
        return false;
      }
      // TODO
      String problem;
      switch (this->checkTargetMutate(Type::AnyQ, op, vtype, problem)) {
      case MutateCheck::Failure:
        this->error(pnode, "Type '", *ctype, "' property ", problem);
        return false;
      case MutateCheck::Unnecessary: // never unnecessary
      case MutateCheck::Success:
      default:
        break;
      }
      return true;
    }
    auto paccessability = metashape->dotable->getAccessability(pname);
    auto ptype = metashape->dotable->getType(pname);
    if ((paccessability == Accessability::None) || (ptype == nullptr)) {
      this->error(pnode, "Type '", *ctype, "' does not support property '", pname, "'");
      return false;
    }
    if (Bits::hasNoneSet(paccessability, Accessability::Mut)) {
      this->error(pnode, "Type '", *ctype, "' does not support modification of property '", pname, "'");
      return false;
    }
    String problem;
    switch (this->checkTargetMutate(ptype, op, vtype, problem)) {
    case MutateCheck::Failure:
      this->error(pnode, "Type '", *ctype, "' property '", pname, "' (declared as '", *ptype, "') ", problem);
      return false;
    case MutateCheck::Unnecessary:
      this->warning(pnode, problem, " when applied to type '", *ctype, "' property '", pname, "' (declared as '", *ptype, "')");
      break;
    case MutateCheck::Success:
    default:
      break;
    }
    return true;
  }
  auto foundAny = false;
  auto foundMut = false;
  if (pname.empty()) {
    // Unknown or runtime-only property name
    forge.foreachDotable(ctype, [&](const IPropertySignature& dotable) {
      // 'pname' is empty here, so we're effectively asking for unknown accessability
      foundAny = true;
      if (Bits::hasAnySet(dotable.getAccessability(pname), Accessability::Mut)) {
        foundMut = true;
      }
      return foundMut; // completed if found a mutable property
    });
    if (!foundAny) {
      this->error(pnode, "Values of type '", *ctype, "' do not support properties");
      return false;
    }
    if (!foundMut) {
      this->error(pnode, "Values of type '", *ctype, "' do not support property modification");
      return false;
    }
    // TODO
    String problem;
    switch (this->checkTargetMutate(Type::AnyQ, op, vtype, problem)) {
    case MutateCheck::Failure:
      this->error(pnode, "'", *ctype, "' property ", problem);
      return false;
    case MutateCheck::Unnecessary: // never unnecessary
    case MutateCheck::Success:
    default:
      break;
    }
    if (!problem.empty()) {
      this->error(pnode, "'", *ctype, "' property ", problem);
      return false;
    }
    return true;
  }
  auto builder = forge.createComplexBuilder();
  forge.foreachDotable(ctype, [&](const IPropertySignature& dotable) {
    auto accessability = dotable.getAccessability(pname);
    foundAny |= (accessability != Accessability::None);
    if (Bits::hasAnySet(accessability, Accessability::Mut)) {
      auto ptype = dotable.getType(pname);
      if (ptype != nullptr) {
        // The builder constructs a union of all plausible property types
        builder->addType(ptype);
      }
      foundMut = true;
    }
    return false;
  });
  if (!foundAny) {
    this->error(pnode, "Values of type '", *ctype, "' do not support property '", pname, "'");
    return false;
  }
  if (!foundMut) {
    this->error(pnode, "Values of type '", *ctype, "' do not support modification of property '", pname, "'");
    return false;
  }
  auto ptype = builder->build();
  String problem;
  switch (this->checkTargetMutate(ptype, op, vtype, problem)) {
  case MutateCheck::Failure:
    this->error(pnode, "'", *ctype, "' property '", pname, "' (declared as '", *ptype, "') ", problem);
    return false;
  case MutateCheck::Unnecessary:
    this->warning(pnode, problem, " when applied to '", *ctype, "' property '", pname, "' (declared as '", *ptype, "')");
    break;
  case MutateCheck::Success:
  default:
    break;
  }
  return true;
}

bool ModuleCompiler::checkStmtIndexMutate(ModuleNode& instance, ModuleNode& index, ValueMutationOp op, ModuleNode& value, ParserNode& pnode, const StmtContext& context) {
  auto ctype = this->deduceExprType(instance, context);
  if (ctype == nullptr) {
    return false;
  }
  auto itype = this->deduceExprType(index, context);
  if (itype == nullptr) {
    return false;
  }
  auto vtype = this->deduceExprType(value, context);
  if (vtype == nullptr) {
    return false;
  }
  auto& forge = this->vm.getTypeForge();
  auto foundAny = false;
  auto foundMut = false;
  auto rbuilder = forge.createComplexBuilder();
  forge.foreachIndexable(ctype, [&](const IIndexSignature& indexable) {
    auto accessability = indexable.getAccessability();
    foundAny |= (accessability != Accessability::None);
    if (Bits::hasAnySet(accessability, Accessability::Mut)) {
      // TODO check index type
      auto rtype = indexable.getResultType();
      if (rtype != nullptr) {
        // The builder constructs a union of all plausible result types
        rbuilder->addType(rtype);
      }
      foundMut = true;
    }
    return false;
  });
  if (!foundAny) {
    this->error(pnode, "Values of type '", *ctype, "' do not support indexing");
    return false;
  }
  if (!foundMut) {
    this->error(pnode, "Values of type '", *ctype, "' do not support modification via indexing");
    return false;
  }
  auto rtype = rbuilder->build();
  String problem;
  switch (this->checkTargetMutate(rtype, op, vtype, problem)) {
  case MutateCheck::Failure:
    this->error(pnode, "'", *ctype, "' indexed value (declared as '", *rtype, "') ", problem);
    return false;
  case MutateCheck::Unnecessary:
    this->warning(pnode, problem, " when applied to '", *ctype, "' indexed value (declared as '", *rtype, "')");
    break;
  case MutateCheck::Success:
  default:
    break;
  }
  return true;
}

bool ModuleCompiler::checkStmtPointeeMutate(ModuleNode& instance, ValueMutationOp op, ModuleNode& value, ParserNode& pnode, const StmtContext& context) {
  auto ctype = this->deduceExprType(instance, context);
  if (ctype == nullptr) {
    return false;
  }
  auto vtype = this->deduceExprType(value, context);
  if (vtype == nullptr) {
    return false;
  }
  auto& forge = this->vm.getTypeForge();
  auto foundAny = false;
  auto foundMut = false;
  auto rbuilder = forge.createComplexBuilder();
  forge.foreachPointable(ctype, [&](const IPointerSignature& pointable) {
    auto modifiability = pointable.getModifiability();
    foundAny |= (modifiability != Modifiability::None);
    if (Bits::hasAnySet(modifiability, Modifiability::Mutate)) {
      auto type = pointable.getPointeeType();
      if (type != nullptr) {
        // The builder constructs a union of all plausible result types
        rbuilder->addType(type);
      }
      foundMut = true;
    }
    return false;
  });
  if (!foundAny) {
    this->error(pnode, "Values of type '", *ctype, "' do not support pointer operator '*'");
    return false;
  }
  if (!foundMut) {
    this->error(pnode, "Values of type '", *ctype, "' do not support modification via pointer operator '*'");
    return false;
  }
  auto rtype = rbuilder->build();
  String problem;
  switch (this->checkTargetMutate(rtype, op, vtype, problem)) {
  case MutateCheck::Failure:
    this->error(pnode, "Dereferenced value (declared as '", *rtype, "') ", problem);
    return false;
  case MutateCheck::Unnecessary:
    this->warning(pnode, problem, " when applied to dereferenced value (declared as '", *rtype, "')");
    break;
  case MutateCheck::Success:
  default:
    break;
  }
  return true;
}

ModuleCompiler::MutateCheck ModuleCompiler::checkTargetMutate(const Type& target, ValueMutationOp op, const Type& value, String& problem) {
  auto& forge = this->vm.getTypeForge();
  auto mutatability = forge.isTypeMutatable(target, op, value);
  switch (mutatability) {
  case Mutatability::Sometimes:
  case Mutatability::Always:
    break;
  case Mutatability::NeverLeft:
    if (op == ValueMutationOp::Assign) {
      problem = this->concat("cannot be assigned a value of type '", *value, "'");
    } else {
      problem = this->concat("cannot have operator '", op, "' applied");
    }
    return MutateCheck::Failure;
  case Mutatability::NeverRight:
    problem = this->concat("cannot have operator '", op, "' applied with a right-hand side of type '", *value, "'");
    return MutateCheck::Failure;
  case Mutatability::Unnecessary:
    problem = this->concat("Operator '", op, "' has no effect");
    return MutateCheck::Unnecessary;
  }
  return MutateCheck::Success;
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
  case ParserNode::Kind::StmtDefineType:
    return "type definition statement";
  case ParserNode::Kind::StmtForEach:
    return "for each statement";
  case ParserNode::Kind::StmtForLoop:
    return "for loop statement";
  case ParserNode::Kind::StmtIf:
    return "if statement";
  case ParserNode::Kind::StmtReturn:
    return "return statement";
  case ParserNode::Kind::StmtYield:
    return "yield statement";
  case ParserNode::Kind::StmtTry:
    return "try statement";
  case ParserNode::Kind::StmtThrow:
    return "throw statement";
  case ParserNode::Kind::StmtCatch:
    return "catch statement";
  case ParserNode::Kind::StmtFinally:
    return "finally statement";
  case ParserNode::Kind::StmtWhile:
    return "while statement";
  case ParserNode::Kind::StmtDo:
    return "do statement";
  case ParserNode::Kind::StmtSwitch:
    return "switch statement";
  case ParserNode::Kind::StmtCase:
    return "case statement";
  case ParserNode::Kind::StmtDefault:
    return "default statement";
  case ParserNode::Kind::StmtBreak:
    return "break statement";
  case ParserNode::Kind::StmtContinue:
    return "continue statement";
  case ParserNode::Kind::StmtMutate:
    return "mutate statement";
  case ParserNode::Kind::ExprUnary:
    return "unary operator";
  case ParserNode::Kind::ExprBinary:
    return "binary operator";
  case ParserNode::Kind::ExprTernary:
    return "ternary operator";
  case ParserNode::Kind::ExprReference:
    return "reference";
  case ParserNode::Kind::ExprDereference:
    return "dereference";
  case ParserNode::Kind::ExprCall:
    return "call expression";
  case ParserNode::Kind::ExprIndex:
    return "index access";
  case ParserNode::Kind::ExprProperty:
    return "property access";
  case ParserNode::Kind::ExprArray:
    return "array expression";
  case ParserNode::Kind::ExprEon:
    return "eon expression";
  case ParserNode::Kind::ExprObject:
    return "object expression";
  case ParserNode::Kind::ExprEllipsis:
    return "ellipsis";
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
  case ParserNode::Kind::TypeSpecification:
    return "type specification";
  case ParserNode::Kind::TypeSpecificationStaticData:
    return "type specification static data";
  case ParserNode::Kind::TypeSpecificationStaticFunction:
    return "type specification static function";
  case ParserNode::Kind::TypeSpecificationInstanceData:
    return "type specification instance data";
  case ParserNode::Kind::TypeSpecificationInstanceFunction:
    return "type specification instance function";
  case ParserNode::Kind::TypeSpecificationAccess:
    return "type specification access";
  case ParserNode::Kind::ObjectSpecification:
    return "object specification";
  case ParserNode::Kind::ObjectSpecificationData:
    return "object specification data";
  case ParserNode::Kind::ObjectSpecificationFunction:
    return "object specification function";
  case ParserNode::Kind::Literal:
    return "literal";
  case ParserNode::Kind::Variable:
    return "variable";
  case ParserNode::Kind::Named:
    return "named expression";
  case ParserNode::Kind::Missing:
    return "nothing";
  }
  return "unknown node kind";
}

HardPtr<IVMProgram> egg::ovum::EggCompilerFactory::compileFromStream(IVM& vm, TextStream& stream) {
  auto lexer = LexerFactory::createFromTextStream(stream);
  auto tokenizer = EggTokenizerFactory::createFromLexer(vm.getAllocator(), lexer);
  auto parser = EggParserFactory::createFromTokenizer(vm.getAllocator(), tokenizer);
  auto pbuilder = vm.createProgramBuilder();
  pbuilder->addBuiltin(vm.createString("assert"), Type::Object); // TODO
  pbuilder->addBuiltin(vm.createString("print"), Type::Object); // TODO
  pbuilder->addBuiltin(vm.createString("symtable"), Type::Object); // TODO
  auto compiler = EggCompilerFactory::createFromProgramBuilder(pbuilder);
  auto module = compiler->compile(*parser);
  if (module != nullptr) {
    return pbuilder->build();
  }
  return nullptr;
}

HardPtr<IVMProgram> egg::ovum::EggCompilerFactory::compileFromPath(IVM& vm, const std::filesystem::path& script, bool swallowBOM) {
  FileTextStream stream{ script, swallowBOM };
  return EggCompilerFactory::compileFromStream(vm, stream);
}

HardPtr<IVMProgram> egg::ovum::EggCompilerFactory::compileFromText(IVM& vm, const std::string& script, const std::string& resource) {
  StringTextStream stream{ script, resource };
  return EggCompilerFactory::compileFromStream(vm, stream);
}

std::shared_ptr<IEggCompiler> egg::ovum::EggCompilerFactory::createFromProgramBuilder(const HardPtr<IVMProgramBuilder>& builder) {
  auto compiler = std::make_shared<EggCompiler>(builder);
  return compiler;
}
