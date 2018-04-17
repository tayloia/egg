#include "test.h"
#include "lexers.h"
#include "egg-tokenizer.h"
#include "egg-syntax.h"
#include "egg-parser.h"
#include "egg-program.h"

using namespace egg::yolk;

#define ASSERT_PARSE_GOOD(parsed, expected) ASSERT_EQ(expected, parsed)
#define ASSERT_PARSE_BAD(parsed, needle) ASSERT_THROW_E(parsed, Exception, ASSERT_CONTAINS(e.what(), needle))

namespace {
  std::shared_ptr<IEggProgramNode> parseFromString(IEggParser& parser, const std::string& text) {
    auto lexer = LexerFactory::createFromString(text);
    auto tokenizer = EggTokenizerFactory::createFromLexer(lexer);
    return parser.parse(*tokenizer);
  }
  std::string dumpToString(IEggProgramNode& tree) {
    std::ostringstream oss;
    tree.dump(oss);
    return oss.str();
  }
  std::string typeFromExpression(const std::string& expression) {
    auto lexer = LexerFactory::createFromString(expression);
    auto tokenizer = EggTokenizerFactory::createFromLexer(lexer);
    auto parser = EggParserFactory::createExpressionParser();
    auto type = parser->parse(*tokenizer)->getType();
    return (type == nullptr) ? "(nullptr)" : type->toString().toUTF8();
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
  ASSERT_PARSE_GOOD(dumpToString(*root), "(module (declare 'a' 'var' (identifier 'b')) (assign '=' (identifier 'a') (identifier 'c')))");
}

TEST(TestEggParser, ExpressionType) {
  ASSERT_PARSE_GOOD(typeFromExpression("null"), "null");
  ASSERT_PARSE_GOOD(typeFromExpression("false"), "bool");
  ASSERT_PARSE_GOOD(typeFromExpression("true"), "bool");
  ASSERT_PARSE_GOOD(typeFromExpression("0"), "int");
  ASSERT_PARSE_GOOD(typeFromExpression("-1"), "int");
  ASSERT_PARSE_GOOD(typeFromExpression("-1.23"), "float");
  ASSERT_PARSE_GOOD(typeFromExpression("\"hi\""), "string");
  ASSERT_PARSE_GOOD(typeFromExpression("`bye`"), "string");
  ASSERT_PARSE_GOOD(typeFromExpression("&123"), "int*"); // TODO
  ASSERT_PARSE_GOOD(typeFromExpression("*123"), "void");
  ASSERT_PARSE_GOOD(typeFromExpression("!true"), "bool");
  ASSERT_PARSE_GOOD(typeFromExpression("- 123"), "int");
  ASSERT_PARSE_GOOD(typeFromExpression("~123"), "int");
  ASSERT_PARSE_GOOD(typeFromExpression("1+2"), "int");
  ASSERT_PARSE_GOOD(typeFromExpression("1.0+2"), "float");
  ASSERT_PARSE_GOOD(typeFromExpression("1+2.0"), "float");
  ASSERT_PARSE_GOOD(typeFromExpression("1.0+2.0"), "float");
  ASSERT_PARSE_GOOD(typeFromExpression("1.0+null"), "void");
  ASSERT_PARSE_GOOD(typeFromExpression("1-2"), "int");
  ASSERT_PARSE_GOOD(typeFromExpression("1.0-2.0"), "float");
  ASSERT_PARSE_GOOD(typeFromExpression("1*2"), "int");
  ASSERT_PARSE_GOOD(typeFromExpression("1.0*2.0"), "float");
  ASSERT_PARSE_GOOD(typeFromExpression("1/2"), "int");
  ASSERT_PARSE_GOOD(typeFromExpression("1.0/2.0"), "float");
  ASSERT_PARSE_GOOD(typeFromExpression("1%2"), "int");
  ASSERT_PARSE_GOOD(typeFromExpression("1.0%2.0"), "float");
  ASSERT_PARSE_GOOD(typeFromExpression("1&2"), "int");
  ASSERT_PARSE_GOOD(typeFromExpression("1|2"), "int");
  ASSERT_PARSE_GOOD(typeFromExpression("1^2"), "int");
  ASSERT_PARSE_GOOD(typeFromExpression("1<<2"), "int");
  ASSERT_PARSE_GOOD(typeFromExpression("1>>2"), "int");
  ASSERT_PARSE_GOOD(typeFromExpression("1>>>2"), "int");
  ASSERT_PARSE_GOOD(typeFromExpression("true&&true"), "bool");
  ASSERT_PARSE_GOOD(typeFromExpression("true||true"), "bool");
  ASSERT_PARSE_GOOD(typeFromExpression("1<2"), "bool");
  ASSERT_PARSE_GOOD(typeFromExpression("1<=2"), "bool");
  ASSERT_PARSE_GOOD(typeFromExpression("1==2"), "bool");
  ASSERT_PARSE_GOOD(typeFromExpression("1!=2"), "bool");
  ASSERT_PARSE_GOOD(typeFromExpression("1>=2"), "bool");
  ASSERT_PARSE_GOOD(typeFromExpression("1>2"), "bool");
  ASSERT_PARSE_GOOD(typeFromExpression("true??123"), "bool");
  ASSERT_PARSE_GOOD(typeFromExpression("null??123"), "int");
  ASSERT_PARSE_GOOD(typeFromExpression("null?123:123.45"), "void");
  ASSERT_PARSE_GOOD(typeFromExpression("true?123:null"), "int?");
  ASSERT_PARSE_GOOD(typeFromExpression("true?123:123.45"), "int|float");
  ASSERT_PARSE_GOOD(typeFromExpression("true?123:true?123.45:`hi`"), "int|float|string");
}

TEST(TestEggParser, ExampleFile) {
  FileTextStream stream("~/cpp/test/data/example.egg");
  auto root = EggParserFactory::parseModule(stream);
  root->dump(std::cout);
  std::cout << std::endl;
}
