#include "test.h"

TEST(TestStrings, Empty) {
  egg::lang::String s1;
  ASSERT_EQ(0, s1.length());
  auto s2 = s1;
  ASSERT_EQ(0, s2.length());
  s1 = egg::lang::String::fromUTF8("nothing");
  ASSERT_EQ(7, s1.length());
  ASSERT_EQ(0, s2.length());
}

TEST(TestStrings, UTF8) {
  auto s1 = egg::lang::String::fromUTF8("hello world");
  ASSERT_EQ(11, s1.length());
  auto s2 = s1;
  ASSERT_EQ(11, s2.length());
  s1 = egg::lang::String::Empty;
  ASSERT_EQ(0, s1.length());
  ASSERT_EQ(11, s2.length());
}

TEST(TestStrings, StartsWith) {
  ASSERT_TRUE(egg::yolk::String::startsWith("Hello World", "Hello"));
  ASSERT_TRUE(egg::yolk::String::startsWith("Hello World", "Hello World"));
  ASSERT_FALSE(egg::yolk::String::startsWith("Hello World", "World"));
  ASSERT_FALSE(egg::yolk::String::startsWith("Hello", "Hello World"));
}

TEST(TestStrings, EndsWith) {
  ASSERT_FALSE(egg::yolk::String::endsWith("Hello World", "Hello"));
  ASSERT_TRUE(egg::yolk::String::endsWith("Hello World", "Hello World"));
  ASSERT_TRUE(egg::yolk::String::endsWith("Hello World", "World"));
  ASSERT_FALSE(egg::yolk::String::endsWith("Hello", "Hello World"));
}

TEST(TestStrings, AssertMacros) {
  ASSERT_CONTAINS("Hello World", "lo");
  ASSERT_NOTCONTAINS("Hello World", "Goodbye");
  ASSERT_STARTSWITH("Hello World", "Hello");
  ASSERT_ENDSWITH("Hello World", "World");
}

TEST(TestStrings, ToLower) {
  ASSERT_EQ("hello world!", egg::yolk::String::toLower("Hello World!"));
}

TEST(TestStrings, ToUpper) {
  ASSERT_EQ("HELLO WORLD!", egg::yolk::String::toUpper("Hello World!"));
}

TEST(TestStrings, Replace) {
  ASSERT_EQ("Hell0 W0rld!", egg::yolk::String::replace("Hello World!", 'o', '0'));
}

TEST(TestStrings, Terminate) {
  std::string str = "Hello World";
  egg::yolk::String::terminate(str, '!');
  ASSERT_EQ("Hello World!", str);
  egg::yolk::String::terminate(str, '!');
  ASSERT_EQ("Hello World!", str);
}

TEST(TestStrings, TryParseSigned) {
  int64_t value = 1;
  ASSERT_TRUE(egg::yolk::String::tryParseSigned(value, "0"));
  ASSERT_EQ(0, value);
  ASSERT_TRUE(egg::yolk::String::tryParseSigned(value, "1234567890"));
  ASSERT_EQ(1234567890, value);
  ASSERT_TRUE(egg::yolk::String::tryParseSigned(value, "-1234567890"));
  ASSERT_EQ(-1234567890, value);
  ASSERT_TRUE(egg::yolk::String::tryParseSigned(value, "1234567890ABCDEF", 16));
  ASSERT_EQ(0x1234567890ABCDEF, value);
  ASSERT_TRUE(egg::yolk::String::tryParseSigned(value, "-1234567890ABCDEF", 16));
  ASSERT_EQ(-0x1234567890ABCDEF, value);
  ASSERT_TRUE(egg::yolk::String::tryParseSigned(value, "0x1234567890ABCDEF", 16));
  ASSERT_EQ(0x1234567890ABCDEF, value);
  ASSERT_TRUE(egg::yolk::String::tryParseSigned(value, "-0x1234567890ABCDEF", 16));
  ASSERT_EQ(-0x1234567890ABCDEF, value);
}

TEST(TestStrings, TryParseSignedBad) {
  int64_t value = -123;
  ASSERT_FALSE(egg::yolk::String::tryParseSigned(value, ""));
  ASSERT_FALSE(egg::yolk::String::tryParseSigned(value, "xxx"));
  ASSERT_FALSE(egg::yolk::String::tryParseSigned(value, "123xxx"));
  ASSERT_FALSE(egg::yolk::String::tryParseSigned(value, "0x123xxx"));
  ASSERT_EQ(-123, value);
}

TEST(TestStrings, TryParseUnsigned) {
  uint64_t value = 1;
  ASSERT_TRUE(egg::yolk::String::tryParseUnsigned(value, "0"));
  ASSERT_EQ(0, value);
  ASSERT_TRUE(egg::yolk::String::tryParseUnsigned(value, "1234567890"));
  ASSERT_EQ(1234567890, value);
  ASSERT_TRUE(egg::yolk::String::tryParseUnsigned(value, "1234567890ABCDEF", 16));
  ASSERT_EQ(0x1234567890ABCDEF, value);
  ASSERT_TRUE(egg::yolk::String::tryParseUnsigned(value, "0x1234567890ABCDEF", 16));
  ASSERT_EQ(0x1234567890ABCDEF, value);
}

TEST(TestStrings, TryParseUnsignedBad) {
  uint64_t value = 123;
  ASSERT_FALSE(egg::yolk::String::tryParseUnsigned(value, ""));
  ASSERT_FALSE(egg::yolk::String::tryParseUnsigned(value, "xxx"));
  ASSERT_FALSE(egg::yolk::String::tryParseUnsigned(value, "123xxx"));
  ASSERT_FALSE(egg::yolk::String::tryParseUnsigned(value, "0x123"));
  ASSERT_EQ(123, value);
}

TEST(TestStrings, TryParseFloat) {
  double value = 1;
  ASSERT_TRUE(egg::yolk::String::tryParseFloat(value, "0"));
  ASSERT_EQ(0, value);
  ASSERT_TRUE(egg::yolk::String::tryParseFloat(value, "1234567890"));
  ASSERT_EQ(1234567890, value);
  ASSERT_TRUE(egg::yolk::String::tryParseFloat(value, "-1234567890"));
  ASSERT_EQ(-1234567890, value);
  ASSERT_TRUE(egg::yolk::String::tryParseFloat(value, "1.0"));
  ASSERT_EQ(1.0, value);
  ASSERT_TRUE(egg::yolk::String::tryParseFloat(value, "-1.0"));
  ASSERT_EQ(-1.0, value);
  ASSERT_TRUE(egg::yolk::String::tryParseFloat(value, "1.23"));
  ASSERT_EQ(1.23, value);
  ASSERT_TRUE(egg::yolk::String::tryParseFloat(value, "-1.23"));
  ASSERT_EQ(-1.23, value);
  ASSERT_TRUE(egg::yolk::String::tryParseFloat(value, "1e3"));
  ASSERT_EQ(1e3, value);
  ASSERT_TRUE(egg::yolk::String::tryParseFloat(value, "-1e3"));
  ASSERT_EQ(-1e3, value);
  ASSERT_TRUE(egg::yolk::String::tryParseFloat(value, "1.2e3"));
  ASSERT_EQ(1.2e3, value);
  ASSERT_TRUE(egg::yolk::String::tryParseFloat(value, "-1.2e3"));
  ASSERT_EQ(-1.2e3, value);
  ASSERT_TRUE(egg::yolk::String::tryParseFloat(value, "1.2e+03"));
  ASSERT_EQ(1.2e+03, value);
  ASSERT_TRUE(egg::yolk::String::tryParseFloat(value, "-1.2e+03"));
  ASSERT_EQ(-1.2e+03, value);
  ASSERT_TRUE(egg::yolk::String::tryParseFloat(value, "1.2e-03"));
  ASSERT_EQ(1.2e-03, value);
  ASSERT_TRUE(egg::yolk::String::tryParseFloat(value, "-1.2e-03"));
  ASSERT_EQ(-1.2e-03, value);
}

TEST(TestStrings, TryParseFloatBad) {
  double value = -123;
  ASSERT_FALSE(egg::yolk::String::tryParseFloat(value, ""));
  ASSERT_FALSE(egg::yolk::String::tryParseFloat(value, "xxx"));
  ASSERT_FALSE(egg::yolk::String::tryParseFloat(value, "123xxx"));
  ASSERT_FALSE(egg::yolk::String::tryParseFloat(value, "1.0xxx"));
  ASSERT_FALSE(egg::yolk::String::tryParseFloat(value, "-1.0xxx"));
  ASSERT_FALSE(egg::yolk::String::tryParseFloat(value, "1.23xxx"));
  ASSERT_FALSE(egg::yolk::String::tryParseFloat(value, "-1.23xxx"));
  ASSERT_FALSE(egg::yolk::String::tryParseFloat(value, "1e3xxx"));
  ASSERT_FALSE(egg::yolk::String::tryParseFloat(value, "-1e3xxx"));
  ASSERT_FALSE(egg::yolk::String::tryParseFloat(value, "1.2e3xxx"));
  ASSERT_FALSE(egg::yolk::String::tryParseFloat(value, "-1.2e3xxx"));
  ASSERT_FALSE(egg::yolk::String::tryParseFloat(value, "1.2e+xx"));
  ASSERT_FALSE(egg::yolk::String::tryParseFloat(value, "-1.2e+xx"));
  ASSERT_FALSE(egg::yolk::String::tryParseFloat(value, "1e-999"));
  ASSERT_FALSE(egg::yolk::String::tryParseFloat(value, "-1e-999"));
  ASSERT_FALSE(egg::yolk::String::tryParseFloat(value, "1e999"));
  ASSERT_FALSE(egg::yolk::String::tryParseFloat(value, "-1e999"));
  ASSERT_EQ(-123, value);
}
