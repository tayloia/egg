#include "test.h"

using namespace egg::yolk;

TEST(TestEggTokenizer, EmptyFile) {
  EggTokenizerItem item;
  auto tokenizer = EggTokenizerFactory::createFromString("");
  EggTokenizerState state = EggTokenizerState::ExpectNone;
  ASSERT_EQ(EggTokenizerKind::EndOfFile, tokenizer->next(item, state));
}

TEST(TestEggTokenizer, Comment) {
  EggTokenizerItem item;
  auto tokenizer = EggTokenizerFactory::createFromString("// Comment\n0");
  EggTokenizerState state = EggTokenizerState::ExpectNone;
  ASSERT_EQ(EggTokenizerKind::Integer, tokenizer->next(item, state));
  tokenizer = EggTokenizerFactory::createFromString("/* Comment */0");
  ASSERT_EQ(EggTokenizerKind::Integer, tokenizer->next(item, state));
}

TEST(TestEggTokenizer, Integer) {
  EggTokenizerItem item;
  auto tokenizer = EggTokenizerFactory::createFromString("12345 -12345");
  EggTokenizerState state = EggTokenizerState::ExpectNumberLiteral;
  ASSERT_EQ(EggTokenizerKind::Integer, tokenizer->next(item, state));
  ASSERT_EQ(12345, item.value.i);
  ASSERT_EQ(EggTokenizerKind::Integer, tokenizer->next(item, state));
  ASSERT_EQ(-12345, item.value.i);
}

TEST(TestEggTokenizer, Float) {
  EggTokenizerItem item;
  auto tokenizer = EggTokenizerFactory::createFromString("3.14159 -3.14159");
  EggTokenizerState state = EggTokenizerState::ExpectNumberLiteral;
  ASSERT_EQ(EggTokenizerKind::Float, tokenizer->next(item, state));
  ASSERT_EQ(3.14159, item.value.f);
  ASSERT_EQ(EggTokenizerKind::Float, tokenizer->next(item, state));
  ASSERT_EQ(-3.14159, item.value.f);
}

TEST(TestEggTokenizer, String) {
  EggTokenizerItem item;
  auto tokenizer = EggTokenizerFactory::createFromString("\"hello\" `world`");
  EggTokenizerState state = EggTokenizerState::ExpectNone;
  ASSERT_EQ(EggTokenizerKind::String, tokenizer->next(item, state));
  ASSERT_EQ("hello", item.value.s);
  ASSERT_EQ(EggTokenizerKind::String, tokenizer->next(item, state));
  ASSERT_EQ("world", item.value.s);
}

TEST(TestEggTokenizer, ExampleFile) {
  EggTokenizerItem item;
  auto tokenizer = EggTokenizerFactory::createFromPath("~/cpp/test/data/example.egg");
  EggTokenizerState state = EggTokenizerState::ExpectNone;
  size_t count = 0;
  while (tokenizer->next(item, state) != EggTokenizerKind::EndOfFile) {
    count++;
  }
  ASSERT_EQ(22, count);
}
