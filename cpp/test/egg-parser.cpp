#include "test.h"

using namespace egg::yolk;

#define ASSERT_PARSE_GOOD(parsed, expected) ASSERT_EQ(expected, parsed)
#define ASSERT_PARSE_BAD(parsed, needle) ASSERT_THROW_E(parsed, Exception, ASSERT_CONTAINS(e.what(), needle))

namespace {
  std::shared_ptr<IEggSyntaxNode> parseFromString(IEggParser& parser, const std::string& text) {
    auto lexer = LexerFactory::createFromString(text);
    auto tokenizer = EggTokenizerFactory::createFromLexer(lexer);
    return parser.parse(*tokenizer);
  }
  std::string printToString(IEggSyntaxNode& tree, bool concise = true) {
    std::ostringstream oss;
    EggSyntaxNode::printToStream(oss, tree, concise);
    return oss.str();
  }
  std::string parseStatementToString(const std::string& text, bool concise = true) {
    auto parser = EggParserFactory::createStatementParser();
    auto root = parseFromString(*parser, text);
    return printToString(*root, concise);
  }
}

TEST(TestEggParser, ModuleEmpty) {
  auto parser = EggParserFactory::createModuleParser();
  auto root = parseFromString(*parser, "");
  ASSERT_EQ(EggSyntaxNodeKind::Module, root->getKind());
  ASSERT_EQ(0, root->getChildren());
  ASSERT_EQ("(module)", printToString(*root));
}

TEST(TestEggParser, ModuleOneStatement) {
  auto parser = EggParserFactory::createModuleParser();
  auto root = parseFromString(*parser, "var foo;");
  ASSERT_EQ(EggSyntaxNodeKind::Module, root->getKind());
  ASSERT_EQ(1, root->getChildren());
  ASSERT_EQ("(module (declare 'foo' (type 'var')))", printToString(*root));
}

TEST(TestEggParser, ModuleTwoStatements) {
  auto parser = EggParserFactory::createModuleParser();
  auto root = parseFromString(*parser, "var foo;\nvar bar;");
  ASSERT_EQ(EggSyntaxNodeKind::Module, root->getKind());
  ASSERT_EQ(2, root->getChildren());
  ASSERT_EQ("(module (declare 'foo' (type 'var')) (declare 'bar' (type 'var')))", printToString(*root));
}

TEST(TestEggParser, VariableDeclaration) {
  // Good
  ASSERT_PARSE_GOOD(parseStatementToString("var foo;"), "(declare 'foo' (type 'var'))");
  ASSERT_PARSE_GOOD(parseStatementToString("any? bar;"), "(declare 'bar' (type 'any?'))");
  // Bad
  ASSERT_PARSE_BAD(parseStatementToString("var"), "(1, 4): Malformed statement");
  ASSERT_PARSE_BAD(parseStatementToString("var foo"), "(1, 8): Malformed variable declaration or initialization");
}

TEST(TestEggParser, VariableInitialization) {
  // Good
  ASSERT_PARSE_GOOD(parseStatementToString("var foo = 42;"), "(initialize 'foo' (type 'var') (literal int 42))");
  ASSERT_PARSE_GOOD(parseStatementToString("any? bar = `hello`;"), "(initialize 'bar' (type 'any?') (literal string 'hello'))");
  // Bad
  ASSERT_PARSE_BAD(parseStatementToString("var foo ="), "(1, 10): Expected expression after assignment");
  ASSERT_PARSE_BAD(parseStatementToString("var foo = ;"), "(1, 10): Expected expression after assignment");
  ASSERT_PARSE_BAD(parseStatementToString("var foo = var"), "(1, 10): Expected expression after assignment");
}

TEST(TestEggParser, ExampleFile) {
  auto lexer = LexerFactory::createFromPath("~/cpp/test/data/example.egg");
  auto tokenizer = EggTokenizerFactory::createFromLexer(lexer);
  auto parser = EggParserFactory::createModuleParser();
  auto root = parser->parse(*tokenizer);
  EggSyntaxNode::printToStream(std::cout, *root, false);
  std::cout << std::endl;
  ASSERT_EQ(EggSyntaxNodeKind::Module, root->getKind());
  ASSERT_EQ(4, root->getChildren());
}
