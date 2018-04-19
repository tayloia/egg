#include "test.h"
#include "lexers.h"
#include "egg-tokenizer.h"

using namespace egg::yolk;

namespace {
  std::shared_ptr<IEggTokenizer> createFromString(const std::string& text) {
    auto lexer = LexerFactory::createFromString(text);
    return EggTokenizerFactory::createFromLexer(lexer);
  }
  std::shared_ptr<IEggTokenizer> createFromPath(const std::string& path) {
    auto lexer = LexerFactory::createFromPath(path);
    return EggTokenizerFactory::createFromLexer(lexer);
  }
}

TEST(TestEggTokenizer, GetKeywordString) {
  ASSERT_EQ("any", EggTokenizerValue::getKeywordString(EggTokenizerKeyword::Any));
  ASSERT_EQ("yield", EggTokenizerValue::getKeywordString(EggTokenizerKeyword::Yield));
}

TEST(TestEggTokenizer, GetOperatorString) {
  ASSERT_EQ("&", EggTokenizerValue::getOperatorString(EggTokenizerOperator::Ampersand));
  ASSERT_EQ(">>>=", EggTokenizerValue::getOperatorString(EggTokenizerOperator::ShiftRightUnsignedEqual));
}

TEST(TestEggTokenizer, TryParseKeyword) {
  EggTokenizerKeyword keyword = EggTokenizerKeyword::Null;
  ASSERT_TRUE(EggTokenizerValue::tryParseKeyword("any", keyword));
  ASSERT_EQ(keyword, EggTokenizerKeyword::Any);
  ASSERT_TRUE(EggTokenizerValue::tryParseKeyword("yield", keyword));
  ASSERT_EQ(keyword, EggTokenizerKeyword::Yield);
  ASSERT_FALSE(EggTokenizerValue::tryParseKeyword("", keyword));
  ASSERT_FALSE(EggTokenizerValue::tryParseKeyword("unknown", keyword));
}

TEST(TestEggTokenizer, TryParseOperator) {
  EggTokenizerOperator op = EggTokenizerOperator::Bang;
  size_t length = 0;
  ASSERT_TRUE(EggTokenizerValue::tryParseOperator("&x", op, length));
  ASSERT_EQ(op, EggTokenizerOperator::Ampersand);
  ASSERT_EQ(length, 1u);
  ASSERT_TRUE(EggTokenizerValue::tryParseOperator("--x", op, length));
  ASSERT_EQ(op, EggTokenizerOperator::MinusMinus);
  ASSERT_EQ(length, 2u);
  ASSERT_TRUE(EggTokenizerValue::tryParseOperator(">>>=", op, length));
  ASSERT_EQ(op, EggTokenizerOperator::ShiftRightUnsignedEqual);
  ASSERT_EQ(length, 4u);
  ASSERT_FALSE(EggTokenizerValue::tryParseOperator("", op, length));
  ASSERT_FALSE(EggTokenizerValue::tryParseOperator("@", op, length));
}

TEST(TestEggTokenizer, EmptyFile) {
  EggTokenizerItem item;
  auto tokenizer = createFromString("");
  ASSERT_EQ(EggTokenizerKind::EndOfFile, tokenizer->next(item));
}

TEST(TestEggTokenizer, Comment) {
  EggTokenizerItem item;
  auto tokenizer = createFromString("// Comment\n0");
  ASSERT_EQ(EggTokenizerKind::Integer, tokenizer->next(item));
  tokenizer = createFromString("/* Comment */0");
  ASSERT_EQ(EggTokenizerKind::Integer, tokenizer->next(item));
}

TEST(TestEggTokenizer, Integer) {
  EggTokenizerItem item;
  auto tokenizer = createFromString("12345 -12345");
  ASSERT_EQ(EggTokenizerKind::Integer, tokenizer->next(item));
  ASSERT_EQ(12345, item.value.i);
  ASSERT_EQ(EggTokenizerKind::Operator, tokenizer->next(item));
  ASSERT_EQ(EggTokenizerOperator::Minus, item.value.o);
  ASSERT_EQ(EggTokenizerKind::Integer, tokenizer->next(item));
  ASSERT_EQ(12345, item.value.i);
}

TEST(TestEggTokenizer, Float) {
  EggTokenizerItem item;
  auto tokenizer = createFromString("3.14159 -3.14159");
  ASSERT_EQ(EggTokenizerKind::Float, tokenizer->next(item));
  ASSERT_EQ(3.14159, item.value.f);
  ASSERT_EQ(EggTokenizerKind::Operator, tokenizer->next(item));
  ASSERT_EQ(EggTokenizerOperator::Minus, item.value.o);
  ASSERT_EQ(EggTokenizerKind::Float, tokenizer->next(item));
  ASSERT_EQ(3.14159, item.value.f);
}

TEST(TestEggTokenizer, String) {
  EggTokenizerItem item;
  auto tokenizer = createFromString("\"hello\" `world`");
  ASSERT_EQ(EggTokenizerKind::String, tokenizer->next(item));
  ASSERT_EQ("hello", item.value.s.toUTF8());
  ASSERT_EQ(EggTokenizerKind::String, tokenizer->next(item));
  ASSERT_EQ("world", item.value.s.toUTF8());
}

TEST(TestEggTokenizer, Keyword) {
  EggTokenizerItem item;
  auto tokenizer = createFromString("null false true any bool int float string object yield");
  ASSERT_EQ(EggTokenizerKind::Keyword, tokenizer->next(item));
  ASSERT_EQ(EggTokenizerKeyword::Null, item.value.k);
  ASSERT_EQ(EggTokenizerKind::Keyword, tokenizer->next(item));
  ASSERT_EQ(EggTokenizerKeyword::False, item.value.k);
  ASSERT_EQ(EggTokenizerKind::Keyword, tokenizer->next(item));
  ASSERT_EQ(EggTokenizerKeyword::True, item.value.k);
  ASSERT_EQ(EggTokenizerKind::Keyword, tokenizer->next(item));
  ASSERT_EQ(EggTokenizerKeyword::Any, item.value.k);
  ASSERT_EQ(EggTokenizerKind::Keyword, tokenizer->next(item));
  ASSERT_EQ(EggTokenizerKeyword::Bool, item.value.k);
  ASSERT_EQ(EggTokenizerKind::Keyword, tokenizer->next(item));
  ASSERT_EQ(EggTokenizerKeyword::Int, item.value.k);
  ASSERT_EQ(EggTokenizerKind::Keyword, tokenizer->next(item));
  ASSERT_EQ(EggTokenizerKeyword::Float, item.value.k);
  ASSERT_EQ(EggTokenizerKind::Keyword, tokenizer->next(item));
  ASSERT_EQ(EggTokenizerKeyword::String, item.value.k);
  ASSERT_EQ(EggTokenizerKind::Keyword, tokenizer->next(item));
  ASSERT_EQ(EggTokenizerKeyword::Object, item.value.k);
  ASSERT_EQ(EggTokenizerKind::Keyword, tokenizer->next(item));
  ASSERT_EQ(EggTokenizerKeyword::Yield, item.value.k);
  ASSERT_EQ(EggTokenizerKind::EndOfFile, tokenizer->next(item));
}

TEST(TestEggTokenizer, Operator) {
  EggTokenizerItem item;
  auto tokenizer = createFromString("!??->>>>=~ $");
  ASSERT_EQ(EggTokenizerKind::Operator, tokenizer->next(item));
  ASSERT_EQ(EggTokenizerOperator::Bang, item.value.o);
  ASSERT_EQ(EggTokenizerKind::Operator, tokenizer->next(item));
  ASSERT_EQ(EggTokenizerOperator::QueryQuery, item.value.o);
  ASSERT_EQ(EggTokenizerKind::Operator, tokenizer->next(item));
  ASSERT_EQ(EggTokenizerOperator::Lambda, item.value.o);
  ASSERT_EQ(EggTokenizerKind::Operator, tokenizer->next(item));
  ASSERT_EQ(EggTokenizerOperator::ShiftRightUnsignedEqual, item.value.o);
  ASSERT_EQ(EggTokenizerKind::Operator, tokenizer->next(item));
  ASSERT_EQ(EggTokenizerOperator::Tilde, item.value.o);
  ASSERT_THROW_E(tokenizer->next(item), Exception, ASSERT_CONTAINS(e.what(), "Unexpected character: '$'"));
}

TEST(TestEggTokenizer, Identifier) {
  EggTokenizerItem item;
  auto tokenizer = createFromString("unknown _");
  ASSERT_EQ(EggTokenizerKind::Identifier, tokenizer->next(item));
  ASSERT_EQ("unknown", item.value.s.toUTF8());
  ASSERT_EQ(EggTokenizerKind::Identifier, tokenizer->next(item));
  ASSERT_EQ("_", item.value.s.toUTF8());
  ASSERT_EQ(EggTokenizerKind::EndOfFile, tokenizer->next(item));
}

TEST(TestEggTokenizer, Attribute) {
  EggTokenizerItem item;
  auto tokenizer = createFromString("@test @and.this .@@twice(2)");
  ASSERT_EQ(EggTokenizerKind::Attribute, tokenizer->next(item));
  ASSERT_EQ("@test", item.value.s.toUTF8());
  ASSERT_EQ(EggTokenizerKind::Attribute, tokenizer->next(item));
  ASSERT_EQ("@and.this", item.value.s.toUTF8());
  ASSERT_EQ(EggTokenizerKind::Operator, tokenizer->next(item));
  ASSERT_EQ(EggTokenizerOperator::Dot, item.value.o);
  ASSERT_EQ(EggTokenizerKind::Attribute, tokenizer->next(item));
  ASSERT_EQ("@@twice", item.value.s.toUTF8());
}

TEST(TestEggTokenizer, Line) {
  EggTokenizerItem item;
  auto tokenizer = createFromString("1 2.3\r\n\r\n`hello\nworld` foo");
  ASSERT_EQ(EggTokenizerKind::Integer, tokenizer->next(item));
  ASSERT_EQ(1u, item.line);
  ASSERT_EQ(EggTokenizerKind::Float, tokenizer->next(item));
  ASSERT_EQ(1u, item.line);
  ASSERT_EQ(EggTokenizerKind::String, tokenizer->next(item));
  ASSERT_EQ(3u, item.line);
  ASSERT_EQ(EggTokenizerKind::Identifier, tokenizer->next(item));
  ASSERT_EQ(4u, item.line);
  ASSERT_EQ(EggTokenizerKind::EndOfFile, tokenizer->next(item));
  ASSERT_EQ(4u, item.line);
}

TEST(TestEggTokenizer, Column) {
  EggTokenizerItem item;
  auto tokenizer = createFromString("1 2.3 \"hello\" foo");
  ASSERT_EQ(EggTokenizerKind::Integer, tokenizer->next(item));
  ASSERT_EQ(1u, item.column);
  ASSERT_EQ(EggTokenizerKind::Float, tokenizer->next(item));
  ASSERT_EQ(3u, item.column);
  ASSERT_EQ(EggTokenizerKind::String, tokenizer->next(item));
  ASSERT_EQ(7u, item.column);
  ASSERT_EQ(EggTokenizerKind::Identifier, tokenizer->next(item));
  ASSERT_EQ(15u, item.column);
  ASSERT_EQ(EggTokenizerKind::EndOfFile, tokenizer->next(item));
  ASSERT_EQ(18u, item.column);
}

TEST(TestEggTokenizer, Vexatious) {
  EggTokenizerItem item;

  // Parsed as "--|x"
  auto tokenizer = createFromString("--x");
  ASSERT_EQ(EggTokenizerKind::Operator, tokenizer->next(item));
  ASSERT_EQ(EggTokenizerOperator::MinusMinus, item.value.o);
  ASSERT_EQ(EggTokenizerKind::Identifier, tokenizer->next(item));
  ASSERT_EQ("x", item.value.s.toUTF8());
  ASSERT_EQ(EggTokenizerKind::EndOfFile, tokenizer->next(item));

  // Parsed as "x|--|1"
  tokenizer = createFromString("x--1");
  ASSERT_EQ(EggTokenizerKind::Identifier, tokenizer->next(item));
  ASSERT_EQ("x", item.value.s.toUTF8());
  ASSERT_EQ(EggTokenizerKind::Operator, tokenizer->next(item));
  ASSERT_EQ(EggTokenizerOperator::MinusMinus, item.value.o);
  ASSERT_EQ(EggTokenizerKind::Integer, tokenizer->next(item));
  ASSERT_EQ(1, item.value.i);
  ASSERT_EQ(EggTokenizerKind::EndOfFile, tokenizer->next(item));

  // Parsed as "-|1"
  tokenizer = createFromString("-1");
  ASSERT_EQ(EggTokenizerKind::Operator, tokenizer->next(item));
  ASSERT_EQ(EggTokenizerOperator::Minus, item.value.o);
  ASSERT_EQ(EggTokenizerKind::Integer, tokenizer->next(item));
  ASSERT_EQ(1, item.value.i);
  ASSERT_EQ(EggTokenizerKind::EndOfFile, tokenizer->next(item));
}

TEST(TestEggTokenizer, ExampleFile) {
  EggTokenizerItem item;
  auto tokenizer = createFromPath("~/cpp/test/data/example.egg");
  size_t count = 0;
  while (tokenizer->next(item) != EggTokenizerKind::EndOfFile) {
    count++;
  }
  ASSERT_EQ(21u, count);
}
