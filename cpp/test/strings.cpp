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

TEST(TestStrings, FromUnsigned) {
  ASSERT_EQ("0", egg::yolk::String::fromUnsigned(0));
  ASSERT_EQ("10", egg::yolk::String::fromUnsigned(10));
  ASSERT_EQ("123456789", egg::yolk::String::fromUnsigned(123456789));
  ASSERT_EQ("18446744073709551615", egg::yolk::String::fromUnsigned(18446744073709551615));
}

TEST(TestStrings, FromSigned) {
  ASSERT_EQ("-9223372036854775808", egg::yolk::String::fromSigned(-9223372036854775807 - 1));
  ASSERT_EQ("-123456789", egg::yolk::String::fromSigned(-123456789));
  ASSERT_EQ("-10", egg::yolk::String::fromSigned(-10));
  ASSERT_EQ("0", egg::yolk::String::fromSigned(0));
  ASSERT_EQ("10", egg::yolk::String::fromSigned(10));
  ASSERT_EQ("123456789", egg::yolk::String::fromSigned(123456789));
  ASSERT_EQ("9223372036854775807", egg::yolk::String::fromSigned(9223372036854775807));
}

TEST(TestStrings, FromFloat) {
  ASSERT_EQ("0.0", egg::yolk::String::fromFloat(0.0));
  ASSERT_EQ("-0.0", egg::yolk::String::fromFloat(-0.0));
  ASSERT_EQ("1.2345", egg::yolk::String::fromFloat(1.2345));
  ASSERT_EQ("-1.2345", egg::yolk::String::fromFloat(-1.2345));
  ASSERT_EQ("0.012345", egg::yolk::String::fromFloat(0.012345));
  ASSERT_EQ("-0.012345", egg::yolk::String::fromFloat(-0.012345));
  ASSERT_EQ("1234567890.0", egg::yolk::String::fromFloat(1234567890.0));
  // Large values
  ASSERT_EQ("1e30", egg::yolk::String::fromFloat(1e30)); // WIBBLE
  ASSERT_EQ("-1e30", egg::yolk::String::fromFloat(-1e30)); // WIBBLE
  ASSERT_EQ("1e300", egg::yolk::String::fromFloat(1e300)); // WIBBLE
  ASSERT_EQ("-1e300", egg::yolk::String::fromFloat(-1e300)); // WIBBLE
  // Small values
  //ASSERT_EQ("1e-30", egg::yolk::String::fromFloat(1e-30)); // WIBBLE dtoa produces "0.0"
  //ASSERT_EQ("-1e-30", egg::yolk::String::fromFloat(-1e-30)); // WIBBLE dtoa produces "-0.0"
  //ASSERT_EQ("1e-300", egg::yolk::String::fromFloat(1e-300)); // WIBBLE dtoa produces "0.0"
  //ASSERT_EQ("-1e-300", egg::yolk::String::fromFloat(-1e-300)); // WIBBLE dtoa produces "-0.0"
  // Denormalized values
  //ASSERT_EQ("1e-310", egg::yolk::String::fromFloat(1e-310)); // WIBBLE dtoa produces "0.0"
  //ASSERT_EQ("-1e-310", egg::yolk::String::fromFloat(-1e-310)); // WIBBLE dtoa produces "-0.0"
  // Rounded values
  ASSERT_EQ("0.3333333333333333", egg::yolk::String::fromFloat(1.0 / 3.0));
  ASSERT_EQ("-0.3333333333333333", egg::yolk::String::fromFloat(-1.0 / 3.0));
  //ASSERT_EQ("0.6666666666666667", egg::yolk::String::fromFloat(2.0 / 3.0)); // WIBBLE dtoa produces "0.6666666666666666"
  //ASSERT_EQ("-0.6666666666666667", egg::yolk::String::fromFloat(-2.0 / 3.0)); // WIBBLE dtoa produces "-0.6666666666666666"
  ASSERT_EQ("0.0077519379844961", egg::yolk::String::fromFloat(1.0 / 129.0));
  ASSERT_EQ("3.141592653589793", egg::yolk::String::fromFloat(3.1415926535897932384626433832795));
}

TEST(TestStrings, FromFloatBad) {
  // These aren't really bad, they're just special
  const double pnan = std::nan("");
  ASSERT_EQ("nan", egg::yolk::String::fromFloat(pnan));
  const double nnan = std::copysign(pnan, -1);
  ASSERT_EQ("-nan", egg::yolk::String::fromFloat(nnan));
  const double pinf = std::numeric_limits<double>::infinity();
  ASSERT_EQ("inf", egg::yolk::String::fromFloat(pinf));
  const double ninf = std::copysign(pinf, -1);
  ASSERT_EQ("-inf", egg::yolk::String::fromFloat(ninf));
}
