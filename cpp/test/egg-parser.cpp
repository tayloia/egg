#include "test.h"

using namespace egg::yolk;

namespace {
  std::shared_ptr<IEggSyntaxNode> parseFromString(const std::string& text) {
    auto lexer = LexerFactory::createFromString(text);
    auto tokenizer = EggTokenizerFactory::createFromLexer(lexer);
    auto parser = EggParserFactory::create(EggSyntaxNodeKind::Module);
    return parser->parse(*tokenizer);
  }
  std::string printToString(IEggSyntaxNode& tree) {
    std::ostringstream oss;
    EggSyntaxNode::printToStream(oss, tree);
    return oss.str();
  }
}

TEST(TestEggParser, Empty) {
  auto root = parseFromString("");
  ASSERT_EQ(EggSyntaxNodeKind::Module, root->getKind());
  ASSERT_EQ(0, root->getChildren());
  ASSERT_EQ("(module)", printToString(*root));
}

TEST(TestEggParser, ExampleFile) {
  auto lexer = LexerFactory::createFromPath("~/cpp/test/data/example.egg");
  auto tokenizer = EggTokenizerFactory::createFromLexer(lexer);
  auto parser = EggParserFactory::create(EggSyntaxNodeKind::Module);
  auto root = parser->parse(*tokenizer);
}
