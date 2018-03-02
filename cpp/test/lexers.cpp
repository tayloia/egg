#include "test.h"

using namespace egg::yolk;

namespace {
  LexerValue lexerStep(ILexer& lexer, LexerKind expected_kind, std::string expected_verbatim) {
    // Cannot use ASSERT_EQ here because we're returning a value
    LexerItem item;
    EXPECT_EQ(expected_kind, lexer.next(item));
    EXPECT_EQ(expected_kind, item.kind);
    EXPECT_EQ(expected_verbatim, item.verbatim);
    return item.value;
  }
  void lexerStepWhitespace(ILexer& lexer, std::string expected_verbatim) {
    auto value = lexerStep(lexer, LexerKind::Whitespace, expected_verbatim);
    ASSERT_TRUE(value.s.empty());
  }
  void lexerStepComment(ILexer& lexer, std::string expected_verbatim) {
    auto value = lexerStep(lexer, LexerKind::Comment, expected_verbatim);
    ASSERT_TRUE(value.s.empty());
  }
  void lexerStepInteger(ILexer& lexer, std::string expected_verbatim, uint64_t expected_value) {
    auto value = lexerStep(lexer, LexerKind::Integer, expected_verbatim);
    ASSERT_EQ(expected_value, value.i);
    ASSERT_TRUE(value.s.empty());
  }
  void lexerStepFloat(ILexer& lexer, std::string expected_verbatim, double expected_value) {
    auto value = lexerStep(lexer, LexerKind::Float, expected_verbatim);
    ASSERT_EQ(expected_value, value.f);
    ASSERT_TRUE(value.s.empty());
  }
  void lexerStepString(ILexer& lexer, std::string expected_verbatim, std::u32string expected_value) {
    auto value = lexerStep(lexer, LexerKind::String, expected_verbatim);
    ASSERT_EQ(expected_value, value.s);
  }
  void lexerStepOperator(ILexer& lexer, std::string expected_verbatim) {
    auto value = lexerStep(lexer, LexerKind::Operator, expected_verbatim);
    ASSERT_TRUE(value.s.empty());
  }
  void lexerStepIdentifier(ILexer& lexer, std::string expected_verbatim) {
    auto value = lexerStep(lexer, LexerKind::Identifier, expected_verbatim);
    ASSERT_TRUE(value.s.empty());
  }
  void lexerStepEndOfFile(ILexer& lexer) {
    auto value = lexerStep(lexer, LexerKind::EndOfFile, "");
    ASSERT_TRUE(value.s.empty());
  }
  void lexerThrowContains(ILexer& lexer, const std::string& needle) {
    LexerItem item;
    ASSERT_THROW_E(lexer.next(item), Exception, ASSERT_CONTAINS(e.reason(), needle))
  }
}

TEST(TestLexers, Verbatim) {
  std::string path = "~/cpp/test/data/example.egg";
  std::string slurped;
  FileTextStream(path).slurp(slurped);
  std::string verbatim;
  auto lexer = LexerFactory::createFromPath(path);
  LexerItem item;
  while (lexer->next(item) != LexerKind::EndOfFile) {
    verbatim.append(item.verbatim);
  }
  ASSERT_EQ("", item.verbatim);
  ASSERT_EQ(slurped, verbatim);
}

TEST(TestLexers, Comment) {
  auto lexer = LexerFactory::createFromString("// Comment");
  lexerStepComment(*lexer, "// Comment");
  lexer = LexerFactory::createFromString("// Comment\n...");
  lexerStepComment(*lexer, "// Comment\n");
  lexer = LexerFactory::createFromString("/* Comment */...");
  lexerStepComment(*lexer, "/* Comment */");
  lexer = LexerFactory::createFromString("/* Multiline \n Comment */...");
  lexerStepComment(*lexer, "/* Multiline \n Comment */");
}

TEST(TestLexers, CommentBad) {
  auto lexer = LexerFactory::createFromString("/* Comment");
  lexerThrowContains(*lexer, "Unexpected end of file found in comment");
}

TEST(TestLexers, Integer) {
  auto lexer = LexerFactory::createFromString("0");
  lexerStepInteger(*lexer, "0", 0);
  lexer = LexerFactory::createFromString("123");
  lexerStepInteger(*lexer, "123", 123);
  lexer = LexerFactory::createFromString("0x0");
  lexerStepInteger(*lexer, "0x0", 0);
  lexer = LexerFactory::createFromString("0x123");
  lexerStepInteger(*lexer, "0x123", 0x123);
}

TEST(TestLexers, IntegerBad) {
  auto lexer = LexerFactory::createFromString("00");
  lexerThrowContains(*lexer, "Invalid integer constant (extraneous leading '0')");
  lexer = LexerFactory::createFromString("01");
  lexerThrowContains(*lexer, "Invalid integer constant (extraneous leading '0')");
  lexer = LexerFactory::createFromString("123xxx");
  lexerThrowContains(*lexer, "Unexpected letter in integer constant");
  lexer = LexerFactory::createFromString("123456789012345678901234567890");
  lexerThrowContains(*lexer, "Invalid integer constant");
  lexer = LexerFactory::createFromString("0x");
  lexerThrowContains(*lexer, "Truncated hexadecimal constant");
  lexer = LexerFactory::createFromString("0x0123456789ABCDEF0");
  lexerThrowContains(*lexer, "Hexadecimal constant too long");
  lexer = LexerFactory::createFromString("0x0Z");
  lexerThrowContains(*lexer, "Unexpected letter in hexadecimal constant");
}

TEST(TestLexers, Float) {
  auto lexer = LexerFactory::createFromString("0.0");
  lexerStepFloat(*lexer, "0.0", 0.0);
  lexer = LexerFactory::createFromString("1.0");
  lexerStepFloat(*lexer, "1.0", 1.0);
  lexer = LexerFactory::createFromString("1.000000");
  lexerStepFloat(*lexer, "1.000000", 1.0);
  lexer = LexerFactory::createFromString("1.23");
  lexerStepFloat(*lexer, "1.23", 1.23);
  lexer = LexerFactory::createFromString("1e3");
  lexerStepFloat(*lexer, "1e3", 1e3);
  lexer = LexerFactory::createFromString("1.2e3");
  lexerStepFloat(*lexer, "1.2e3", 1.2e3);
  lexer = LexerFactory::createFromString("1.2E03");
  lexerStepFloat(*lexer, "1.2E03", 1.2E03);
  lexer = LexerFactory::createFromString("1.2e+03");
  lexerStepFloat(*lexer, "1.2e+03", 1.2e+03);
  lexer = LexerFactory::createFromString("1.2e-03");
  lexerStepFloat(*lexer, "1.2e-03", 1.2e-03);
}

TEST(TestLexers, FloatBad) {
  double value = -123;
  auto lexer = LexerFactory::createFromString("1.");
  lexerThrowContains(*lexer, "Expected digit to follow decimal point in floating-point constant");
  lexer = LexerFactory::createFromString("1.0xxx");
  lexerThrowContains(*lexer, "Unexpected letter in floating-point constant");
  lexer = LexerFactory::createFromString("1.23xxx");
  lexerThrowContains(*lexer, "Unexpected letter in floating-point constant");
  lexer = LexerFactory::createFromString("1e3xxx");
  lexerThrowContains(*lexer, "Unexpected letter in exponent of floating-point constant");
  lexer = LexerFactory::createFromString("1.2e3xxx");
  lexerThrowContains(*lexer, "Unexpected letter in exponent of floating-point constant");
  lexer = LexerFactory::createFromString("1.2e+xx");
  lexerThrowContains(*lexer, "Expected digit in exponent of floating-point constant");
  lexer = LexerFactory::createFromString("1e-999");
  lexerThrowContains(*lexer, "Invalid floating-point constant");
  lexer = LexerFactory::createFromString("1e+999");
  lexerThrowContains(*lexer, "Invalid floating-point constant");
  lexer = LexerFactory::createFromString("1e999");
  lexerThrowContains(*lexer, "Invalid floating-point constant");
  ASSERT_EQ(-123, value);
}

TEST(TestLexers, Factory) {
  auto lexer = LexerFactory::createFromPath("~/cpp/test/data/example.egg");
  // "// This is a test file\r\n"
  lexerStepComment(*lexer, "// This is a test file\r\n");
  // "var result = first--second;"
  lexerStepIdentifier(*lexer, "var");
  lexerStepWhitespace(*lexer, " ");
  lexerStepIdentifier(*lexer, "result");
  lexerStepWhitespace(*lexer, " ");
  lexerStepOperator(*lexer, "=");
  lexerStepWhitespace(*lexer, " ");
  lexerStepIdentifier(*lexer, "first");
  lexerStepOperator(*lexer, "--");
  lexerStepIdentifier(*lexer, "second");
  lexerStepOperator(*lexer, ";");
  lexerStepWhitespace(*lexer, "\r\n");
  // "string greeting="Hello World";"
  lexerStepIdentifier(*lexer, "string");
  lexerStepWhitespace(*lexer, " ");
  lexerStepIdentifier(*lexer, "greeting");
  lexerStepOperator(*lexer, "=");
  lexerStepString(*lexer, "\"Hello World\"", U"Hello World");
  lexerStepOperator(*lexer, ";");
  lexerStepWhitespace(*lexer, "\r\n");
  // "greeting=`Hello\r\nWorld`;"
  lexerStepIdentifier(*lexer, "greeting");
  lexerStepOperator(*lexer, "=");
  lexerStepString(*lexer, "`Hello\r\nWorld`", U"Hello\r\nWorld");
  lexerStepOperator(*lexer, ";");
  lexerStepWhitespace(*lexer, "\r\n");
  // "int answer = 42;"
  lexerStepIdentifier(*lexer, "int");
  lexerStepWhitespace(*lexer, " ");
  lexerStepIdentifier(*lexer, "answer");
  lexerStepWhitespace(*lexer, " ");
  lexerStepOperator(*lexer, "=");
  lexerStepWhitespace(*lexer, " ");
  lexerStepInteger(*lexer, "42", 42);
  lexerStepOperator(*lexer, ";");
  lexerStepWhitespace(*lexer, "\r\n");
  // EOF
  lexerStepEndOfFile(*lexer);
  lexerStepEndOfFile(*lexer);
}

