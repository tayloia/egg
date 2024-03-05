#include "ovum/test.h"
#include "ovum/exception.h"
#include "ovum/lexer.h"
#include "ovum/egg-tokenizer.h"

using namespace egg::ovum;

namespace {
  std::shared_ptr<IEggTokenizer> createFromString(IAllocator& allocator, const std::string& text) {
    auto lexer = LexerFactory::createFromString(text);
    return EggTokenizerFactory::createFromLexer(allocator, lexer);
  }
  std::shared_ptr<IEggTokenizer> createFromPath(IAllocator& allocator, const std::string& path) {
    auto lexer = LexerFactory::createFromPath(path);
    return EggTokenizerFactory::createFromLexer(allocator, lexer);
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
  ASSERT_TRUE(EggTokenizerValue::tryParseOperator("??", op, length));
  ASSERT_EQ(op, EggTokenizerOperator::QueryQuery);
  ASSERT_EQ(length, 2u);
  ASSERT_TRUE(EggTokenizerValue::tryParseOperator("!!=", op, length));
  ASSERT_EQ(op, EggTokenizerOperator::BangBangEqual);
  ASSERT_EQ(length, 3u);
  ASSERT_FALSE(EggTokenizerValue::tryParseOperator("", op, length));
  ASSERT_FALSE(EggTokenizerValue::tryParseOperator("@", op, length));
}

TEST(TestEggTokenizer, EmptyFile) {
  egg::test::Allocator allocator{ egg::test::Allocator::Expectation::NoAllocations };
  auto tokenizer = createFromString(allocator, "");
  EggTokenizerItem item;
  ASSERT_EQ(EggTokenizerKind::EndOfFile, tokenizer->next(item));
}

TEST(TestEggTokenizer, Comment) {
  egg::test::Allocator allocator;
  auto tokenizer = createFromString(allocator, "// Comment\n0");
  EggTokenizerItem item;
  ASSERT_EQ(EggTokenizerKind::Integer, tokenizer->next(item));
  tokenizer = createFromString(allocator, "/* Comment */0");
  ASSERT_EQ(EggTokenizerKind::Integer, tokenizer->next(item));
}

TEST(TestEggTokenizer, Integer) {
  egg::test::Allocator allocator;
  auto tokenizer = createFromString(allocator, "12345 -12345");
  EggTokenizerItem item;
  ASSERT_EQ(EggTokenizerKind::Integer, tokenizer->next(item));
  ASSERT_EQ(12345, item.value.i);
  ASSERT_EQ(EggTokenizerKind::Operator, tokenizer->next(item));
  ASSERT_EQ(EggTokenizerOperator::Minus, item.value.o);
  ASSERT_EQ(EggTokenizerKind::Integer, tokenizer->next(item));
  ASSERT_EQ(12345, item.value.i);
}

TEST(TestEggTokenizer, Float) {
  egg::test::Allocator allocator;
  auto tokenizer = createFromString(allocator, "3.14159 -3.14159");
  EggTokenizerItem item;
  ASSERT_EQ(EggTokenizerKind::Float, tokenizer->next(item));
  ASSERT_EQ(3.14159, item.value.f);
  ASSERT_EQ(EggTokenizerKind::Operator, tokenizer->next(item));
  ASSERT_EQ(EggTokenizerOperator::Minus, item.value.o);
  ASSERT_EQ(EggTokenizerKind::Float, tokenizer->next(item));
  ASSERT_EQ(3.14159, item.value.f);
}

TEST(TestEggTokenizer, String) {
  egg::test::Allocator allocator;
  auto tokenizer = createFromString(allocator, "\"hello\" `world`");
  EggTokenizerItem item;
  ASSERT_EQ(EggTokenizerKind::String, tokenizer->next(item));
  ASSERT_EQ("hello", item.value.s.toUTF8());
  ASSERT_EQ(EggTokenizerKind::String, tokenizer->next(item));
  ASSERT_EQ("world", item.value.s.toUTF8());
}

TEST(TestEggTokenizer, Keyword) {
  egg::test::Allocator allocator;
  auto tokenizer = createFromString(allocator, "null false true any bool int float string object yield");
  EggTokenizerItem item;
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
  egg::test::Allocator allocator{ egg::test::Allocator::Expectation::NoAllocations };
  auto tokenizer = createFromString(allocator, "!??.->>>>=~ $");
  EggTokenizerItem item;
  ASSERT_EQ(EggTokenizerKind::Operator, tokenizer->next(item));
  ASSERT_EQ(EggTokenizerOperator::Bang, item.value.o);
  ASSERT_EQ(EggTokenizerKind::Operator, tokenizer->next(item));
  ASSERT_EQ(EggTokenizerOperator::QueryQuery, item.value.o);
  ASSERT_EQ(EggTokenizerKind::Operator, tokenizer->next(item));
  ASSERT_EQ(EggTokenizerOperator::Dot, item.value.o);
  ASSERT_EQ(EggTokenizerKind::Operator, tokenizer->next(item));
  ASSERT_EQ(EggTokenizerOperator::Lambda, item.value.o);
  ASSERT_EQ(EggTokenizerKind::Operator, tokenizer->next(item));
  ASSERT_EQ(EggTokenizerOperator::ShiftRightUnsignedEqual, item.value.o);
  ASSERT_EQ(EggTokenizerKind::Operator, tokenizer->next(item));
  ASSERT_EQ(EggTokenizerOperator::Tilde, item.value.o);
  ASSERT_THROW_E(tokenizer->next(item), Exception, ASSERT_CONTAINS(e.what(), "Unexpected character: '$'"));
}

TEST(TestEggTokenizer, Identifier) {
  egg::test::Allocator allocator;
  auto tokenizer = createFromString(allocator, "unknown _");
  EggTokenizerItem item;
  ASSERT_EQ(EggTokenizerKind::Identifier, tokenizer->next(item));
  ASSERT_EQ("unknown", item.value.s.toUTF8());
  ASSERT_EQ(EggTokenizerKind::Identifier, tokenizer->next(item));
  ASSERT_EQ("_", item.value.s.toUTF8());
  ASSERT_EQ(EggTokenizerKind::EndOfFile, tokenizer->next(item));
}

TEST(TestEggTokenizer, Attribute) {
  egg::test::Allocator allocator;
  auto tokenizer = createFromString(allocator, "@test @and.this .@@twice(2)");
  EggTokenizerItem item;
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
  egg::test::Allocator allocator;
  auto tokenizer = createFromString(allocator, "1 2.3\r\n\r\n`hello\nworld` foo");
  EggTokenizerItem item;
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
  egg::test::Allocator allocator;
  auto tokenizer = createFromString(allocator, "1 2.3 \"hello\" foo");
  EggTokenizerItem item;
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
  egg::test::Allocator allocator;
  EggTokenizerItem item;
  // Parsed as "--|x"
  auto tokenizer = createFromString(allocator, "--x");
  ASSERT_EQ(EggTokenizerKind::Operator, tokenizer->next(item));
  ASSERT_EQ(EggTokenizerOperator::MinusMinus, item.value.o);
  ASSERT_EQ(EggTokenizerKind::Identifier, tokenizer->next(item));
  ASSERT_EQ("x", item.value.s.toUTF8());
  ASSERT_EQ(EggTokenizerKind::EndOfFile, tokenizer->next(item));
  // Parsed as "x|--|1"
  tokenizer = createFromString(allocator, "x--1");
  ASSERT_EQ(EggTokenizerKind::Identifier, tokenizer->next(item));
  ASSERT_EQ("x", item.value.s.toUTF8());
  ASSERT_EQ(EggTokenizerKind::Operator, tokenizer->next(item));
  ASSERT_EQ(EggTokenizerOperator::MinusMinus, item.value.o);
  ASSERT_EQ(EggTokenizerKind::Integer, tokenizer->next(item));
  ASSERT_EQ(1, item.value.i);
  ASSERT_EQ(EggTokenizerKind::EndOfFile, tokenizer->next(item));
  // Parsed as "-|1"
  tokenizer = createFromString(allocator, "-1");
  ASSERT_EQ(EggTokenizerKind::Operator, tokenizer->next(item));
  ASSERT_EQ(EggTokenizerOperator::Minus, item.value.o);
  ASSERT_EQ(EggTokenizerKind::Integer, tokenizer->next(item));
  ASSERT_EQ(1, item.value.i);
  ASSERT_EQ(EggTokenizerKind::EndOfFile, tokenizer->next(item));
}

TEST(TestEggTokenizer, ExampleFile) {
  egg::test::Allocator allocator;
  auto tokenizer = createFromPath(allocator, "~/cpp/data/example.egg");
  size_t count = 0;
  EggTokenizerItem item;
  while (tokenizer->next(item) != EggTokenizerKind::EndOfFile) {
    count++;
  }
  ASSERT_EQ(21u, count);
}
