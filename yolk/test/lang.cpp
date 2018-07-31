#include "test.h"

namespace {
  enum class Spectrum {
    // ZX Spectrum colour codes
    Black = 0x0,
    Blue = 0x1,
    Red = 0x2,
    Magenta = 0x3,
    Green = 0x4,
    Cyan = 0x5,
    Yellow = 0x6,
    White = 0x7
  };
}

TEST(TestLang, BitsHasAllSet) {
  ASSERT_FALSE(egg::lang::Bits::hasAllSet(Spectrum::Black, Spectrum::Yellow));
  ASSERT_FALSE(egg::lang::Bits::hasAllSet(Spectrum::Blue, Spectrum::Yellow));
  ASSERT_FALSE(egg::lang::Bits::hasAllSet(Spectrum::Red, Spectrum::Yellow));
  ASSERT_FALSE(egg::lang::Bits::hasAllSet(Spectrum::Magenta, Spectrum::Yellow));
  ASSERT_FALSE(egg::lang::Bits::hasAllSet(Spectrum::Green, Spectrum::Yellow));
  ASSERT_FALSE(egg::lang::Bits::hasAllSet(Spectrum::Cyan, Spectrum::Yellow));
  ASSERT_TRUE(egg::lang::Bits::hasAllSet(Spectrum::Yellow, Spectrum::Yellow));
  ASSERT_TRUE(egg::lang::Bits::hasAllSet(Spectrum::White, Spectrum::Yellow));
}

TEST(TestLang, BitsHasAnySet) {
  ASSERT_FALSE(egg::lang::Bits::hasAnySet(Spectrum::Black, Spectrum::Yellow));
  ASSERT_FALSE(egg::lang::Bits::hasAnySet(Spectrum::Blue, Spectrum::Yellow));
  ASSERT_TRUE(egg::lang::Bits::hasAnySet(Spectrum::Red, Spectrum::Yellow));
  ASSERT_TRUE(egg::lang::Bits::hasAnySet(Spectrum::Magenta, Spectrum::Yellow));
  ASSERT_TRUE(egg::lang::Bits::hasAnySet(Spectrum::Green, Spectrum::Yellow));
  ASSERT_TRUE(egg::lang::Bits::hasAnySet(Spectrum::Cyan, Spectrum::Yellow));
  ASSERT_TRUE(egg::lang::Bits::hasAnySet(Spectrum::Yellow, Spectrum::Yellow));
  ASSERT_TRUE(egg::lang::Bits::hasAnySet(Spectrum::White, Spectrum::Yellow));
}

TEST(TestLang, BitsMask) {
  ASSERT_EQ(Spectrum::Black, egg::lang::Bits::mask(Spectrum::Black, Spectrum::Yellow));
  ASSERT_EQ(Spectrum::Black, egg::lang::Bits::mask(Spectrum::Blue, Spectrum::Yellow));
  ASSERT_EQ(Spectrum::Red, egg::lang::Bits::mask(Spectrum::Red, Spectrum::Yellow));
  ASSERT_EQ(Spectrum::Red, egg::lang::Bits::mask(Spectrum::Magenta, Spectrum::Yellow));
  ASSERT_EQ(Spectrum::Green, egg::lang::Bits::mask(Spectrum::Green, Spectrum::Yellow));
  ASSERT_EQ(Spectrum::Green, egg::lang::Bits::mask(Spectrum::Cyan, Spectrum::Yellow));
  ASSERT_EQ(Spectrum::Yellow, egg::lang::Bits::mask(Spectrum::Yellow, Spectrum::Yellow));
  ASSERT_EQ(Spectrum::Yellow, egg::lang::Bits::mask(Spectrum::White, Spectrum::Yellow));
}

TEST(TestLang, BitsSet) {
  ASSERT_EQ(Spectrum::Yellow, egg::lang::Bits::set(Spectrum::Black, Spectrum::Yellow));
  ASSERT_EQ(Spectrum::White, egg::lang::Bits::set(Spectrum::Blue, Spectrum::Yellow));
  ASSERT_EQ(Spectrum::Yellow, egg::lang::Bits::set(Spectrum::Red, Spectrum::Yellow));
  ASSERT_EQ(Spectrum::White, egg::lang::Bits::set(Spectrum::Magenta, Spectrum::Yellow));
  ASSERT_EQ(Spectrum::Yellow, egg::lang::Bits::set(Spectrum::Green, Spectrum::Yellow));
  ASSERT_EQ(Spectrum::White, egg::lang::Bits::set(Spectrum::Cyan, Spectrum::Yellow));
  ASSERT_EQ(Spectrum::Yellow, egg::lang::Bits::set(Spectrum::Yellow, Spectrum::Yellow));
  ASSERT_EQ(Spectrum::White, egg::lang::Bits::set(Spectrum::White, Spectrum::Yellow));
}

TEST(TestLang, BitsClear) {
  ASSERT_EQ(Spectrum::Black, egg::lang::Bits::clear(Spectrum::Black, Spectrum::Yellow));
  ASSERT_EQ(Spectrum::Blue, egg::lang::Bits::clear(Spectrum::Blue, Spectrum::Yellow));
  ASSERT_EQ(Spectrum::Black, egg::lang::Bits::clear(Spectrum::Red, Spectrum::Yellow));
  ASSERT_EQ(Spectrum::Blue, egg::lang::Bits::clear(Spectrum::Magenta, Spectrum::Yellow));
  ASSERT_EQ(Spectrum::Black, egg::lang::Bits::clear(Spectrum::Green, Spectrum::Yellow));
  ASSERT_EQ(Spectrum::Blue, egg::lang::Bits::clear(Spectrum::Cyan, Spectrum::Yellow));
  ASSERT_EQ(Spectrum::Black, egg::lang::Bits::clear(Spectrum::Yellow, Spectrum::Yellow));
  ASSERT_EQ(Spectrum::Blue, egg::lang::Bits::clear(Spectrum::White, Spectrum::Yellow));
}

TEST(TestLang, BitsInvert) {
  ASSERT_EQ(Spectrum::Yellow, egg::lang::Bits::invert(Spectrum::Black, Spectrum::Yellow));
  ASSERT_EQ(Spectrum::White, egg::lang::Bits::invert(Spectrum::Blue, Spectrum::Yellow));
  ASSERT_EQ(Spectrum::Green, egg::lang::Bits::invert(Spectrum::Red, Spectrum::Yellow));
  ASSERT_EQ(Spectrum::Cyan, egg::lang::Bits::invert(Spectrum::Magenta, Spectrum::Yellow));
  ASSERT_EQ(Spectrum::Red, egg::lang::Bits::invert(Spectrum::Green, Spectrum::Yellow));
  ASSERT_EQ(Spectrum::Magenta, egg::lang::Bits::invert(Spectrum::Cyan, Spectrum::Yellow));
  ASSERT_EQ(Spectrum::Black, egg::lang::Bits::invert(Spectrum::Yellow, Spectrum::Yellow));
  ASSERT_EQ(Spectrum::Blue, egg::lang::Bits::invert(Spectrum::White, Spectrum::Yellow));
}

TEST(TestLang, StringBuilder) {
  egg::lang::StringBuilder sb;
  ASSERT_TRUE(sb.empty());
  sb.add("Hello", ' ', "World");
  ASSERT_EQ("Hello World", sb.str().toUTF8());
  ASSERT_EQ("Hello World", sb.toUTF8());
  ASSERT_FALSE(sb.empty());
  sb.add('!');
  ASSERT_EQ("Hello World!", sb.str().toUTF8());
  ASSERT_EQ("Hello World!", sb.toUTF8());
  ASSERT_FALSE(sb.empty());
}
