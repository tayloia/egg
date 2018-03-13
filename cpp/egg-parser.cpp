#include "yolk.h"
#include "lexers.h"
#include "egg-tokenizer.h"
#include "egg-syntax.h"
#include "egg-parser.h"

namespace {
  using namespace egg::yolk;

  const EggParserType typeVoid(EggParserType::Simple::Void);

  class EggParserNode_Module : public IEggParserNode {
    EGG_NO_COPY(EggParserNode_Module);
  public:
    EggParserNode_Module() {
    }
    virtual const EggParserType& getType() const override {
      return typeVoid;
    }
    virtual void dump(std::ostream & os) const override {
      os << "(module)";
    }
  };

  class EggParserContext : public IEggParserContext {
  public:
    virtual bool isAllowed(EggParserAllowed) override {
      return false; // TODO
    }
  };

  class EggParserModule : public IEggParser {
  public:
    virtual std::shared_ptr<IEggParserNode> parse(IEggTokenizer& tokenizer) override {
      auto syntax = EggParserFactory::createModuleSyntaxParser();
      auto ast = syntax->parse(tokenizer);
      EggParserContext context;
      return ast->promote(context);
    }
  };
}

std::shared_ptr<egg::yolk::IEggParserNode> egg::yolk::EggSyntaxNode_Empty::promote(egg::yolk::IEggParserContext&) {
  EGG_THROW(__FUNCTION__ " TODO"); // TODO
}

std::shared_ptr<egg::yolk::IEggParserNode> egg::yolk::EggSyntaxNode_Module::promote(egg::yolk::IEggParserContext&) {
  return std::make_shared<EggParserNode_Module>();
}

std::shared_ptr<egg::yolk::IEggParserNode> egg::yolk::EggSyntaxNode_Block::promote(egg::yolk::IEggParserContext&) {
  EGG_THROW(__FUNCTION__ " TODO"); // TODO
}

std::shared_ptr<egg::yolk::IEggParserNode> egg::yolk::EggSyntaxNode_Type::promote(egg::yolk::IEggParserContext&) {
  EGG_THROW(__FUNCTION__ " TODO"); // TODO
}

std::shared_ptr<egg::yolk::IEggParserNode> egg::yolk::EggSyntaxNode_VariableDeclaration::promote(egg::yolk::IEggParserContext&) {
  EGG_THROW(__FUNCTION__ " TODO"); // TODO
}

std::shared_ptr<egg::yolk::IEggParserNode> egg::yolk::EggSyntaxNode_VariableInitialization::promote(egg::yolk::IEggParserContext&) {
  EGG_THROW(__FUNCTION__ " TODO"); // TODO
}

std::shared_ptr<egg::yolk::IEggParserNode> egg::yolk::EggSyntaxNode_Assignment::promote(egg::yolk::IEggParserContext&) {
  EGG_THROW(__FUNCTION__ " TODO"); // TODO
}

std::shared_ptr<egg::yolk::IEggParserNode> egg::yolk::EggSyntaxNode_Mutate::promote(egg::yolk::IEggParserContext&) {
  EGG_THROW(__FUNCTION__ " TODO"); // TODO
}

std::shared_ptr<egg::yolk::IEggParserNode> egg::yolk::EggSyntaxNode_Break::promote(egg::yolk::IEggParserContext&) {
  EGG_THROW(__FUNCTION__ " TODO"); // TODO
}

std::shared_ptr<egg::yolk::IEggParserNode> egg::yolk::EggSyntaxNode_Case::promote(egg::yolk::IEggParserContext&) {
  EGG_THROW(__FUNCTION__ " TODO"); // TODO
}

std::shared_ptr<egg::yolk::IEggParserNode> egg::yolk::EggSyntaxNode_Catch::promote(egg::yolk::IEggParserContext&) {
  EGG_THROW(__FUNCTION__ " TODO"); // TODO
}

std::shared_ptr<egg::yolk::IEggParserNode> egg::yolk::EggSyntaxNode_Continue::promote(egg::yolk::IEggParserContext&) {
  EGG_THROW(__FUNCTION__ " TODO"); // TODO
}

std::shared_ptr<egg::yolk::IEggParserNode> egg::yolk::EggSyntaxNode_Default::promote(egg::yolk::IEggParserContext&) {
  EGG_THROW(__FUNCTION__ " TODO"); // TODO
}

std::shared_ptr<egg::yolk::IEggParserNode> egg::yolk::EggSyntaxNode_Do::promote(egg::yolk::IEggParserContext&) {
  EGG_THROW(__FUNCTION__ " TODO"); // TODO
}

std::shared_ptr<egg::yolk::IEggParserNode> egg::yolk::EggSyntaxNode_If::promote(egg::yolk::IEggParserContext&) {
  EGG_THROW(__FUNCTION__ " TODO"); // TODO
}

std::shared_ptr<egg::yolk::IEggParserNode> egg::yolk::EggSyntaxNode_Finally::promote(egg::yolk::IEggParserContext&) {
  EGG_THROW(__FUNCTION__ " TODO"); // TODO
}

std::shared_ptr<egg::yolk::IEggParserNode> egg::yolk::EggSyntaxNode_For::promote(egg::yolk::IEggParserContext&) {
  EGG_THROW(__FUNCTION__ " TODO"); // TODO
}

std::shared_ptr<egg::yolk::IEggParserNode> egg::yolk::EggSyntaxNode_Foreach::promote(egg::yolk::IEggParserContext&) {
  EGG_THROW(__FUNCTION__ " TODO"); // TODO
}

std::shared_ptr<egg::yolk::IEggParserNode> egg::yolk::EggSyntaxNode_Return::promote(egg::yolk::IEggParserContext&) {
  EGG_THROW(__FUNCTION__ " TODO"); // TODO
}

std::shared_ptr<egg::yolk::IEggParserNode> egg::yolk::EggSyntaxNode_Switch::promote(egg::yolk::IEggParserContext&) {
  EGG_THROW(__FUNCTION__ " TODO"); // TODO
}

std::shared_ptr<egg::yolk::IEggParserNode> egg::yolk::EggSyntaxNode_Throw::promote(egg::yolk::IEggParserContext&) {
  EGG_THROW(__FUNCTION__ " TODO"); // TODO
}

std::shared_ptr<egg::yolk::IEggParserNode> egg::yolk::EggSyntaxNode_Try::promote(egg::yolk::IEggParserContext&) {
  EGG_THROW(__FUNCTION__ " TODO"); // TODO
}

std::shared_ptr<egg::yolk::IEggParserNode> egg::yolk::EggSyntaxNode_Using::promote(egg::yolk::IEggParserContext&) {
  EGG_THROW(__FUNCTION__ " TODO"); // TODO
}

std::shared_ptr<egg::yolk::IEggParserNode> egg::yolk::EggSyntaxNode_While::promote(egg::yolk::IEggParserContext&) {
  EGG_THROW(__FUNCTION__ " TODO"); // TODO
}

std::shared_ptr<egg::yolk::IEggParserNode> egg::yolk::EggSyntaxNode_Yield::promote(egg::yolk::IEggParserContext&) {
  EGG_THROW(__FUNCTION__ " TODO"); // TODO
}

std::shared_ptr<egg::yolk::IEggParserNode> egg::yolk::EggSyntaxNode_UnaryOperator::promote(egg::yolk::IEggParserContext&) {
  EGG_THROW(__FUNCTION__ " TODO"); // TODO
}

std::shared_ptr<egg::yolk::IEggParserNode> egg::yolk::EggSyntaxNode_BinaryOperator::promote(egg::yolk::IEggParserContext&) {
  EGG_THROW(__FUNCTION__ " TODO"); // TODO
}

std::shared_ptr<egg::yolk::IEggParserNode> egg::yolk::EggSyntaxNode_TernaryOperator::promote(egg::yolk::IEggParserContext&) {
  EGG_THROW(__FUNCTION__ " TODO"); // TODO
}

std::shared_ptr<egg::yolk::IEggParserNode> egg::yolk::EggSyntaxNode_Call::promote(egg::yolk::IEggParserContext&) {
  EGG_THROW(__FUNCTION__ " TODO"); // TODO
}

std::shared_ptr<egg::yolk::IEggParserNode> egg::yolk::EggSyntaxNode_Named::promote(egg::yolk::IEggParserContext&) {
  EGG_THROW(__FUNCTION__ " TODO"); // TODO
}

std::shared_ptr<egg::yolk::IEggParserNode> egg::yolk::EggSyntaxNode_Identifier::promote(egg::yolk::IEggParserContext&) {
  EGG_THROW(__FUNCTION__ " TODO"); // TODO
}

std::shared_ptr<egg::yolk::IEggParserNode> egg::yolk::EggSyntaxNode_Literal::promote(egg::yolk::IEggParserContext&) {
  EGG_THROW(__FUNCTION__ " TODO"); // TODO
}

std::shared_ptr<egg::yolk::IEggParser> egg::yolk::EggParserFactory::createModuleParser() {
  return std::make_shared<EggParserModule>();
}
