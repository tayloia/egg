#include "yolk.h"
#include "lexers.h"
#include "egg-tokenizer.h"
#include "egg-syntax.h"
#include "egg-parser.h"
#include "egg-engine.h"

namespace {
  using namespace egg::yolk;

  class EggEngineExecution : public IEggEngineExecution {
  private:
    std::shared_ptr<IEggEngineLogger> logger;
  public:
    explicit EggEngineExecution(const std::shared_ptr<IEggEngineLogger>& logger)
      : logger(logger) {
      assert(logger != nullptr);
    }
    virtual ~EggEngineExecution() {
    }
    virtual void print(const std::string& text) {
      this->logger->log(egg::lang::LogSource::User, egg::lang::LogSeverity::Information, text);
    }
  };

  class EggEngine : public IEggEngine {
  private:
    std::shared_ptr<IEggParserNode> root;
  public:
    explicit EggEngine(const std::shared_ptr<IEggParserNode>& root)
      : root(root) {
      assert(root != nullptr);
    }
    virtual ~EggEngine() {
    }
    virtual std::shared_ptr<IEggEngineExecution> createExecutionFromLogger(const std::shared_ptr<IEggEngineLogger>& logger) {
      return std::make_shared<EggEngineExecution>(logger);
    }
    virtual void execute(IEggEngineExecution& execution) {
      execution.print("hello");
    }
  };
}

std::shared_ptr<egg::yolk::IEggEngine> egg::yolk::EggEngineFactory::createEngineFromParsed(const std::shared_ptr<IEggParserNode>& root) {
  return std::make_shared<EggEngine>(root);
}
