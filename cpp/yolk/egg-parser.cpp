#include "yolk/yolk.h"
#include "yolk/lexers.h"
#include "yolk/egg-tokenizer.h"
#include "yolk/egg-parser.h"

using namespace egg::yolk;

namespace {
  class EggParser : public IEggParser {
    EggParser(const EggParser&) = delete;
    EggParser& operator=(const EggParser&) = delete;
  private:
    egg::ovum::IAllocator& allocator;
    std::shared_ptr<IEggTokenizer> tokenizer;
    std::vector<Issue> issues;
  public:
    EggParser(egg::ovum::IAllocator& allocator, const std::shared_ptr<IEggTokenizer>& tokenizer)
      : allocator(allocator),
        tokenizer(tokenizer) {
      assert(this->tokenizer != nullptr);
    }
    Result parse() override {
      // WIBBLE
      assert(this->issues.empty());
      auto root = std::make_shared<Node>();
      root->kind = Node::Kind::ModuleRoot;
      return{ std::move(root), std::move(this->issues) };
    }
    egg::ovum::String resource() const override {
      return this->tokenizer->resource();
    }
  private:

  };
}

std::shared_ptr<IEggParser> EggParserFactory::createFromTokenizer(egg::ovum::IAllocator& allocator, const std::shared_ptr<IEggTokenizer>& tokenizer) {
  return std::make_shared<EggParser>(allocator, tokenizer);
}
