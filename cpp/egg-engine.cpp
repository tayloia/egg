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
  egg::lang::LogSeverity captureExceptions(egg::lang::LogSource source, IEggEngineLogger& logger, ACTION action) {
    try {
      return action();
    } catch (const Exception& ex) {
      // We have an opportunity to extract more information in the future
      logger.log(source, egg::lang::LogSeverity::Error, ex.what());
    } catch (const std::exception& ex) {
      logger.log(source, egg::lang::LogSeverity::Error, ex.what());
    }
    return egg::lang::LogSeverity::Error;
  }

  template<class BASE>
  class EggEngineBaseContext : public BASE {
  protected:
    std::shared_ptr<IEggEngineLogger> logger;
  public:
    explicit EggEngineBaseContext(const std::shared_ptr<IEggEngineLogger>& logger)
      : logger(logger) {
      assert(logger != nullptr);
    }
    virtual ~EggEngineBaseContext() {
    }
    virtual void log(egg::lang::LogSource source, egg::lang::LogSeverity severity, const std::string& message) {
      this->logger->log(source, severity, message);
    }
  };

  class EggEnginePreparationContext : public EggEngineBaseContext<IEggEnginePreparationContext> {
  private:
    std::shared_ptr<IEggEngineLogger> logger;
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
      this->log(egg::lang::LogSource::User, egg::lang::LogSeverity::Information, text);
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
    virtual egg::lang::LogSeverity prepare(IEggEnginePreparationContext& preparation) {
      preparation.log(egg::lang::LogSource::Compiler, egg::lang::LogSeverity::Error, "Unnecessary program preparation");
      return egg::lang::LogSeverity::Error;
    }
    virtual egg::lang::LogSeverity execute(IEggEngineExecutionContext& execution) {
      EggEngineProgramContext context(execution);
      this->program.execute(context);
      return context.getMaximumSeverity();
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
    virtual egg::lang::LogSeverity prepare(IEggEnginePreparationContext& preparation) {
      if (this->program != nullptr) {
        preparation.log(egg::lang::LogSource::Compiler, egg::lang::LogSeverity::Error, "Program prepared more than once");
        return egg::lang::LogSeverity::Error;
      }
      return captureExceptions(egg::lang::LogSource::Compiler, preparation, [this]{
        auto root = EggParserFactory::parseModule(*this->stream);
        this->program = std::make_unique<EggEngineProgram>(root);
        return egg::lang::LogSeverity::None;
      });
    }
    virtual egg::lang::LogSeverity execute(IEggEngineExecutionContext& execution) {
      if (this->program == nullptr) {
        execution.log(egg::lang::LogSource::Runtime, egg::lang::LogSeverity::Error, "Program not prepared before execution");
        return egg::lang::LogSeverity::Error;
      }
      EggEngineProgramContext context(execution);
      this->program->execute(context);
      return context.getMaximumSeverity();
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
