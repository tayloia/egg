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
  void lexerStepInteger(ILexer& lexer, std::string expected_verbatim, int expected_value) {
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

TEST(TestLexers, Factory) {
  FileTextStream stream("~/cpp/test/data/example.egg");
  auto lexer = LexerFactory::createFromTextStream(stream);
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

