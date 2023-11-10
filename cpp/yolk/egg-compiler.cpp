#include "yolk/yolk.h"
#include "yolk/lexers.h"
#include "yolk/egg-tokenizer.h"
#include "yolk/egg-parser.h"
#include "yolk/egg-compiler.h"
#include "yolk/egg-module.h"

using namespace egg::yolk;

namespace {
  class EggModule : public IEggModule {
    EggModule(const EggModule&) = delete;
    EggModule& operator=(const EggModule&) = delete;
  private:
    egg::ovum::IVM& vm;
    std::shared_ptr<IEggParser::Node> root;
  public:
    EggModule(egg::ovum::IVM& vm, const std::shared_ptr<IEggParser::Node>& root)
      : vm(vm),
        root(root) {
      assert(this->root != nullptr);
    }
    virtual void execute() const override {
      // WIBBLE
      auto message = this->vm.createString("Hello, World!");
      this->vm.getLogger().log(egg::ovum::ILogger::Source::User, egg::ovum::ILogger::Severity::None, message);
    }
  };

  class EggCompiler : public IEggCompiler {
    EggCompiler(const EggCompiler&) = delete;
    EggCompiler& operator=(const EggCompiler&) = delete;
  private:
    egg::ovum::IVM& vm;
    std::shared_ptr<IEggParser> parser;
  public:
    EggCompiler(egg::ovum::IVM& vm, const std::shared_ptr<IEggParser>& parser)
      : vm(vm),
        parser(parser) {
      assert(this->parser != nullptr);
    }
    virtual std::shared_ptr<IEggModule> compile() override {
      IEggParser::Result result;
      auto severity = this->parse(result);
      (void)severity; // TODO
      return std::make_shared<EggModule>(this->vm, result.root);
    }
    virtual egg::ovum::String resource() const override {
      return this->parser->resource();
    }
  private:
    egg::ovum::ILogger::Severity parse(IEggParser::Result& result) {
      auto& logger = this->vm.getLogger();
      result = this->parser->parse();
      auto severity = egg::ovum::ILogger::Severity::None;
      for (const auto& issue : result.issues) {
        switch (issue.severity) {
        case IEggParser::Issue::Severity::Information:
          logger.log(egg::ovum::ILogger::Source::Compiler, egg::ovum::ILogger::Severity::Information, this->formatIssue(issue));
          if (severity == egg::ovum::ILogger::Severity::None) {
            severity = egg::ovum::ILogger::Severity::Information;
          }
          break;
        case IEggParser::Issue::Severity::Warning:
          logger.log(egg::ovum::ILogger::Source::Compiler, egg::ovum::ILogger::Severity::Warning, this->formatIssue(issue));
          if (severity != egg::ovum::ILogger::Severity::Error) {
            severity = egg::ovum::ILogger::Severity::Warning;
          }
          break;
        case IEggParser::Issue::Severity::Error:
          logger.log(egg::ovum::ILogger::Source::Compiler, egg::ovum::ILogger::Severity::Warning, this->formatIssue(issue));
          severity = egg::ovum::ILogger::Severity::Error;
          break;
        }
      }
      return severity;
    }
    egg::ovum::String formatIssue(const IEggParser::Issue& issue) {
      if (issue.begin.line == 0) {
        return issue.message;
      }
      // See https://learn.microsoft.com/en-us/visualstudio/msbuild/msbuild-diagnostic-format-for-tasks
      egg::ovum::StringBuilder sb;
      // resource(line
      sb.add(this->parser->resource(), '(', issue.begin.line);
      if (issue.begin.column > 0) {
        // resource(line,column
        sb.add(',', issue.begin.column);
        if ((issue.end.line > 0) && (issue.end.column > 0)) {
          if (issue.end.line > issue.begin.line) {
            // resource(line,column,line,column
            sb.add(',', issue.end.line, ',', issue.end.column);
          } else if (issue.end.column > issue.begin.column) {
            // resource(line,column-column
            sb.add('-', issue.end.column);
          }
        }
      }
      sb.add(") : ", issue.message);
      return sb.build(this->vm.getAllocator());
    }
  };
}

std::shared_ptr<IEggCompiler> EggCompilerFactory::createFromParser(egg::ovum::IVM& vm, const std::shared_ptr<IEggParser>& parser) {
  return std::make_shared<EggCompiler>(vm, parser);
}
