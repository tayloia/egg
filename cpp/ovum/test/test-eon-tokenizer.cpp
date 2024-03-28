#include "ovum/test.h"
#include "ovum/lexer.h"
#include "ovum/eon-tokenizer.h"

using namespace egg::ovum;

namespace {
  std::shared_ptr<IEonTokenizer> createFromString(IAllocator& allocator, const std::string& text) {
    auto lexer = LexerFactory::createFromString(text);
    return EonTokenizerFactory::createFromLexer(allocator, lexer);
  }
  std::shared_ptr<IEonTokenizer> createFromPath(IAllocator& allocator, const std::string& path) {
    auto lexer = LexerFactory::createFromPath(path);
    return EonTokenizerFactory::createFromLexer(allocator, lexer);
  }
}

TEST(TestEonTokenizer, EmptyFile) {
  egg::test::Allocator allocator(egg::test::Allocator::Expectation::NoAllocations);
  EonTokenizerItem item;
  auto tokenizer = createFromString(allocator, "");
  ASSERT_EQ(EonTokenizerKind::EndOfFile, tokenizer->next(item));
}

TEST(TestEonTokenizer, Comment) {
  egg::test::Allocator allocator(egg::test::Allocator::Expectation::NoAllocations);
  EonTokenizerItem item;
  auto tokenizer = createFromString(allocator, "// Comment\nnull");
  ASSERT_EQ(EonTokenizerKind::Null, tokenizer->next(item));
  tokenizer = createFromString(allocator, "/* Comment */null");
  ASSERT_EQ(EonTokenizerKind::Null, tokenizer->next(item));
}

TEST(TestEonTokenizer, EmptyObject) {
  egg::test::Allocator allocator(egg::test::Allocator::Expectation::NoAllocations);
  EonTokenizerItem item;
  auto tokenizer = createFromString(allocator, "{}");
  ASSERT_EQ(EonTokenizerKind::ObjectStart, tokenizer->next(item));
  ASSERT_EQ(EonTokenizerKind::ObjectEnd, tokenizer->next(item));
  ASSERT_EQ(EonTokenizerKind::EndOfFile, tokenizer->next(item));
}

TEST(TestEonTokenizer, EmptyArray) {
  egg::test::Allocator allocator(egg::test::Allocator::Expectation::NoAllocations);
  EonTokenizerItem item;
  auto tokenizer = createFromString(allocator, "[]");
  ASSERT_EQ(EonTokenizerKind::ArrayStart, tokenizer->next(item));
  ASSERT_EQ(EonTokenizerKind::ArrayEnd, tokenizer->next(item));
}

TEST(TestEonTokenizer, Null) {
  egg::test::Allocator allocator;
  EonTokenizerItem item;
  auto tokenizer = createFromString(allocator, "{ \"null\": null }");
  ASSERT_EQ(EonTokenizerKind::ObjectStart, tokenizer->next(item));
  ASSERT_EQ(EonTokenizerKind::String, tokenizer->next(item));
  String string;
  ASSERT_TRUE(item.value->getString(string));
  ASSERT_STRING("null", string);
  ASSERT_EQ(EonTokenizerKind::Colon, tokenizer->next(item));
  ASSERT_EQ(EonTokenizerKind::Null, tokenizer->next(item));
  ASSERT_EQ(EonTokenizerKind::ObjectEnd, tokenizer->next(item));
  ASSERT_EQ(EonTokenizerKind::EndOfFile, tokenizer->next(item));
}

TEST(TestEonTokenizer, BooleanFalse) {
  egg::test::Allocator allocator;
  EonTokenizerItem item;
  auto tokenizer = createFromString(allocator, "{ \"no\": false }");
  ASSERT_EQ(EonTokenizerKind::ObjectStart, tokenizer->next(item));
  ASSERT_EQ(EonTokenizerKind::String, tokenizer->next(item));
  String string;
  ASSERT_TRUE(item.value->getString(string));
  ASSERT_STRING("no", string);
  ASSERT_EQ(EonTokenizerKind::Colon, tokenizer->next(item));
  ASSERT_EQ(EonTokenizerKind::Boolean, tokenizer->next(item));
  Bool flag;
  ASSERT_TRUE(item.value->getBool(flag));
  ASSERT_EQ(false, flag);
  ASSERT_EQ(EonTokenizerKind::ObjectEnd, tokenizer->next(item));
  ASSERT_EQ(EonTokenizerKind::EndOfFile, tokenizer->next(item));
}

TEST(TestEonTokenizer, BooleanTrue) {
  egg::test::Allocator allocator;
  EonTokenizerItem item;
  auto tokenizer = createFromString(allocator, "{ \"yes\": true }");
  ASSERT_EQ(EonTokenizerKind::ObjectStart, tokenizer->next(item));
  ASSERT_EQ(EonTokenizerKind::String, tokenizer->next(item));
  String string;
  ASSERT_TRUE(item.value->getString(string));
  ASSERT_STRING("yes", string);
  ASSERT_EQ(EonTokenizerKind::Colon, tokenizer->next(item));
  ASSERT_EQ(EonTokenizerKind::Boolean, tokenizer->next(item));
  Bool flag;
  ASSERT_TRUE(item.value->getBool(flag));
  ASSERT_EQ(true, flag);
  ASSERT_EQ(EonTokenizerKind::ObjectEnd, tokenizer->next(item));
  ASSERT_EQ(EonTokenizerKind::EndOfFile, tokenizer->next(item));
}

TEST(TestEonTokenizer, Integer) {
  egg::test::Allocator allocator;
  EonTokenizerItem item;
  auto tokenizer = createFromString(allocator, "{ \"positive\": 123 \"negative\": -123 }");
  ASSERT_EQ(EonTokenizerKind::ObjectStart, tokenizer->next(item));
  ASSERT_EQ(EonTokenizerKind::String, tokenizer->next(item));
  String string;
  ASSERT_TRUE(item.value->getString(string));
  ASSERT_STRING("positive", string);
  ASSERT_EQ(EonTokenizerKind::Colon, tokenizer->next(item));
  ASSERT_EQ(EonTokenizerKind::Integer, tokenizer->next(item));
  Int value;
  ASSERT_TRUE(item.value->getInt(value));
  ASSERT_EQ(123, value);
  ASSERT_EQ(EonTokenizerKind::String, tokenizer->next(item));
  ASSERT_TRUE(item.value->getString(string));
  ASSERT_STRING("negative", string);
  ASSERT_EQ(EonTokenizerKind::Colon, tokenizer->next(item));
  ASSERT_EQ(EonTokenizerKind::Integer, tokenizer->next(item));
  ASSERT_TRUE(item.value->getInt(value));
  ASSERT_EQ(-123, value);
  ASSERT_EQ(EonTokenizerKind::ObjectEnd, tokenizer->next(item));
  ASSERT_EQ(EonTokenizerKind::EndOfFile, tokenizer->next(item));
}

TEST(TestEonTokenizer, Float) {
  egg::test::Allocator allocator;
  EonTokenizerItem item;
  auto tokenizer = createFromString(allocator, "{ positive: 3.14159 negative: -3.14159 }");
  ASSERT_EQ(EonTokenizerKind::ObjectStart, tokenizer->next(item));
  ASSERT_EQ(EonTokenizerKind::Identifier, tokenizer->next(item));
  String string;
  ASSERT_TRUE(item.value->getString(string));
  ASSERT_STRING("positive", string);
  ASSERT_EQ(EonTokenizerKind::Colon, tokenizer->next(item));
  ASSERT_EQ(EonTokenizerKind::Float, tokenizer->next(item));
  Float value;
  ASSERT_TRUE(item.value->getFloat(value));
  ASSERT_EQ(3.14159, value);
  ASSERT_EQ(EonTokenizerKind::Identifier, tokenizer->next(item));
  ASSERT_TRUE(item.value->getString(string));
  ASSERT_STRING("negative", string);
  ASSERT_EQ(EonTokenizerKind::Colon, tokenizer->next(item));
  ASSERT_EQ(EonTokenizerKind::Float, tokenizer->next(item));
  ASSERT_TRUE(item.value->getFloat(value));
  ASSERT_EQ(-3.14159, value);
  ASSERT_EQ(EonTokenizerKind::ObjectEnd, tokenizer->next(item));
  ASSERT_EQ(EonTokenizerKind::EndOfFile, tokenizer->next(item));
}

TEST(TestEonTokenizer, String) {
  egg::test::Allocator allocator;
  EonTokenizerItem item;
  auto tokenizer = createFromString(allocator, "{ \"greeting\": \"hello world\" }");
  ASSERT_EQ(EonTokenizerKind::ObjectStart, tokenizer->next(item));
  ASSERT_EQ(EonTokenizerKind::String, tokenizer->next(item));
  String string;
  ASSERT_TRUE(item.value->getString(string));
  ASSERT_STRING("greeting", string);
  ASSERT_EQ(EonTokenizerKind::Colon, tokenizer->next(item));
  ASSERT_EQ(EonTokenizerKind::String, tokenizer->next(item));
  ASSERT_TRUE(item.value->getString(string));
  ASSERT_STRING("hello world", string);
  ASSERT_EQ(EonTokenizerKind::ObjectEnd, tokenizer->next(item));
  ASSERT_EQ(EonTokenizerKind::EndOfFile, tokenizer->next(item));

  tokenizer = createFromString(allocator, "{ `greeting`: `hello\nworld` }");
  ASSERT_EQ(EonTokenizerKind::ObjectStart, tokenizer->next(item));
  ASSERT_EQ(EonTokenizerKind::String, tokenizer->next(item));
  ASSERT_TRUE(item.value->getString(string));
  ASSERT_STRING("greeting", string);
  ASSERT_EQ(EonTokenizerKind::Colon, tokenizer->next(item));
  ASSERT_EQ(EonTokenizerKind::String, tokenizer->next(item));
  ASSERT_TRUE(item.value->getString(string));
  ASSERT_STRING("hello\nworld", string);
  ASSERT_EQ(EonTokenizerKind::ObjectEnd, tokenizer->next(item));
  ASSERT_EQ(EonTokenizerKind::EndOfFile, tokenizer->next(item));
}

TEST(TestEonTokenizer, Identifier) {
  egg::test::Allocator allocator;
  EonTokenizerItem item;
  auto tokenizer = createFromString(allocator, "identifier");
  ASSERT_EQ(EonTokenizerKind::Identifier, tokenizer->next(item));
  String string;
  ASSERT_TRUE(item.value->getString(string));
  ASSERT_STRING("identifier", string);
}

TEST(TestEonTokenizer, SequentialOperators) {
  egg::test::Allocator allocator;
  EonTokenizerItem item;
  auto tokenizer = createFromString(allocator, "{:-1}");
  ASSERT_EQ(EonTokenizerKind::ObjectStart, tokenizer->next(item));
  ASSERT_EQ(EonTokenizerKind::Colon, tokenizer->next(item));
  ASSERT_EQ(EonTokenizerKind::Integer, tokenizer->next(item));
  Int value;
  ASSERT_TRUE(item.value->getInt(value));
  ASSERT_EQ(-1, value);
  ASSERT_EQ(EonTokenizerKind::ObjectEnd, tokenizer->next(item));
  ASSERT_EQ(EonTokenizerKind::EndOfFile, tokenizer->next(item));
}

TEST(TestEonTokenizer, CharacterBad) {
  egg::test::Allocator allocator(egg::test::Allocator::Expectation::NoAllocations);
  EonTokenizerItem item;
  auto tokenizer = createFromString(allocator, "\a");
  ASSERT_THROW_E(tokenizer->next(item), Exception, ASSERT_CONTAINS(e.what(), "Unexpected character: U+0007")); 
  tokenizer = createFromString(allocator, "$");
  ASSERT_THROW_E(tokenizer->next(item), Exception, ASSERT_CONTAINS(e.what(), "Unexpected character"));
}

TEST(TestEonTokenizer, NumberBad) {
  egg::test::Allocator allocator(egg::test::Allocator::Expectation::NoAllocations);
  EonTokenizerItem item;
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

TEST(TestEonTokenizer, StringBad) {
  egg::test::Allocator allocator(egg::test::Allocator::Expectation::NoAllocations);
  EonTokenizerItem item;
  auto tokenizer = createFromString(allocator, "\"");
  ASSERT_THROW_E(tokenizer->next(item), Exception, ASSERT_CONTAINS(e.what(), "Unexpected end of file found in quoted string"));
  tokenizer = createFromString(allocator, "\"\n\"");
  ASSERT_THROW_E(tokenizer->next(item), Exception, ASSERT_CONTAINS(e.what(), "Unexpected end of line found in quoted string"));
  tokenizer = createFromString(allocator, "`");
  ASSERT_THROW_E(tokenizer->next(item), Exception, ASSERT_CONTAINS(e.what(), "Unexpected end of file found in backquoted string"));
}

TEST(TestEonTokenizer, OperatorBad) {
  egg::test::Allocator allocator(egg::test::Allocator::Expectation::NoAllocations);
  EonTokenizerItem item;
  auto tokenizer = createFromString(allocator, "+1");
  ASSERT_THROW_E(tokenizer->next(item), Exception, ASSERT_CONTAINS(e.what(), "Unexpected character: '+'"));
}

TEST(TestEonTokenizer, Contiguous) {
  egg::test::Allocator allocator;
  EonTokenizerItem item;
  auto tokenizer = createFromString(allocator, "/*comment*/{}/*comment*/");
  ASSERT_EQ(EonTokenizerKind::ObjectStart, tokenizer->next(item));
  ASSERT_FALSE(item.contiguous);
  ASSERT_EQ(EonTokenizerKind::ObjectEnd, tokenizer->next(item));
  ASSERT_TRUE(item.contiguous);
  ASSERT_EQ(EonTokenizerKind::EndOfFile, tokenizer->next(item));
  ASSERT_FALSE(item.contiguous);
  tokenizer = createFromString(allocator, "\"hello\"\"world\"");
  ASSERT_EQ(EonTokenizerKind::String, tokenizer->next(item));
  ASSERT_TRUE(item.contiguous);
  ASSERT_EQ(EonTokenizerKind::String, tokenizer->next(item));
  ASSERT_TRUE(item.contiguous);
  ASSERT_EQ(EonTokenizerKind::EndOfFile, tokenizer->next(item));
  ASSERT_TRUE(item.contiguous);
  tokenizer = createFromString(allocator, " \"hello\" \"world\" ");
  ASSERT_EQ(EonTokenizerKind::String, tokenizer->next(item));
  ASSERT_FALSE(item.contiguous);
  ASSERT_EQ(EonTokenizerKind::String, tokenizer->next(item));
  ASSERT_FALSE(item.contiguous);
  ASSERT_EQ(EonTokenizerKind::EndOfFile, tokenizer->next(item));
  ASSERT_FALSE(item.contiguous);
}

TEST(TestEonTokenizer, ExampleFile) {
  egg::test::Allocator allocator;
  EonTokenizerItem item;
  auto tokenizer = createFromPath(allocator, "~/cpp/data/example.eon");
  size_t count = 0;
  while (tokenizer->next(item) != EonTokenizerKind::EndOfFile) {
    count++;
  }
  ASSERT_EQ(55u, count);
}
