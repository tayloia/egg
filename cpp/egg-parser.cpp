#include "yolk.h"
#include "lexers.h"
#include "egg-tokenizer.h"
#include "egg-syntax.h"
#include "egg-parser.h"

namespace {
  using namespace egg::yolk;

  class EggParserNode_Module : public IEggParserNode {
    virtual void dump(std::ostream & os) const override {
      os << "(module)";
    }
  };

  class EggParser : public IEggParser {
  public:
    virtual std::shared_ptr<IEggParserNode> parse(IEggTokenizer& tokenizer) override {
      auto syntax = EggParserFactory::createModuleSyntaxParser();
      auto ast = syntax->parse(tokenizer);
      return ast->promote();
    }
  };
}

std::shared_ptr<egg::yolk::IEggParser> egg::yolk::EggParserFactory::createModuleParser() {
  return std::make_shared<EggParser>();
}

std::unique_ptr<egg::yolk::IEggParserNode> egg::yolk::IEggSyntaxNode::promote() {
  return std::make_unique<EggParserNode_Module>();
}
