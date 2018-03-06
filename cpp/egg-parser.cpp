#include "yolk.h"

namespace {
  using namespace egg::yolk;

  class EggParser : public IEggParser {
  public:
    virtual ~EggParser() {
    }
    virtual std::shared_ptr<EggSyntaxNode> parse(IEggTokenizer&) override {
      return nullptr;
    }
  };
}

std::shared_ptr<egg::yolk::IEggParser> egg::yolk::EggParserFactory::create() {
  return std::make_shared<EggParser>();
}
