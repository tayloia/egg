#include "test.h"
#include "lexers.h"
#include "egg-tokenizer.h"
#include "egg-syntax.h"
#include "egg-parser.h"

using namespace egg::yolk;

namespace {
  std::shared_ptr<IEggParserNode> parseFromString(IEggParser& parser, const std::string& text) {
    auto lexer = LexerFactory::createFromString(text);
    auto tokenizer = EggTokenizerFactory::createFromLexer(lexer);
    return parser.parse(*tokenizer);
  }
  std::string dumpToString(IEggParserNode& tree) {
    std::ostringstream oss;
    tree.dump(oss);
    return oss.str();
  }
}

TEST(TestEggParser, ModuleEmpty) {
  auto parser = EggParserFactory::createModuleParser();
  auto root = parseFromString(*parser, "");
  auto dumped = dumpToString(*root);
  ASSERT_EQ("(module)", dumped);
}
