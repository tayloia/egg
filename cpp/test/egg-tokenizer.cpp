#include "test.h"

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
  ASSERT_EQ("any", EggTokenizerState::getKeywordString(EggTokenizerKeyword::Any));
  ASSERT_EQ("yield", EggTokenizerState::getKeywordString(EggTokenizerKeyword::Yield));
}

TEST(TestEggTokenizer, GetOperatorString) {
  ASSERT_EQ("&", EggTokenizerState::getOperatorString(EggTokenizerOperator::Ampersand));
  ASSERT_EQ(">>>=", EggTokenizerState::getOperatorString(EggTokenizerOperator::ShiftRightUnsignedEqual));
}

TEST(TestEggTokenizer, TryParseKeyword) {
  EggTokenizerKeyword keyword = EggTokenizerKeyword::Null;
  ASSERT_TRUE(EggTokenizerState::tryParseKeyword("any", keyword));
  ASSERT_EQ(keyword, EggTokenizerKeyword::Any);
  ASSERT_TRUE(EggTokenizerState::tryParseKeyword("yield", keyword));
  ASSERT_EQ(keyword, EggTokenizerKeyword::Yield);
  ASSERT_FALSE(EggTokenizerState::tryParseKeyword("", keyword));
  ASSERT_FALSE(EggTokenizerState::tryParseKeyword("unknown", keyword));
}

TEST(TestEggTokenizer, TryParseOperator) {
  EggTokenizerOperator op = EggTokenizerOperator::Bang;
  size_t length = 0;
  ASSERT_TRUE(EggTokenizerState::tryParseOperator("&x", op, length));
  ASSERT_EQ(op, EggTokenizerOperator::Ampersand);
  ASSERT_EQ(length, 1);
  ASSERT_TRUE(EggTokenizerState::tryParseOperator("--x", op, length));
  ASSERT_EQ(op, EggTokenizerOperator::MinusMinus);
  ASSERT_EQ(length, 2);
  ASSERT_TRUE(EggTokenizerState::tryParseOperator(">>>=", op, length));
  ASSERT_EQ(op, EggTokenizerOperator::ShiftRightUnsignedEqual);
  ASSERT_EQ(length, 4);
  ASSERT_FALSE(EggTokenizerState::tryParseOperator("", op, length));
  ASSERT_FALSE(EggTokenizerState::tryParseOperator("@", op, length));
}

TEST(TestEggTokenizer, EmptyFile) {
  EggTokenizerItem item;
  auto tokenizer = createFromString("");
  EggTokenizerState state = EggTokenizerState::ExpectNone;
  ASSERT_EQ(EggTokenizerKind::EndOfFile, tokenizer->next(item, state));
}

TEST(TestEggTokenizer, Comment) {
  EggTokenizerItem item;
  auto tokenizer = createFromString("// Comment\n0");
  EggTokenizerState state = EggTokenizerState::ExpectNone;
  ASSERT_EQ(EggTokenizerKind::Integer, tokenizer->next(item, state));
  tokenizer = createFromString("/* Comment */0");
  ASSERT_EQ(EggTokenizerKind::Integer, tokenizer->next(item, state));
}

TEST(TestEggTokenizer, Integer) {
  EggTokenizerItem item;
  auto tokenizer = createFromString("12345 -12345");
  EggTokenizerState state = EggTokenizerState::ExpectNumberLiteral;
  ASSERT_EQ(EggTokenizerKind::Integer, tokenizer->next(item, state));
  ASSERT_EQ(12345, item.value.i);
  ASSERT_EQ(EggTokenizerKind::Integer, tokenizer->next(item, state));
  ASSERT_EQ(-12345, item.value.i);
}

TEST(TestEggTokenizer, Float) {
  EggTokenizerItem item;
  auto tokenizer = createFromString("3.14159 -3.14159");
  EggTokenizerState state = EggTokenizerState::ExpectNumberLiteral;
  ASSERT_EQ(EggTokenizerKind::Float, tokenizer->next(item, state));
  ASSERT_EQ(3.14159, item.value.f);
  ASSERT_EQ(EggTokenizerKind::Float, tokenizer->next(item, state));
  ASSERT_EQ(-3.14159, item.value.f);
}

TEST(TestEggTokenizer, String) {
  EggTokenizerItem item;
  auto tokenizer = createFromString("\"hello\" `world`");
  EggTokenizerState state = EggTokenizerState::ExpectNone;
  ASSERT_EQ(EggTokenizerKind::String, tokenizer->next(item, state));
  ASSERT_EQ("hello", item.value.s);
  ASSERT_EQ(EggTokenizerKind::String, tokenizer->next(item, state));
  ASSERT_EQ("world", item.value.s);
}

TEST(TestEggTokenizer, Keyword) {
  EggTokenizerItem item;
  auto tokenizer = createFromString("null false true any bool int float string object yield");
  EggTokenizerState state = EggTokenizerState::ExpectNone;
  ASSERT_EQ(EggTokenizerKind::Keyword, tokenizer->next(item, state));
  ASSERT_EQ(EggTokenizerKeyword::Null, item.value.k);
  ASSERT_EQ(EggTokenizerKind::Keyword, tokenizer->next(item, state));
  ASSERT_EQ(EggTokenizerKeyword::False, item.value.k);
  ASSERT_EQ(EggTokenizerKind::Keyword, tokenizer->next(item, state));
  ASSERT_EQ(EggTokenizerKeyword::True, item.value.k);
  ASSERT_EQ(EggTokenizerKind::Keyword, tokenizer->next(item, state));
  ASSERT_EQ(EggTokenizerKeyword::Any, item.value.k);
  ASSERT_EQ(EggTokenizerKind::Keyword, tokenizer->next(item, state));
  ASSERT_EQ(EggTokenizerKeyword::Bool, item.value.k);
  ASSERT_EQ(EggTokenizerKind::Keyword, tokenizer->next(item, state));
  ASSERT_EQ(EggTokenizerKeyword::Int, item.value.k);
  ASSERT_EQ(EggTokenizerKind::Keyword, tokenizer->next(item, state));
  ASSERT_EQ(EggTokenizerKeyword::Float, item.value.k);
  ASSERT_EQ(EggTokenizerKind::Keyword, tokenizer->next(item, state));
  ASSERT_EQ(EggTokenizerKeyword::String, item.value.k);
  ASSERT_EQ(EggTokenizerKind::Keyword, tokenizer->next(item, state));
  ASSERT_EQ(EggTokenizerKeyword::Object, item.value.k);
  ASSERT_EQ(EggTokenizerKind::Keyword, tokenizer->next(item, state));
  ASSERT_EQ(EggTokenizerKeyword::Yield, item.value.k);
  ASSERT_EQ(EggTokenizerKind::EndOfFile, tokenizer->next(item, state));
}

TEST(TestEggTokenizer, Operator) {
  EggTokenizerItem item;
  auto tokenizer = createFromString("!??->>>>=~ $");
  EggTokenizerState state = EggTokenizerState::ExpectNone;
  ASSERT_EQ(EggTokenizerKind::Operator, tokenizer->next(item, state));
  ASSERT_EQ(EggTokenizerOperator::Bang, item.value.o);
  ASSERT_EQ(EggTokenizerKind::Operator, tokenizer->next(item, state));
  ASSERT_EQ(EggTokenizerOperator::QueryQuery, item.value.o);
  ASSERT_EQ(EggTokenizerKind::Operator, tokenizer->next(item, state));
  ASSERT_EQ(EggTokenizerOperator::Lambda, item.value.o);
  ASSERT_EQ(EggTokenizerKind::Operator, tokenizer->next(item, state));
  ASSERT_EQ(EggTokenizerOperator::ShiftRightUnsignedEqual, item.value.o);
  ASSERT_EQ(EggTokenizerKind::Operator, tokenizer->next(item, state));
  ASSERT_EQ(EggTokenizerOperator::Tilde, item.value.o);
  ASSERT_THROW_E(tokenizer->next(item, state), Exception, ASSERT_CONTAINS(e.what(), "Unexpected character: '$'"));
}

TEST(TestEggTokenizer, Identifier) {
  EggTokenizerItem item;
  auto tokenizer = createFromString("unknown _");
  EggTokenizerState state = EggTokenizerState::ExpectNone;
  ASSERT_EQ(EggTokenizerKind::Identifier, tokenizer->next(item, state));
  ASSERT_EQ("unknown", item.value.s);
  ASSERT_EQ(EggTokenizerKind::Identifier, tokenizer->next(item, state));
  ASSERT_EQ("_", item.value.s);
  ASSERT_EQ(EggTokenizerKind::EndOfFile, tokenizer->next(item, state));
}

TEST(TestEggTokenizer, Attribute) {
  EggTokenizerItem item;
  auto tokenizer = createFromString("@test @and.this .@@twice(2)");
  EggTokenizerState state = EggTokenizerState::ExpectNone;
  ASSERT_EQ(EggTokenizerKind::Attribute, tokenizer->next(item, state));
  ASSERT_EQ("@test", item.value.s);
  ASSERT_EQ(EggTokenizerKind::Attribute, tokenizer->next(item, state));
  ASSERT_EQ("@and.this", item.value.s);
  ASSERT_EQ(EggTokenizerKind::Operator, tokenizer->next(item, state));
  ASSERT_EQ(EggTokenizerOperator::Dot, item.value.o);
  ASSERT_EQ(EggTokenizerKind::Attribute, tokenizer->next(item, state));
  ASSERT_EQ("@@twice", item.value.s);
}

TEST(TestEggTokenizer, Vexatious) {
  EggTokenizerItem item;

  // Parsed as "-|-|x"
  auto tokenizer = createFromString("--x");
  EggTokenizerState state = EggTokenizerState::ExpectNone;
  ASSERT_EQ(EggTokenizerKind::Operator, tokenizer->next(item, state));
  ASSERT_EQ(EggTokenizerOperator::Minus, item.value.o);
  ASSERT_EQ(EggTokenizerKind::Operator, tokenizer->next(item, state));
  ASSERT_EQ(EggTokenizerOperator::Minus, item.value.o);
  ASSERT_EQ(EggTokenizerKind::Identifier, tokenizer->next(item, state));
  ASSERT_EQ("x", item.value.s);
  ASSERT_EQ(EggTokenizerKind::EndOfFile, tokenizer->next(item, state));

  // Parsed as "--|x"
  tokenizer = createFromString("--x");
  state = EggTokenizerState::ExpectIncrement;
  ASSERT_EQ(EggTokenizerKind::Operator, tokenizer->next(item, state));
  ASSERT_EQ(EggTokenizerOperator::MinusMinus, item.value.o);
  ASSERT_EQ(EggTokenizerKind::Identifier, tokenizer->next(item, state));
  ASSERT_EQ("x", item.value.s);
  ASSERT_EQ(EggTokenizerKind::EndOfFile, tokenizer->next(item, state));

  // Parsed as "-|-|x"
  tokenizer = createFromString("--x");
  state = EggTokenizerState::ExpectNumberLiteral;
  ASSERT_EQ(EggTokenizerKind::Operator, tokenizer->next(item, state));
  ASSERT_EQ(EggTokenizerOperator::Minus, item.value.o);
  ASSERT_EQ(EggTokenizerKind::Operator, tokenizer->next(item, state));
  ASSERT_EQ(EggTokenizerOperator::Minus, item.value.o);
  ASSERT_EQ(EggTokenizerKind::Identifier, tokenizer->next(item, state));
  ASSERT_EQ("x", item.value.s);
  ASSERT_EQ(EggTokenizerKind::EndOfFile, tokenizer->next(item, state));

  // Parsed as "x|-|-|1"
  tokenizer = createFromString("x--1");
  state = EggTokenizerState::ExpectNone;
  ASSERT_EQ(EggTokenizerKind::Identifier, tokenizer->next(item, state));
  ASSERT_EQ("x", item.value.s);
  ASSERT_EQ(EggTokenizerKind::Operator, tokenizer->next(item, state));
  ASSERT_EQ(EggTokenizerOperator::Minus, item.value.o);
  ASSERT_EQ(EggTokenizerKind::Operator, tokenizer->next(item, state));
  ASSERT_EQ(EggTokenizerOperator::Minus, item.value.o);
  ASSERT_EQ(EggTokenizerKind::Integer, tokenizer->next(item, state));
  ASSERT_EQ(1, item.value.i);
  ASSERT_EQ(EggTokenizerKind::EndOfFile, tokenizer->next(item, state));

  // Parsed as "x|--|1"
  tokenizer = createFromString("x--1");
  state = EggTokenizerState::ExpectIncrement;
  ASSERT_EQ(EggTokenizerKind::Identifier, tokenizer->next(item, state));
  ASSERT_EQ("x", item.value.s);
  ASSERT_EQ(EggTokenizerKind::Operator, tokenizer->next(item, state));
  ASSERT_EQ(EggTokenizerOperator::MinusMinus, item.value.o);
  ASSERT_EQ(EggTokenizerKind::Integer, tokenizer->next(item, state));
  ASSERT_EQ(1, item.value.i);
  ASSERT_EQ(EggTokenizerKind::EndOfFile, tokenizer->next(item, state));

  // Parsed as "x|-|-1"
  tokenizer = createFromString("x--1");
  state = EggTokenizerState::ExpectNumberLiteral;
  ASSERT_EQ(EggTokenizerKind::Identifier, tokenizer->next(item, state));
  ASSERT_EQ("x", item.value.s);
  ASSERT_EQ(EggTokenizerKind::Operator, tokenizer->next(item, state));
  ASSERT_EQ(EggTokenizerOperator::Minus, item.value.o);
  ASSERT_EQ(EggTokenizerKind::Integer, tokenizer->next(item, state));
  ASSERT_EQ(-1, item.value.i);
  ASSERT_EQ(EggTokenizerKind::EndOfFile, tokenizer->next(item, state));

  // Parsed as "-|1"
  tokenizer = createFromString("-1");
  state = EggTokenizerState::ExpectNone;
  ASSERT_EQ(EggTokenizerKind::Operator, tokenizer->next(item, state));
  ASSERT_EQ(EggTokenizerOperator::Minus, item.value.o);
  ASSERT_EQ(EggTokenizerKind::Integer, tokenizer->next(item, state));
  ASSERT_EQ(1, item.value.i);
  ASSERT_EQ(EggTokenizerKind::EndOfFile, tokenizer->next(item, state));

  // Parsed as "-|1"
  tokenizer = createFromString("-1");
  state = EggTokenizerState::ExpectIncrement;
  ASSERT_EQ(EggTokenizerKind::Operator, tokenizer->next(item, state));
  ASSERT_EQ(EggTokenizerOperator::Minus, item.value.o);
  ASSERT_EQ(EggTokenizerKind::Integer, tokenizer->next(item, state));
  ASSERT_EQ(1, item.value.i);
  ASSERT_EQ(EggTokenizerKind::EndOfFile, tokenizer->next(item, state));

  // Parsed as "-1"
  tokenizer = createFromString("-1");
  state = EggTokenizerState::ExpectNumberLiteral;
  ASSERT_EQ(EggTokenizerKind::Integer, tokenizer->next(item, state));
  ASSERT_EQ(-1, item.value.i);
  ASSERT_EQ(EggTokenizerKind::EndOfFile, tokenizer->next(item, state));
}

TEST(TestEggTokenizer, ExampleFile) {
  EggTokenizerItem item;
  auto tokenizer = createFromPath("~/cpp/test/data/example.egg");
  EggTokenizerState state = EggTokenizerState::ExpectNone;
  size_t count = 0;
  while (tokenizer->next(item, state) != EggTokenizerKind::EndOfFile) {
    count++;
  }
  ASSERT_EQ(22, count);
}
