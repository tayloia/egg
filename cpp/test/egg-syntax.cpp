#include "test.h"

using namespace egg::yolk;

#define ASSERT_PARSE_GOOD(parsed, expected) ASSERT_EQ(expected, parsed)
#define ASSERT_PARSE_BAD(parsed, needle) ASSERT_THROW_E(parsed, Exception, ASSERT_CONTAINS(e.what(), needle))

namespace {
  std::shared_ptr<IEggSyntaxNode> parseFromString(IEggSyntaxParser& parser, const std::string& text) {
    auto lexer = LexerFactory::createFromString(text);
    auto tokenizer = EggTokenizerFactory::createFromLexer(lexer);
    return parser.parse(*tokenizer);
  }
  std::string dumpToString(IEggSyntaxNode& tree) {
    std::ostringstream oss;
    tree.dump(oss);
    return oss.str();
  }
  std::string parseStatementToString(const std::string& text) {
    auto parser = EggParserFactory::createStatementSyntaxParser();
    auto root = parseFromString(*parser, text);
    return dumpToString(*root);
  }
  std::string parseExpressionToString(const std::string& text) {
    auto parser = EggParserFactory::createExpressionSyntaxParser();
    auto root = parseFromString(*parser, text);
    return dumpToString(*root);
  }
}

TEST(TestEggSyntaxParser, ModuleEmpty) {
  auto parser = EggParserFactory::createModuleSyntaxParser();
  auto root = parseFromString(*parser, "");
  ASSERT_EQ("(module)", dumpToString(*root));
}

TEST(TestEggSyntaxParser, ModuleOneStatement) {
  auto parser = EggParserFactory::createModuleSyntaxParser();
  auto root = parseFromString(*parser, "var foo;");
  ASSERT_EQ("(module (declare 'foo' (type 'var')))", dumpToString(*root));
}

TEST(TestEggSyntaxParser, ModuleTwoStatements) {
  auto parser = EggParserFactory::createModuleSyntaxParser();
  auto root = parseFromString(*parser, "var foo;\nvar bar;");
  ASSERT_EQ("(module (declare 'foo' (type 'var')) (declare 'bar' (type 'var')))", dumpToString(*root));
}

TEST(TestEggSyntaxParser, Extraneous) {
  // Bad
  ASSERT_PARSE_BAD(parseStatementToString("var foo; bar"), "(1, 10): Expected end of input after statement, not identifier: 'bar'");
  ASSERT_PARSE_BAD(parseExpressionToString("foo bar"), "(1, 5): Expected end of input after expression, not identifier: 'bar'");
}

TEST(TestEggSyntaxParser, VariableDeclaration) {
  //TODO should we allow 'var' without an initializer?
  // Good
  ASSERT_PARSE_GOOD(parseStatementToString("var foo;"), "(declare 'foo' (type 'var'))");
  ASSERT_PARSE_GOOD(parseStatementToString("any? bar;"), "(declare 'bar' (type 'any?'))");
  // Bad
  ASSERT_PARSE_BAD(parseStatementToString("var"), "(1, 4): Expected variable identifier after type");
  ASSERT_PARSE_BAD(parseStatementToString("var foo"), "(1, 5): Malformed variable declaration or initialization");
}

TEST(TestEggSyntaxParser, VariableInitialization) {
  // Good
  ASSERT_PARSE_GOOD(parseStatementToString("var foo = 42;"), "(initialize 'foo' (type 'var') (literal int 42))");
  ASSERT_PARSE_GOOD(parseStatementToString("any? bar = `hello`;"), "(initialize 'bar' (type 'any?') (literal string 'hello'))");
  // Bad
  ASSERT_PARSE_BAD(parseStatementToString("var foo ="), "(1, 10): Expected expression after assignment");
  ASSERT_PARSE_BAD(parseStatementToString("var foo = ;"), "(1, 11): Expected expression after assignment");
  ASSERT_PARSE_BAD(parseStatementToString("var foo = var"), "(1, 11): Expected expression after assignment");
}

TEST(TestEggSyntaxParser, Assignment) {
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
  ASSERT_PARSE_BAD(parseStatementToString("lhs = rhs"), "(1, 10): Expected ';' after assignment statement");
  ASSERT_PARSE_BAD(parseStatementToString("lhs *= var"), "(1, 8): Expected expression after assignment '*=' operator");
  ASSERT_PARSE_BAD(parseStatementToString("lhs = rhs extra"), "(1, 11): Expected ';' after assignment statement");
}

TEST(TestEggSyntaxParser, Mutate) {
  // Good
  ASSERT_PARSE_GOOD(parseStatementToString("++x;"), "(mutate '++' (identifier 'x'))");
  ASSERT_PARSE_GOOD(parseStatementToString("--x;"), "(mutate '--' (identifier 'x'))");
  // Bad
  ASSERT_PARSE_BAD(parseStatementToString("x++;"), "(1, 2): Unexpected '+' after infix '+' operator");
  ASSERT_PARSE_BAD(parseStatementToString("x--;"), "(1, 4): Expected expression after prefix '-' operator");
}

TEST(TestEggSyntaxParser, ExpressionTernary) {
  // Good
  ASSERT_PARSE_GOOD(parseExpressionToString("a ? b : c"), "(ternary (identifier 'a') (identifier 'b') (identifier 'c'))");
  ASSERT_PARSE_GOOD(parseExpressionToString("a ? b : c ? d : e"), "(ternary (identifier 'a') (identifier 'b') (ternary (identifier 'c') (identifier 'd') (identifier 'e')))");
  ASSERT_PARSE_GOOD(parseExpressionToString("a ? b ? c : d : e"), "(ternary (identifier 'a') (ternary (identifier 'b') (identifier 'c') (identifier 'd')) (identifier 'e'))");
  // Bad
  ASSERT_PARSE_BAD(parseExpressionToString("a ? : c"), "(1, 5): Expected expression after '?' of ternary operator");
  ASSERT_PARSE_BAD(parseExpressionToString("a ? b :"), "(1, 8): Expected expression after ':' of ternary operator");
}

TEST(TestEggSyntaxParser, ExpressionBinary) {
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

TEST(TestEggSyntaxParser, ExpressionUnary) {
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

TEST(TestEggSyntaxParser, ExpressionPostfix) {
  // Good
  ASSERT_PARSE_GOOD(parseExpressionToString("a[0]"), "(binary '[' (identifier 'a') (literal int 0))");
  ASSERT_PARSE_GOOD(parseExpressionToString("a()"), "(call (identifier 'a'))");
  ASSERT_PARSE_GOOD(parseExpressionToString("a(x)"), "(call (identifier 'a') (identifier 'x'))");
  ASSERT_PARSE_GOOD(parseExpressionToString("a(x,y)"), "(call (identifier 'a') (identifier 'x') (identifier 'y'))");
  ASSERT_PARSE_GOOD(parseExpressionToString("a(x,y,name:z)"), "(call (identifier 'a') (identifier 'x') (identifier 'y') (named 'name' (identifier 'z')))");
  ASSERT_PARSE_GOOD(parseExpressionToString("a.b"), "(binary '.' (identifier 'a') (identifier 'b'))");
  ASSERT_PARSE_GOOD(parseExpressionToString("a.b.c"), "(binary '.' (binary '.' (identifier 'a') (identifier 'b')) (identifier 'c'))");
  ASSERT_PARSE_GOOD(parseExpressionToString("a?.b"), "(binary '?' (identifier 'a') (identifier 'b'))");
  ASSERT_PARSE_GOOD(parseExpressionToString("a?.b?.c"), "(binary '?' (binary '?' (identifier 'a') (identifier 'b')) (identifier 'c'))");
  // Bad
  ASSERT_PARSE_BAD(parseExpressionToString("a[]"), "(1, 3): Expected expression inside indexing '[]' operators");
  ASSERT_PARSE_BAD(parseExpressionToString("a[0,1]"), "(1, 4): Expected ']' after indexing expression following '['");
  ASSERT_PARSE_BAD(parseExpressionToString("a(var)"), "(1, 3): Expected expression for function call parameter value");
  ASSERT_PARSE_BAD(parseExpressionToString("a(,)"), "(1, 3): Expected expression for function call parameter value");
  ASSERT_PARSE_BAD(parseExpressionToString("a(name=z)"), "(1, 7): Expected ')' at end of function call parameter list");
  ASSERT_PARSE_BAD(parseExpressionToString("a..b"), "(1, 3): Expected field name to follow '.' operator");
  ASSERT_PARSE_BAD(parseExpressionToString("a.?b"), "(1, 3): Expected field name to follow '.' operator");
  ASSERT_PARSE_BAD(parseExpressionToString("a?.?b"), "(1, 4): Expected field name to follow '?.' operator");
}

TEST(TestEggSyntaxParser, StatementCompound) {
  // Good
  ASSERT_PARSE_GOOD(parseStatementToString("{}"), "(block)");
  ASSERT_PARSE_GOOD(parseStatementToString("{{}}"), "(block (block))");
  ASSERT_PARSE_GOOD(parseStatementToString("{{}{}}"), "(block (block) (block))");
  ASSERT_PARSE_GOOD(parseStatementToString("{a();}"), "(block (call (identifier 'a')))");
  // Bad
  ASSERT_PARSE_BAD(parseStatementToString("{"), "(1, 2): Expected statement, not end-of-file");
  ASSERT_PARSE_BAD(parseStatementToString("}"), "(1, 1): Unexpected '}' (no matching '{' seen before)");
  ASSERT_PARSE_BAD(parseStatementToString(";"), "(1, 1): Unexpected ';' (empty statements are not permitted)");
}

TEST(TestEggSyntaxParser, StatementBreak) {
  // Good
  ASSERT_PARSE_GOOD(parseStatementToString("break;"), "(break)");
  // Bad
  ASSERT_PARSE_BAD(parseStatementToString("break"), "(1, 6): Expected ';' after 'break' keyword");
  ASSERT_PARSE_BAD(parseStatementToString("break 0;"), "(1, 7): Expected ';' after 'break' keyword");
}

TEST(TestEggSyntaxParser, StatementCase) {
  // Good
  ASSERT_PARSE_GOOD(parseStatementToString("case 0:"), "(case (literal int 0))");
  ASSERT_PARSE_GOOD(parseStatementToString("case a + b:"), "(case (binary '+' (identifier 'a') (identifier 'b')))");
  // Bad
  ASSERT_PARSE_BAD(parseStatementToString("case"), "(1, 5): Expected expression after 'case' keyword");
  ASSERT_PARSE_BAD(parseStatementToString("case 0"), "(1, 7): Expected colon after 'case' expression");
  ASSERT_PARSE_BAD(parseStatementToString("case a +"), "(1, 9): Expected expression after infix '+' operator");
}

TEST(TestEggSyntaxParser, StatementContinue) {
  // Good
  ASSERT_PARSE_GOOD(parseStatementToString("continue;"), "(continue)");
  // Bad
  ASSERT_PARSE_BAD(parseStatementToString("continue"), "(1, 9): Expected ';' after 'continue' keyword");
  ASSERT_PARSE_BAD(parseStatementToString("continue 0;"), "(1, 10): Expected ';' after 'continue' keyword");
}

TEST(TestEggSyntaxParser, StatementDefault) {
  // Good
  ASSERT_PARSE_GOOD(parseStatementToString("default:"), "(default)");
  // Bad
  ASSERT_PARSE_BAD(parseStatementToString("default"), "(1, 8): Expected colon after 'default' keyword");
  ASSERT_PARSE_BAD(parseStatementToString("default 0:"), "(1, 9): Expected colon after 'default' keyword");
}

TEST(TestEggSyntaxParser, StatementDo) {
  // Good
  ASSERT_PARSE_GOOD(parseStatementToString("do {} while (a);"), "(do (identifier 'a') (block))");
  // Bad
  ASSERT_PARSE_BAD(parseStatementToString("do ("), "(1, 4): Expected '{' after 'do' keyword");
  ASSERT_PARSE_BAD(parseStatementToString("do {"), "(1, 5): Expected statement");
  ASSERT_PARSE_BAD(parseStatementToString("do {}"), "(1, 6): Expected 'while' after '}' in 'do' statement");
  ASSERT_PARSE_BAD(parseStatementToString("do {} do"), "(1, 7): Expected 'while' after '}' in 'do' statement");
  ASSERT_PARSE_BAD(parseStatementToString("do {} while"), "(1, 12): Expected '(' after 'while' keyword in 'do' statement");
  ASSERT_PARSE_BAD(parseStatementToString("do {} while ()"), "(1, 14): Expected condition expression after 'while (' in 'do' statement");
  ASSERT_PARSE_BAD(parseStatementToString("do {} while (a)"), "(1, 16): Expected ';' after ')' at end of 'do' statement");
}

TEST(TestEggSyntaxParser, StatementFor) {
  // Good
  ASSERT_PARSE_GOOD(parseStatementToString("for (;;) {}"), "(for () () () (block))");
  ASSERT_PARSE_GOOD(parseStatementToString("for (int i = 0; i < 10; ++i) {}"), "(for (initialize 'i' (type 'int') (literal int 0)) (binary '<' (identifier 'i') (literal int 10)) (mutate '++' (identifier 'i')) (block))");
  ASSERT_PARSE_GOOD(parseStatementToString("for (a : b) {}"), "(foreach (identifier 'a') (identifier 'b') (block))");
  ASSERT_PARSE_GOOD(parseStatementToString("for (*a : b) {}"), "(foreach (unary '*' (identifier 'a')) (identifier 'b') (block))");
  ASSERT_PARSE_GOOD(parseStatementToString("for (var a : b) {}"), "(foreach (declare 'a' (type 'var')) (identifier 'b') (block))");
  // Bad
  ASSERT_PARSE_BAD(parseStatementToString("for {"), "(1, 5): Expected '(' after 'for' keyword");
  ASSERT_PARSE_BAD(parseStatementToString("for ("), "(1, 6): Expected simple statement after '(' in 'for' statement");
  ASSERT_PARSE_BAD(parseStatementToString("for (;"), "(1, 7): Expected condition expression as second clause in 'for' statement");
  ASSERT_PARSE_BAD(parseStatementToString("for (;;"), "(1, 8): Expected simple statement as third clause in 'for' statement");
  ASSERT_PARSE_BAD(parseStatementToString("for (;;)"), "(1, 9): Expected '{' after ')' in 'for' statement");
  ASSERT_PARSE_BAD(parseStatementToString("for (;;) do"), "(1, 10): Expected '{' after ')' in 'for' statement");
  ASSERT_PARSE_BAD(parseStatementToString("for (;;) {"), "(1, 11): Expected statement");
}

TEST(TestEggSyntaxParser, StatementIf) {
  // Good
  ASSERT_PARSE_GOOD(parseStatementToString("if (a) {}"), "(if (identifier 'a') (block))");
  ASSERT_PARSE_GOOD(parseStatementToString("if (a) {} else {}"), "(if (identifier 'a') (block) (block))");
  // Bad
  ASSERT_PARSE_BAD(parseStatementToString("if {"), "(1, 4): Expected '(' after 'if' keyword");
  ASSERT_PARSE_BAD(parseStatementToString("if ("), "(1, 5): Expected condition expression after '(' in 'if' statement");
  ASSERT_PARSE_BAD(parseStatementToString("if ()"), "(1, 5): Expected condition expression after '(' in 'if' statement");
  ASSERT_PARSE_BAD(parseStatementToString("if (a"), "(1, 6): Expected ')' after 'if' condition expression");
  ASSERT_PARSE_BAD(parseStatementToString("if (a)"), "(1, 7): Expected '{' after ')' in 'if' statement");
  ASSERT_PARSE_BAD(parseStatementToString("if (a) do"), "(1, 8): Expected '{' after ')' in 'if' statement");
  ASSERT_PARSE_BAD(parseStatementToString("if (a) {"), "(1, 9): Expected statement");
  ASSERT_PARSE_BAD(parseStatementToString("if (a) {} else"), "(1, 15): Expected '{' after 'else' in 'if' statement");
  ASSERT_PARSE_BAD(parseStatementToString("if (a) {} else do"), "(1, 16): Expected '{' after 'else' in 'if' statement");
  ASSERT_PARSE_BAD(parseStatementToString("if (a) {} else {"), "(1, 17): Expected statement");
}

TEST(TestEggSyntaxParser, StatementReturn) {
  // Good
  ASSERT_PARSE_GOOD(parseStatementToString("return;"), "(return)");
  ASSERT_PARSE_GOOD(parseStatementToString("return a;"), "(return (identifier 'a'))");
  ASSERT_PARSE_GOOD(parseStatementToString("return a, b;"), "(return (identifier 'a') (identifier 'b'))");
  ASSERT_PARSE_GOOD(parseStatementToString("return a, b, c;"), "(return (identifier 'a') (identifier 'b') (identifier 'c'))");
  ASSERT_PARSE_GOOD(parseStatementToString("return ...a;"), "(return (unary '...' (identifier 'a')))");
  // Bad
  ASSERT_PARSE_BAD(parseStatementToString("return"), "(1, 7): Expected ';' at end of 'return' statement");
  ASSERT_PARSE_BAD(parseStatementToString("return a"), "(1, 9): Expected ';' at end of 'return' statement");
  ASSERT_PARSE_BAD(parseStatementToString("return a,"), "(1, 10): Expected expression after ',' in 'return' statement");
  ASSERT_PARSE_BAD(parseStatementToString("return a b"), "(1, 10): Expected ';' at end of 'return' statement");
  ASSERT_PARSE_BAD(parseStatementToString("return a,;"), "(1, 10): Expected expression after ',' in 'return' statement");
  ASSERT_PARSE_BAD(parseStatementToString("return ..."), "(1, 11): Expected expression after '...' in 'return' statement");
  ASSERT_PARSE_BAD(parseStatementToString("return ...;"), "(1, 11): Expected expression after '...' in 'return' statement");
  ASSERT_PARSE_BAD(parseStatementToString("return ...a"), "(1, 12): Expected ';' at end of 'return' statement");
  ASSERT_PARSE_BAD(parseStatementToString("return ...a,"), "(1, 12): Expected ';' at end of 'return' statement");
}

TEST(TestEggSyntaxParser, StatementSwitch) {
  // Good
  ASSERT_PARSE_GOOD(parseStatementToString("switch (a) {}"), "(switch (identifier 'a') (block))");
  // Bad
  ASSERT_PARSE_BAD(parseStatementToString("switch {}"), "(1, 8): Expected '(' after 'switch' keyword");
  ASSERT_PARSE_BAD(parseStatementToString("switch () {}"), "(1, 9): Expected condition expression after '(' in 'switch' statement");
  ASSERT_PARSE_BAD(parseStatementToString("switch (a {}"), "(1, 11): Expected ')' after 'switch' condition expression");
  ASSERT_PARSE_BAD(parseStatementToString("switch (a) }"), "(1, 12): Expected '{' after ')' in 'switch' statement");
}

TEST(TestEggSyntaxParser, StatementThrow) {
  // Good
  ASSERT_PARSE_GOOD(parseStatementToString("throw;"), "(throw)");
  ASSERT_PARSE_GOOD(parseStatementToString("throw a;"), "(throw (identifier 'a'))");
  // Bad
  ASSERT_PARSE_BAD(parseStatementToString("throw"), "(1, 6): Expected expression or ';' after 'throw' keyword");
  ASSERT_PARSE_BAD(parseStatementToString("throw a"), "(1, 8): Expected ';' at end of 'throw' statement");
  ASSERT_PARSE_BAD(parseStatementToString("throw a,"), "(1, 8): Expected ';' at end of 'throw' statement");
  ASSERT_PARSE_BAD(parseStatementToString("throw a b"), "(1, 9): Expected ';' at end of 'throw' statement");
  ASSERT_PARSE_BAD(parseStatementToString("throw a,;"), "(1, 8): Expected ';' at end of 'throw' statement");
  ASSERT_PARSE_BAD(parseStatementToString("throw a, b;"), "(1, 8): Expected ';' at end of 'throw' statement");
  ASSERT_PARSE_BAD(parseStatementToString("throw ...a"), "(1, 7): Expected expression or ';' after 'throw' keyword");
}

TEST(TestEggSyntaxParser, StatementTry) {
  // Good
  ASSERT_PARSE_GOOD(parseStatementToString("try {} catch (object a) {}"), "(try (block) (catch 'a' (type 'object') (block)))");
  ASSERT_PARSE_GOOD(parseStatementToString("try {} finally {}"), "(try (block) (finally (block)))");
  ASSERT_PARSE_GOOD(parseStatementToString("try {} catch (object a) {} finally {}"), "(try (block) (catch 'a' (type 'object') (block)) (finally (block)))");
  ASSERT_PARSE_GOOD(parseStatementToString("try {} catch (string a) {} catch (object b) {}"), "(try (block) (catch 'a' (type 'string') (block)) (catch 'b' (type 'object') (block)))");
  ASSERT_PARSE_GOOD(parseStatementToString("try {} catch (string a) {} catch (object b) {} finally {}"), "(try (block) (catch 'a' (type 'string') (block)) (catch 'b' (type 'object') (block)) (finally (block)))");
  // Bad
  ASSERT_PARSE_BAD(parseStatementToString("catch"), "(1, 1): Unexpected 'catch' clause without matching 'try'");
  ASSERT_PARSE_BAD(parseStatementToString("finally"), "(1, 1): Unexpected 'finally' clause without matching 'try'");
  ASSERT_PARSE_BAD(parseStatementToString("try"), "(1, 4): Expected '{' after 'try' keyword");
  ASSERT_PARSE_BAD(parseStatementToString("try catch"), "(1, 5): Expected '{' after 'try' keyword");
  ASSERT_PARSE_BAD(parseStatementToString("try {"), "(1, 6): Expected statement");
  ASSERT_PARSE_BAD(parseStatementToString("try {}"), "(1, 7): Expected at least one 'catch' or 'finally' clause in 'try' statement");
  ASSERT_PARSE_BAD(parseStatementToString("try {} catch"), "(1, 13): Expected '(' after 'catch' keyword in 'try' statement");
  ASSERT_PARSE_BAD(parseStatementToString("try {} catch {"), "(1, 14): Expected '(' after 'catch' keyword in 'try' statement");
  ASSERT_PARSE_BAD(parseStatementToString("try {} catch ("), "(1, 15): Expected exception type after '(' in 'catch' clause of 'try' statement");
  ASSERT_PARSE_BAD(parseStatementToString("try {} catch (object"), "(1, 21): Expected identifier after exception type in 'catch' clause of 'try' statement");
  ASSERT_PARSE_BAD(parseStatementToString("try {} catch (object)"), "(1, 21): Expected identifier after exception type in 'catch' clause of 'try' statement");
  ASSERT_PARSE_BAD(parseStatementToString("try {} catch (object a"), "(1, 23): Expected ')' after identifier in 'catch' clause of 'try' statement");
  ASSERT_PARSE_BAD(parseStatementToString("try {} catch (object a)"), "(1, 24): Expected '{' after 'catch' clause of 'try' statement");
  ASSERT_PARSE_BAD(parseStatementToString("try {} catch (object a) {"), "(1, 26): Expected statement");
  ASSERT_PARSE_BAD(parseStatementToString("try {} catch (object a) {} finally"), "(1, 35): Expected '{' after 'finally' keyword of 'try' statement");
  ASSERT_PARSE_BAD(parseStatementToString("try {} catch (object a) {} finally {"), "(1, 37): Expected statement");
  ASSERT_PARSE_BAD(parseStatementToString("try {} catch (object a) {} finally {} catch"), "(1, 39): Unexpected 'catch' clause after 'finally' clause in 'try' statement");
  ASSERT_PARSE_BAD(parseStatementToString("try {} catch (object a) {} finally {} finally"), "(1, 39): Unexpected second 'finally' clause in 'try' statement");
  ASSERT_PARSE_BAD(parseStatementToString("try {} finally {} finally"), "(1, 19): Unexpected second 'finally' clause in 'try' statement");
}

TEST(TestEggSyntaxParser, StatementWhile) {
  // Good
  ASSERT_PARSE_GOOD(parseStatementToString("while (a) {}"), "(while (identifier 'a') (block))");
  // Bad
  ASSERT_PARSE_BAD(parseStatementToString("while {"), "(1, 7): Expected '(' after 'while' keyword");
  ASSERT_PARSE_BAD(parseStatementToString("while ("), "(1, 8): Expected condition expression after '(' in 'while' statement");
  ASSERT_PARSE_BAD(parseStatementToString("while ()"), "(1, 8): Expected condition expression after '(' in 'while' statement");
  ASSERT_PARSE_BAD(parseStatementToString("while (a"), "(1, 9): Expected ')' after 'while' condition expression");
  ASSERT_PARSE_BAD(parseStatementToString("while (a)"), "(1, 10): Expected '{' after ')' in 'while' statement");
  ASSERT_PARSE_BAD(parseStatementToString("while (a) do"), "(1, 11): Expected '{' after ')' in 'while' statement");
  ASSERT_PARSE_BAD(parseStatementToString("while (a) {"), "(1, 12): Expected statement");
}

TEST(TestEggSyntaxParser, StatementUsing) {
  // Good
  ASSERT_PARSE_GOOD(parseStatementToString("using (a) {}"), "(using (identifier 'a') (block))");
  ASSERT_PARSE_GOOD(parseStatementToString("using (var a = b) {}"), "(using (initialize 'a' (type 'var') (identifier 'b')) (block))");
  // Bad
  ASSERT_PARSE_BAD(parseStatementToString("using {"), "(1, 7): Expected '(' after 'using' keyword");
  ASSERT_PARSE_BAD(parseStatementToString("using ("), "(1, 8): Expected expression or type after '(' in 'using' statement");
  ASSERT_PARSE_BAD(parseStatementToString("using ()"), "(1, 8): Expected expression or type after '(' in 'using' statement");
  ASSERT_PARSE_BAD(parseStatementToString("using (a"), "(1, 9): Expected ')' after expression in 'using' statement");
  ASSERT_PARSE_BAD(parseStatementToString("using (a) do"), "(1, 11): Expected '{' after ')' in 'using' statement");
  ASSERT_PARSE_BAD(parseStatementToString("using (a) {"), "(1, 12): Expected statement");
  ASSERT_PARSE_BAD(parseStatementToString("using (var)"), "(1, 11): Expected variable identifier after type in 'using' statement");
  ASSERT_PARSE_BAD(parseStatementToString("using (var a)"), "(1, 13): Expected '=' after variable identifier in 'using' statement");
  ASSERT_PARSE_BAD(parseStatementToString("using (var a =)"), "(1, 15): Expected expression after '=' in 'using' statement");
  ASSERT_PARSE_BAD(parseStatementToString("using (var a = b) do"), "(1, 19): Expected '{' after ')' in 'using' statement");
  ASSERT_PARSE_BAD(parseStatementToString("using (var a = b) {"), "(1, 20): Expected statement");
}

TEST(TestEggSyntaxParser, StatementYield) {
  // Good
  ASSERT_PARSE_GOOD(parseStatementToString("yield a;"), "(yield (identifier 'a'))");
  ASSERT_PARSE_GOOD(parseStatementToString("yield ...a;"), "(yield (unary '...' (identifier 'a')))");
  // Bad
  ASSERT_PARSE_BAD(parseStatementToString("yield"), "(1, 6): Expected expression in 'yield' statement");
  ASSERT_PARSE_BAD(parseStatementToString("yield;"), "(1, 6): Expected expression in 'yield' statement");
  ASSERT_PARSE_BAD(parseStatementToString("yield a"), "(1, 8): Expected ';' at end of 'yield' statement");
  ASSERT_PARSE_BAD(parseStatementToString("yield a,"), "(1, 8): Expected ';' at end of 'yield' statement");
  ASSERT_PARSE_BAD(parseStatementToString("yield a b"), "(1, 9): Expected ';' at end of 'yield' statement");
  ASSERT_PARSE_BAD(parseStatementToString("yield a,;"), "(1, 8): Expected ';' at end of 'yield' statement");
  ASSERT_PARSE_BAD(parseStatementToString("yield a, b;"), "(1, 8): Expected ';' at end of 'yield' statement");
  ASSERT_PARSE_BAD(parseStatementToString("yield ..."), "(1, 10): Expected expression after '...' in 'yield' statement");
  ASSERT_PARSE_BAD(parseStatementToString("yield ...;"), "(1, 10): Expected expression after '...' in 'yield' statement");
  ASSERT_PARSE_BAD(parseStatementToString("yield ...a"), "(1, 11): Expected ';' at end of 'yield' statement");
  ASSERT_PARSE_BAD(parseStatementToString("yield ...a,"), "(1, 11): Expected ';' at end of 'yield' statement");
}

TEST(TestEggSyntaxParser, Vexatious) {
  ASSERT_PARSE_GOOD(parseExpressionToString("a--b"), "(binary '-' (identifier 'a') (unary '-' (identifier 'b')))");
  ASSERT_PARSE_GOOD(parseExpressionToString("a--1"), "(binary '-' (identifier 'a') (unary '-' (literal int 1)))");
  ASSERT_PARSE_GOOD(parseExpressionToString("-1"), "(unary '-' (literal int 1))");
}

TEST(TestEggSyntaxParser, ExampleFile) {
  auto lexer = LexerFactory::createFromPath("~/cpp/test/data/example.egg"); //TODO
  auto tokenizer = EggTokenizerFactory::createFromLexer(lexer);
  auto parser = EggParserFactory::createModuleSyntaxParser();
  auto root = parser->parse(*tokenizer);
  root->dump(std::cout);
  std::cout << std::endl;
}
