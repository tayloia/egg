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
    ASSERT_EQ(true, value.valid);
    ASSERT_EQ("", value.s);
  }
  void lexerStepComment(ILexer& lexer, std::string expected_verbatim) {
    auto value = lexerStep(lexer, LexerKind::Comment, expected_verbatim);
    ASSERT_EQ(true, value.valid);
    ASSERT_EQ("", value.s);
  }
  void lexerStepInt(ILexer& lexer, std::string expected_verbatim, int expected_value) {
    auto value = lexerStep(lexer, LexerKind::Int, expected_verbatim);
    ASSERT_EQ(expected_value, value.i);
    ASSERT_EQ("", value.s);
  }
  void lexerStepFloat(ILexer& lexer, std::string expected_verbatim, double expected_value) {
    auto value = lexerStep(lexer, LexerKind::Float, expected_verbatim);
    ASSERT_EQ(expected_value, value.f);
    ASSERT_EQ("", value.s);
  }
  void lexerStepString(ILexer& lexer, std::string expected_verbatim, std::string expected_value) {
    auto value = lexerStep(lexer, LexerKind::String, expected_verbatim);
    ASSERT_EQ(true, value.valid);
    ASSERT_EQ(expected_value, value.s);
  }
  void lexerStepOperator(ILexer& lexer, std::string expected_verbatim) {
    auto value = lexerStep(lexer, LexerKind::Operator, expected_verbatim);
    ASSERT_EQ(true, value.valid);
    ASSERT_EQ("", value.s);
  }
  void lexerStepIdentifier(ILexer& lexer, std::string expected_verbatim) {
    auto value = lexerStep(lexer, LexerKind::Identifier, expected_verbatim);
    ASSERT_EQ(true, value.valid);
    ASSERT_EQ("", value.s);
  }
  void lexerStepEndOfFile(ILexer& lexer) {
    auto value = lexerStep(lexer, LexerKind::EndOfFile, "");
    ASSERT_EQ(true, value.valid);
    ASSERT_EQ("", value.s);
  }
}

TEST(TestLexers, Verbatim) {
  //TODO FileTextStream("~/cpp/test/data/example.egg").slurp();
}

TEST(TestLexers, Factory) {
  FileTextStream stream("~/cpp/test/data/example.egg");
  auto lexer = LexerFactory::createFromTextStream(stream);
  lexerStepComment(*lexer, "// This is a test file\r\n");
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
  lexerStepEndOfFile(*lexer);
  lexerStepEndOfFile(*lexer);
}

