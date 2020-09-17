#include "yolk/yolk.h"
#include "yolk/lexers.h"
#include "yolk/egg-tokenizer.h"
#include "yolk/egg-syntax.h"
#include "yolk/egg-parser.h"
#include "yolk/egg-engine.h"

namespace {
  using namespace egg::yolk;

  template<typename ACTION>
  egg::ovum::ILogger::Severity captureExceptions(egg::ovum::ILogger::Source source, egg::ovum::ILogger& logger, ACTION action) {
    try {
      return action();
    } catch (const Exception& ex) {
      // We have an opportunity to extract more information in the future
      logger.log(source, egg::ovum::ILogger::Severity::Error, ex.what());
    } catch (const std::exception& ex) {
      logger.log(source, egg::ovum::ILogger::Severity::Error, ex.what());
    }
    return egg::ovum::ILogger::Severity::Error;
  }

  class EggEngineContext : public IEggEnginePreparationContext, public IEggEngineExecutionContext, public IEggEngineCompilationContext {
    EGG_NO_COPY(EggEngineContext);
  private:
    egg::ovum::IAllocator& mallocator;
    std::shared_ptr<egg::ovum::ILogger> logger;
  public:
    EggEngineContext(egg::ovum::IAllocator& allocator, const std::shared_ptr<egg::ovum::ILogger>& logger)
      : mallocator(allocator), logger(logger) {
      assert(logger != nullptr);
    }
    virtual void log(egg::ovum::ILogger::Source source, egg::ovum::ILogger::Severity severity, const std::string& message) override {
      this->logger->log(source, severity, message);
    }
    virtual egg::ovum::IAllocator& allocator() const override {
      return this->mallocator;
    }
  };

  class EggEngineProgram {
    EGG_NO_COPY(EggEngineProgram);
  private:
    egg::ovum::Basket basket;
    egg::ovum::String resource;
    std::shared_ptr<IEggProgramNode> root;
  public:
    EggEngineProgram(egg::ovum::IAllocator& allocator, const egg::ovum::String& resource, const std::shared_ptr<IEggProgramNode>& root)
      : basket(egg::ovum::BasketFactory::createBasket(allocator)),
        resource(resource),
        root(root) {
      assert(root != nullptr);
    }
    ~EggEngineProgram() {
      this->root.reset();
      (void)this->basket->collect();
      // The destructor for 'basket' will assert if this collection doesn't free up everything in the basket
    }
    egg::ovum::ILogger::Severity prepare(IEggEnginePreparationContext& preparation) {
      preparation.log(egg::ovum::ILogger::Source::Compiler, egg::ovum::ILogger::Severity::Error, "WIBBLE: EggEngineProgram::prepare not yet implemented");
      return egg::ovum::ILogger::Severity::Error;
    }
    egg::ovum::ILogger::Severity execute(IEggEngineExecutionContext& execution) {
      execution.log(egg::ovum::ILogger::Source::Compiler, egg::ovum::ILogger::Severity::Error, "WIBBLE: EggEngineProgram::execute not yet implemented");
      return egg::ovum::ILogger::Severity::Error;
    }
    egg::ovum::ILogger::Severity compile(IEggEngineCompilationContext& compilation, egg::ovum::Module&) {
      compilation.log(egg::ovum::ILogger::Source::Compiler, egg::ovum::ILogger::Severity::Error, "WIBBLE: EggEngineProgram::compile not yet implemented");
      return egg::ovum::ILogger::Severity::Error;
    }
  };

  class EggEngineParsed : public IEggEngine {
    EGG_NO_COPY(EggEngineParsed);
  private:
    EggEngineProgram program;
    bool prepared;
  public:
    EggEngineParsed(egg::ovum::IAllocator& allocator, const egg::ovum::String& resource, const std::shared_ptr<IEggProgramNode>& root)
      : program(allocator, resource, root), prepared(false) {
    }
    virtual egg::ovum::ILogger::Severity prepare(IEggEnginePreparationContext& preparation) override {
      if (this->prepared) {
        preparation.log(egg::ovum::ILogger::Source::Compiler, egg::ovum::ILogger::Severity::Error, "Program prepared more than once");
        return egg::ovum::ILogger::Severity::Error;
      }
      this->prepared = true;
      return this->program.prepare(preparation);
    }
    virtual egg::ovum::ILogger::Severity execute(IEggEngineExecutionContext& execution) override {
      if (!this->prepared) {
        execution.log(egg::ovum::ILogger::Source::Runtime, egg::ovum::ILogger::Severity::Error, "Program not prepared before execution");
        return egg::ovum::ILogger::Severity::Error;
      }
      return this->program.execute(execution);
    }
    virtual egg::ovum::ILogger::Severity compile(IEggEngineCompilationContext& compilation, egg::ovum::Module& out) override {
      if (!this->prepared) {
        compilation.log(egg::ovum::ILogger::Source::Runtime, egg::ovum::ILogger::Severity::Error, "Program not prepared before compilation");
        return egg::ovum::ILogger::Severity::Error;
      }
      return this->program.compile(compilation, out);
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
    virtual egg::ovum::ILogger::Severity prepare(IEggEnginePreparationContext& preparation) override {
      if (this->program != nullptr) {
        preparation.log(egg::ovum::ILogger::Source::Compiler, egg::ovum::ILogger::Severity::Error, "Program prepared more than once");
        return egg::ovum::ILogger::Severity::Error;
      }
      return captureExceptions(egg::ovum::ILogger::Source::Compiler, preparation, [this, &preparation]{
        auto& allocator = preparation.allocator();
        auto root = EggParserFactory::parseModule(allocator, *this->stream);
        this->program = std::make_unique<EggEngineProgram>(allocator, this->stream->getResourceName(), root);
        return this->program->prepare(preparation);
      });
    }
    virtual egg::ovum::ILogger::Severity execute(IEggEngineExecutionContext& execution) override {
      if (this->program == nullptr) {
        execution.log(egg::ovum::ILogger::Source::Runtime, egg::ovum::ILogger::Severity::Error, "Program not prepared before execution");
        return egg::ovum::ILogger::Severity::Error;
      }
      return this->program->execute(execution);
    }
    virtual egg::ovum::ILogger::Severity compile(IEggEngineCompilationContext& compilation, egg::ovum::Module& out) override {
      if (this->program == nullptr) {
        compilation.log(egg::ovum::ILogger::Source::Runtime, egg::ovum::ILogger::Severity::Error, "Program not prepared before compilation");
        return egg::ovum::ILogger::Severity::Error;
      }
      return this->program->compile(compilation, out);
    }
  };
}

std::shared_ptr<IEggEnginePreparationContext> egg::yolk::EggEngineFactory::createPreparationContext(egg::ovum::IAllocator& allocator, const std::shared_ptr<egg::ovum::ILogger>& logger) {
  return std::make_shared<EggEngineContext>(allocator, logger);
}

std::shared_ptr<IEggEngineExecutionContext> egg::yolk::EggEngineFactory::createExecutionContext(egg::ovum::IAllocator& allocator, const std::shared_ptr<egg::ovum::ILogger>& logger) {
  return std::make_shared<EggEngineContext>(allocator, logger);
}

std::shared_ptr<IEggEngineCompilationContext> egg::yolk::EggEngineFactory::createCompilationContext(egg::ovum::IAllocator& allocator, const std::shared_ptr<egg::ovum::ILogger>& logger) {
  return std::make_shared<EggEngineContext>(allocator, logger);
}

std::shared_ptr<egg::yolk::IEggEngine> egg::yolk::EggEngineFactory::createEngineFromParsed(egg::ovum::IAllocator& allocator, const egg::ovum::String& resource, const std::shared_ptr<IEggProgramNode>& root) {
  return std::make_shared<EggEngineParsed>(allocator, resource, root);
}

std::shared_ptr<egg::yolk::IEggEngine> egg::yolk::EggEngineFactory::createEngineFromTextStream(TextStream& stream) {
  return std::make_shared<EggEngineTextStream>(stream);
}
