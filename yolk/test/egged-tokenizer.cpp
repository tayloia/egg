#include "yolk/test.h"
#include "yolk/lexers.h"
#include "yolk/egged-tokenizer.h"

using namespace egg::yolk;

namespace {
  std::shared_ptr<IEggedTokenizer> createFromString(const std::string& text) {
    auto lexer = LexerFactory::createFromString(text);
    return EggedTokenizerFactory::createFromLexer(lexer);
  }
  std::shared_ptr<IEggedTokenizer> createFromPath(const std::string& path) {
    auto lexer = LexerFactory::createFromPath(path);
    return EggedTokenizerFactory::createFromLexer(lexer);
  }
}

TEST(TestEggedTokenizer, EmptyFile) {
  EggedTokenizerItem item;
  auto tokenizer = createFromString("");
  ASSERT_EQ(EggedTokenizerKind::EndOfFile, tokenizer->next(item));
}

TEST(TestEggedTokenizer, Comment) {
  EggedTokenizerItem item;
  auto tokenizer = createFromString("// Comment\nnull");
  ASSERT_EQ(EggedTokenizerKind::Null, tokenizer->next(item));
  tokenizer = createFromString("/* Comment */null");
  ASSERT_EQ(EggedTokenizerKind::Null, tokenizer->next(item));
}

TEST(TestEggedTokenizer, EmptyObject) {
  EggedTokenizerItem item;
  auto tokenizer = createFromString("{}");
  ASSERT_EQ(EggedTokenizerKind::ObjectStart, tokenizer->next(item));
  ASSERT_EQ(EggedTokenizerKind::ObjectEnd, tokenizer->next(item));
  ASSERT_EQ(EggedTokenizerKind::EndOfFile, tokenizer->next(item));
}

TEST(TestEggedTokenizer, EmptyArray) {
  EggedTokenizerItem item;
  auto tokenizer = createFromString("[]");
  ASSERT_EQ(EggedTokenizerKind::ArrayStart, tokenizer->next(item));
  ASSERT_EQ(EggedTokenizerKind::ArrayEnd, tokenizer->next(item));
}

TEST(TestEggedTokenizer, Null) {
  EggedTokenizerItem item;
  auto tokenizer = createFromString("{ \"null\": null }");
  ASSERT_EQ(EggedTokenizerKind::ObjectStart, tokenizer->next(item));
  ASSERT_EQ(EggedTokenizerKind::String, tokenizer->next(item));
  ASSERT_EQ("null", item.value.getString().toUTF8());
  ASSERT_EQ(EggedTokenizerKind::Colon, tokenizer->next(item));
  ASSERT_EQ(EggedTokenizerKind::Null, tokenizer->next(item));
  ASSERT_EQ(EggedTokenizerKind::ObjectEnd, tokenizer->next(item));
  ASSERT_EQ(EggedTokenizerKind::EndOfFile, tokenizer->next(item));
}

TEST(TestEggedTokenizer, BooleanFalse) {
  EggedTokenizerItem item;
  auto tokenizer = createFromString("{ \"no\": false }");
  ASSERT_EQ(EggedTokenizerKind::ObjectStart, tokenizer->next(item));
  ASSERT_EQ(EggedTokenizerKind::String, tokenizer->next(item));
  ASSERT_EQ("no", item.value.getString().toUTF8());
  ASSERT_EQ(EggedTokenizerKind::Colon, tokenizer->next(item));
  ASSERT_EQ(EggedTokenizerKind::Boolean, tokenizer->next(item));
  ASSERT_EQ(false, item.value.getBool());
  ASSERT_EQ(EggedTokenizerKind::ObjectEnd, tokenizer->next(item));
  ASSERT_EQ(EggedTokenizerKind::EndOfFile, tokenizer->next(item));
}

TEST(TestEggedTokenizer, BooleanTrue) {
  EggedTokenizerItem item;
  auto tokenizer = createFromString("{ \"yes\": true }");
  ASSERT_EQ(EggedTokenizerKind::ObjectStart, tokenizer->next(item));
  ASSERT_EQ(EggedTokenizerKind::String, tokenizer->next(item));
  ASSERT_EQ("yes", item.value.getString().toUTF8());
  ASSERT_EQ(EggedTokenizerKind::Colon, tokenizer->next(item));
  ASSERT_EQ(EggedTokenizerKind::Boolean, tokenizer->next(item));
  ASSERT_EQ(true, item.value.getBool());
  ASSERT_EQ(EggedTokenizerKind::ObjectEnd, tokenizer->next(item));
  ASSERT_EQ(EggedTokenizerKind::EndOfFile, tokenizer->next(item));
}

TEST(TestEggedTokenizer, Integer) {
  EggedTokenizerItem item;
  auto tokenizer = createFromString("{ \"positive\": 123 \"negative\": -123 }");
  ASSERT_EQ(EggedTokenizerKind::ObjectStart, tokenizer->next(item));
  ASSERT_EQ(EggedTokenizerKind::String, tokenizer->next(item));
  ASSERT_EQ("positive", item.value.getString().toUTF8());
  ASSERT_EQ(EggedTokenizerKind::Colon, tokenizer->next(item));
  ASSERT_EQ(EggedTokenizerKind::Integer, tokenizer->next(item));
  ASSERT_EQ(123, item.value.getInt());
  ASSERT_EQ(EggedTokenizerKind::String, tokenizer->next(item));
  ASSERT_EQ("negative", item.value.getString().toUTF8());
  ASSERT_EQ(EggedTokenizerKind::Colon, tokenizer->next(item));
  ASSERT_EQ(EggedTokenizerKind::Integer, tokenizer->next(item));
  ASSERT_EQ(-123, item.value.getInt());
  ASSERT_EQ(EggedTokenizerKind::ObjectEnd, tokenizer->next(item));
  ASSERT_EQ(EggedTokenizerKind::EndOfFile, tokenizer->next(item));
}

TEST(TestEggedTokenizer, Float) {
  EggedTokenizerItem item;
  auto tokenizer = createFromString("{ positive: 3.14159 negative: -3.14159 }");
  ASSERT_EQ(EggedTokenizerKind::ObjectStart, tokenizer->next(item));
  ASSERT_EQ(EggedTokenizerKind::Identifier, tokenizer->next(item));
  ASSERT_EQ("positive", item.value.getString().toUTF8());
  ASSERT_EQ(EggedTokenizerKind::Colon, tokenizer->next(item));
  ASSERT_EQ(EggedTokenizerKind::Float, tokenizer->next(item));
  ASSERT_EQ(3.14159, item.value.getFloat());
  ASSERT_EQ(EggedTokenizerKind::Identifier, tokenizer->next(item));
  ASSERT_EQ("negative", item.value.getString().toUTF8());
  ASSERT_EQ(EggedTokenizerKind::Colon, tokenizer->next(item));
  ASSERT_EQ(EggedTokenizerKind::Float, tokenizer->next(item));
  ASSERT_EQ(-3.14159, item.value.getFloat());
  ASSERT_EQ(EggedTokenizerKind::ObjectEnd, tokenizer->next(item));
  ASSERT_EQ(EggedTokenizerKind::EndOfFile, tokenizer->next(item));
}

TEST(TestEggedTokenizer, String) {
  EggedTokenizerItem item;
  auto tokenizer = createFromString("{ \"greeting\": \"hello world\" }");
  ASSERT_EQ(EggedTokenizerKind::ObjectStart, tokenizer->next(item));
  ASSERT_EQ(EggedTokenizerKind::String, tokenizer->next(item));
  ASSERT_EQ("greeting", item.value.getString().toUTF8());
  ASSERT_EQ(EggedTokenizerKind::Colon, tokenizer->next(item));
  ASSERT_EQ(EggedTokenizerKind::String, tokenizer->next(item));
  ASSERT_EQ("hello world", item.value.getString().toUTF8());
  ASSERT_EQ(EggedTokenizerKind::ObjectEnd, tokenizer->next(item));
  ASSERT_EQ(EggedTokenizerKind::EndOfFile, tokenizer->next(item));

  tokenizer = createFromString("{ `greeting`: `hello\nworld` }");
  ASSERT_EQ(EggedTokenizerKind::ObjectStart, tokenizer->next(item));
  ASSERT_EQ(EggedTokenizerKind::String, tokenizer->next(item));
  ASSERT_EQ("greeting", item.value.getString().toUTF8());
  ASSERT_EQ(EggedTokenizerKind::Colon, tokenizer->next(item));
  ASSERT_EQ(EggedTokenizerKind::String, tokenizer->next(item));
  ASSERT_EQ("hello\nworld", item.value.getString().toUTF8());
  ASSERT_EQ(EggedTokenizerKind::ObjectEnd, tokenizer->next(item));
  ASSERT_EQ(EggedTokenizerKind::EndOfFile, tokenizer->next(item));
}

TEST(TestEggedTokenizer, Identifier) {
  EggedTokenizerItem item;
  auto tokenizer = createFromString("identifier");
  ASSERT_EQ(EggedTokenizerKind::Identifier, tokenizer->next(item));
  ASSERT_EQ("identifier", item.value.getString().toUTF8());
}

TEST(TestEggedTokenizer, SequentialOperators) {
  EggedTokenizerItem item;
  auto tokenizer = createFromString("{:-1}");
  ASSERT_EQ(EggedTokenizerKind::ObjectStart, tokenizer->next(item));
  ASSERT_EQ(EggedTokenizerKind::Colon, tokenizer->next(item));
  ASSERT_EQ(EggedTokenizerKind::Integer, tokenizer->next(item));
  ASSERT_EQ(-1, item.value.getInt());
  ASSERT_EQ(EggedTokenizerKind::ObjectEnd, tokenizer->next(item));
  ASSERT_EQ(EggedTokenizerKind::EndOfFile, tokenizer->next(item));
}

TEST(TestEggedTokenizer, CharacterBad) {
  EggedTokenizerItem item;
  auto tokenizer = createFromString("\a");
  ASSERT_THROW_E(tokenizer->next(item), Exception, ASSERT_CONTAINS(e.what(), "Unexpected character: U+0007")); 
  tokenizer = createFromString("$");
  ASSERT_THROW_E(tokenizer->next(item), Exception, ASSERT_CONTAINS(e.what(), "Unexpected character"));
}

TEST(TestEggedTokenizer, NumberBad) {
  EggedTokenizerItem item;
  auto tokenizer = createFromString("18446744073709551616");
  ASSERT_THROW_E(tokenizer->next(item), Exception, ASSERT_CONTAINS(e.what(), "Invalid integer constant"));
  tokenizer = createFromString("-9223372036854775809");
  ASSERT_THROW_E(tokenizer->next(item), Exception, ASSERT_CONTAINS(e.what(), "Invalid negative integer constant"));
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
  ASSERT_THROW_E(tokenizer->next(item), Exception, ASSERT_CONTAINS(e.what(), "Expected number to follow minus sign"));
}

TEST(TestEggedTokenizer, StringBad) {
  EggedTokenizerItem item;
  auto tokenizer = createFromString("\"");
  ASSERT_THROW_E(tokenizer->next(item), Exception, ASSERT_CONTAINS(e.what(), "Unexpected end of file found in quoted string"));
  tokenizer = createFromString("\"\n\"");
  ASSERT_THROW_E(tokenizer->next(item), Exception, ASSERT_CONTAINS(e.what(), "Unexpected end of line found in quoted string"));
  tokenizer = createFromString("`");
  ASSERT_THROW_E(tokenizer->next(item), Exception, ASSERT_CONTAINS(e.what(), "Unexpected end of file found in backquoted string"));
}

TEST(TestEggedTokenizer, OperatorBad) {
  EggedTokenizerItem item;
  auto tokenizer = createFromString("+1");
  ASSERT_THROW_E(tokenizer->next(item), Exception, ASSERT_CONTAINS(e.what(), "Unexpected character: '+'"));
}

TEST(TestEggedTokenizer, Contiguous) {
  EggedTokenizerItem item;
  auto tokenizer = createFromString("/*comment*/{}/*comment*/");
  ASSERT_EQ(EggedTokenizerKind::ObjectStart, tokenizer->next(item));
  ASSERT_FALSE(item.contiguous);
  ASSERT_EQ(EggedTokenizerKind::ObjectEnd, tokenizer->next(item));
  ASSERT_TRUE(item.contiguous);
  ASSERT_EQ(EggedTokenizerKind::EndOfFile, tokenizer->next(item));
  ASSERT_FALSE(item.contiguous);
  tokenizer = createFromString("\"hello\"\"world\"");
  ASSERT_EQ(EggedTokenizerKind::String, tokenizer->next(item));
  ASSERT_TRUE(item.contiguous);
  ASSERT_EQ(EggedTokenizerKind::String, tokenizer->next(item));
  ASSERT_TRUE(item.contiguous);
  ASSERT_EQ(EggedTokenizerKind::EndOfFile, tokenizer->next(item));
  ASSERT_TRUE(item.contiguous);
  tokenizer = createFromString(" \"hello\" \"world\" ");
  ASSERT_EQ(EggedTokenizerKind::String, tokenizer->next(item));
  ASSERT_FALSE(item.contiguous);
  ASSERT_EQ(EggedTokenizerKind::String, tokenizer->next(item));
  ASSERT_FALSE(item.contiguous);
  ASSERT_EQ(EggedTokenizerKind::EndOfFile, tokenizer->next(item));
  ASSERT_FALSE(item.contiguous);
}

TEST(TestEggedTokenizer, ExampleFile) {
  EggedTokenizerItem item;
  auto tokenizer = createFromPath("~/yolk/test/data/example.egd");
  size_t count = 0;
  while (tokenizer->next(item) != EggedTokenizerKind::EndOfFile) {
    count++;
  }
  ASSERT_EQ(55u, count);
}
