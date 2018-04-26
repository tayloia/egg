#include "test.h"

#include <cmath>

TEST(TestStrings, Empty) {
  egg::lang::String s1;
  ASSERT_EQ(0u, s1.length());
  auto s2 = s1;
  ASSERT_EQ(0u, s2.length());
  s1 = egg::lang::String::fromUTF8("nothing");
  ASSERT_EQ(7u, s1.length());
  ASSERT_EQ(0u, s2.length());
}

TEST(TestStrings, UTF8) {
  auto s1 = egg::lang::String::fromUTF8("hello world");
  ASSERT_EQ(11u, s1.length());
  auto s2 = s1;
  ASSERT_EQ(11u, s2.length());
  s1 = egg::lang::String::Empty;
  ASSERT_EQ(0u, s1.length());
  ASSERT_EQ(11u, s2.length());
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
  ASSERT_EQ(0u, value);
  ASSERT_TRUE(egg::yolk::String::tryParseUnsigned(value, "1234567890"));
  ASSERT_EQ(1234567890u, value);
  ASSERT_TRUE(egg::yolk::String::tryParseUnsigned(value, "1234567890ABCDEF", 16));
  ASSERT_EQ(0x1234567890ABCDEFu, value);
  ASSERT_TRUE(egg::yolk::String::tryParseUnsigned(value, "0x1234567890ABCDEF", 16));
  ASSERT_EQ(0x1234567890ABCDEFu, value);
}

TEST(TestStrings, TryParseUnsignedBad) {
  uint64_t value = 123456;
  ASSERT_FALSE(egg::yolk::String::tryParseUnsigned(value, ""));
  ASSERT_FALSE(egg::yolk::String::tryParseUnsigned(value, "xxx"));
  ASSERT_FALSE(egg::yolk::String::tryParseUnsigned(value, "123xxx"));
  ASSERT_FALSE(egg::yolk::String::tryParseUnsigned(value, "0x123"));
  ASSERT_EQ(123456u, value);
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
  ASSERT_EQ("0", egg::yolk::String::fromUnsigned(0u));
  ASSERT_EQ("10", egg::yolk::String::fromUnsigned(10u));
  ASSERT_EQ("123456789", egg::yolk::String::fromUnsigned(123456789u));
  ASSERT_EQ("18446744073709551615", egg::yolk::String::fromUnsigned(18446744073709551615u));
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
  ASSERT_EQ("1234567890.0", egg::yolk::String::fromFloat(1234567890));
  // Large values
  ASSERT_EQ("1.0e+030", egg::yolk::String::fromFloat(1e30));
  ASSERT_EQ("-1.0e+030", egg::yolk::String::fromFloat(-1e30));
  ASSERT_EQ("1.0e+300", egg::yolk::String::fromFloat(1e300));
  ASSERT_EQ("-1.0e+300", egg::yolk::String::fromFloat(-1e300));
  // Small values
  ASSERT_EQ("1.0e-030", egg::yolk::String::fromFloat(1e-30));
  ASSERT_EQ("-1.0e-030", egg::yolk::String::fromFloat(-1e-30));
  ASSERT_EQ("1.0e-300", egg::yolk::String::fromFloat(1e-300));
  ASSERT_EQ("-1.0e-300", egg::yolk::String::fromFloat(-1e-300));
  // Denormalized values
  ASSERT_EQ("1.0e-310", egg::yolk::String::fromFloat(1e-310));
  ASSERT_EQ("-1.0e-310", egg::yolk::String::fromFloat(-1e-310));
  // Rounded values
  ASSERT_EQ("0.333333333333", egg::yolk::String::fromFloat(1.0 / 3.0));
  ASSERT_EQ("-0.333333333333", egg::yolk::String::fromFloat(-1.0 / 3.0));
  ASSERT_EQ("0.666666666667", egg::yolk::String::fromFloat(2.0 / 3.0));
  ASSERT_EQ("-0.666666666667", egg::yolk::String::fromFloat(-2.0 / 3.0));
  ASSERT_EQ("0.00775193798450", egg::yolk::String::fromFloat(1.0 / 129.0)); // Note trailing zero
  ASSERT_EQ("3.14159265359", egg::yolk::String::fromFloat(3.1415926535897932384626433832795));
  // Scientific notation
  ASSERT_EQ("0.000000000000001", egg::yolk::String::fromFloat(1e-15));
  ASSERT_EQ("1.0e-016", egg::yolk::String::fromFloat(1e-16));
  ASSERT_EQ("100000000000000.0", egg::yolk::String::fromFloat(1e14));
  ASSERT_EQ("1.0e+015", egg::yolk::String::fromFloat(1e15));
  ASSERT_EQ("1.23e-015", egg::yolk::String::fromFloat(1.23e-15));
  ASSERT_EQ("1.23e-014", egg::yolk::String::fromFloat(1.23e-14));
  ASSERT_EQ("0.000000000000123", egg::yolk::String::fromFloat(1.23e-13));
  ASSERT_EQ("12300000000000.0", egg::yolk::String::fromFloat(1.23e13));
  ASSERT_EQ("123000000000000.0", egg::yolk::String::fromFloat(1.23e14));
  ASSERT_EQ("1.23e+015", egg::yolk::String::fromFloat(1.23e15));
  // Significant digits
  ASSERT_EQ("1.0e+005", egg::yolk::String::fromFloat(123456, 1));
  ASSERT_EQ("1.2e+005", egg::yolk::String::fromFloat(123456, 2));
  ASSERT_EQ("123000.0", egg::yolk::String::fromFloat(123456, 3));
  ASSERT_EQ("123500.0", egg::yolk::String::fromFloat(123456, 4));
  ASSERT_EQ("123460.0", egg::yolk::String::fromFloat(123456, 5));
  ASSERT_EQ("123456.0", egg::yolk::String::fromFloat(123456, 6));
  ASSERT_EQ("123456.0", egg::yolk::String::fromFloat(123456, 7));
  ASSERT_EQ("0.1", egg::yolk::String::fromFloat(0.123456, 1));
  ASSERT_EQ("0.12", egg::yolk::String::fromFloat(0.123456, 2));
  ASSERT_EQ("0.123", egg::yolk::String::fromFloat(0.123456, 3));
  ASSERT_EQ("0.1235", egg::yolk::String::fromFloat(0.123456, 4));
  ASSERT_EQ("0.12346", egg::yolk::String::fromFloat(0.123456, 5));
  ASSERT_EQ("0.123456", egg::yolk::String::fromFloat(0.123456, 6));
  ASSERT_EQ("0.123456", egg::yolk::String::fromFloat(0.123456, 7));
  ASSERT_EQ("0.0001", egg::yolk::String::fromFloat(0.000123456, 1));
  ASSERT_EQ("0.00012", egg::yolk::String::fromFloat(0.000123456, 2));
  ASSERT_EQ("0.000123", egg::yolk::String::fromFloat(0.000123456, 3));
  ASSERT_EQ("0.0001235", egg::yolk::String::fromFloat(0.000123456, 4));
  ASSERT_EQ("0.00012346", egg::yolk::String::fromFloat(0.000123456, 5));
  ASSERT_EQ("0.000123456", egg::yolk::String::fromFloat(0.000123456, 6));
  ASSERT_EQ("0.000123456", egg::yolk::String::fromFloat(0.000123456, 7));
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

TEST(TestStrings, Contains) {
  ASSERT_TRUE(egg::lang::String::Empty.contains(egg::lang::String::Empty));
  ASSERT_FALSE(egg::lang::String::Empty.contains(egg::lang::String::fromCodePoint('e')));
  ASSERT_FALSE(egg::lang::String::Empty.contains(egg::lang::String::fromUTF8("egg")));
  ASSERT_FALSE(egg::lang::String::Empty.contains(egg::lang::String::fromUTF8("beggar")));

  ASSERT_TRUE(egg::lang::String::fromCodePoint('e').contains(egg::lang::String::Empty));
  ASSERT_TRUE(egg::lang::String::fromCodePoint('e').contains(egg::lang::String::fromCodePoint('e')));
  ASSERT_FALSE(egg::lang::String::fromCodePoint('e').contains(egg::lang::String::fromUTF8("egg")));
  ASSERT_FALSE(egg::lang::String::fromCodePoint('e').contains(egg::lang::String::fromUTF8("beggar")));

  ASSERT_TRUE(egg::lang::String::fromUTF8("egg").contains(egg::lang::String::Empty));
  ASSERT_TRUE(egg::lang::String::fromUTF8("egg").contains(egg::lang::String::fromCodePoint('e')));
  ASSERT_TRUE(egg::lang::String::fromUTF8("egg").contains(egg::lang::String::fromUTF8("egg")));
  ASSERT_FALSE(egg::lang::String::fromUTF8("egg").contains(egg::lang::String::fromUTF8("beggar")));

  ASSERT_TRUE(egg::lang::String::fromUTF8("beggar").contains(egg::lang::String::Empty));
  ASSERT_TRUE(egg::lang::String::fromUTF8("beggar").contains(egg::lang::String::fromCodePoint('e')));
  ASSERT_TRUE(egg::lang::String::fromUTF8("beggar").contains(egg::lang::String::fromUTF8("egg")));
  ASSERT_TRUE(egg::lang::String::fromUTF8("beggar").contains(egg::lang::String::fromUTF8("beggar")));
}