#include "yolk/yolk.h"
#include "yolk/lexers.h"
#include "yolk/egg-tokenizer.h"
#include "yolk/egg-syntax.h"
#include "yolk/egg-parser.h"
#include "yolk/egg-engine.h"
#include "yolk/egg-program.h"

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

  class EggEngineContext : public IEggEnginePreparationContext, public IEggEngineExecutionContext {
    EGG_NO_COPY(EggEngineContext);
  private:
    egg::ovum::IAllocator& mallocator;
    std::shared_ptr<IEggEngineLogger> logger;
  public:
    EggEngineContext(egg::ovum::IAllocator& allocator, const std::shared_ptr<IEggEngineLogger>& logger)
      : mallocator(allocator), logger(logger) {
      assert(logger != nullptr);
    }
    virtual void log(egg::lang::LogSource source, egg::lang::LogSeverity severity, const std::string& message) override {
      this->logger->log(source, severity, message);
    }
    virtual egg::ovum::IAllocator& allocator() const override {
      return this->mallocator;
    }
  };

  class EggEngineParsed : public IEggEngine {
    EGG_NO_COPY(EggEngineParsed);
  private:
    EggProgram program;
    bool prepared;
  public:
    EggEngineParsed(egg::ovum::IAllocator& allocator, const std::shared_ptr<IEggProgramNode>& root)
      : program(allocator, root), prepared(false) {
    }
    virtual egg::lang::LogSeverity prepare(IEggEnginePreparationContext& preparation) override {
      if (this->prepared) {
        preparation.log(egg::lang::LogSource::Compiler, egg::lang::LogSeverity::Error, "Program prepared more than once");
        return egg::lang::LogSeverity::Error;
      }
      this->prepared = true;
      return this->program.prepare(preparation);
    }
    virtual egg::lang::LogSeverity execute(IEggEngineExecutionContext& execution) override {
      if (!this->prepared) {
        execution.log(egg::lang::LogSource::Runtime, egg::lang::LogSeverity::Error, "Program not prepared before execution");
        return egg::lang::LogSeverity::Error;
      }
      return this->program.execute(execution);
    }
  };

  class EggEngineTextStream : public IEggEngine {
    EGG_NO_COPY(EggEngineTextStream);
  private:
    TextStream* stream;
    std::unique_ptr<EggProgram> program;
  public:
    explicit EggEngineTextStream(TextStream& stream)
      : stream(&stream) {
    }
    virtual egg::lang::LogSeverity prepare(IEggEnginePreparationContext& preparation) override {
      if (this->program != nullptr) {
        preparation.log(egg::lang::LogSource::Compiler, egg::lang::LogSeverity::Error, "Program prepared more than once");
        return egg::lang::LogSeverity::Error;
      }
      return captureExceptions(egg::lang::LogSource::Compiler, preparation, [this, &preparation]{
        auto& allocator = preparation.allocator();
        auto root = EggParserFactory::parseModule(allocator, *this->stream);
        this->program = std::make_unique<EggProgram>(allocator, root);
        return this->program->prepare(preparation);
      });
    }
    virtual egg::lang::LogSeverity execute(IEggEngineExecutionContext& execution) override {
      if (this->program == nullptr) {
        execution.log(egg::lang::LogSource::Runtime, egg::lang::LogSeverity::Error, "Program not prepared before execution");
        return egg::lang::LogSeverity::Error;
      }
      return this->program->execute(execution);
    }
  };
}

std::shared_ptr<IEggEngineExecutionContext> egg::yolk::EggEngineFactory::createExecutionContext(egg::ovum::IAllocator& allocator, const std::shared_ptr<IEggEngineLogger>& logger) {
  return std::make_shared<EggEngineContext>(allocator, logger);
}

std::shared_ptr<IEggEnginePreparationContext> egg::yolk::EggEngineFactory::createPreparationContext(egg::ovum::IAllocator& allocator, const std::shared_ptr<IEggEngineLogger>& logger) {
  return std::make_shared<EggEngineContext>(allocator, logger);
}

std::shared_ptr<egg::yolk::IEggEngine> egg::yolk::EggEngineFactory::createEngineFromParsed(egg::ovum::IAllocator& allocator, const std::shared_ptr<IEggProgramNode>& root) {
  return std::make_shared<EggEngineParsed>(allocator, root);
}

std::shared_ptr<egg::yolk::IEggEngine> egg::yolk::EggEngineFactory::createEngineFromTextStream(TextStream& stream) {
  return std::make_shared<EggEngineTextStream>(stream);
}
