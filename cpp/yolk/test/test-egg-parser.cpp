#include "yolk/test.h"
#include "yolk/lexers.h"
#include "yolk/egg-tokenizer.h"
#include "yolk/egg-parser.h"

using Node = egg::yolk::IEggParser::Node;

TEST(TestEggParser, ExampleFile) {
  egg::test::Allocator allocator;
  std::string path = "~/cpp/yolk/test/data/example.egg";
  auto lexer = egg::yolk::LexerFactory::createFromPath(path);
  auto tokenizer = egg::yolk::EggTokenizerFactory::createFromLexer(allocator, lexer);
  auto parser = egg::yolk::EggParserFactory::createFromTokenizer(allocator, tokenizer);
  ASSERT_STRING(path.c_str(), parser->resource());
  auto result = parser->parse();
  ASSERT_TRUE(result.root != nullptr);
  ASSERT_EQ(Node::Kind::ModuleRoot, result.root->kind);
  ASSERT_TRUE(result.root->children.empty());
}
