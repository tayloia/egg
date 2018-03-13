#include "test.h"
#include "lexers.h"
#include "egg-tokenizer.h"
#include "egg-syntax.h"
#include "egg-parser.h"

using namespace egg::yolk;

#define ASSERT_PARSE_GOOD(parsed, expected) ASSERT_EQ(expected, parsed)
#define ASSERT_PARSE_BAD(parsed, needle) ASSERT_THROW_E(parsed, Exception, ASSERT_CONTAINS(e.what(), needle))

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
  ASSERT_PARSE_GOOD(dumpToString(*root), "(module)");
}

TEST(TestEggParser, ModuleBlock) {
  auto parser = EggParserFactory::createModuleParser();
  auto root = parseFromString(*parser, "{}");
  ASSERT_PARSE_GOOD(dumpToString(*root), "(module (block))");
}

TEST(TestEggParser, ModuleSimple) {
  auto parser = EggParserFactory::createModuleParser();
  auto root = parseFromString(*parser, "var a = b; a = c;");
  ASSERT_PARSE_GOOD(dumpToString(*root), "(module (declare 'a' (type 'var') (identifier 'b')) (assign '==' (identifier 'a') (identifier 'c')))");
}
