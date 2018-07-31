#include "yolk/test.h"
#include "yolk/lexers.h"
#include "yolk/json-tokenizer.h"

using namespace egg::yolk;

namespace {
  std::shared_ptr<IJsonTokenizer> createFromString(const std::string& text) {
    auto lexer = LexerFactory::createFromString(text);
    return JsonTokenizerFactory::createFromLexer(lexer);
  }
  std::shared_ptr<IJsonTokenizer> createFromPath(const std::string& path) {
    auto lexer = LexerFactory::createFromPath(path);
    return JsonTokenizerFactory::createFromLexer(lexer);
  }
}

TEST(TestJsonTokenizer, EmptyFile) {
  JsonTokenizerItem item;
  auto tokenizer = createFromString("");
  ASSERT_EQ(JsonTokenizerKind::EndOfFile, tokenizer->next(item));
}

TEST(TestJsonTokenizer, EmptyObject) {
  JsonTokenizerItem item;
  auto tokenizer = createFromString("{}");
  ASSERT_EQ(JsonTokenizerKind::ObjectStart, tokenizer->next(item));
  ASSERT_EQ(JsonTokenizerKind::ObjectEnd, tokenizer->next(item));
  ASSERT_EQ(JsonTokenizerKind::EndOfFile, tokenizer->next(item));
}

TEST(TestJsonTokenizer, EmptyArray) {
  JsonTokenizerItem item;
  auto tokenizer = createFromString("[]");
  ASSERT_EQ(JsonTokenizerKind::ArrayStart, tokenizer->next(item));
  ASSERT_EQ(JsonTokenizerKind::ArrayEnd, tokenizer->next(item));
}

TEST(TestJsonTokenizer, Null) {
  JsonTokenizerItem item;
  auto tokenizer = createFromString("{ \"null\": null }");
  ASSERT_EQ(JsonTokenizerKind::ObjectStart, tokenizer->next(item));
  ASSERT_EQ(JsonTokenizerKind::String, tokenizer->next(item));
  ASSERT_EQ("null", item.value.s);
  ASSERT_EQ(JsonTokenizerKind::Colon, tokenizer->next(item));
  ASSERT_EQ(JsonTokenizerKind::Null, tokenizer->next(item));
  ASSERT_EQ(JsonTokenizerKind::ObjectEnd, tokenizer->next(item));
  ASSERT_EQ(JsonTokenizerKind::EndOfFile, tokenizer->next(item));
}

TEST(TestJsonTokenizer, BooleanFalse) {
  JsonTokenizerItem item;
  auto tokenizer = createFromString("{ \"no\": false }");
  ASSERT_EQ(JsonTokenizerKind::ObjectStart, tokenizer->next(item));
  ASSERT_EQ(JsonTokenizerKind::String, tokenizer->next(item));
  ASSERT_EQ("no", item.value.s);
  ASSERT_EQ(JsonTokenizerKind::Colon, tokenizer->next(item));
  ASSERT_EQ(JsonTokenizerKind::Boolean, tokenizer->next(item));
  ASSERT_EQ(false, item.value.b);
  ASSERT_EQ(JsonTokenizerKind::ObjectEnd, tokenizer->next(item));
  ASSERT_EQ(JsonTokenizerKind::EndOfFile, tokenizer->next(item));
}

TEST(TestJsonTokenizer, BooleanTrue) {
  JsonTokenizerItem item;
  auto tokenizer = createFromString("{ \"yes\": true }");
  ASSERT_EQ(JsonTokenizerKind::ObjectStart, tokenizer->next(item));
  ASSERT_EQ(JsonTokenizerKind::String, tokenizer->next(item));
  ASSERT_EQ("yes", item.value.s);
  ASSERT_EQ(JsonTokenizerKind::Colon, tokenizer->next(item));
  ASSERT_EQ(JsonTokenizerKind::Boolean, tokenizer->next(item));
  ASSERT_EQ(true, item.value.b);
  ASSERT_EQ(JsonTokenizerKind::ObjectEnd, tokenizer->next(item));
  ASSERT_EQ(JsonTokenizerKind::EndOfFile, tokenizer->next(item));
}

TEST(TestJsonTokenizer, IntegerPositive) {
  JsonTokenizerItem item;
  auto tokenizer = createFromString("{ \"positive\": 123 }");
  ASSERT_EQ(JsonTokenizerKind::ObjectStart, tokenizer->next(item));
  ASSERT_EQ(JsonTokenizerKind::String, tokenizer->next(item));
  ASSERT_EQ("positive", item.value.s);
  ASSERT_EQ(JsonTokenizerKind::Colon, tokenizer->next(item));
  ASSERT_EQ(JsonTokenizerKind::Unsigned, tokenizer->next(item));
  ASSERT_EQ(123u, item.value.u);
  ASSERT_EQ(JsonTokenizerKind::ObjectEnd, tokenizer->next(item));
  ASSERT_EQ(JsonTokenizerKind::EndOfFile, tokenizer->next(item));
}

TEST(TestJsonTokenizer, IntegerNegative) {
  JsonTokenizerItem item;
  auto tokenizer = createFromString("{ \"negative\": -123 }");
  ASSERT_EQ(JsonTokenizerKind::ObjectStart, tokenizer->next(item));
  ASSERT_EQ(JsonTokenizerKind::String, tokenizer->next(item));
  ASSERT_EQ("negative", item.value.s);
  ASSERT_EQ(JsonTokenizerKind::Colon, tokenizer->next(item));
  ASSERT_EQ(JsonTokenizerKind::Signed, tokenizer->next(item));
  ASSERT_EQ(-123, item.value.i);
  ASSERT_EQ(JsonTokenizerKind::ObjectEnd, tokenizer->next(item));
  ASSERT_EQ(JsonTokenizerKind::EndOfFile, tokenizer->next(item));
}

TEST(TestJsonTokenizer, FloatPositive) {
  JsonTokenizerItem item;
  auto tokenizer = createFromString("{ \"pi\": 3.14159 }");
  ASSERT_EQ(JsonTokenizerKind::ObjectStart, tokenizer->next(item));
  ASSERT_EQ(JsonTokenizerKind::String, tokenizer->next(item));
  ASSERT_EQ("pi", item.value.s);
  ASSERT_EQ(JsonTokenizerKind::Colon, tokenizer->next(item));
  ASSERT_EQ(JsonTokenizerKind::Float, tokenizer->next(item));
  ASSERT_EQ(3.14159, item.value.f);
  ASSERT_EQ(JsonTokenizerKind::ObjectEnd, tokenizer->next(item));
  ASSERT_EQ(JsonTokenizerKind::EndOfFile, tokenizer->next(item));
}

TEST(TestJsonTokenizer, FloatNegative) {
  JsonTokenizerItem item;
  auto tokenizer = createFromString("{ \"pi\": -3.14159 }");
  ASSERT_EQ(JsonTokenizerKind::ObjectStart, tokenizer->next(item));
  ASSERT_EQ(JsonTokenizerKind::String, tokenizer->next(item));
  ASSERT_EQ("pi", item.value.s);
  ASSERT_EQ(JsonTokenizerKind::Colon, tokenizer->next(item));
  ASSERT_EQ(JsonTokenizerKind::Float, tokenizer->next(item));
  ASSERT_EQ(-3.14159, item.value.f);
  ASSERT_EQ(JsonTokenizerKind::ObjectEnd, tokenizer->next(item));
  ASSERT_EQ(JsonTokenizerKind::EndOfFile, tokenizer->next(item));
}

TEST(TestJsonTokenizer, String) {
  JsonTokenizerItem item;
  auto tokenizer = createFromString("{ \"greeting\": \"hello world\" }");
  ASSERT_EQ(JsonTokenizerKind::ObjectStart, tokenizer->next(item));
  ASSERT_EQ(JsonTokenizerKind::String, tokenizer->next(item));
  ASSERT_EQ("greeting", item.value.s);
  ASSERT_EQ(JsonTokenizerKind::Colon, tokenizer->next(item));
  ASSERT_EQ(JsonTokenizerKind::String, tokenizer->next(item));
  ASSERT_EQ("hello world", item.value.s);
  ASSERT_EQ(JsonTokenizerKind::ObjectEnd, tokenizer->next(item));
  ASSERT_EQ(JsonTokenizerKind::EndOfFile, tokenizer->next(item));
}

TEST(TestJsonTokenizer, SequentialOperators) {
  JsonTokenizerItem item;
  auto tokenizer = createFromString("{:-1}");
  ASSERT_EQ(JsonTokenizerKind::ObjectStart, tokenizer->next(item));
  ASSERT_EQ(JsonTokenizerKind::Colon, tokenizer->next(item));
  ASSERT_EQ(JsonTokenizerKind::Signed, tokenizer->next(item));
  ASSERT_EQ(-1, item.value.i);
  ASSERT_EQ(JsonTokenizerKind::ObjectEnd, tokenizer->next(item));
  ASSERT_EQ(JsonTokenizerKind::EndOfFile, tokenizer->next(item));
}

TEST(TestJsonTokenizer, CharacterBad) {
  JsonTokenizerItem item;
  auto tokenizer = createFromString("\a");
  ASSERT_THROW_E(tokenizer->next(item), Exception, ASSERT_CONTAINS(e.what(), "Unexpected character: U+0007")); 
  tokenizer = createFromString("$");
  ASSERT_THROW_E(tokenizer->next(item), Exception, ASSERT_CONTAINS(e.what(), "Unexpected character in JSON"));
}

TEST(TestJsonTokenizer, CommentBad) {
  JsonTokenizerItem item;
  auto tokenizer = createFromString("// Comment");
  ASSERT_THROW_E(tokenizer->next(item), Exception, ASSERT_CONTAINS(e.what(), "Strict JSON does not permit comments"));
  tokenizer = createFromString("/* Comment */");
  ASSERT_THROW_E(tokenizer->next(item), Exception, ASSERT_CONTAINS(e.what(), "Strict JSON does not permit comments"));
}

TEST(TestJsonTokenizer, IdentifierBad) {
  JsonTokenizerItem item;
  auto tokenizer = createFromString("identifier");
  ASSERT_THROW_E(tokenizer->next(item), Exception, ASSERT_CONTAINS(e.what(), "Unexpected identifier in JSON"));
}

TEST(TestJsonTokenizer, NumberBad) {
  JsonTokenizerItem item;
  auto tokenizer = createFromString("18446744073709551616");
  ASSERT_THROW_E(tokenizer->next(item), Exception, ASSERT_CONTAINS(e.what(), "Invalid integer constant"));
  tokenizer = createFromString("-9223372036854775809");
  ASSERT_THROW_E(tokenizer->next(item), Exception, ASSERT_CONTAINS(e.what(), "Invalid negative integer constant in JSON"));
  tokenizer = createFromString("1e999");
  ASSERT_THROW_E(tokenizer->next(item), Exception, ASSERT_CONTAINS(e.what(), "Invalid floating-point constant"));
  tokenizer = createFromString("00");
  ASSERT_THROW_E(tokenizer->next(item), Exception, ASSERT_CONTAINS(e.what(), "Invalid integer constant (extraneous leading '0')"));
  tokenizer = createFromString("0.x");
  ASSERT_THROW_E(tokenizer->next(item), Exception, ASSERT_CONTAINS(e.what(), "Expected digit to follow decimal point in floating-point constant"));
  tokenizer = createFromString("0ex");
  ASSERT_THROW_E(tokenizer->next(item), Exception, ASSERT_CONTAINS(e.what(), "Expected digit in exponent of floating-point constant"));
  tokenizer = createFromString("0e+x");
  ASSERT_THROW_E(tokenizer->next(item), Exception, ASSERT_CONTAINS(e.what(), "Expected digit in exponent of floating-point constant"));
  tokenizer = createFromString("-x");
  ASSERT_THROW_E(tokenizer->next(item), Exception, ASSERT_CONTAINS(e.what(), "Expected number to follow minus sign in JSON"));
}

TEST(TestJsonTokenizer, StringBad) {
  JsonTokenizerItem item;
  auto tokenizer = createFromString("\"");
  ASSERT_THROW_E(tokenizer->next(item), Exception, ASSERT_CONTAINS(e.what(), "Unexpected end of file found in quoted string"));
  tokenizer = createFromString("\"\n\"");
  ASSERT_THROW_E(tokenizer->next(item), Exception, ASSERT_CONTAINS(e.what(), "Unexpected end of line found in quoted string"));
  tokenizer = createFromString("``");
  ASSERT_THROW_E(tokenizer->next(item), Exception, ASSERT_CONTAINS(e.what(), "Strict JSON does not permit backquoted strings"));
}

TEST(TestJsonTokenizer, OperatorBad) {
  JsonTokenizerItem item;
  auto tokenizer = createFromString("+1");
  ASSERT_THROW_E(tokenizer->next(item), Exception, ASSERT_CONTAINS(e.what(), "Unexpected character in JSON: '+'"));
}

TEST(TestJsonTokenizer, ExampleFile) {
  // From https://en.wikipedia.org/wiki/JSON#JSON_sample
  JsonTokenizerItem item;
  auto tokenizer = createFromPath("~/yolk/test/data/example.json");
  size_t count = 0;
  while (tokenizer->next(item) != JsonTokenizerKind::EndOfFile) {
    count++;
  }
  ASSERT_EQ(65u, count);
}
