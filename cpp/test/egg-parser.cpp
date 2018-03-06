#include "test.h"

using namespace egg::yolk;

namespace {
  std::shared_ptr<EggSyntaxNode> parseFromString(const std::string& text) {
    auto lexer = LexerFactory::createFromString(text);
    auto tokenizer = EggTokenizerFactory::createFromLexer(lexer);
    auto parser = EggParserFactory::create();
    return parser->parse(*tokenizer);
  }
}

TEST(TestEggParser, Empty) {
  auto tree = parseFromString("/* Comment */");
}

TEST(TestEggParser, ExampleFile) {
  auto lexer = LexerFactory::createFromPath("~/cpp/test/data/example.egg");
  auto tokenizer = EggTokenizerFactory::createFromLexer(lexer);
  auto parser = EggParserFactory::create();
  auto tree = parser->parse(*tokenizer);
}
