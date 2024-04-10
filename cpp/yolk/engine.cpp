#include "yolk/yolk.h"
#include "yolk/engine.h"
#include "ovum/file.h"
#include "ovum/stream.h"
#include "ovum/lexer.h"
#include "ovum/egg-tokenizer.h"
#include "ovum/egg-parser.h"
#include "ovum/egg-compiler.h"

#include <iostream>

using namespace egg::ovum;
using namespace egg::yolk;

namespace {
  class EngineScript : public HardReferenceCounted<IEngineScript> {
    EngineScript(const EngineScript&) = delete;
    EngineScript& operator=(const EngineScript&) = delete;
  private:
    std::shared_ptr<IEngine> engine;
    std::shared_ptr<ILexer> lexer;
  public:
    EngineScript(const std::shared_ptr<IEngine>& engine, const std::shared_ptr<ILexer>& lexer)
      : engine(engine),
        lexer(lexer) {
    }
    virtual HardValue run() override {
      auto program = this->build();
      if (program == nullptr) {
        auto& allocator = this->engine->getAllocator();
        return ValueFactory::createHardThrow(allocator, ValueFactory::createStringASCII(allocator, "Build failed"));
      }
      return this->execute(*program);
    }
  private:
    HardPtr<IVMProgram> build() {
      auto& vm = this->engine->getVM();
      auto& allocator = vm.getAllocator();
      auto tokenizer = EggTokenizerFactory::createFromLexer(allocator, lexer);
      auto parser = EggParserFactory::createFromTokenizer(allocator, tokenizer);
      auto builder = vm.createProgramBuilder();
      size_t index = 0;
      auto symbol = this->engine->getBuiltinSymbol(index);
      while (!symbol.empty()) {
        auto type = this->engine->getBuiltinInstance(symbol)->vmRuntimeType();
        builder->addBuiltin(symbol, type);
        symbol = this->engine->getBuiltinSymbol(++index);
      }
      auto compiler = EggCompilerFactory::createFromProgramBuilder(builder);
      auto module = compiler->compile(*parser);
      if (module == nullptr) {
        return nullptr;
      }
      return builder->build();
    }
    HardValue execute(IVMProgram& program) {
      auto runner = program.createRunner();
      size_t index = 0;
      auto symbol = this->engine->getBuiltinSymbol(index);
      while (!symbol.empty()) {
        auto instance = this->engine->getBuiltinInstance(symbol);
        runner->addBuiltin(symbol, this->engine->createHardValueObject(instance));
        symbol = this->engine->getBuiltinSymbol(++index);
      }
      return runner->run();
    }
  protected:
    virtual void hardDestroy() const override {
      this->engine->getAllocator().destroy(this);
    }
  };

  class EngineLogger : public ILogger {
  public:
    virtual void log(Source source, Severity severity, const String& message) override {
      std::string origin;
      switch (source) {
      case Source::Compiler:
        origin = "<COMPILER>";
        break;
      case Source::Runtime:
        origin = "<RUNTIME>";
        break;
      case Source::Command:
        origin = "<COMMAND>";
        break;
      case Source::User:
        break;
      }
      if (Bits::hasAnySet(severity, Bits::set(Severity::Warning, Severity::Error))) {
        std::cerr << origin << message.toUTF8() << std::endl;
      } else {
        std::cout << origin << message.toUTF8() << std::endl;
      }
    }
  };

  class EngineDefault : public IEngine, public std::enable_shared_from_this<EngineDefault> {
    EngineDefault(const EngineDefault&) = delete;
    EngineDefault& operator=(const EngineDefault&) = delete;
  private:
    const Options options;
    std::unique_ptr<IAllocator> uallocator = nullptr;
    EngineLogger ulogger;
    IAllocator* qallocator = nullptr;
    ILogger* qlogger = nullptr;
    HardPtr<IBasket> qbasket = nullptr;
    HardPtr<IVM> qvm = nullptr;
    std::map<String, HardObject> builtins;
    bool needStandardBuiltins;
  public:
    explicit EngineDefault(const Options& options = {})
      : options(options) {
      this->needStandardBuiltins = options.includeStandardBuiltins;
    }
    virtual String createStringUTF8(const void* utf8, size_t bytes, size_t codepoints) override {
      return String::fromUTF8(this->getAllocator(), utf8, bytes, codepoints);
    }
    virtual String createStringUTF32(const void* utf32, size_t codepoints) override {
      return String::fromUTF32(this->getAllocator(), utf32, codepoints);
    }
    virtual HardValue createHardValueVoid() override {
      return HardValue::Void;
    }
    virtual HardValue createHardValueNull() override {
      return HardValue::Null;
    }
    virtual HardValue createHardValueBool(Bool value) override {
      return ValueFactory::createBool(value);
    }
    virtual HardValue createHardValueInt(Int value) override {
      return ValueFactory::createInt(this->getAllocator(), value);
    }
    virtual HardValue createHardValueFloat(Float value) override {
      return ValueFactory::createFloat(this->getAllocator(), value);
    }
    virtual HardValue createHardValueString(const String& value) override {
      return ValueFactory::createString(this->getAllocator(), value);
    }
    virtual HardValue createHardValueObject(const HardObject& value) override {
      return ValueFactory::createHardObject(this->getAllocator(), value);
    }
    virtual HardValue createHardValueType(const Type& value) override {
      return ValueFactory::createType(this->getAllocator(), value);
    }
    IEngine& withAllocator(IAllocator& allocator) override {
      assert(this->qallocator == nullptr);
      this->qallocator = &allocator;
      return *this;
    }
    IEngine& withBasket(IBasket& basket) override {
      assert(this->qbasket == nullptr);
      this->qbasket.set(&basket);
      return *this;
    }
    IEngine& withLogger(ILogger& logger) override {
      assert(this->qlogger == nullptr);
      this->qlogger = &logger;
      return *this;
    }
    IEngine& withVM(IVM& vm) override {
      assert(this->qvm == nullptr);
      this->qvm.set(&vm);
      return *this;
    }
    IEngine& withBuiltin(const String& symbol, const HardObject& instance) override {
      this->builtins.emplace(symbol, instance);
      return *this;
    }
    virtual const Options& getOptions() override {
      return this->options;
    }
    virtual IAllocator& getAllocator() override {
      if (this->qallocator == nullptr) {
        this->uallocator = std::make_unique<AllocatorDefault>();
        this->withAllocator(*this->uallocator);
      }
      return *this->qallocator;
    }
    virtual IBasket& getBasket() override {
      if (this->qbasket == nullptr) {
        this->withBasket(*BasketFactory::createBasket(this->getAllocator()));
      }
      return *this->qbasket;
    }
    virtual ILogger& getLogger() override {
      if (this->qlogger == nullptr) {
        this->withLogger(this->ulogger);
      }
      return *this->qlogger;
    }
    virtual IVM& getVM() override {
      if (this->qvm == nullptr) {
        this->withVM(*VMFactory::createDefault(this->getAllocator(), this->getLogger()));
      }
      return *this->qvm;
    }
    virtual String getBuiltinSymbol(size_t index) override {
      auto& ensured = this->ensureBuiltins();
      if (index < ensured.size()) {
        auto iterator = ensured.begin();
        std::advance(iterator, index);
        return iterator->first;
      }
      return {};
    }
    virtual HardObject getBuiltinInstance(const String& symbol) override {
      auto& ensured = this->ensureBuiltins();
      auto found = ensured.find(symbol);
      if (found != ensured.end()) {
        return found->second;
      }
      return HardObject();
    }
    virtual HardPtr<IEngineScript> loadScriptFromString(const String& script, const String& resource) override {
      auto lexer = LexerFactory::createFromString(script.toUTF8(), resource.toUTF8());
      return HardPtr(this->getAllocator().makeRaw<EngineScript>(this->shared_from_this(), lexer));
    }
    virtual HardPtr<IEngineScript> loadScriptFromTextStream(TextStream& stream) override {
      auto lexer = LexerFactory::createFromTextStream(stream);
      return HardPtr(this->getAllocator().makeRaw<EngineScript>(this->shared_from_this(), lexer));
    }
    virtual HardPtr<IEngineScript> loadScriptFromEggbox(IEggbox& eggbox, const String& subpath) override {
      EggboxTextStream stream{ eggbox, subpath.toUTF8() };
      auto lexer = LexerFactory::createFromTextStream(stream);
      return HardPtr(this->getAllocator().makeRaw<EngineScript>(this->shared_from_this(), lexer));
    }
  private:
    std::map<String, HardObject>& ensureBuiltins() {
      if (this->needStandardBuiltins) {
        auto& vm = this->getVM();
        auto& allocator = this->getAllocator();
        this->withBuiltin(String::fromUTF8(allocator, "assert"), ObjectFactory::createBuiltinAssert(vm));
        this->withBuiltin(String::fromUTF8(allocator, "print"), ObjectFactory::createBuiltinPrint(vm));
        this->needStandardBuiltins = false;
      }
      return this->builtins;
    }
  };
}

std::shared_ptr<egg::yolk::IEngine> egg::yolk::EngineFactory::createDefault() {
  return std::make_shared<EngineDefault>();
}

std::shared_ptr<egg::yolk::IEngine> egg::yolk::EngineFactory::createWithOptions(const IEngine::Options& options) {
  return std::make_shared<EngineDefault>(options);
}
