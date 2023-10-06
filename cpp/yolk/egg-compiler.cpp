#include "yolk/yolk.h"
#include "yolk/lexers.h"
#include "yolk/egg-tokenizer.h"
#include "yolk/egg-compiler.h"

using namespace egg::yolk;

namespace {
  class EggCompiler : public IEggCompiler {
    EggCompiler(const EggCompiler&) = delete;
    EggCompiler& operator=(const EggCompiler&) = delete;
  private:
    egg::ovum::IVM& vm;
    std::shared_ptr<IEggTokenizer> tokenizer;
  public:
    EggCompiler(egg::ovum::IVM& vm, const std::shared_ptr<IEggTokenizer>& tokenizer)
      : vm(vm),
        tokenizer(tokenizer) {
      assert(this->tokenizer != nullptr);
    }
    bool compile() override {
      return true; // WIBBLE
    }
    egg::ovum::String resource() const override {
      return this->tokenizer->resource();
    }
  };
}

std::shared_ptr<IEggCompiler> EggCompilerFactory::createFromTokenizer(egg::ovum::IVM& vm, const std::shared_ptr<IEggTokenizer>& tokenizer) {
  return std::make_shared<EggCompiler>(vm, tokenizer);
}
