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

  class EggEngineContext : public IEggEngineContext {
    EGG_NO_COPY(EggEngineContext);
  private:
    egg::ovum::IAllocator& allocator;
    std::shared_ptr<egg::ovum::ILogger> logger;
  public:
    EggEngineContext(egg::ovum::IAllocator& allocator, const std::shared_ptr<egg::ovum::ILogger>& logger)
      : allocator(allocator), logger(logger) {
      assert(logger != nullptr);
    }
    virtual void log(egg::ovum::ILogger::Source source, egg::ovum::ILogger::Severity severity, const std::string& message) override {
      this->logger->log(source, severity, message);
    }
    virtual egg::ovum::IAllocator& getAllocator() const override {
      return this->allocator;
    }
  };

  class EggEngineParsed : public IEggEngine {
    EGG_NO_COPY(EggEngineParsed);
  private:
    EggProgram program;
    bool prepared;
  public:
    EggEngineParsed(egg::ovum::IAllocator& allocator, const egg::ovum::String& resource, const std::shared_ptr<IEggProgramNode>& root)
      : program(allocator, resource, root), prepared(false) {
    }
    virtual egg::ovum::ILogger::Severity prepare(IEggEngineContext& context) override {
      if (this->prepared) {
        context.log(egg::ovum::ILogger::Source::Compiler, egg::ovum::ILogger::Severity::Error, "Program prepared more than once");
        return egg::ovum::ILogger::Severity::Error;
      }
      this->prepared = true;
      return this->program.prepare(context);
    }
    virtual egg::ovum::ILogger::Severity execute(IEggEngineContext& context, const egg::ovum::Module& module) override {
      return this->program.execute(context, module);
    }
    virtual egg::ovum::ILogger::Severity compile(IEggEngineContext& context, egg::ovum::Module& out) override {
      if (!this->prepared) {
        context.log(egg::ovum::ILogger::Source::Runtime, egg::ovum::ILogger::Severity::Error, "Program not prepared before compilation");
        return egg::ovum::ILogger::Severity::Error;
      }
      return this->program.compile(context, out);
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
    virtual egg::ovum::ILogger::Severity prepare(IEggEngineContext& context) override {
      if (this->program != nullptr) {
        context.log(egg::ovum::ILogger::Source::Compiler, egg::ovum::ILogger::Severity::Error, "Program prepared more than once");
        return egg::ovum::ILogger::Severity::Error;
      }
      return captureExceptions(egg::ovum::ILogger::Source::Compiler, context, [this, &context]{
        auto& allocator = context.getAllocator();
        auto root = EggParserFactory::parseModule(allocator, *this->stream);
        this->program = std::make_unique<EggProgram>(allocator, this->stream->getResourceName(), root);
        return this->program->prepare(context);
      });
    }
    virtual egg::ovum::ILogger::Severity compile(IEggEngineContext& context, egg::ovum::Module& out) override {
      if (this->program == nullptr) {
        context.log(egg::ovum::ILogger::Source::Runtime, egg::ovum::ILogger::Severity::Error, "Program not prepared before compilation");
        return egg::ovum::ILogger::Severity::Error;
      }
      return this->program->compile(context, out);
    }
    virtual egg::ovum::ILogger::Severity execute(IEggEngineContext& context, const egg::ovum::Module& module) override {
      if (this->program == nullptr) {
        context.log(egg::ovum::ILogger::Source::Runtime, egg::ovum::ILogger::Severity::Error, "Program not prepared before execution");
        return egg::ovum::ILogger::Severity::Error;
      }
      return this->program->execute(context, module);
    }
  };
}

egg::ovum::ILogger::Severity egg::yolk::IEggEngine::execute(IEggEngineContext& context) {
  // Helper for compile-and-execute
  egg::ovum::Module module;
  auto compilation = this->compile(context, module);
  if (compilation != egg::ovum::ILogger::Severity::Error) {
    assert(module != nullptr);
    auto execution = this->execute(context, module);
    return std::max(compilation, execution);
  }
  return compilation;
}

egg::ovum::ILogger::Severity egg::yolk::IEggEngine::run(IEggEngineContext& context) {
  // Helper for prepare-compile-and-execute
  auto preparation = this->prepare(context);
  if (preparation != egg::ovum::ILogger::Severity::Error) {
    auto execution = this->execute(context);
    return std::max(preparation, execution);
  }
  return preparation;
}

std::shared_ptr<IEggEngineContext> egg::yolk::EggEngineFactory::createContext(egg::ovum::IAllocator& allocator, const std::shared_ptr<egg::ovum::ILogger>& logger) {
  return std::make_shared<EggEngineContext>(allocator, logger);
}

std::shared_ptr<egg::yolk::IEggEngine> egg::yolk::EggEngineFactory::createEngineFromParsed(egg::ovum::IAllocator& allocator, const egg::ovum::String& resource, const std::shared_ptr<IEggProgramNode>& root) {
  return std::make_shared<EggEngineParsed>(allocator, resource, root);
}

std::shared_ptr<egg::yolk::IEggEngine> egg::yolk::EggEngineFactory::createEngineFromTextStream(TextStream& stream) {
  return std::make_shared<EggEngineTextStream>(stream);
}
