#include "yolk.h"
#include "lexers.h"
#include "egg-tokenizer.h"
#include "egg-syntax.h"
#include "egg-parser.h"
#include "egg-engine.h"
#include "egg-program.h"

namespace {
  using namespace egg::yolk;

  template<typename ACTION>
  void captureExceptions(egg::lang::LogSource source, IEggEngineLogger& logger, ACTION action) {
    try {
      action();
    } catch (const Exception& ex) {
      // We have an opportunity to extract more information in the future
      logger.log(source, egg::lang::LogSeverity::Error, ex.what());
    } catch (const std::exception& ex) {
      logger.log(source, egg::lang::LogSeverity::Error, ex.what());
    }
  }

  template<class BASE>
  class EggEngineBaseContext : public BASE {
  protected:
    std::shared_ptr<IEggEngineLogger> logger;
    egg::lang::LogSeverity maximumSeverity;
  public:
    explicit EggEngineBaseContext(const std::shared_ptr<IEggEngineLogger>& logger)
      : logger(logger), maximumSeverity(egg::lang::LogSeverity::None) {
      assert(logger != nullptr);
    }
    virtual ~EggEngineBaseContext() {
    }
    virtual void log(egg::lang::LogSource source, egg::lang::LogSeverity severity, const std::string& message) {
      if (severity > this->maximumSeverity) {
        this->maximumSeverity = severity;
      }
      this->logger->log(source, severity, message);
    }
    virtual egg::lang::LogSeverity getMaximumSeverity() const {
      return this->maximumSeverity;
    }
  };

  class EggEnginePreparationContext : public EggEngineBaseContext<IEggEnginePreparationContext> {
  public:
    explicit EggEnginePreparationContext(const std::shared_ptr<IEggEngineLogger>& logger)
      : EggEngineBaseContext(logger) {
      assert(logger != nullptr);
    }
  };

  class EggEngineExecutionContext : public EggEngineBaseContext<IEggEngineExecutionContext> {
  public:
    explicit EggEngineExecutionContext(const std::shared_ptr<IEggEngineLogger>& logger)
      : EggEngineBaseContext(logger) {
      assert(logger != nullptr);
    }
    virtual void print(const std::string& text) {
      this->log(LogSource::User, LogSeverity::Information, text);
    }
  };

  class EggEngineParsed : public IEggEngine {
  private:
    EggEngineProgram program;
  public:
    explicit EggEngineParsed(const std::shared_ptr<IEggParserNode>& root)
      : program(root) {
    }
    virtual ~EggEngineParsed() {
    }
    virtual Severity prepare(IEggEnginePreparationContext& preparation) {
      preparation.log(egg::lang::LogSource::Compiler, egg::lang::LogSeverity::Error, "Unnecessary program preparation");
      return preparation.getMaximumSeverity();
    }
    virtual Severity execute(IEggEngineExecutionContext& execution) {
      this->program.execute(execution);
      return execution.getMaximumSeverity();
    }
  };

  class EggEngineTextStream : public IEggEngine {
    EGG_NO_COPY(EggEngineTextStream);
  private:
    TextStream* stream;
    std::unique_ptr<EggEngineProgram> program;
  public:
    explicit EggEngineTextStream(TextStream& stream)
      : stream(&stream) {
    }
    virtual ~EggEngineTextStream() {
    }
    virtual Severity prepare(IEggEnginePreparationContext& preparation) {
      if (this->program != nullptr) {
        preparation.log(egg::lang::LogSource::Compiler, egg::lang::LogSeverity::Error, "Program prepared more than once");
      } else {
        captureExceptions(egg::lang::LogSource::Compiler, preparation, [this]{
          auto root = EggParserFactory::parseModule(*this->stream);
          this->program = std::make_unique<EggEngineProgram>(root);
        });
      }
      return preparation.getMaximumSeverity();
    }
    virtual Severity execute(IEggEngineExecutionContext& execution) {
      if (this->program == nullptr) {
        execution.log(egg::lang::LogSource::Runtime, egg::lang::LogSeverity::Error, "Program not prepared before execution");
      } else {
        this->program->execute(execution);
      }
      return execution.getMaximumSeverity();
    }
  };
}

std::shared_ptr<IEggEngineExecutionContext> egg::yolk::EggEngineFactory::createExecutionContext(const std::shared_ptr<IEggEngineLogger>& logger) {
  return std::make_shared<EggEngineExecutionContext>(logger);
}

std::shared_ptr<IEggEnginePreparationContext> egg::yolk::EggEngineFactory::createPreparationContext(const std::shared_ptr<IEggEngineLogger>& logger) {
  return std::make_shared<EggEnginePreparationContext>(logger);
}

std::shared_ptr<egg::yolk::IEggEngine> egg::yolk::EggEngineFactory::createEngineFromTextStream(TextStream& stream) {
  return std::make_shared<EggEngineTextStream>(stream);
}

std::shared_ptr<egg::yolk::IEggEngine> egg::yolk::EggEngineFactory::createEngineFromParsed(const std::shared_ptr<IEggParserNode>& root) {
  return std::make_shared<EggEngineParsed>(root);
}
