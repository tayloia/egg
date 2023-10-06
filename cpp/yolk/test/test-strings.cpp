#include "yolk/test.h"

TEST(TestStrings, Empty) {
  egg::test::Allocator allocator;
  egg::ovum::String s1;
  ASSERT_EQ(0u, s1.length());
  auto s2{ s1 };
  ASSERT_EQ(0u, s2.length());
  s1 = allocator.concat("nothing");
  ASSERT_EQ(7u, s1.length());
  ASSERT_EQ(0u, s2.length());
}

TEST(TestStrings, UTF8) {
  egg::test::Allocator allocator;
  auto s1 = allocator.concat("hello world");
  ASSERT_EQ(11u, s1.length());
  auto s2{ s1 };
  ASSERT_EQ(11u, s2.length());
  s1 = egg::ovum::String();
  ASSERT_EQ(0u, s1.length());
  ASSERT_EQ(11u, s2.length());
}

TEST(TestStrings, YolkStartsWith) {
  ASSERT_TRUE(egg::yolk::String::startsWith("Hello World", "Hello"));
  ASSERT_TRUE(egg::yolk::String::startsWith("Hello World", "Hello World"));
  ASSERT_FALSE(egg::yolk::String::startsWith("Hello World", "World"));
  ASSERT_FALSE(egg::yolk::String::startsWith("Hello", "Hello World"));
}

TEST(TestStrings, YolkEndsWith) {
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

TEST(TestStrings, YolkToLower) {
  ASSERT_EQ("hello world!", egg::yolk::String::toLower("Hello World!"));
}

TEST(TestStrings, YolkToUpper) {
  ASSERT_EQ("HELLO WORLD!", egg::yolk::String::toUpper("Hello World!"));
}

TEST(TestStrings, YolkReplace) {
  ASSERT_EQ("Hell0 W0rld!", egg::yolk::String::replace("Hello World!", 'o', '0'));
}

TEST(TestStrings, YolkTerminate) {
  std::string str = "Hello World";
  egg::yolk::String::terminate(str, '!');
  ASSERT_EQ("Hello World!", str);
  egg::yolk::String::terminate(str, '!');
  ASSERT_EQ("Hello World!", str);
}

TEST(TestStrings, YolkTryParseSigned) {
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

TEST(TestStrings, YolkTryParseSignedBad) {
  int64_t value = -123;
  ASSERT_FALSE(egg::yolk::String::tryParseSigned(value, ""));
  ASSERT_FALSE(egg::yolk::String::tryParseSigned(value, "xxx"));
  ASSERT_FALSE(egg::yolk::String::tryParseSigned(value, "123xxx"));
  ASSERT_FALSE(egg::yolk::String::tryParseSigned(value, "0x123xxx"));
  ASSERT_EQ(-123, value);
}

TEST(TestStrings, YolkTryParseUnsigned) {
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

TEST(TestStrings, YolkTryParseUnsignedBad) {
  uint64_t value = 123456;
  ASSERT_FALSE(egg::yolk::String::tryParseUnsigned(value, ""));
  ASSERT_FALSE(egg::yolk::String::tryParseUnsigned(value, "xxx"));
  ASSERT_FALSE(egg::yolk::String::tryParseUnsigned(value, "123xxx"));
  ASSERT_FALSE(egg::yolk::String::tryParseUnsigned(value, "0x123"));
  ASSERT_EQ(123456u, value);
}

TEST(TestStrings, YolkTryParseFloat) {
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

TEST(TestStrings, YolkTryParseFloatBad) {
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

TEST(TestStrings, YolkFromUnsigned) {
  ASSERT_EQ("0", egg::yolk::String::fromUnsigned(0u));
  ASSERT_EQ("10", egg::yolk::String::fromUnsigned(10u));
  ASSERT_EQ("123456789", egg::yolk::String::fromUnsigned(123456789u));
  ASSERT_EQ("18446744073709551615", egg::yolk::String::fromUnsigned(18446744073709551615u));
}

TEST(TestStrings, YolkFromSigned) {
  ASSERT_EQ("-9223372036854775808", egg::yolk::String::fromSigned(-9223372036854775807 - 1));
  ASSERT_EQ("-123456789", egg::yolk::String::fromSigned(-123456789));
  ASSERT_EQ("-10", egg::yolk::String::fromSigned(-10));
  ASSERT_EQ("0", egg::yolk::String::fromSigned(0));
  ASSERT_EQ("10", egg::yolk::String::fromSigned(10));
  ASSERT_EQ("123456789", egg::yolk::String::fromSigned(123456789));
  ASSERT_EQ("9223372036854775807", egg::yolk::String::fromSigned(9223372036854775807));
}

TEST(TestStrings, YolkFromFloat) {
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

TEST(TestStrings, YolkFromFloatBad) {
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

TEST(TestStrings, Equals) {
  egg::test::Allocator allocator;

  ASSERT_TRUE(egg::ovum::String().equals(egg::ovum::String()));
  ASSERT_FALSE(egg::ovum::String().equals(allocator.concat('e')));
  ASSERT_FALSE(egg::ovum::String().equals(allocator.concat("egg")));
  ASSERT_FALSE(egg::ovum::String().equals(allocator.concat("beggar")));

  ASSERT_FALSE(allocator.concat('e').equals(egg::ovum::String()));
  ASSERT_TRUE(allocator.concat('e').equals(allocator.concat('e')));
  ASSERT_FALSE(allocator.concat('e').equals(allocator.concat("egg")));
  ASSERT_FALSE(allocator.concat('e').equals(allocator.concat("beggar")));

  ASSERT_FALSE(allocator.concat("egg").equals(egg::ovum::String()));
  ASSERT_FALSE(allocator.concat("egg").equals(allocator.concat('e')));
  ASSERT_TRUE(allocator.concat("egg").equals(allocator.concat("egg")));
  ASSERT_FALSE(allocator.concat("egg").equals(allocator.concat("beggar")));

  ASSERT_FALSE(allocator.concat("beggar").equals(egg::ovum::String()));
  ASSERT_FALSE(allocator.concat("beggar").equals(allocator.concat('e')));
  ASSERT_FALSE(allocator.concat("beggar").equals(allocator.concat("egg")));
  ASSERT_TRUE(allocator.concat("beggar").equals(allocator.concat("beggar")));
}

TEST(TestStrings, Less) {
  egg::test::Allocator allocator;

  ASSERT_FALSE(egg::ovum::String().lessThan(egg::ovum::String()));
  ASSERT_TRUE(egg::ovum::String().lessThan(allocator.concat('e')));
  ASSERT_TRUE(egg::ovum::String().lessThan(allocator.concat("egg")));
  ASSERT_TRUE(egg::ovum::String().lessThan(allocator.concat("beggar")));

  ASSERT_FALSE(allocator.concat('e').lessThan(egg::ovum::String()));
  ASSERT_FALSE(allocator.concat('e').lessThan(allocator.concat('e')));
  ASSERT_TRUE(allocator.concat('e').lessThan(allocator.concat("egg")));
  ASSERT_FALSE(allocator.concat('e').lessThan(allocator.concat("beggar")));

  ASSERT_FALSE(allocator.concat("egg").lessThan(egg::ovum::String()));
  ASSERT_FALSE(allocator.concat("egg").lessThan(allocator.concat('e')));
  ASSERT_FALSE(allocator.concat("egg").lessThan(allocator.concat("egg")));
  ASSERT_FALSE(allocator.concat("egg").lessThan(allocator.concat("beggar")));

  ASSERT_FALSE(allocator.concat("beggar").lessThan(egg::ovum::String()));
  ASSERT_TRUE(allocator.concat("beggar").lessThan(allocator.concat('e')));
  ASSERT_TRUE(allocator.concat("beggar").lessThan(allocator.concat("egg")));
  ASSERT_FALSE(allocator.concat("beggar").lessThan(allocator.concat("beggar")));
}

TEST(TestStrings, Compare) {
  egg::test::Allocator allocator;

  ASSERT_EQ(0, egg::ovum::String().compareTo(egg::ovum::String()));
  ASSERT_EQ(-1, egg::ovum::String().compareTo(allocator.concat('e')));
  ASSERT_EQ(-1, egg::ovum::String().compareTo(allocator.concat("egg")));
  ASSERT_EQ(-1, egg::ovum::String().compareTo(allocator.concat("beggar")));

  ASSERT_EQ(1, allocator.concat('e').compareTo(egg::ovum::String()));
  ASSERT_EQ(0, allocator.concat('e').compareTo(allocator.concat('e')));
  ASSERT_EQ(-1, allocator.concat('e').compareTo(allocator.concat("egg")));
  ASSERT_EQ(1, allocator.concat('e').compareTo(allocator.concat("beggar")));

  ASSERT_EQ(1, allocator.concat("egg").compareTo(egg::ovum::String()));
  ASSERT_EQ(1, allocator.concat("egg").compareTo(allocator.concat('e')));
  ASSERT_EQ(0, allocator.concat("egg").compareTo(allocator.concat("egg")));
  ASSERT_EQ(1, allocator.concat("egg").compareTo(allocator.concat("beggar")));

  ASSERT_EQ(1, allocator.concat("beggar").compareTo(egg::ovum::String()));
  ASSERT_EQ(-1, allocator.concat("beggar").compareTo(allocator.concat('e')));
  ASSERT_EQ(-1, allocator.concat("beggar").compareTo(allocator.concat("egg")));
  ASSERT_EQ(0, allocator.concat("beggar").compareTo(allocator.concat("beggar")));
}

TEST(TestStrings, Contains) {
  egg::test::Allocator allocator;

  ASSERT_TRUE(egg::ovum::String().contains(egg::ovum::String()));
  ASSERT_FALSE(egg::ovum::String().contains(allocator.concat('e')));
  ASSERT_FALSE(egg::ovum::String().contains(allocator.concat("egg")));
  ASSERT_FALSE(egg::ovum::String().contains(allocator.concat("beggar")));

  ASSERT_TRUE(allocator.concat('e').contains(egg::ovum::String()));
  ASSERT_TRUE(allocator.concat('e').contains(allocator.concat('e')));
  ASSERT_FALSE(allocator.concat('e').contains(allocator.concat("egg")));
  ASSERT_FALSE(allocator.concat('e').contains(allocator.concat("beggar")));

  ASSERT_TRUE(allocator.concat("egg").contains(egg::ovum::String()));
  ASSERT_TRUE(allocator.concat("egg").contains(allocator.concat('e')));
  ASSERT_TRUE(allocator.concat("egg").contains(allocator.concat("egg")));
  ASSERT_FALSE(allocator.concat("egg").contains(allocator.concat("beggar")));

  ASSERT_TRUE(allocator.concat("beggar").contains(egg::ovum::String()));
  ASSERT_TRUE(allocator.concat("beggar").contains(allocator.concat('e')));
  ASSERT_TRUE(allocator.concat("beggar").contains(allocator.concat("egg")));
  ASSERT_TRUE(allocator.concat("beggar").contains(allocator.concat("beggar")));
}

TEST(TestStrings, StartsWith) {
  egg::test::Allocator allocator;

  ASSERT_TRUE(egg::ovum::String().startsWith(egg::ovum::String()));
  ASSERT_FALSE(egg::ovum::String().startsWith(allocator.concat('e')));
  ASSERT_FALSE(egg::ovum::String().startsWith(allocator.concat("egg")));
  ASSERT_FALSE(egg::ovum::String().startsWith(allocator.concat("beggar")));

  ASSERT_TRUE(allocator.concat('e').startsWith(egg::ovum::String()));
  ASSERT_TRUE(allocator.concat('e').startsWith(allocator.concat('e')));
  ASSERT_FALSE(allocator.concat('e').startsWith(allocator.concat("egg")));
  ASSERT_FALSE(allocator.concat('e').startsWith(allocator.concat("beggar")));

  ASSERT_TRUE(allocator.concat("egg").startsWith(egg::ovum::String()));
  ASSERT_TRUE(allocator.concat("egg").startsWith(allocator.concat('e')));
  ASSERT_TRUE(allocator.concat("egg").startsWith(allocator.concat("egg")));
  ASSERT_FALSE(allocator.concat("egg").startsWith(allocator.concat("beggar")));

  ASSERT_TRUE(allocator.concat("beggar").startsWith(egg::ovum::String()));
  ASSERT_FALSE(allocator.concat("beggar").startsWith(allocator.concat('e')));
  ASSERT_FALSE(allocator.concat("beggar").startsWith(allocator.concat("egg")));
  ASSERT_TRUE(allocator.concat("beggar").startsWith(allocator.concat("beggar")));
}

TEST(TestStrings, IndexOfCodePoint) {
  egg::test::Allocator allocator;

  ASSERT_EQ(-1, egg::ovum::String().indexOfCodePoint(U'e'));
  ASSERT_EQ(0, allocator.concat('e').indexOfCodePoint(U'e'));
  ASSERT_EQ(0, allocator.concat("egg").indexOfCodePoint(U'e'));
  ASSERT_EQ(1, allocator.concat("beggar").indexOfCodePoint(U'e'));

  ASSERT_EQ(-1, egg::ovum::String().indexOfCodePoint(U'g'));
  ASSERT_EQ(-1, allocator.concat('e').indexOfCodePoint(U'g'));
  ASSERT_EQ(1, allocator.concat("egg").indexOfCodePoint(U'g'));
  ASSERT_EQ(2, allocator.concat("beggar").indexOfCodePoint(U'g'));

  ASSERT_EQ(-1, egg::ovum::String().indexOfCodePoint(U'r'));
  ASSERT_EQ(-1, allocator.concat('e').indexOfCodePoint(U'r'));
  ASSERT_EQ(-1, allocator.concat("egg").indexOfCodePoint(U'r'));
  ASSERT_EQ(5, allocator.concat("beggar").indexOfCodePoint(U'r'));
}

TEST(TestStrings, IndexOfString) {
  egg::test::Allocator allocator;

  ASSERT_EQ(0, egg::ovum::String().indexOfString(egg::ovum::String()));
  ASSERT_EQ(-1, egg::ovum::String().indexOfString(allocator.concat('e')));
  ASSERT_EQ(-1, egg::ovum::String().indexOfString(allocator.concat('g')));
  ASSERT_EQ(-1, egg::ovum::String().indexOfString(allocator.concat("egg")));
  ASSERT_EQ(-1, egg::ovum::String().indexOfString(allocator.concat("beggar")));

  ASSERT_EQ(0, allocator.concat('e').indexOfString(egg::ovum::String()));
  ASSERT_EQ(0, allocator.concat('e').indexOfString(allocator.concat('e')));
  ASSERT_EQ(-1, allocator.concat('e').indexOfString(allocator.concat('g')));
  ASSERT_EQ(-1, allocator.concat('e').indexOfString(allocator.concat("egg")));
  ASSERT_EQ(-1, allocator.concat('e').indexOfString(allocator.concat("beggar")));

  ASSERT_EQ(0, allocator.concat("egg").indexOfString(egg::ovum::String()));
  ASSERT_EQ(0, allocator.concat("egg").indexOfString(allocator.concat('e')));
  ASSERT_EQ(1, allocator.concat("egg").indexOfString(allocator.concat('g')));
  ASSERT_EQ(0, allocator.concat("egg").indexOfString(allocator.concat("egg")));
  ASSERT_EQ(-1, allocator.concat("egg").indexOfString(allocator.concat("beggar")));

  ASSERT_EQ(0, allocator.concat("beggar").indexOfString(egg::ovum::String()));
  ASSERT_EQ(1, allocator.concat("beggar").indexOfString(allocator.concat('e')));
  ASSERT_EQ(2, allocator.concat("beggar").indexOfString(allocator.concat('g')));
  ASSERT_EQ(1, allocator.concat("beggar").indexOfString(allocator.concat("egg")));
  ASSERT_EQ(0, allocator.concat("beggar").indexOfString(allocator.concat("beggar")));
}

TEST(TestStrings, LastIndexOfCodePoint) {
  egg::test::Allocator allocator;

  ASSERT_EQ(-1, egg::ovum::String().lastIndexOfCodePoint(U'e'));
  ASSERT_EQ(0, allocator.concat('e').lastIndexOfCodePoint(U'e'));
  ASSERT_EQ(0, allocator.concat("egg").lastIndexOfCodePoint(U'e'));
  ASSERT_EQ(1, allocator.concat("beggar").lastIndexOfCodePoint(U'e'));

  ASSERT_EQ(-1, egg::ovum::String().lastIndexOfCodePoint(U'g'));
  ASSERT_EQ(-1, allocator.concat('e').lastIndexOfCodePoint(U'g'));
  ASSERT_EQ(2, allocator.concat("egg").lastIndexOfCodePoint(U'g'));
  ASSERT_EQ(3, allocator.concat("beggar").lastIndexOfCodePoint(U'g'));

  ASSERT_EQ(-1, egg::ovum::String().lastIndexOfCodePoint(U'r'));
  ASSERT_EQ(-1, allocator.concat('e').lastIndexOfCodePoint(U'r'));
  ASSERT_EQ(-1, allocator.concat("egg").lastIndexOfCodePoint(U'r'));
  ASSERT_EQ(5, allocator.concat("beggar").lastIndexOfCodePoint(U'r'));
}

TEST(TestStrings, LastIndexOfString) {
  egg::test::Allocator allocator;

  ASSERT_EQ(0, egg::ovum::String().lastIndexOfString(egg::ovum::String()));
  ASSERT_EQ(-1, egg::ovum::String().lastIndexOfString(allocator.concat('e')));
  ASSERT_EQ(-1, egg::ovum::String().lastIndexOfString(allocator.concat('g')));
  ASSERT_EQ(-1, egg::ovum::String().lastIndexOfString(allocator.concat("egg")));
  ASSERT_EQ(-1, egg::ovum::String().lastIndexOfString(allocator.concat("beggar")));

  ASSERT_EQ(1, allocator.concat('e').lastIndexOfString(egg::ovum::String()));
  ASSERT_EQ(0, allocator.concat('e').lastIndexOfString(allocator.concat('e')));
  ASSERT_EQ(-1, allocator.concat('e').lastIndexOfString(allocator.concat('g')));
  ASSERT_EQ(-1, allocator.concat('e').lastIndexOfString(allocator.concat("egg")));
  ASSERT_EQ(-1, allocator.concat('e').lastIndexOfString(allocator.concat("beggar")));

  ASSERT_EQ(3, allocator.concat("egg").lastIndexOfString(egg::ovum::String()));
  ASSERT_EQ(0, allocator.concat("egg").lastIndexOfString(allocator.concat('e')));
  ASSERT_EQ(2, allocator.concat("egg").lastIndexOfString(allocator.concat('g')));
  ASSERT_EQ(0, allocator.concat("egg").lastIndexOfString(allocator.concat("egg")));
  ASSERT_EQ(-1, allocator.concat("egg").lastIndexOfString(allocator.concat("beggar")));

  ASSERT_EQ(6, allocator.concat("beggar").lastIndexOfString(egg::ovum::String()));
  ASSERT_EQ(1, allocator.concat("beggar").lastIndexOfString(allocator.concat('e')));
  ASSERT_EQ(3, allocator.concat("beggar").lastIndexOfString(allocator.concat('g')));
  ASSERT_EQ(1, allocator.concat("beggar").lastIndexOfString(allocator.concat("egg")));
  ASSERT_EQ(0, allocator.concat("beggar").lastIndexOfString(allocator.concat("beggar")));
}

TEST(TestStrings, Substring) {
  egg::test::Allocator allocator;

  ASSERT_EQ("", egg::ovum::String().substring(allocator, 0).toUTF8());
  ASSERT_EQ("", egg::ovum::String().substring(allocator, 1).toUTF8());
  ASSERT_EQ("", egg::ovum::String().substring(allocator, 0, 1).toUTF8());
  ASSERT_EQ("", egg::ovum::String().substring(allocator, 0, 2).toUTF8());
  ASSERT_EQ("", egg::ovum::String().substring(allocator, 1, 0).toUTF8());
  ASSERT_EQ("", egg::ovum::String().substring(allocator, 10, 10).toUTF8());
  ASSERT_EQ("", egg::ovum::String().substring(allocator, 10, 11).toUTF8());
  ASSERT_EQ("", egg::ovum::String().substring(allocator, 11, 10).toUTF8());

  ASSERT_EQ("e", allocator.concat('e').substring(allocator, 0).toUTF8());
  ASSERT_EQ("", allocator.concat('e').substring(allocator, 1).toUTF8());
  ASSERT_EQ("e", allocator.concat('e').substring(allocator, 0, 1).toUTF8());
  ASSERT_EQ("e", allocator.concat('e').substring(allocator, 0, 2).toUTF8());
  ASSERT_EQ("", allocator.concat('e').substring(allocator, 1, 0).toUTF8());
  ASSERT_EQ("", allocator.concat('e').substring(allocator, 10, 10).toUTF8());
  ASSERT_EQ("", allocator.concat('e').substring(allocator, 10, 11).toUTF8());
  ASSERT_EQ("", allocator.concat('e').substring(allocator, 11, 10).toUTF8());

  ASSERT_EQ("egg", allocator.concat("egg").substring(allocator, 0).toUTF8());
  ASSERT_EQ("gg", allocator.concat("egg").substring(allocator, 1).toUTF8());
  ASSERT_EQ("e", allocator.concat("egg").substring(allocator, 0, 1).toUTF8());
  ASSERT_EQ("eg", allocator.concat("egg").substring(allocator, 0, 2).toUTF8());
  ASSERT_EQ("", allocator.concat("egg").substring(allocator, 1, 0).toUTF8());
  ASSERT_EQ("", allocator.concat("egg").substring(allocator, 10, 10).toUTF8());
  ASSERT_EQ("", allocator.concat("egg").substring(allocator, 10, 11).toUTF8());
  ASSERT_EQ("", allocator.concat("egg").substring(allocator, 11, 10).toUTF8());
}

TEST(TestStrings, Slice) {
  static const char expected_egg[9][9][4] = {
    { "", "", "e", "eg", "", "e", "eg", "egg", "egg" },
    { "", "", "e", "eg", "", "e", "eg", "egg", "egg" },
    { "", "", "", "g", "", "", "g", "gg", "gg" },
    { "", "", "", "", "", "", "", "g", "g" },
    { "", "", "e", "eg", "", "e", "eg", "egg", "egg" },
    { "", "", "", "g", "", "", "g", "gg", "gg" },
    { "", "", "", "", "", "", "", "g", "g" },
    { "", "", "", "", "", "", "", "", "" },
    { "", "", "", "", "", "", "", "", "" }
  };
  egg::test::Allocator allocator;
  for (int i = 0; i < 9; ++i) {
    int p = i - 4;
    for (int j = 0; j < 9; ++j) {
      int q = j - 4;
      auto actual = egg::ovum::String().slice(allocator, p, q);
      ASSERT_EQ("", actual.toUTF8());
      actual = allocator.concat('e').slice(allocator, p, q);
      if ((p <= 0) && (q >= 1)) {
        ASSERT_EQ("e", actual.toUTF8());
      } else {
        ASSERT_EQ("", actual.toUTF8());
      }
      actual = allocator.concat("egg").slice(allocator, p, q);
      ASSERT_EQ(expected_egg[i][j], actual.toUTF8());
    }
  }
}

TEST(TestStrings, SplitEmpty) {
  egg::test::Allocator allocator;

  auto banana = allocator.concat("banana");
  auto empty = egg::ovum::String();
  auto split = banana.split(allocator, empty);
  ASSERT_EQ(6u, split.size());
  ASSERT_EQ("b", split[0].toUTF8());
  ASSERT_EQ("a", split[1].toUTF8());
  ASSERT_EQ("n", split[2].toUTF8());
  ASSERT_EQ("a", split[3].toUTF8());
  ASSERT_EQ("n", split[4].toUTF8());
  ASSERT_EQ("a", split[5].toUTF8());

  split = banana.split(allocator, empty, 3);
  ASSERT_EQ(3u, split.size());
  ASSERT_EQ("b", split[0].toUTF8());
  ASSERT_EQ("a", split[1].toUTF8());
  ASSERT_EQ("nana", split[2].toUTF8());

  split = banana.split(allocator, empty, -3);
  ASSERT_EQ(3u, split.size());
  ASSERT_EQ("bana", split[0].toUTF8());
  ASSERT_EQ("n", split[1].toUTF8());
  ASSERT_EQ("a", split[2].toUTF8());

  split = banana.split(allocator, empty, 0);
  ASSERT_EQ(0u, split.size());
}

TEST(TestStrings, SplitSingle) {
  egg::test::Allocator allocator;

  auto banana = allocator.concat("banana");
  auto a = allocator.concat('a');
  auto split = banana.split(allocator, a);
  ASSERT_EQ(4u, split.size());
  ASSERT_EQ("b", split[0].toUTF8());
  ASSERT_EQ("n", split[1].toUTF8());
  ASSERT_EQ("n", split[2].toUTF8());
  ASSERT_EQ("",  split[3].toUTF8());

  split = banana.split(allocator, a, 3);
  ASSERT_EQ(3u, split.size());
  ASSERT_EQ("b", split[0].toUTF8());
  ASSERT_EQ("n", split[1].toUTF8());
  ASSERT_EQ("na", split[2].toUTF8());

  split = banana.split(allocator, a, -3);
  ASSERT_EQ(3u, split.size());
  ASSERT_EQ("ban", split[0].toUTF8());
  ASSERT_EQ("n", split[1].toUTF8());
  ASSERT_EQ("", split[2].toUTF8());

  split = banana.split(allocator, a, 0);
  ASSERT_EQ(0u, split.size());
}

TEST(TestStrings, SplitString) {
  egg::test::Allocator allocator;

  auto banana = allocator.concat("banana");
  auto ana = allocator.concat("ana");
  auto split = banana.split(allocator, ana);
  ASSERT_EQ(2u, split.size());
  ASSERT_EQ("b", split[0].toUTF8());
  ASSERT_EQ("na", split[1].toUTF8());

  split = banana.split(allocator, ana, 3);
  ASSERT_EQ(2u, split.size());
  ASSERT_EQ("b", split[0].toUTF8());
  ASSERT_EQ("na", split[1].toUTF8());

  split = banana.split(allocator, ana, -3);
  ASSERT_EQ(2u, split.size());
  ASSERT_EQ("ban", split[0].toUTF8());
  ASSERT_EQ("", split[1].toUTF8());

  split = banana.split(allocator, ana, 0);
  ASSERT_EQ(0u, split.size());
}

TEST(TestStrings, Repeat) {
  egg::test::Allocator allocator;

  auto s = egg::ovum::String();
  ASSERT_EQ("", s.repeat(allocator, 0).toUTF8());
  ASSERT_EQ("", s.repeat(allocator, 1).toUTF8());
  ASSERT_EQ("", s.repeat(allocator, 2).toUTF8());
  ASSERT_EQ("", s.repeat(allocator, 3).toUTF8());

  s = allocator.concat('e');
  ASSERT_EQ("", s.repeat(allocator, 0).toUTF8());
  ASSERT_EQ("e", s.repeat(allocator, 1).toUTF8());
  ASSERT_EQ("ee", s.repeat(allocator, 2).toUTF8());
  ASSERT_EQ("eee", s.repeat(allocator, 3).toUTF8());

  s = allocator.concat("egg");
  ASSERT_EQ("", s.repeat(allocator, 0).toUTF8());
  ASSERT_EQ("egg", s.repeat(allocator, 1).toUTF8());
  ASSERT_EQ("eggegg", s.repeat(allocator, 2).toUTF8());
  ASSERT_EQ("eggeggegg", s.repeat(allocator, 3).toUTF8());
}

TEST(TestStrings, Replace) {
  egg::test::Allocator allocator;

  auto brackets = allocator.concat("[]");
  auto empty = egg::ovum::String();
  auto a = allocator.concat('a');

  ASSERT_EQ("", empty.replace(allocator, empty, brackets).toUTF8());
  ASSERT_EQ("", empty.replace(allocator, a, brackets).toUTF8());
  ASSERT_EQ("", empty.replace(allocator, allocator.concat("ana"), brackets).toUTF8());
  ASSERT_EQ("", empty.replace(allocator, a, empty).toUTF8());

  ASSERT_EQ("a", a.replace(allocator, empty, brackets).toUTF8());
  ASSERT_EQ("[]", a.replace(allocator, a, brackets).toUTF8());
  ASSERT_EQ("a", a.replace(allocator, allocator.concat("ana"), brackets).toUTF8());
  ASSERT_EQ("", a.replace(allocator, a, empty).toUTF8());

  auto banana = allocator.concat("banana");
  ASSERT_EQ("b[]a[]n[]a[]n[]a", banana.replace(allocator, empty, brackets).toUTF8());
  ASSERT_EQ("b[]n[]n[]", banana.replace(allocator, a, brackets).toUTF8());
  ASSERT_EQ("b[]na", banana.replace(allocator, allocator.concat("ana"), brackets).toUTF8());
  ASSERT_EQ("bnn", banana.replace(allocator, a, empty).toUTF8());

  auto o = allocator.concat('o');
  ASSERT_EQ("banana", banana.replace(allocator, a, o, 0).toUTF8());
  ASSERT_EQ("bonona", banana.replace(allocator, a, o, 2).toUTF8());
  ASSERT_EQ("banono", banana.replace(allocator, a, o, -2).toUTF8());
}

TEST(TestStrings, PadLeft) {
  egg::test::Allocator allocator;

  auto egg = allocator.concat("egg");
  ASSERT_EQ("     egg", egg.padLeft(allocator, 8).toUTF8());
  ASSERT_EQ(" egg", egg.padLeft(allocator, 4).toUTF8());
  ASSERT_EQ("egg", egg.padLeft(allocator, 2).toUTF8());
  ASSERT_EQ("egg", egg.padLeft(allocator, 0).toUTF8());

  auto pad = allocator.concat("123");
  ASSERT_EQ("23123egg", egg.padLeft(allocator, 8, pad).toUTF8());
  ASSERT_EQ("3egg", egg.padLeft(allocator, 4, pad).toUTF8());
  ASSERT_EQ("egg", egg.padLeft(allocator, 2, pad).toUTF8());
  ASSERT_EQ("egg", egg.padLeft(allocator, 0, pad).toUTF8());
}

TEST(TestStrings, PadRight) {
  egg::test::Allocator allocator;

  auto egg = allocator.concat("egg");
  ASSERT_EQ("egg     ", egg.padRight(allocator, 8).toUTF8());
  ASSERT_EQ("egg ", egg.padRight(allocator, 4).toUTF8());
  ASSERT_EQ("egg", egg.padRight(allocator, 2).toUTF8());
  ASSERT_EQ("egg", egg.padRight(allocator, 0).toUTF8());

  auto pad = allocator.concat("123");
  ASSERT_EQ("egg12312", egg.padRight(allocator, 8, pad).toUTF8());
  ASSERT_EQ("egg1", egg.padRight(allocator, 4, pad).toUTF8());
  ASSERT_EQ("egg", egg.padRight(allocator, 2, pad).toUTF8());
  ASSERT_EQ("egg", egg.padRight(allocator, 0, pad).toUTF8());
}
