#include "yolk/yolk.h"
#include "yolk/lexers.h"
#include "yolk/egg-tokenizer.h"
#include "yolk/egg-parser.h"
#include "yolk/egg-compiler.h"

using namespace egg::yolk;

namespace {
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
    egg::ovum::HardPtr<egg::ovum::IVMModule> compile(const IEggParser::Node& root);
  private:
    bool compileRootStatement(const IEggParser::Node& node);
    void log(egg::ovum::ILogger::Source source, egg::ovum::ILogger::Severity severity, const egg::ovum::String& message) const {
      this->vm.getLogger().log(source, severity, message);
    }
    template<typename... ARGS>
    egg::ovum::String concat(ARGS&&... args) const {
      return egg::ovum::StringBuilder::concat(this->vm.getAllocator(), std::forward<ARGS>(args)...);
    }
    template<typename... ARGS>
    bool error(const IEggParser::Node& node, ARGS&&... args) const {
      egg::ovum::StringBuilder sb;
      printIssueRange(sb, this->resource, node.begin, node.end);
      sb.add(std::forward<ARGS>(args)...);
      auto message = sb.build(this->vm.getAllocator());
      this->log(egg::ovum::ILogger::Source::Compiler, egg::ovum::ILogger::Severity::Error, message);
      return false;
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
        auto mbuilder = this->pbuilder->createModuleBuilder();
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

egg::ovum::HardPtr<egg::ovum::IVMModule> ModuleCompiler::compile(const IEggParser::Node& root) {
  if (root.kind != IEggParser::Node::Kind::ModuleRoot) {
    this->error(root, "Expected module root node");
    return nullptr;
  }
  for (const auto& child : root.children) {
    if (!this->compileRootStatement(*child)) {
      return nullptr;
    }
  }
  return this->mbuilder.build();
}

bool ModuleCompiler::compileRootStatement(const IEggParser::Node& node) {
  (void)node; // WIBBLE
  return true;
}

egg::ovum::HardPtr<egg::ovum::IVMProgram> egg::yolk::EggCompilerFactory::compileFromPath(egg::ovum::IVM& vm, const std::string& path, bool swallowBOM) {
  FileTextStream stream{ path, swallowBOM };
  return EggCompilerFactory::compileFromStream(vm, stream);
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

std::shared_ptr<IEggCompiler> EggCompilerFactory::createFromProgramBuilder(const egg::ovum::HardPtr<egg::ovum::IVMProgramBuilder>& builder) {
  return std::make_shared<EggCompiler>(builder);
}
