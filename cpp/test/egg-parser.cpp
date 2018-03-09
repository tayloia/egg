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
  std::string parseExpressionToString(const std::string& text, bool concise = true) {
    auto parser = EggParserFactory::createExpressionParser();
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

TEST(TestEggParser, Extraneous) {
  // Bad
  ASSERT_PARSE_BAD(parseStatementToString("var foo; bar"), "(1, 10): Expected end of input after statement, not identifier: 'bar'");
  ASSERT_PARSE_BAD(parseExpressionToString("foo bar"), "(1, 5): Expected end of input after expression, not identifier: 'bar'");
}

TEST(TestEggParser, VariableDeclaration) {
  //TODO should we allow 'var' without an initializer?
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
  ASSERT_PARSE_BAD(parseStatementToString("var foo = ;"), "(1, 11): Expected expression after assignment");
  ASSERT_PARSE_BAD(parseStatementToString("var foo = var"), "(1, 11): Expected expression after assignment");
}

TEST(TestEggParser, Assignment) {
  // Good
  ASSERT_PARSE_GOOD(parseStatementToString("lhs = rhs;"), "(assign '=' (identifier 'lhs') (identifier 'rhs'))");
  ASSERT_PARSE_GOOD(parseStatementToString("lhs += rhs;"), "(assign '+=' (identifier 'lhs') (identifier 'rhs'))");
  ASSERT_PARSE_GOOD(parseStatementToString("lhs -= rhs;"), "(assign '-=' (identifier 'lhs') (identifier 'rhs'))");
  ASSERT_PARSE_GOOD(parseStatementToString("lhs *= rhs;"), "(assign '*=' (identifier 'lhs') (identifier 'rhs'))");
  ASSERT_PARSE_GOOD(parseStatementToString("lhs /= rhs;"), "(assign '/=' (identifier 'lhs') (identifier 'rhs'))");
  ASSERT_PARSE_GOOD(parseStatementToString("lhs %= rhs;"), "(assign '%=' (identifier 'lhs') (identifier 'rhs'))");
  ASSERT_PARSE_GOOD(parseStatementToString("lhs &= rhs;"), "(assign '&=' (identifier 'lhs') (identifier 'rhs'))");
  ASSERT_PARSE_GOOD(parseStatementToString("lhs |= rhs;"), "(assign '|=' (identifier 'lhs') (identifier 'rhs'))");
  ASSERT_PARSE_GOOD(parseStatementToString("lhs ^= rhs;"), "(assign '^=' (identifier 'lhs') (identifier 'rhs'))");
  ASSERT_PARSE_GOOD(parseStatementToString("lhs <<= rhs;"), "(assign '<<=' (identifier 'lhs') (identifier 'rhs'))");
  ASSERT_PARSE_GOOD(parseStatementToString("lhs >>= rhs;"), "(assign '>>=' (identifier 'lhs') (identifier 'rhs'))");
  ASSERT_PARSE_GOOD(parseStatementToString("lhs >>>= rhs;"), "(assign '>>>=' (identifier 'lhs') (identifier 'rhs'))");
  // Bad
  ASSERT_PARSE_BAD(parseStatementToString("lhs = rhs"), "(1, 10): Expected semicolon after assignment statement");
  ASSERT_PARSE_BAD(parseStatementToString("lhs *= var"), "(1, 8): Expected expression after assignment '*=' operator");
  ASSERT_PARSE_BAD(parseStatementToString("lhs = rhs extra"), "(1, 11): Expected semicolon after assignment statement");
}

TEST(TestEggParser, Mutate) {
  // Good
  ASSERT_PARSE_GOOD(parseStatementToString("++x;"), "(mutate '++' (identifier 'x'))");
  ASSERT_PARSE_GOOD(parseStatementToString("--x;"), "(mutate '--' (identifier 'x'))");
  // Bad
  ASSERT_PARSE_BAD(parseStatementToString("x++;"), "(1, 2): Unexpected '+' after infix '+' operator");
  ASSERT_PARSE_BAD(parseStatementToString("x--;"), "(1, 4): Expected expression after prefix '-' operator");
}

TEST(TestEggParser, ExpressionTernary) {
  // Good
  ASSERT_PARSE_GOOD(parseExpressionToString("a ? b : c"), "(ternary (identifier 'a') (identifier 'b') (identifier 'c'))");
  ASSERT_PARSE_GOOD(parseExpressionToString("a ? b : c ? d : e"), "(ternary (identifier 'a') (identifier 'b') (ternary (identifier 'c') (identifier 'd') (identifier 'e')))");
  ASSERT_PARSE_GOOD(parseExpressionToString("a ? b ? c : d : e"), "(ternary (identifier 'a') (ternary (identifier 'b') (identifier 'c') (identifier 'd')) (identifier 'e'))");
  // Bad
  ASSERT_PARSE_BAD(parseExpressionToString("a ? : c"), "(1, 5): Expected expression after '?' of ternary operator");
  ASSERT_PARSE_BAD(parseExpressionToString("a ? b :"), "(1, 8): Expected expression after ':' of ternary operator");
}

TEST(TestEggParser, ExpressionBinary) {
  // Good
  ASSERT_PARSE_GOOD(parseExpressionToString("a + b"), "(binary '+' (identifier 'a') (identifier 'b'))");
  ASSERT_PARSE_GOOD(parseExpressionToString("a - b"), "(binary '-' (identifier 'a') (identifier 'b'))");
  ASSERT_PARSE_GOOD(parseExpressionToString("a * b"), "(binary '*' (identifier 'a') (identifier 'b'))");
  ASSERT_PARSE_GOOD(parseExpressionToString("a / b"), "(binary '/' (identifier 'a') (identifier 'b'))");
  ASSERT_PARSE_GOOD(parseExpressionToString("a % b"), "(binary '%' (identifier 'a') (identifier 'b'))");
  ASSERT_PARSE_GOOD(parseExpressionToString("a & b"), "(binary '&' (identifier 'a') (identifier 'b'))");
  ASSERT_PARSE_GOOD(parseExpressionToString("a | b"), "(binary '|' (identifier 'a') (identifier 'b'))");
  ASSERT_PARSE_GOOD(parseExpressionToString("a ^ b"), "(binary '^' (identifier 'a') (identifier 'b'))");
  ASSERT_PARSE_GOOD(parseExpressionToString("a << b"), "(binary '<<' (identifier 'a') (identifier 'b'))");
  ASSERT_PARSE_GOOD(parseExpressionToString("a >> b"), "(binary '>>' (identifier 'a') (identifier 'b'))");
  ASSERT_PARSE_GOOD(parseExpressionToString("a >>> b"), "(binary '>>>' (identifier 'a') (identifier 'b'))");
  ASSERT_PARSE_GOOD(parseExpressionToString("a && b"), "(binary '&&' (identifier 'a') (identifier 'b'))");
  ASSERT_PARSE_GOOD(parseExpressionToString("a || b"), "(binary '||' (identifier 'a') (identifier 'b'))");
  ASSERT_PARSE_GOOD(parseExpressionToString("a == b"), "(binary '==' (identifier 'a') (identifier 'b'))");
  ASSERT_PARSE_GOOD(parseExpressionToString("a != b"), "(binary '!=' (identifier 'a') (identifier 'b'))");
  ASSERT_PARSE_GOOD(parseExpressionToString("a < b"), "(binary '<' (identifier 'a') (identifier 'b'))");
  ASSERT_PARSE_GOOD(parseExpressionToString("a <= b"), "(binary '<=' (identifier 'a') (identifier 'b'))");
  ASSERT_PARSE_GOOD(parseExpressionToString("a > b"), "(binary '>' (identifier 'a') (identifier 'b'))");
  ASSERT_PARSE_GOOD(parseExpressionToString("a >= b"), "(binary '>=' (identifier 'a') (identifier 'b'))");
  ASSERT_PARSE_GOOD(parseExpressionToString("a ?? b"), "(binary '??' (identifier 'a') (identifier 'b'))");
  ASSERT_PARSE_GOOD(parseExpressionToString("a + b + c"), "(binary '+' (binary '+' (identifier 'a') (identifier 'b')) (identifier 'c'))");
  ASSERT_PARSE_GOOD(parseExpressionToString("a + b - c"), "(binary '-' (binary '+' (identifier 'a') (identifier 'b')) (identifier 'c'))");
  ASSERT_PARSE_GOOD(parseExpressionToString("a - b + c"), "(binary '+' (binary '-' (identifier 'a') (identifier 'b')) (identifier 'c'))");
  ASSERT_PARSE_GOOD(parseExpressionToString("a * b + c"), "(binary '+' (binary '*' (identifier 'a') (identifier 'b')) (identifier 'c'))");
  ASSERT_PARSE_GOOD(parseExpressionToString("a + b * c"), "(binary '+' (identifier 'a') (binary '*' (identifier 'b') (identifier 'c')))");
  // Bad
  ASSERT_PARSE_BAD(parseExpressionToString("a +"), "(1, 4): Expected expression after infix '+' operator");
  ASSERT_PARSE_BAD(parseExpressionToString("++a"), "(1, 1): Expression expected, not operator: '++'");
  ASSERT_PARSE_BAD(parseExpressionToString("a--"), "(1, 4): Expected expression after prefix '-' operator");
}

TEST(TestEggParser, ExpressionUnary) {
  // Good
  ASSERT_PARSE_GOOD(parseExpressionToString("-a"), "(unary '-' (identifier 'a'))");
  ASSERT_PARSE_GOOD(parseExpressionToString("--a"), "(unary '-' (unary '-' (identifier 'a')))");
  ASSERT_PARSE_GOOD(parseExpressionToString("---a"), "(unary '-' (unary '-' (unary '-' (identifier 'a'))))");
  ASSERT_PARSE_GOOD(parseExpressionToString("&a"), "(unary '&' (identifier 'a'))");
  ASSERT_PARSE_GOOD(parseExpressionToString("*a"), "(unary '*' (identifier 'a'))");
  ASSERT_PARSE_GOOD(parseExpressionToString("~a"), "(unary '~' (identifier 'a'))");
  ASSERT_PARSE_GOOD(parseExpressionToString("!a"), "(unary '!' (identifier 'a'))");
  ASSERT_PARSE_GOOD(parseExpressionToString("&*-~!a"), "(unary '&' (unary '*' (unary '-' (unary '~' (unary '!' (identifier 'a'))))))");
  // Bad
  ASSERT_PARSE_BAD(parseExpressionToString("+a"), "(1, 1): Expression expected, not operator: '+'");
  ASSERT_PARSE_BAD(parseExpressionToString("++a"), "(1, 1): Expression expected, not operator: '++'");
  ASSERT_PARSE_BAD(parseExpressionToString("+++a"), "(1, 1): Expression expected, not operator: '++'");
  ASSERT_PARSE_BAD(parseExpressionToString("-var"), "(1, 2): Expected expression after prefix '-' operator");
}

TEST(TestEggParser, ExpressionPostfix) {
  // TODO
  ASSERT_PARSE_GOOD(parseExpressionToString("a[0]"), "(binary '[' (identifier 'a') (literal int 0))");
  ASSERT_PARSE_GOOD(parseExpressionToString("a()"), "(call (identifier 'a'))");
  ASSERT_PARSE_GOOD(parseExpressionToString("a(x)"), "(call (identifier 'a') (identifier 'x'))");
  ASSERT_PARSE_GOOD(parseExpressionToString("a(x,y)"), "(call (identifier 'a') (identifier 'x') (identifier 'y'))");
  ASSERT_PARSE_GOOD(parseExpressionToString("a(x,y,name:z)"), "(call (identifier 'a') (identifier 'x') (identifier 'y') (named 'name' (identifier 'z')))");
  ASSERT_PARSE_GOOD(parseExpressionToString("a.b"), "(binary '.' (identifier 'a') (identifier 'b'))");
  ASSERT_PARSE_GOOD(parseExpressionToString("a.b.c"), "(binary '.' (binary '.' (identifier 'a') (identifier 'b')) (identifier 'c'))");
  ASSERT_PARSE_GOOD(parseExpressionToString("a?.b"), "(binary '?' (identifier 'a') (identifier 'b'))");
  ASSERT_PARSE_GOOD(parseExpressionToString("a?.b?.c"), "(binary '?' (binary '?' (identifier 'a') (identifier 'b')) (identifier 'c'))");
}

TEST(TestEggParser, Vexatious) {
  ASSERT_PARSE_GOOD(parseExpressionToString("a--b"), "(binary '-' (identifier 'a') (unary '-' (identifier 'b')))");
  ASSERT_PARSE_GOOD(parseExpressionToString("a--1"), "(binary '-' (identifier 'a') (unary '-' (literal int 1)))");
  ASSERT_PARSE_GOOD(parseExpressionToString("-1"), "(unary '-' (literal int 1))");
  //TODO ASSERT_PARSE_GOOD(parseExpressionToString("a?...b:c"), "(ternary ...)");
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
