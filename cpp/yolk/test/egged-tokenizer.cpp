#include "yolk/test.h"
#include "yolk/lexers.h"
#include "yolk/egged-tokenizer.h"

using namespace egg::yolk;

namespace {
  std::shared_ptr<IEggedTokenizer> createFromString(egg::ovum::IAllocator& allocator, const std::string& text) {
    auto lexer = LexerFactory::createFromString(text);
    return EggedTokenizerFactory::createFromLexer(allocator, lexer);
  }
  std::shared_ptr<IEggedTokenizer> createFromPath(egg::ovum::IAllocator& allocator, const std::string& path) {
    auto lexer = LexerFactory::createFromPath(path);
    return EggedTokenizerFactory::createFromLexer(allocator, lexer);
  }
}

TEST(TestEggedTokenizer, EmptyFile) {
  egg::test::Allocator allocator(egg::test::Allocator::Expectation::NoAllocations);
  EggedTokenizerItem item;
  auto tokenizer = createFromString(allocator, "");
  ASSERT_EQ(EggedTokenizerKind::EndOfFile, tokenizer->next(item));
}

TEST(TestEggedTokenizer, Comment) {
  egg::test::Allocator allocator(egg::test::Allocator::Expectation::NoAllocations);
  EggedTokenizerItem item;
  auto tokenizer = createFromString(allocator, "// Comment\nnull");
  ASSERT_EQ(EggedTokenizerKind::Null, tokenizer->next(item));
  tokenizer = createFromString(allocator, "/* Comment */null");
  ASSERT_EQ(EggedTokenizerKind::Null, tokenizer->next(item));
}

TEST(TestEggedTokenizer, EmptyObject) {
  egg::test::Allocator allocator(egg::test::Allocator::Expectation::NoAllocations);
  EggedTokenizerItem item;
  auto tokenizer = createFromString(allocator, "{}");
  ASSERT_EQ(EggedTokenizerKind::ObjectStart, tokenizer->next(item));
  ASSERT_EQ(EggedTokenizerKind::ObjectEnd, tokenizer->next(item));
  ASSERT_EQ(EggedTokenizerKind::EndOfFile, tokenizer->next(item));
}

TEST(TestEggedTokenizer, EmptyArray) {
  egg::test::Allocator allocator(egg::test::Allocator::Expectation::NoAllocations);
  EggedTokenizerItem item;
  auto tokenizer = createFromString(allocator, "[]");
  ASSERT_EQ(EggedTokenizerKind::ArrayStart, tokenizer->next(item));
  ASSERT_EQ(EggedTokenizerKind::ArrayEnd, tokenizer->next(item));
}

TEST(TestEggedTokenizer, Null) {
  egg::test::Allocator allocator;
  EggedTokenizerItem item;
  auto tokenizer = createFromString(allocator, "{ \"null\": null }");
  ASSERT_EQ(EggedTokenizerKind::ObjectStart, tokenizer->next(item));
  ASSERT_EQ(EggedTokenizerKind::String, tokenizer->next(item));
  egg::ovum::String string;
  ASSERT_TRUE(item.value->getString(string));
  ASSERT_STRING("null", string);
  ASSERT_EQ(EggedTokenizerKind::Colon, tokenizer->next(item));
  ASSERT_EQ(EggedTokenizerKind::Null, tokenizer->next(item));
  ASSERT_EQ(EggedTokenizerKind::ObjectEnd, tokenizer->next(item));
  ASSERT_EQ(EggedTokenizerKind::EndOfFile, tokenizer->next(item));
}

TEST(TestEggedTokenizer, BooleanFalse) {
  egg::test::Allocator allocator;
  EggedTokenizerItem item;
  auto tokenizer = createFromString(allocator, "{ \"no\": false }");
  ASSERT_EQ(EggedTokenizerKind::ObjectStart, tokenizer->next(item));
  ASSERT_EQ(EggedTokenizerKind::String, tokenizer->next(item));
  egg::ovum::String string;
  ASSERT_TRUE(item.value->getString(string));
  ASSERT_STRING("no", string);
  ASSERT_EQ(EggedTokenizerKind::Colon, tokenizer->next(item));
  ASSERT_EQ(EggedTokenizerKind::Boolean, tokenizer->next(item));
  egg::ovum::Bool flag;
  ASSERT_TRUE(item.value->getBool(flag));
  ASSERT_EQ(false, flag);
  ASSERT_EQ(EggedTokenizerKind::ObjectEnd, tokenizer->next(item));
  ASSERT_EQ(EggedTokenizerKind::EndOfFile, tokenizer->next(item));
}

TEST(TestEggedTokenizer, BooleanTrue) {
  egg::test::Allocator allocator;
  EggedTokenizerItem item;
  auto tokenizer = createFromString(allocator, "{ \"yes\": true }");
  ASSERT_EQ(EggedTokenizerKind::ObjectStart, tokenizer->next(item));
  ASSERT_EQ(EggedTokenizerKind::String, tokenizer->next(item));
  egg::ovum::String string;
  ASSERT_TRUE(item.value->getString(string));
  ASSERT_STRING("yes", string);
  ASSERT_EQ(EggedTokenizerKind::Colon, tokenizer->next(item));
  ASSERT_EQ(EggedTokenizerKind::Boolean, tokenizer->next(item));
  egg::ovum::Bool flag;
  ASSERT_TRUE(item.value->getBool(flag));
  ASSERT_EQ(true, flag);
  ASSERT_EQ(EggedTokenizerKind::ObjectEnd, tokenizer->next(item));
  ASSERT_EQ(EggedTokenizerKind::EndOfFile, tokenizer->next(item));
}

TEST(TestEggedTokenizer, Integer) {
  egg::test::Allocator allocator;
  EggedTokenizerItem item;
  auto tokenizer = createFromString(allocator, "{ \"positive\": 123 \"negative\": -123 }");
  ASSERT_EQ(EggedTokenizerKind::ObjectStart, tokenizer->next(item));
  ASSERT_EQ(EggedTokenizerKind::String, tokenizer->next(item));
  egg::ovum::String string;
  ASSERT_TRUE(item.value->getString(string));
  ASSERT_STRING("positive", string);
  ASSERT_EQ(EggedTokenizerKind::Colon, tokenizer->next(item));
  ASSERT_EQ(EggedTokenizerKind::Integer, tokenizer->next(item));
  egg::ovum::Int value;
  ASSERT_TRUE(item.value->getInt(value));
  ASSERT_EQ(123, value);
  ASSERT_EQ(EggedTokenizerKind::String, tokenizer->next(item));
  ASSERT_TRUE(item.value->getString(string));
  ASSERT_STRING("negative", string);
  ASSERT_EQ(EggedTokenizerKind::Colon, tokenizer->next(item));
  ASSERT_EQ(EggedTokenizerKind::Integer, tokenizer->next(item));
  ASSERT_TRUE(item.value->getInt(value));
  ASSERT_EQ(-123, value);
  ASSERT_EQ(EggedTokenizerKind::ObjectEnd, tokenizer->next(item));
  ASSERT_EQ(EggedTokenizerKind::EndOfFile, tokenizer->next(item));
}

TEST(TestEggedTokenizer, Float) {
  egg::test::Allocator allocator;
  EggedTokenizerItem item;
  auto tokenizer = createFromString(allocator, "{ positive: 3.14159 negative: -3.14159 }");
  ASSERT_EQ(EggedTokenizerKind::ObjectStart, tokenizer->next(item));
  ASSERT_EQ(EggedTokenizerKind::Identifier, tokenizer->next(item));
  egg::ovum::String string;
  ASSERT_TRUE(item.value->getString(string));
  ASSERT_STRING("positive", string);
  ASSERT_EQ(EggedTokenizerKind::Colon, tokenizer->next(item));
  ASSERT_EQ(EggedTokenizerKind::Float, tokenizer->next(item));
  egg::ovum::Float value;
  ASSERT_TRUE(item.value->getFloat(value));
  ASSERT_EQ(3.14159, value);
  ASSERT_EQ(EggedTokenizerKind::Identifier, tokenizer->next(item));
  ASSERT_TRUE(item.value->getString(string));
  ASSERT_STRING("negative", string);
  ASSERT_EQ(EggedTokenizerKind::Colon, tokenizer->next(item));
  ASSERT_EQ(EggedTokenizerKind::Float, tokenizer->next(item));
  ASSERT_TRUE(item.value->getFloat(value));
  ASSERT_EQ(-3.14159, value);
  ASSERT_EQ(EggedTokenizerKind::ObjectEnd, tokenizer->next(item));
  ASSERT_EQ(EggedTokenizerKind::EndOfFile, tokenizer->next(item));
}

TEST(TestEggedTokenizer, String) {
  egg::test::Allocator allocator;
  EggedTokenizerItem item;
  auto tokenizer = createFromString(allocator, "{ \"greeting\": \"hello world\" }");
  ASSERT_EQ(EggedTokenizerKind::ObjectStart, tokenizer->next(item));
  ASSERT_EQ(EggedTokenizerKind::String, tokenizer->next(item));
  egg::ovum::String string;
  ASSERT_TRUE(item.value->getString(string));
  ASSERT_STRING("greeting", string);
  ASSERT_EQ(EggedTokenizerKind::Colon, tokenizer->next(item));
  ASSERT_EQ(EggedTokenizerKind::String, tokenizer->next(item));
  ASSERT_TRUE(item.value->getString(string));
  ASSERT_STRING("hello world", string);
  ASSERT_EQ(EggedTokenizerKind::ObjectEnd, tokenizer->next(item));
  ASSERT_EQ(EggedTokenizerKind::EndOfFile, tokenizer->next(item));

  tokenizer = createFromString(allocator, "{ `greeting`: `hello\nworld` }");
  ASSERT_EQ(EggedTokenizerKind::ObjectStart, tokenizer->next(item));
  ASSERT_EQ(EggedTokenizerKind::String, tokenizer->next(item));
  ASSERT_TRUE(item.value->getString(string));
  ASSERT_STRING("greeting", string);
  ASSERT_EQ(EggedTokenizerKind::Colon, tokenizer->next(item));
  ASSERT_EQ(EggedTokenizerKind::String, tokenizer->next(item));
  ASSERT_TRUE(item.value->getString(string));
  ASSERT_STRING("hello\nworld", string);
  ASSERT_EQ(EggedTokenizerKind::ObjectEnd, tokenizer->next(item));
  ASSERT_EQ(EggedTokenizerKind::EndOfFile, tokenizer->next(item));
}

TEST(TestEggedTokenizer, Identifier) {
  egg::test::Allocator allocator;
  EggedTokenizerItem item;
  auto tokenizer = createFromString(allocator, "identifier");
  ASSERT_EQ(EggedTokenizerKind::Identifier, tokenizer->next(item));
  egg::ovum::String string;
  ASSERT_TRUE(item.value->getString(string));
  ASSERT_STRING("identifier", string);
}

TEST(TestEggedTokenizer, SequentialOperators) {
  egg::test::Allocator allocator;
  EggedTokenizerItem item;
  auto tokenizer = createFromString(allocator, "{:-1}");
  ASSERT_EQ(EggedTokenizerKind::ObjectStart, tokenizer->next(item));
  ASSERT_EQ(EggedTokenizerKind::Colon, tokenizer->next(item));
  ASSERT_EQ(EggedTokenizerKind::Integer, tokenizer->next(item));
  egg::ovum::Int value;
  ASSERT_TRUE(item.value->getInt(value));
  ASSERT_EQ(-1, value);
  ASSERT_EQ(EggedTokenizerKind::ObjectEnd, tokenizer->next(item));
  ASSERT_EQ(EggedTokenizerKind::EndOfFile, tokenizer->next(item));
}

TEST(TestEggedTokenizer, CharacterBad) {
  egg::test::Allocator allocator(egg::test::Allocator::Expectation::NoAllocations);
  EggedTokenizerItem item;
  auto tokenizer = createFromString(allocator, "\a");
  ASSERT_THROW_E(tokenizer->next(item), Exception, ASSERT_CONTAINS(e.what(), "Unexpected character: U+0007")); 
  tokenizer = createFromString(allocator, "$");
  ASSERT_THROW_E(tokenizer->next(item), Exception, ASSERT_CONTAINS(e.what(), "Unexpected character"));
}

TEST(TestEggedTokenizer, NumberBad) {
  egg::test::Allocator allocator(egg::test::Allocator::Expectation::NoAllocations);
  EggedTokenizerItem item;
  auto tokenizer = createFromString(allocator, "18446744073709551616");
  ASSERT_THROW_E(tokenizer->next(item), Exception, ASSERT_CONTAINS(e.what(), "Invalid integer constant"));
  tokenizer = createFromString(allocator, "-9223372036854775809");
  ASSERT_THROW_E(tokenizer->next(item), Exception, ASSERT_CONTAINS(e.what(), "Invalid negative integer constant"));
  tokenizer = createFromString(allocator, "1e999");
  ASSERT_THROW_E(tokenizer->next(item), Exception, ASSERT_CONTAINS(e.what(), "Invalid floating-point constant"));
  tokenizer = createFromString(allocator, "00");
  ASSERT_THROW_E(tokenizer->next(item), Exception, ASSERT_CONTAINS(e.what(), "Invalid integer constant (extraneous leading '0')"));
  tokenizer = createFromString(allocator, "0.x");
  ASSERT_THROW_E(tokenizer->next(item), Exception, ASSERT_CONTAINS(e.what(), "Expected digit to follow decimal point in floating-point constant"));
  tokenizer = createFromString(allocator, "0ex");
  ASSERT_THROW_E(tokenizer->next(item), Exception, ASSERT_CONTAINS(e.what(), "Expected digit in exponent of floating-point constant"));
  tokenizer = createFromString(allocator, "0e+x");
  ASSERT_THROW_E(tokenizer->next(item), Exception, ASSERT_CONTAINS(e.what(), "Expected digit in exponent of floating-point constant"));
  tokenizer = createFromString(allocator, "-x");
  ASSERT_THROW_E(tokenizer->next(item), Exception, ASSERT_CONTAINS(e.what(), "Expected number to follow minus sign"));
}

TEST(TestEggedTokenizer, StringBad) {
  egg::test::Allocator allocator(egg::test::Allocator::Expectation::NoAllocations);
  EggedTokenizerItem item;
  auto tokenizer = createFromString(allocator, "\"");
  ASSERT_THROW_E(tokenizer->next(item), Exception, ASSERT_CONTAINS(e.what(), "Unexpected end of file found in quoted string"));
  tokenizer = createFromString(allocator, "\"\n\"");
  ASSERT_THROW_E(tokenizer->next(item), Exception, ASSERT_CONTAINS(e.what(), "Unexpected end of line found in quoted string"));
  tokenizer = createFromString(allocator, "`");
  ASSERT_THROW_E(tokenizer->next(item), Exception, ASSERT_CONTAINS(e.what(), "Unexpected end of file found in backquoted string"));
}

TEST(TestEggedTokenizer, OperatorBad) {
  egg::test::Allocator allocator(egg::test::Allocator::Expectation::NoAllocations);
  EggedTokenizerItem item;
  auto tokenizer = createFromString(allocator, "+1");
  ASSERT_THROW_E(tokenizer->next(item), Exception, ASSERT_CONTAINS(e.what(), "Unexpected character: '+'"));
}

TEST(TestEggedTokenizer, Contiguous) {
  egg::test::Allocator allocator;
  EggedTokenizerItem item;
  auto tokenizer = createFromString(allocator, "/*comment*/{}/*comment*/");
  ASSERT_EQ(EggedTokenizerKind::ObjectStart, tokenizer->next(item));
  ASSERT_FALSE(item.contiguous);
  ASSERT_EQ(EggedTokenizerKind::ObjectEnd, tokenizer->next(item));
  ASSERT_TRUE(item.contiguous);
  ASSERT_EQ(EggedTokenizerKind::EndOfFile, tokenizer->next(item));
  ASSERT_FALSE(item.contiguous);
  tokenizer = createFromString(allocator, "\"hello\"\"world\"");
  ASSERT_EQ(EggedTokenizerKind::String, tokenizer->next(item));
  ASSERT_TRUE(item.contiguous);
  ASSERT_EQ(EggedTokenizerKind::String, tokenizer->next(item));
  ASSERT_TRUE(item.contiguous);
  ASSERT_EQ(EggedTokenizerKind::EndOfFile, tokenizer->next(item));
  ASSERT_TRUE(item.contiguous);
  tokenizer = createFromString(allocator, " \"hello\" \"world\" ");
  ASSERT_EQ(EggedTokenizerKind::String, tokenizer->next(item));
  ASSERT_FALSE(item.contiguous);
  ASSERT_EQ(EggedTokenizerKind::String, tokenizer->next(item));
  ASSERT_FALSE(item.contiguous);
  ASSERT_EQ(EggedTokenizerKind::EndOfFile, tokenizer->next(item));
  ASSERT_FALSE(item.contiguous);
}

TEST(TestEggedTokenizer, ExampleFile) {
  egg::test::Allocator allocator;
  EggedTokenizerItem item;
  auto tokenizer = createFromPath(allocator, "~/cpp/yolk/test/data/example.egd");
  size_t count = 0;
  while (tokenizer->next(item) != EggedTokenizerKind::EndOfFile) {
    count++;
  }
  ASSERT_EQ(55u, count);
}
