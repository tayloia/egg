#include "yolk/test.h"
#include "yolk/lexers.h"
#include "yolk/egg-tokenizer.h"
#include "yolk/egg-syntax.h"
#include "yolk/egg-parser.h"
#include "yolk/egg-engine.h"
#include "yolk/egg-program.h"

using namespace egg::yolk;

#define ASSERT_PARSE_GOOD(parsed, expected) ASSERT_EQ(expected, parsed)
#define ASSERT_PARSE_BAD(parsed, needle) ASSERT_THROW_E(parsed, Exception, ASSERT_CONTAINS(e.what(), needle))

namespace {
  std::shared_ptr<IEggProgramNode> parseFromString(egg::ovum::IAllocator& allocator, IEggParser& parser, const std::string& text) {
    egg::ovum::TypeFactory factory{ allocator };
    auto lexer = LexerFactory::createFromString(text);
    auto tokenizer = EggTokenizerFactory::createFromLexer(lexer);
    return parser.parse(factory, *tokenizer);
  }
  std::string dumpToString(IEggProgramNode& tree) {
    std::ostringstream oss;
    tree.dump(oss);
    return oss.str();
  }
  std::string typeFromExpression(egg::ovum::IAllocator& allocator, const std::string& expression) {
    egg::ovum::TypeFactory factory{ allocator };
    auto lexer = LexerFactory::createFromString(expression);
    auto tokenizer = EggTokenizerFactory::createFromLexer(lexer);
    auto parser = EggParserFactory::createExpressionParser();
    auto type = parser->parse(factory, *tokenizer)->getType();
    return type.toString().toUTF8();
  }
}

TEST(TestEggParser, ModuleEmpty) {
  egg::test::Allocator allocator{ egg::test::Allocator::Expectation::NoAllocations }; // TODO
  auto parser = EggParserFactory::createModuleParser();
  auto root = parseFromString(allocator, *parser, "");
  ASSERT_PARSE_GOOD(dumpToString(*root), "(module)");
}

TEST(TestEggParser, ModuleBlock) {
  egg::test::Allocator allocator{ egg::test::Allocator::Expectation::NoAllocations }; // TODO
  auto parser = EggParserFactory::createModuleParser();
  auto root = parseFromString(allocator, *parser, "{}");
  ASSERT_PARSE_GOOD(dumpToString(*root), "(module (block))");
}

TEST(TestEggParser, ModuleSimple) {
  egg::test::Allocator allocator{ egg::test::Allocator::Expectation::NoAllocations }; // TODO
  auto parser = EggParserFactory::createModuleParser();
  auto root = parseFromString(allocator, *parser, "var a = b; a = c;");
  ASSERT_PARSE_GOOD(dumpToString(*root), "(module (declare 'a' 'var' (identifier 'b')) (assign '=' (identifier 'a') (identifier 'c')))");
}

TEST(TestEggParser, ExpressionType) {
  egg::test::Allocator allocator;
  ASSERT_PARSE_GOOD(typeFromExpression(allocator, "null"), "null");
  ASSERT_PARSE_GOOD(typeFromExpression(allocator, "false"), "bool");
  ASSERT_PARSE_GOOD(typeFromExpression(allocator, "true"), "bool");
  ASSERT_PARSE_GOOD(typeFromExpression(allocator, "0"), "int");
  ASSERT_PARSE_GOOD(typeFromExpression(allocator, "-1"), "int");
  ASSERT_PARSE_GOOD(typeFromExpression(allocator, "-1.23"), "float");
  ASSERT_PARSE_GOOD(typeFromExpression(allocator, "\"hi\""), "string");
  ASSERT_PARSE_GOOD(typeFromExpression(allocator, "`bye`"), "string");
  ASSERT_PARSE_GOOD(typeFromExpression(allocator, "[]"), "any?[]");
  ASSERT_PARSE_GOOD(typeFromExpression(allocator, "[1,2,3]"), "any?[]");
  ASSERT_PARSE_GOOD(typeFromExpression(allocator, "{}"), "any?{string}");
  ASSERT_PARSE_GOOD(typeFromExpression(allocator, "{a:1,b:2,c:3}"), "any?{string}");
  ASSERT_PARSE_GOOD(typeFromExpression(allocator, "&123"), "int*");
  ASSERT_PARSE_GOOD(typeFromExpression(allocator, "*123"), "<unknown>");
  ASSERT_PARSE_GOOD(typeFromExpression(allocator, "!true"), "bool");
  ASSERT_PARSE_GOOD(typeFromExpression(allocator, "- 123"), "int");
  ASSERT_PARSE_GOOD(typeFromExpression(allocator, "~123"), "int");
  ASSERT_PARSE_GOOD(typeFromExpression(allocator, "1+2"), "int");
  ASSERT_PARSE_GOOD(typeFromExpression(allocator, "1.0+2"), "float");
  ASSERT_PARSE_GOOD(typeFromExpression(allocator, "1+2.0"), "float");
  ASSERT_PARSE_GOOD(typeFromExpression(allocator, "1.0+2.0"), "float");
  ASSERT_PARSE_GOOD(typeFromExpression(allocator, "1.0+null"), "void");
  ASSERT_PARSE_GOOD(typeFromExpression(allocator, "1-2"), "int");
  ASSERT_PARSE_GOOD(typeFromExpression(allocator, "1.0-2.0"), "float");
  ASSERT_PARSE_GOOD(typeFromExpression(allocator, "1*2"), "int");
  ASSERT_PARSE_GOOD(typeFromExpression(allocator, "1.0*2.0"), "float");
  ASSERT_PARSE_GOOD(typeFromExpression(allocator, "1/2"), "int");
  ASSERT_PARSE_GOOD(typeFromExpression(allocator, "1.0/2.0"), "float");
  ASSERT_PARSE_GOOD(typeFromExpression(allocator, "1%2"), "int");
  ASSERT_PARSE_GOOD(typeFromExpression(allocator, "1.0%2.0"), "float");
  ASSERT_PARSE_GOOD(typeFromExpression(allocator, "1&2"), "int");
  ASSERT_PARSE_GOOD(typeFromExpression(allocator, "1|2"), "int");
  ASSERT_PARSE_GOOD(typeFromExpression(allocator, "1^2"), "int");
  ASSERT_PARSE_GOOD(typeFromExpression(allocator, "1<<2"), "int");
  ASSERT_PARSE_GOOD(typeFromExpression(allocator, "1>>2"), "int");
  ASSERT_PARSE_GOOD(typeFromExpression(allocator, "1>>>2"), "int");
  ASSERT_PARSE_GOOD(typeFromExpression(allocator, "true&&true"), "bool");
  ASSERT_PARSE_GOOD(typeFromExpression(allocator, "true||true"), "bool");
  ASSERT_PARSE_GOOD(typeFromExpression(allocator, "1<2"), "bool");
  ASSERT_PARSE_GOOD(typeFromExpression(allocator, "1<=2"), "bool");
  ASSERT_PARSE_GOOD(typeFromExpression(allocator, "1==2"), "bool");
  ASSERT_PARSE_GOOD(typeFromExpression(allocator, "1!=2"), "bool");
  ASSERT_PARSE_GOOD(typeFromExpression(allocator, "1>=2"), "bool");
  ASSERT_PARSE_GOOD(typeFromExpression(allocator, "1>2"), "bool");
  ASSERT_PARSE_GOOD(typeFromExpression(allocator, "true??123"), "bool");
  ASSERT_PARSE_GOOD(typeFromExpression(allocator, "null??123"), "int");
  ASSERT_PARSE_GOOD(typeFromExpression(allocator, "null?123:123.45"), "void");
  ASSERT_PARSE_GOOD(typeFromExpression(allocator, "true?123:null"), "int?");
  ASSERT_PARSE_GOOD(typeFromExpression(allocator, "true?123:123.45"), "int|float");
  ASSERT_PARSE_GOOD(typeFromExpression(allocator, "true?123:true?123.45:`hi`"), "int|float|string");
  // TODO
  ASSERT_PARSE_BAD(typeFromExpression(allocator, "true?123"), "Expected ':' as part of ternary '?:' operator");
}

TEST(TestEggParser, ExampleFile) {
  egg::test::Allocator allocator{ egg::test::Allocator::Expectation::NoAllocations }; // TODO
  egg::ovum::TypeFactory factory{ allocator };
  FileTextStream stream("~/yolk/test/data/example.egg");
  auto root = EggParserFactory::parseModule(factory, stream);
  root->dump(std::cout);
  std::cout << std::endl;
}
