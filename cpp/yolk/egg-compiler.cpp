#include "yolk/yolk.h"
#include "yolk/lexers.h"
#include "yolk/egg-tokenizer.h"
#include "yolk/egg-parser.h"
#include "yolk/egg-compiler.h"

using namespace egg::yolk;

namespace {
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
    bool compile() override {
      // WIBBLE
      return true;
    }
    egg::ovum::String resource() const override {
      return this->parser->resource();
    }
  };
}

std::shared_ptr<IEggCompiler> EggCompilerFactory::createFromParser(egg::ovum::IVM& vm, const std::shared_ptr<IEggParser>& parser) {
  return std::make_shared<EggCompiler>(vm, parser);
}
