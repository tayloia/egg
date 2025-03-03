#include "ovum/test.h"

namespace {
  inline constexpr double zero = 0.0;
  inline constexpr double half = 0.5;
  inline constexpr double nine = 9.0;
  inline constexpr double qnan = std::numeric_limits<double>::quiet_NaN();
  inline constexpr double snan = std::numeric_limits<double>::signaling_NaN();
  inline constexpr double pinf = std::numeric_limits<double>::infinity();
  inline constexpr double ninf = -pinf;

  std::string format(double value, size_t sigfigs = egg::ovum::Arithmetic::DEFAULT_SIGFIGS) {
    std::stringstream oss;
    egg::ovum::Arithmetic::print(oss, value, sigfigs);
    return oss.str();
  }
}

#define ASSERT_COMPARE(op, lhs, rhs, expected_nonieee, expected_ieee) \
  ASSERT_EQ(expected_nonieee, egg::ovum::Arithmetic::compare(egg::ovum::Arithmetic::Compare::op, lhs, rhs, false)); \
  ASSERT_EQ(expected_ieee, egg::ovum::Arithmetic::compare(egg::ovum::Arithmetic::Compare::op, lhs, rhs, true))

TEST(TestArithmetic, Zero) {
  // Compare against integers
  ASSERT_TRUE(egg::ovum::Arithmetic::equal(zero, 0));
  ASSERT_FALSE(egg::ovum::Arithmetic::equal(zero, 9));
  // Compare against IEEE (key)
  ASSERT_TRUE(egg::ovum::Arithmetic::equal(zero, zero, false));
  ASSERT_FALSE(egg::ovum::Arithmetic::equal(zero, half, false));
  ASSERT_FALSE(egg::ovum::Arithmetic::equal(zero, nine, false));
  ASSERT_FALSE(egg::ovum::Arithmetic::equal(zero, snan, false));
  ASSERT_FALSE(egg::ovum::Arithmetic::equal(zero, qnan, false));
  ASSERT_FALSE(egg::ovum::Arithmetic::equal(zero, pinf, false));
  ASSERT_FALSE(egg::ovum::Arithmetic::equal(zero, ninf, false));
  // Compare against IEEE (strict)
  ASSERT_TRUE(egg::ovum::Arithmetic::equal(zero, zero, true));
  ASSERT_FALSE(egg::ovum::Arithmetic::equal(zero, half, true));
  ASSERT_FALSE(egg::ovum::Arithmetic::equal(zero, nine, true));
  ASSERT_FALSE(egg::ovum::Arithmetic::equal(zero, snan, true));
  ASSERT_FALSE(egg::ovum::Arithmetic::equal(zero, qnan, true));
  ASSERT_FALSE(egg::ovum::Arithmetic::equal(zero, pinf, true));
  ASSERT_FALSE(egg::ovum::Arithmetic::equal(zero, ninf, true));
  // IEEE decomposition
  egg::ovum::MantissaExponent me;
  me.fromFloat(zero);
  ASSERT_EQ(0, me.mantissa);
  ASSERT_EQ(0, me.exponent);
  ASSERT_EQ(0.0, me.toFloat());
  // Format
  ASSERT_EQ("0.0", format(zero));
}

TEST(TestArithmetic, Half) {
  // Compare against integers
  ASSERT_FALSE(egg::ovum::Arithmetic::equal(half, 0));
  ASSERT_FALSE(egg::ovum::Arithmetic::equal(half, 9));
  // Compare against IEEE (key)
  ASSERT_FALSE(egg::ovum::Arithmetic::equal(half, zero, false));
  ASSERT_TRUE(egg::ovum::Arithmetic::equal(half, half, false));
  ASSERT_FALSE(egg::ovum::Arithmetic::equal(half, nine, false));
  ASSERT_FALSE(egg::ovum::Arithmetic::equal(half, snan, false));
  ASSERT_FALSE(egg::ovum::Arithmetic::equal(half, qnan, false));
  ASSERT_FALSE(egg::ovum::Arithmetic::equal(half, pinf, false));
  ASSERT_FALSE(egg::ovum::Arithmetic::equal(half, ninf, false));
  // Compare against IEEE (strict)
  ASSERT_FALSE(egg::ovum::Arithmetic::equal(half, zero, true));
  ASSERT_TRUE(egg::ovum::Arithmetic::equal(half, half, true));
  ASSERT_FALSE(egg::ovum::Arithmetic::equal(half, nine, true));
  ASSERT_FALSE(egg::ovum::Arithmetic::equal(half, snan, true));
  ASSERT_FALSE(egg::ovum::Arithmetic::equal(half, qnan, true));
  ASSERT_FALSE(egg::ovum::Arithmetic::equal(half, pinf, true));
  ASSERT_FALSE(egg::ovum::Arithmetic::equal(half, ninf, true));
  // IEEE decomposition
  egg::ovum::MantissaExponent me;
  me.fromFloat(half);
  ASSERT_EQ(1, me.mantissa);
  ASSERT_EQ(-1, me.exponent);
  ASSERT_EQ(0.5, me.toFloat());
  // Format
  ASSERT_EQ("0.5", format(half));
}

TEST(TestArithmetic, Nine) {
  // Compare against integers
  ASSERT_FALSE(egg::ovum::Arithmetic::equal(nine, 0));
  ASSERT_TRUE(egg::ovum::Arithmetic::equal(nine, 9));
  // Compare against IEEE (key)
  ASSERT_FALSE(egg::ovum::Arithmetic::equal(nine, zero, false));
  ASSERT_FALSE(egg::ovum::Arithmetic::equal(nine, half, false));
  ASSERT_TRUE(egg::ovum::Arithmetic::equal(nine, nine, false));
  ASSERT_FALSE(egg::ovum::Arithmetic::equal(nine, snan, false));
  ASSERT_FALSE(egg::ovum::Arithmetic::equal(nine, qnan, false));
  ASSERT_FALSE(egg::ovum::Arithmetic::equal(nine, pinf, false));
  ASSERT_FALSE(egg::ovum::Arithmetic::equal(nine, ninf, false));
  // Compare against IEEE (strict)
  ASSERT_FALSE(egg::ovum::Arithmetic::equal(nine, zero, true));
  ASSERT_FALSE(egg::ovum::Arithmetic::equal(nine, half, true));
  ASSERT_TRUE(egg::ovum::Arithmetic::equal(nine, nine, true));
  ASSERT_FALSE(egg::ovum::Arithmetic::equal(nine, snan, true));
  ASSERT_FALSE(egg::ovum::Arithmetic::equal(nine, qnan, true));
  ASSERT_FALSE(egg::ovum::Arithmetic::equal(nine, pinf, true));
  ASSERT_FALSE(egg::ovum::Arithmetic::equal(nine, ninf, true));
  // IEEE decomposition
  egg::ovum::MantissaExponent me;
  me.fromFloat(nine);
  ASSERT_EQ(9, me.mantissa);
  ASSERT_EQ(0, me.exponent);
  ASSERT_EQ(9.0, me.toFloat());
  // Format
  ASSERT_EQ("9.0", format(nine));
}

TEST(TestArithmetic, SNaN) {
  // Compare against integers
  ASSERT_FALSE(egg::ovum::Arithmetic::equal(snan, 0));
  ASSERT_FALSE(egg::ovum::Arithmetic::equal(snan, 9));
  // Compare against IEEE (key)
  ASSERT_FALSE(egg::ovum::Arithmetic::equal(snan, zero, false));
  ASSERT_FALSE(egg::ovum::Arithmetic::equal(snan, half, false));
  ASSERT_FALSE(egg::ovum::Arithmetic::equal(snan, nine, false));
  ASSERT_TRUE(egg::ovum::Arithmetic::equal(snan, snan, false));
  ASSERT_TRUE(egg::ovum::Arithmetic::equal(snan, qnan, false));
  ASSERT_FALSE(egg::ovum::Arithmetic::equal(snan, pinf, false));
  ASSERT_FALSE(egg::ovum::Arithmetic::equal(snan, ninf, false));
  // Compare against IEEE (strict)
  ASSERT_FALSE(egg::ovum::Arithmetic::equal(snan, zero, true));
  ASSERT_FALSE(egg::ovum::Arithmetic::equal(snan, half, true));
  ASSERT_FALSE(egg::ovum::Arithmetic::equal(snan, nine, true));
  ASSERT_FALSE(egg::ovum::Arithmetic::equal(snan, snan, true));
  ASSERT_FALSE(egg::ovum::Arithmetic::equal(snan, qnan, true));
  ASSERT_FALSE(egg::ovum::Arithmetic::equal(snan, pinf, true));
  ASSERT_FALSE(egg::ovum::Arithmetic::equal(snan, ninf, true));
  // IEEE decomposition
  egg::ovum::MantissaExponent me;
  me.fromFloat(snan);
  ASSERT_EQ(0, me.mantissa);
  ASSERT_EQ(egg::ovum::MantissaExponent::ExponentNaN, me.exponent);
  ASSERT_TRUE(std::isnan(me.toFloat()));
  // Format
  ASSERT_EQ("#NAN", format(snan));
}

TEST(TestArithmetic, QNaN) {
  // Compare against integers
  ASSERT_FALSE(egg::ovum::Arithmetic::equal(qnan, 0));
  ASSERT_FALSE(egg::ovum::Arithmetic::equal(qnan, 9));
  // Compare against IEEE (key)
  ASSERT_FALSE(egg::ovum::Arithmetic::equal(qnan, zero, false));
  ASSERT_FALSE(egg::ovum::Arithmetic::equal(qnan, half, false));
  ASSERT_FALSE(egg::ovum::Arithmetic::equal(qnan, nine, false));
  ASSERT_TRUE(egg::ovum::Arithmetic::equal(qnan, snan, false));
  ASSERT_TRUE(egg::ovum::Arithmetic::equal(qnan, qnan, false));
  ASSERT_FALSE(egg::ovum::Arithmetic::equal(qnan, pinf, false));
  ASSERT_FALSE(egg::ovum::Arithmetic::equal(qnan, ninf, false));
  // Compare against IEEE (strict)
  ASSERT_FALSE(egg::ovum::Arithmetic::equal(qnan, zero, true));
  ASSERT_FALSE(egg::ovum::Arithmetic::equal(qnan, half, true));
  ASSERT_FALSE(egg::ovum::Arithmetic::equal(qnan, nine, true));
  ASSERT_FALSE(egg::ovum::Arithmetic::equal(qnan, snan, true));
  ASSERT_FALSE(egg::ovum::Arithmetic::equal(qnan, qnan, true));
  ASSERT_FALSE(egg::ovum::Arithmetic::equal(qnan, pinf, true));
  ASSERT_FALSE(egg::ovum::Arithmetic::equal(qnan, ninf, true));
  // IEEE decomposition
  egg::ovum::MantissaExponent me;
  me.fromFloat(qnan);
  ASSERT_EQ(0, me.mantissa);
  ASSERT_EQ(egg::ovum::MantissaExponent::ExponentNaN, me.exponent);
  ASSERT_TRUE(std::isnan(me.toFloat()));
  // Format
  ASSERT_EQ("#NAN", format(qnan));
}

TEST(TestArithmetic, PInf) {
  // Compare against integers
  ASSERT_FALSE(egg::ovum::Arithmetic::equal(pinf, 0));
  ASSERT_FALSE(egg::ovum::Arithmetic::equal(pinf, 9));
  // Compare against IEEE (key)
  ASSERT_FALSE(egg::ovum::Arithmetic::equal(pinf, zero, false));
  ASSERT_FALSE(egg::ovum::Arithmetic::equal(pinf, half, false));
  ASSERT_FALSE(egg::ovum::Arithmetic::equal(pinf, nine, false));
  ASSERT_FALSE(egg::ovum::Arithmetic::equal(pinf, snan, false));
  ASSERT_FALSE(egg::ovum::Arithmetic::equal(pinf, qnan, false));
  ASSERT_TRUE(egg::ovum::Arithmetic::equal(pinf, pinf, false));
  ASSERT_FALSE(egg::ovum::Arithmetic::equal(pinf, ninf, false));
  // Compare against IEEE (strict)
  ASSERT_FALSE(egg::ovum::Arithmetic::equal(pinf, zero, true));
  ASSERT_FALSE(egg::ovum::Arithmetic::equal(pinf, half, true));
  ASSERT_FALSE(egg::ovum::Arithmetic::equal(pinf, nine, true));
  ASSERT_FALSE(egg::ovum::Arithmetic::equal(pinf, snan, true));
  ASSERT_FALSE(egg::ovum::Arithmetic::equal(pinf, qnan, true));
  ASSERT_TRUE(egg::ovum::Arithmetic::equal(pinf, pinf, true));
  ASSERT_FALSE(egg::ovum::Arithmetic::equal(pinf, ninf, true));
  // IEEE decomposition
  egg::ovum::MantissaExponent me;
  me.fromFloat(pinf);
  ASSERT_EQ(0, me.mantissa);
  ASSERT_EQ(egg::ovum::MantissaExponent::ExponentPositiveInfinity, me.exponent);
  auto value = me.toFloat();
  ASSERT_TRUE(std::isinf(value));
  ASSERT_TRUE(value > 0);
  // Format
  ASSERT_EQ("#+INF", format(pinf));
}

TEST(TestArithmetic, NInf) {
  // Compare against integers
  ASSERT_FALSE(egg::ovum::Arithmetic::equal(ninf, 0));
  ASSERT_FALSE(egg::ovum::Arithmetic::equal(ninf, 9));
  // Compare against IEEE (key)
  ASSERT_FALSE(egg::ovum::Arithmetic::equal(ninf, zero, false));
  ASSERT_FALSE(egg::ovum::Arithmetic::equal(ninf, half, false));
  ASSERT_FALSE(egg::ovum::Arithmetic::equal(ninf, nine, false));
  ASSERT_FALSE(egg::ovum::Arithmetic::equal(ninf, snan, false));
  ASSERT_FALSE(egg::ovum::Arithmetic::equal(ninf, qnan, false));
  ASSERT_FALSE(egg::ovum::Arithmetic::equal(ninf, pinf, false));
  ASSERT_TRUE(egg::ovum::Arithmetic::equal(ninf, ninf, false));
  // Compare against IEEE (strict)
  ASSERT_FALSE(egg::ovum::Arithmetic::equal(ninf, zero, true));
  ASSERT_FALSE(egg::ovum::Arithmetic::equal(ninf, half, true));
  ASSERT_FALSE(egg::ovum::Arithmetic::equal(ninf, nine, true));
  ASSERT_FALSE(egg::ovum::Arithmetic::equal(ninf, snan, true));
  ASSERT_FALSE(egg::ovum::Arithmetic::equal(ninf, qnan, true));
  ASSERT_FALSE(egg::ovum::Arithmetic::equal(ninf, pinf, true));
  ASSERT_TRUE(egg::ovum::Arithmetic::equal(ninf, ninf, true));
  // IEEE decomposition
  egg::ovum::MantissaExponent me;
  me.fromFloat(ninf);
  ASSERT_EQ(0, me.mantissa);
  ASSERT_EQ(egg::ovum::MantissaExponent::ExponentNegativeInfinity, me.exponent);
  auto value = me.toFloat();
  ASSERT_TRUE(std::isinf(value));
  ASSERT_TRUE(value < 0);
  // Format
  ASSERT_EQ("#-INF", format(ninf));
}

TEST(TestArithmetic, Format) {
  ASSERT_EQ("0.0", format(0.0));
  ASSERT_EQ("0.0", format(-0.0));
  ASSERT_EQ("0.12345", format(0.12345));
  ASSERT_EQ("-123.45", format(-123.45));
  ASSERT_EQ("1.0e+100", format(1e100));
  ASSERT_EQ("1.0e-100", format(1e-100));
  ASSERT_EQ("1.0e+008", format(123456789.123456789, 1));
  ASSERT_EQ("1.2e+008", format(123456789.123456789, 2));
  ASSERT_EQ("1.23e+008", format(123456789.123456789, 3));
  ASSERT_EQ("1.235e+008", format(123456789.123456789, 4));
  ASSERT_EQ("1.2346e+008", format(123456789.123456789, 5));
  ASSERT_EQ("123457000.0", format(123456789.123456789, 6));
  ASSERT_EQ("123456800.0", format(123456789.123456789, 7));
  ASSERT_EQ("123456790.0", format(123456789.123456789, 8));
  ASSERT_EQ("123456789.0", format(123456789.123456789, 9));
  ASSERT_EQ("123456789.1", format(123456789.123456789, 10));
  ASSERT_EQ("123456789.12", format(123456789.123456789, 11));
  ASSERT_EQ("123456789.123", format(123456789.123456789, 12));
  ASSERT_EQ("123456789.1235", format(123456789.123456789, 13));
  ASSERT_EQ("123456789.12346", format(123456789.123456789, 14));
  ASSERT_EQ("123456789.123457", format(123456789.123456789, 15));
  ASSERT_EQ("123456789.1234568", format(123456789.123456789, 16));
  ASSERT_EQ("123456789.12345679", format(123456789.123456789, 17));
  ASSERT_EQ("123456789.123456789", format(123456789.123456789, 18));
}

TEST(TestArithmetic, Denormals) {
  constexpr double denormal = std::numeric_limits<double>::denorm_min();
  ASSERT_NE("0.0", format(denormal));
  ASSERT_NE("0.0", format(-denormal));
  ASSERT_EQ("0.0", format(0.5 * denormal));
  ASSERT_EQ("0.0", format(-0.5 * denormal));
}

TEST(TestArithmetic, OrderInt) {
  ASSERT_EQ(+0, egg::ovum::Arithmetic::order(-123, -123));
  ASSERT_EQ(-1, egg::ovum::Arithmetic::order(-123, 0));
  ASSERT_EQ(-1, egg::ovum::Arithmetic::order(-123, 123));
  ASSERT_EQ(+1, egg::ovum::Arithmetic::order(0, -123));
  ASSERT_EQ(+0, egg::ovum::Arithmetic::order(0, 0));
  ASSERT_EQ(-1, egg::ovum::Arithmetic::order(0, 123));
  ASSERT_EQ(+1, egg::ovum::Arithmetic::order(123, -123));
  ASSERT_EQ(+1, egg::ovum::Arithmetic::order(123, 0));
  ASSERT_EQ(+0, egg::ovum::Arithmetic::order(123, 123));
}

TEST(TestArithmetic, OrderFloat) {
  // Finites
  ASSERT_EQ(+0, egg::ovum::Arithmetic::order(-123.0, -123.0));
  ASSERT_EQ(-1, egg::ovum::Arithmetic::order(-123.0, 0.0));
  ASSERT_EQ(-1, egg::ovum::Arithmetic::order(-123.0, 123.0));
  ASSERT_EQ(+1, egg::ovum::Arithmetic::order(0.0, -123.0));
  ASSERT_EQ(+0, egg::ovum::Arithmetic::order(0.0, 0.0));
  ASSERT_EQ(-1, egg::ovum::Arithmetic::order(0.0, 123.0));
  ASSERT_EQ(+1, egg::ovum::Arithmetic::order(123.0, -123.0));
  ASSERT_EQ(+1, egg::ovum::Arithmetic::order(123.0, 0.0));
  ASSERT_EQ(+0, egg::ovum::Arithmetic::order(123.0, 123.0));
  // Infinites
  ASSERT_EQ(+0, egg::ovum::Arithmetic::order(ninf, ninf));
  ASSERT_EQ(-1, egg::ovum::Arithmetic::order(ninf, zero));
  ASSERT_EQ(-1, egg::ovum::Arithmetic::order(ninf, pinf));
  ASSERT_EQ(+1, egg::ovum::Arithmetic::order(zero, ninf));
  ASSERT_EQ(-1, egg::ovum::Arithmetic::order(zero, pinf));
  ASSERT_EQ(+1, egg::ovum::Arithmetic::order(pinf, ninf));
  ASSERT_EQ(+1, egg::ovum::Arithmetic::order(pinf, zero));
  ASSERT_EQ(+0, egg::ovum::Arithmetic::order(pinf, pinf));
  // NaNs
  ASSERT_EQ(+0, egg::ovum::Arithmetic::order(qnan, qnan));
  ASSERT_EQ(-1, egg::ovum::Arithmetic::order(qnan, ninf));
  ASSERT_EQ(-1, egg::ovum::Arithmetic::order(qnan, zero));
  ASSERT_EQ(-1, egg::ovum::Arithmetic::order(qnan, pinf));
  ASSERT_EQ(+1, egg::ovum::Arithmetic::order(ninf, qnan));
  ASSERT_EQ(+1, egg::ovum::Arithmetic::order(zero, qnan));
  ASSERT_EQ(+1, egg::ovum::Arithmetic::order(pinf, qnan));
}

TEST(TestArithmetic, CompareInt) {
  // Less than
  ASSERT_FALSE(egg::ovum::Arithmetic::compare(egg::ovum::Arithmetic::Compare::LessThan, -123, -123));
  ASSERT_TRUE(egg::ovum::Arithmetic::compare(egg::ovum::Arithmetic::Compare::LessThan, -123, 0));
  ASSERT_TRUE(egg::ovum::Arithmetic::compare(egg::ovum::Arithmetic::Compare::LessThan, -123, 123));
  ASSERT_FALSE(egg::ovum::Arithmetic::compare(egg::ovum::Arithmetic::Compare::LessThan, 0, -123));
  ASSERT_FALSE(egg::ovum::Arithmetic::compare(egg::ovum::Arithmetic::Compare::LessThan, 0, 0));
  ASSERT_TRUE(egg::ovum::Arithmetic::compare(egg::ovum::Arithmetic::Compare::LessThan, 0, 123));
  ASSERT_FALSE(egg::ovum::Arithmetic::compare(egg::ovum::Arithmetic::Compare::LessThan, 123, -123));
  ASSERT_FALSE(egg::ovum::Arithmetic::compare(egg::ovum::Arithmetic::Compare::LessThan, 123, 0));
  ASSERT_FALSE(egg::ovum::Arithmetic::compare(egg::ovum::Arithmetic::Compare::LessThan, 123, 123));
  // Less than or equal
  ASSERT_TRUE(egg::ovum::Arithmetic::compare(egg::ovum::Arithmetic::Compare::LessThanOrEqual, -123, -123));
  ASSERT_TRUE(egg::ovum::Arithmetic::compare(egg::ovum::Arithmetic::Compare::LessThanOrEqual, -123, 0));
  ASSERT_TRUE(egg::ovum::Arithmetic::compare(egg::ovum::Arithmetic::Compare::LessThanOrEqual, -123, 123));
  ASSERT_FALSE(egg::ovum::Arithmetic::compare(egg::ovum::Arithmetic::Compare::LessThanOrEqual, 0, -123));
  ASSERT_TRUE(egg::ovum::Arithmetic::compare(egg::ovum::Arithmetic::Compare::LessThanOrEqual, 0, 0));
  ASSERT_TRUE(egg::ovum::Arithmetic::compare(egg::ovum::Arithmetic::Compare::LessThanOrEqual, 0, 123));
  ASSERT_FALSE(egg::ovum::Arithmetic::compare(egg::ovum::Arithmetic::Compare::LessThanOrEqual, 123, -123));
  ASSERT_FALSE(egg::ovum::Arithmetic::compare(egg::ovum::Arithmetic::Compare::LessThanOrEqual, 123, 0));
  ASSERT_TRUE(egg::ovum::Arithmetic::compare(egg::ovum::Arithmetic::Compare::LessThanOrEqual, 123, 123));
  // Equality
  ASSERT_TRUE(egg::ovum::Arithmetic::compare(egg::ovum::Arithmetic::Compare::Equal, -123, -123));
  ASSERT_FALSE(egg::ovum::Arithmetic::compare(egg::ovum::Arithmetic::Compare::Equal, -123, 0));
  ASSERT_FALSE(egg::ovum::Arithmetic::compare(egg::ovum::Arithmetic::Compare::Equal, -123, 123));
  ASSERT_FALSE(egg::ovum::Arithmetic::compare(egg::ovum::Arithmetic::Compare::Equal, 0, -123));
  ASSERT_TRUE(egg::ovum::Arithmetic::compare(egg::ovum::Arithmetic::Compare::Equal, 0, 0));
  ASSERT_FALSE(egg::ovum::Arithmetic::compare(egg::ovum::Arithmetic::Compare::Equal, 0, 123));
  ASSERT_FALSE(egg::ovum::Arithmetic::compare(egg::ovum::Arithmetic::Compare::Equal, 123, -123));
  ASSERT_FALSE(egg::ovum::Arithmetic::compare(egg::ovum::Arithmetic::Compare::Equal, 123, 0));
  ASSERT_TRUE(egg::ovum::Arithmetic::compare(egg::ovum::Arithmetic::Compare::Equal, 123, 123));
  // Inequality
  ASSERT_FALSE(egg::ovum::Arithmetic::compare(egg::ovum::Arithmetic::Compare::NotEqual, -123, -123));
  ASSERT_TRUE(egg::ovum::Arithmetic::compare(egg::ovum::Arithmetic::Compare::NotEqual, -123, 0));
  ASSERT_TRUE(egg::ovum::Arithmetic::compare(egg::ovum::Arithmetic::Compare::NotEqual, -123, 123));
  ASSERT_TRUE(egg::ovum::Arithmetic::compare(egg::ovum::Arithmetic::Compare::NotEqual, 0, -123));
  ASSERT_FALSE(egg::ovum::Arithmetic::compare(egg::ovum::Arithmetic::Compare::NotEqual, 0, 0));
  ASSERT_TRUE(egg::ovum::Arithmetic::compare(egg::ovum::Arithmetic::Compare::NotEqual, 0, 123));
  ASSERT_TRUE(egg::ovum::Arithmetic::compare(egg::ovum::Arithmetic::Compare::NotEqual, 123, -123));
  ASSERT_TRUE(egg::ovum::Arithmetic::compare(egg::ovum::Arithmetic::Compare::NotEqual, 123, 0));
  ASSERT_FALSE(egg::ovum::Arithmetic::compare(egg::ovum::Arithmetic::Compare::NotEqual, 123, 123));
  // Greater than or equal
  ASSERT_TRUE(egg::ovum::Arithmetic::compare(egg::ovum::Arithmetic::Compare::GreaterThanOrEqual, -123, -123));
  ASSERT_FALSE(egg::ovum::Arithmetic::compare(egg::ovum::Arithmetic::Compare::GreaterThanOrEqual, -123, 0));
  ASSERT_FALSE(egg::ovum::Arithmetic::compare(egg::ovum::Arithmetic::Compare::GreaterThanOrEqual, -123, 123));
  ASSERT_TRUE(egg::ovum::Arithmetic::compare(egg::ovum::Arithmetic::Compare::GreaterThanOrEqual, 0, -123));
  ASSERT_TRUE(egg::ovum::Arithmetic::compare(egg::ovum::Arithmetic::Compare::GreaterThanOrEqual, 0, 0));
  ASSERT_FALSE(egg::ovum::Arithmetic::compare(egg::ovum::Arithmetic::Compare::GreaterThanOrEqual, 0, 123));
  ASSERT_TRUE(egg::ovum::Arithmetic::compare(egg::ovum::Arithmetic::Compare::GreaterThanOrEqual, 123, -123));
  ASSERT_TRUE(egg::ovum::Arithmetic::compare(egg::ovum::Arithmetic::Compare::GreaterThanOrEqual, 123, 0));
  ASSERT_TRUE(egg::ovum::Arithmetic::compare(egg::ovum::Arithmetic::Compare::GreaterThanOrEqual, 123, 123));
  // Greater than
  ASSERT_FALSE(egg::ovum::Arithmetic::compare(egg::ovum::Arithmetic::Compare::GreaterThan, -123, -123));
  ASSERT_FALSE(egg::ovum::Arithmetic::compare(egg::ovum::Arithmetic::Compare::GreaterThan, -123, 0));
  ASSERT_FALSE(egg::ovum::Arithmetic::compare(egg::ovum::Arithmetic::Compare::GreaterThan, -123, 123));
  ASSERT_TRUE(egg::ovum::Arithmetic::compare(egg::ovum::Arithmetic::Compare::GreaterThan, 0, -123));
  ASSERT_FALSE(egg::ovum::Arithmetic::compare(egg::ovum::Arithmetic::Compare::GreaterThan, 0, 0));
  ASSERT_FALSE(egg::ovum::Arithmetic::compare(egg::ovum::Arithmetic::Compare::GreaterThan, 0, 123));
  ASSERT_TRUE(egg::ovum::Arithmetic::compare(egg::ovum::Arithmetic::Compare::GreaterThan, 123, -123));
  ASSERT_TRUE(egg::ovum::Arithmetic::compare(egg::ovum::Arithmetic::Compare::GreaterThan, 123, 0));
  ASSERT_FALSE(egg::ovum::Arithmetic::compare(egg::ovum::Arithmetic::Compare::GreaterThan, 123, 123));
}

TEST(TestArithmetic, CompareLT) {
  // Finites
  ASSERT_COMPARE(LessThan, -123.0, -123.0, false, false);
  ASSERT_COMPARE(LessThan, -123.0, 0.0, true, true);
  ASSERT_COMPARE(LessThan, -123.0, 123.0, true, true);
  ASSERT_COMPARE(LessThan, 0.0, -123.0, false, false);
  ASSERT_COMPARE(LessThan, 0.0, 0.0, false, false);
  ASSERT_COMPARE(LessThan, 0.0, 123.0, true, true);
  ASSERT_COMPARE(LessThan, 123.0, -123.0, false, false);
  ASSERT_COMPARE(LessThan, 123.0, 0.0, false, false);
  ASSERT_COMPARE(LessThan, 123.0, 123.0, false, false);
  // Infinites
  ASSERT_COMPARE(LessThan, ninf, ninf, false, false);
  ASSERT_COMPARE(LessThan, ninf, zero, true, true);
  ASSERT_COMPARE(LessThan, ninf, pinf, true, true);
  ASSERT_COMPARE(LessThan, zero, ninf, false, false);
  ASSERT_COMPARE(LessThan, zero, pinf, true, true);
  ASSERT_COMPARE(LessThan, pinf, ninf, false, false);
  ASSERT_COMPARE(LessThan, pinf, zero, false, false);
  ASSERT_COMPARE(LessThan, pinf, pinf, false, false);
  // NaNs
  ASSERT_COMPARE(LessThan, qnan, qnan, false, false);
  ASSERT_COMPARE(LessThan, qnan, ninf, true, false);
  ASSERT_COMPARE(LessThan, qnan, zero, true, false);
  ASSERT_COMPARE(LessThan, qnan, pinf, true, false);
  ASSERT_COMPARE(LessThan, ninf, qnan, false, false);
  ASSERT_COMPARE(LessThan, zero, qnan, false, false);
  ASSERT_COMPARE(LessThan, pinf, qnan, false, false);
}

TEST(TestArithmetic, CompareLE) {
  // Finites
  ASSERT_COMPARE(LessThanOrEqual, -123.0, -123.0, true, true);
  ASSERT_COMPARE(LessThanOrEqual, -123.0, 0.0, true, true);
  ASSERT_COMPARE(LessThanOrEqual, -123.0, 123.0, true, true);
  ASSERT_COMPARE(LessThanOrEqual, 0.0, -123.0, false, false);
  ASSERT_COMPARE(LessThanOrEqual, 0.0, 0.0, true, true);
  ASSERT_COMPARE(LessThanOrEqual, 0.0, 123.0, true, true);
  ASSERT_COMPARE(LessThanOrEqual, 123.0, -123.0, false, false);
  ASSERT_COMPARE(LessThanOrEqual, 123.0, 0.0, false, false);
  ASSERT_COMPARE(LessThanOrEqual, 123.0, 123.0, true, true);
  // Infinites
  ASSERT_COMPARE(LessThanOrEqual, ninf, ninf, true, true);
  ASSERT_COMPARE(LessThanOrEqual, ninf, zero, true, true);
  ASSERT_COMPARE(LessThanOrEqual, ninf, pinf, true, true);
  ASSERT_COMPARE(LessThanOrEqual, zero, ninf, false, false);
  ASSERT_COMPARE(LessThanOrEqual, zero, pinf, true, true);
  ASSERT_COMPARE(LessThanOrEqual, pinf, ninf, false, false);
  ASSERT_COMPARE(LessThanOrEqual, pinf, zero, false, false);
  ASSERT_COMPARE(LessThanOrEqual, pinf, pinf, true, true);
  // NaNs
  ASSERT_COMPARE(LessThanOrEqual, qnan, qnan, true, false);
  ASSERT_COMPARE(LessThanOrEqual, qnan, ninf, true, false);
  ASSERT_COMPARE(LessThanOrEqual, qnan, zero, true, false);
  ASSERT_COMPARE(LessThanOrEqual, qnan, pinf, true, false);
  ASSERT_COMPARE(LessThanOrEqual, ninf, qnan, false, false);
  ASSERT_COMPARE(LessThanOrEqual, zero, qnan, false, false);
  ASSERT_COMPARE(LessThanOrEqual, pinf, qnan, false, false);
}

TEST(TestArithmetic, CompareEQ) {
  // Finites
  ASSERT_COMPARE(Equal, -123.0, -123.0, true, true);
  ASSERT_COMPARE(Equal, -123.0, 0.0, false, false);
  ASSERT_COMPARE(Equal, -123.0, 123.0, false, false);
  ASSERT_COMPARE(Equal, 0.0, -123.0, false, false);
  ASSERT_COMPARE(Equal, 0.0, 0.0, true, true);
  ASSERT_COMPARE(Equal, 0.0, 123.0, false, false);
  ASSERT_COMPARE(Equal, 123.0, -123.0, false, false);
  ASSERT_COMPARE(Equal, 123.0, 0.0, false, false);
  ASSERT_COMPARE(Equal, 123.0, 123.0, true, true);
  // Infinites
  ASSERT_COMPARE(Equal, ninf, ninf, true, true);
  ASSERT_COMPARE(Equal, ninf, zero, false, false);
  ASSERT_COMPARE(Equal, ninf, pinf, false, false);
  ASSERT_COMPARE(Equal, zero, ninf, false, false);
  ASSERT_COMPARE(Equal, zero, pinf, false, false);
  ASSERT_COMPARE(Equal, pinf, ninf, false, false);
  ASSERT_COMPARE(Equal, pinf, zero, false, false);
  ASSERT_COMPARE(Equal, pinf, pinf, true, true);
  // NaNs
  ASSERT_COMPARE(Equal, qnan, qnan, true, false);
  ASSERT_COMPARE(Equal, qnan, ninf, false, false);
  ASSERT_COMPARE(Equal, qnan, zero, false, false);
  ASSERT_COMPARE(Equal, qnan, pinf, false, false);
  ASSERT_COMPARE(Equal, ninf, qnan, false, false);
  ASSERT_COMPARE(Equal, zero, qnan, false, false);
  ASSERT_COMPARE(Equal, pinf, qnan, false, false);
}

TEST(TestArithmetic, CompareNE) {
  // Finites
  ASSERT_COMPARE(NotEqual, -123.0, -123.0, false, false);
  ASSERT_COMPARE(NotEqual, -123.0, 0.0, true, true);
  ASSERT_COMPARE(NotEqual, -123.0, 123.0, true, true);
  ASSERT_COMPARE(NotEqual, 0.0, -123.0, true, true);
  ASSERT_COMPARE(NotEqual, 0.0, 0.0, false, false);
  ASSERT_COMPARE(NotEqual, 0.0, 123.0, true, true);
  ASSERT_COMPARE(NotEqual, 123.0, -123.0, true, true);
  ASSERT_COMPARE(NotEqual, 123.0, 0.0, true, true);
  ASSERT_COMPARE(NotEqual, 123.0, 123.0, false, false);
  // Infinites
  ASSERT_COMPARE(NotEqual, ninf, ninf, false, false);
  ASSERT_COMPARE(NotEqual, ninf, zero, true, true);
  ASSERT_COMPARE(NotEqual, ninf, pinf, true, true);
  ASSERT_COMPARE(NotEqual, zero, ninf, true, true);
  ASSERT_COMPARE(NotEqual, zero, pinf, true, true);
  ASSERT_COMPARE(NotEqual, pinf, ninf, true, true);
  ASSERT_COMPARE(NotEqual, pinf, zero, true, true);
  ASSERT_COMPARE(NotEqual, pinf, pinf, false, false);
  // NaNs
  ASSERT_COMPARE(NotEqual, qnan, qnan, false, true);
  ASSERT_COMPARE(NotEqual, qnan, ninf, true, true);
  ASSERT_COMPARE(NotEqual, qnan, zero, true, true);
  ASSERT_COMPARE(NotEqual, qnan, pinf, true, true);
  ASSERT_COMPARE(NotEqual, ninf, qnan, true, true);
  ASSERT_COMPARE(NotEqual, zero, qnan, true, true);
  ASSERT_COMPARE(NotEqual, pinf, qnan, true, true);
}

TEST(TestArithmetic, CompareGE) {
  // Finites
  ASSERT_COMPARE(GreaterThanOrEqual, -123.0, -123.0, true, true);
  ASSERT_COMPARE(GreaterThanOrEqual, -123.0, 0.0, false, false);
  ASSERT_COMPARE(GreaterThanOrEqual, -123.0, 123.0, false, false);
  ASSERT_COMPARE(GreaterThanOrEqual, 0.0, -123.0, true, true);
  ASSERT_COMPARE(GreaterThanOrEqual, 0.0, 0.0, true, true);
  ASSERT_COMPARE(GreaterThanOrEqual, 0.0, 123.0, false, false);
  ASSERT_COMPARE(GreaterThanOrEqual, 123.0, -123.0, true, true);
  ASSERT_COMPARE(GreaterThanOrEqual, 123.0, 0.0, true, true);
  ASSERT_COMPARE(GreaterThanOrEqual, 123.0, 123.0, true, true);
  // Infinites
  ASSERT_COMPARE(GreaterThanOrEqual, ninf, ninf, true, true);
  ASSERT_COMPARE(GreaterThanOrEqual, ninf, zero, false, false);
  ASSERT_COMPARE(GreaterThanOrEqual, ninf, pinf, false, false);
  ASSERT_COMPARE(GreaterThanOrEqual, zero, ninf, true, true);
  ASSERT_COMPARE(GreaterThanOrEqual, zero, pinf, false, false);
  ASSERT_COMPARE(GreaterThanOrEqual, pinf, ninf, true, true);
  ASSERT_COMPARE(GreaterThanOrEqual, pinf, zero, true, true);
  ASSERT_COMPARE(GreaterThanOrEqual, pinf, pinf, true, true);
  // NaNs
  ASSERT_COMPARE(GreaterThanOrEqual, qnan, qnan, true, false);
  ASSERT_COMPARE(GreaterThanOrEqual, qnan, ninf, false, false);
  ASSERT_COMPARE(GreaterThanOrEqual, qnan, zero, false, false);
  ASSERT_COMPARE(GreaterThanOrEqual, qnan, pinf, false, false);
  ASSERT_COMPARE(GreaterThanOrEqual, ninf, qnan, true, false);
  ASSERT_COMPARE(GreaterThanOrEqual, zero, qnan, true, false);
  ASSERT_COMPARE(GreaterThanOrEqual, pinf, qnan, true, false);
}

TEST(TestArithmetic, CompareGT) {
  // Finites
  ASSERT_COMPARE(GreaterThan, -123.0, -123.0, false, false);
  ASSERT_COMPARE(GreaterThan, -123.0, 0.0, false, false);
  ASSERT_COMPARE(GreaterThan, -123.0, 123.0, false, false);
  ASSERT_COMPARE(GreaterThan, 0.0, -123.0, true, true);
  ASSERT_COMPARE(GreaterThan, 0.0, 0.0, false, false);
  ASSERT_COMPARE(GreaterThan, 0.0, 123.0, false, false);
  ASSERT_COMPARE(GreaterThan, 123.0, -123.0, true, true);
  ASSERT_COMPARE(GreaterThan, 123.0, 0.0, true, true);
  ASSERT_COMPARE(GreaterThan, 123.0, 123.0, false, false);
  // Infinites
  ASSERT_COMPARE(GreaterThan, ninf, ninf, false, false);
  ASSERT_COMPARE(GreaterThan, ninf, zero, false, false);
  ASSERT_COMPARE(GreaterThan, ninf, pinf, false, false);
  ASSERT_COMPARE(GreaterThan, zero, ninf, true, true);
  ASSERT_COMPARE(GreaterThan, zero, pinf, false, false);
  ASSERT_COMPARE(GreaterThan, pinf, ninf, true, true);
  ASSERT_COMPARE(GreaterThan, pinf, zero, true, true);
  ASSERT_COMPARE(GreaterThan, pinf, pinf, false, false);
  // NaNs
  ASSERT_COMPARE(GreaterThan, qnan, qnan, false, false);
  ASSERT_COMPARE(GreaterThan, qnan, ninf, false, false);
  ASSERT_COMPARE(GreaterThan, qnan, zero, false, false);
  ASSERT_COMPARE(GreaterThan, qnan, pinf, false, false);
  ASSERT_COMPARE(GreaterThan, ninf, qnan, true, false);
  ASSERT_COMPARE(GreaterThan, zero, qnan, true, false);
  ASSERT_COMPARE(GreaterThan, pinf, qnan, true, false);
}

TEST(TestArithmetic, MinimumInt) {
  ASSERT_EQ(-123, egg::ovum::Arithmetic::minimum(-123, -123));
  ASSERT_EQ(-123, egg::ovum::Arithmetic::minimum(-123, 0));
  ASSERT_EQ(-123, egg::ovum::Arithmetic::minimum(-123, 123));
  ASSERT_EQ(-123, egg::ovum::Arithmetic::minimum(0, -123));
  ASSERT_EQ(0, egg::ovum::Arithmetic::minimum(0, 0));
  ASSERT_EQ(0, egg::ovum::Arithmetic::minimum(0, 123));
  ASSERT_EQ(-123, egg::ovum::Arithmetic::minimum(123, -123));
  ASSERT_EQ(0, egg::ovum::Arithmetic::minimum(123, 0));
  ASSERT_EQ(123, egg::ovum::Arithmetic::minimum(123, 123));
}

TEST(TestArithmetic, MaximumInt) {
  ASSERT_EQ(-123, egg::ovum::Arithmetic::maximum(-123, -123));
  ASSERT_EQ(0, egg::ovum::Arithmetic::maximum(-123, 0));
  ASSERT_EQ(123, egg::ovum::Arithmetic::maximum(-123, 123));
  ASSERT_EQ(0, egg::ovum::Arithmetic::maximum(0, -123));
  ASSERT_EQ(0, egg::ovum::Arithmetic::maximum(0, 0));
  ASSERT_EQ(123, egg::ovum::Arithmetic::maximum(0, 123));
  ASSERT_EQ(123, egg::ovum::Arithmetic::maximum(123, -123));
  ASSERT_EQ(123, egg::ovum::Arithmetic::maximum(123, 0));
  ASSERT_EQ(123, egg::ovum::Arithmetic::maximum(123, 123));
}

TEST(TestArithmetic, MinimumFloat) {
  // Finites
  ASSERT_EQ(-123.0, egg::ovum::Arithmetic::minimum(-123.0, -123.0, false));
  ASSERT_EQ(-123.0, egg::ovum::Arithmetic::minimum(-123.0, 0.0, false));
  ASSERT_EQ(-123.0, egg::ovum::Arithmetic::minimum(-123.0, 123.0, false));
  ASSERT_EQ(-123.0, egg::ovum::Arithmetic::minimum(0.0, -123.0, false));
  ASSERT_EQ(0.0, egg::ovum::Arithmetic::minimum(0.0, 0.0, false));
  ASSERT_EQ(0.0, egg::ovum::Arithmetic::minimum(0.0, 123.0, false));
  ASSERT_EQ(-123.0, egg::ovum::Arithmetic::minimum(123.0, -123.0, false));
  ASSERT_EQ(0.0, egg::ovum::Arithmetic::minimum(123.0, 0.0, false));
  ASSERT_EQ(123.0, egg::ovum::Arithmetic::minimum(123.0, 123.0, false));
  // Infinites
  ASSERT_EQ(ninf, egg::ovum::Arithmetic::minimum(ninf, ninf, false));
  ASSERT_EQ(ninf, egg::ovum::Arithmetic::minimum(ninf, zero, false));
  ASSERT_EQ(ninf, egg::ovum::Arithmetic::minimum(ninf, pinf, false));
  ASSERT_EQ(ninf, egg::ovum::Arithmetic::minimum(zero, ninf, false));
  ASSERT_EQ(zero, egg::ovum::Arithmetic::minimum(zero, pinf, false));
  ASSERT_EQ(ninf, egg::ovum::Arithmetic::minimum(pinf, ninf, false));
  ASSERT_EQ(zero, egg::ovum::Arithmetic::minimum(pinf, zero, false));
  ASSERT_EQ(pinf, egg::ovum::Arithmetic::minimum(pinf, pinf, false));
  // NaNs
  ASSERT_TRUE(std::isnan(egg::ovum::Arithmetic::minimum(qnan, qnan, false)));
  ASSERT_EQ(ninf, egg::ovum::Arithmetic::minimum(qnan, ninf, false));
  ASSERT_EQ(zero, egg::ovum::Arithmetic::minimum(qnan, zero, false));
  ASSERT_EQ(pinf, egg::ovum::Arithmetic::minimum(qnan, pinf, false));
  ASSERT_EQ(ninf, egg::ovum::Arithmetic::minimum(ninf, qnan, false));
  ASSERT_EQ(zero, egg::ovum::Arithmetic::minimum(zero, qnan, false));
  ASSERT_EQ(pinf, egg::ovum::Arithmetic::minimum(pinf, qnan, false));
}

TEST(TestArithmetic, MinimumFloatIEEE) {
  // Finites
  ASSERT_EQ(-123.0, egg::ovum::Arithmetic::minimum(-123.0, -123.0, true));
  ASSERT_EQ(-123.0, egg::ovum::Arithmetic::minimum(-123.0, 0.0, true));
  ASSERT_EQ(-123.0, egg::ovum::Arithmetic::minimum(-123.0, 123.0, true));
  ASSERT_EQ(-123.0, egg::ovum::Arithmetic::minimum(0.0, -123.0, true));
  ASSERT_EQ(0.0, egg::ovum::Arithmetic::minimum(0.0, 0.0, true));
  ASSERT_EQ(0.0, egg::ovum::Arithmetic::minimum(0.0, 123.0, true));
  ASSERT_EQ(-123.0, egg::ovum::Arithmetic::minimum(123.0, -123.0, true));
  ASSERT_EQ(0.0, egg::ovum::Arithmetic::minimum(123.0, 0.0, true));
  ASSERT_EQ(123.0, egg::ovum::Arithmetic::minimum(123.0, 123.0, true));
  // Infinites
  ASSERT_EQ(ninf, egg::ovum::Arithmetic::minimum(ninf, ninf, true));
  ASSERT_EQ(ninf, egg::ovum::Arithmetic::minimum(ninf, zero, true));
  ASSERT_EQ(ninf, egg::ovum::Arithmetic::minimum(ninf, pinf, true));
  ASSERT_EQ(ninf, egg::ovum::Arithmetic::minimum(zero, ninf, true));
  ASSERT_EQ(zero, egg::ovum::Arithmetic::minimum(zero, pinf, true));
  ASSERT_EQ(ninf, egg::ovum::Arithmetic::minimum(pinf, ninf, true));
  ASSERT_EQ(zero, egg::ovum::Arithmetic::minimum(pinf, zero, true));
  ASSERT_EQ(pinf, egg::ovum::Arithmetic::minimum(pinf, pinf, true));
  // NaNs
  ASSERT_TRUE(std::isnan(egg::ovum::Arithmetic::minimum(qnan, qnan, true)));
  ASSERT_TRUE(std::isnan(egg::ovum::Arithmetic::minimum(qnan, ninf, true)));
  ASSERT_TRUE(std::isnan(egg::ovum::Arithmetic::minimum(qnan, zero, true)));
  ASSERT_TRUE(std::isnan(egg::ovum::Arithmetic::minimum(qnan, pinf, true)));
  ASSERT_TRUE(std::isnan(egg::ovum::Arithmetic::minimum(ninf, qnan, true)));
  ASSERT_TRUE(std::isnan(egg::ovum::Arithmetic::minimum(zero, qnan, true)));
  ASSERT_TRUE(std::isnan(egg::ovum::Arithmetic::minimum(pinf, qnan, true)));
}

TEST(TestArithmetic, MaximumFloat) {
  // Finites
  ASSERT_EQ(-123.0, egg::ovum::Arithmetic::maximum(-123.0, -123.0, false));
  ASSERT_EQ(0.0, egg::ovum::Arithmetic::maximum(-123.0, 0.0, false));
  ASSERT_EQ(123.0, egg::ovum::Arithmetic::maximum(-123.0, 123.0, false));
  ASSERT_EQ(0.0, egg::ovum::Arithmetic::maximum(0.0, -123.0, false));
  ASSERT_EQ(0.0, egg::ovum::Arithmetic::maximum(0.0, 0.0, false));
  ASSERT_EQ(123.0, egg::ovum::Arithmetic::maximum(0.0, 123.0, false));
  ASSERT_EQ(123.0, egg::ovum::Arithmetic::maximum(123.0, -123.0, false));
  ASSERT_EQ(123.0, egg::ovum::Arithmetic::maximum(123.0, 0.0, false));
  ASSERT_EQ(123.0, egg::ovum::Arithmetic::maximum(123.0, 123.0, false));
  // Infinites
  ASSERT_EQ(ninf, egg::ovum::Arithmetic::maximum(ninf, ninf, false));
  ASSERT_EQ(zero, egg::ovum::Arithmetic::maximum(ninf, zero, false));
  ASSERT_EQ(pinf, egg::ovum::Arithmetic::maximum(ninf, pinf, false));
  ASSERT_EQ(zero, egg::ovum::Arithmetic::maximum(zero, ninf, false));
  ASSERT_EQ(pinf, egg::ovum::Arithmetic::maximum(zero, pinf, false));
  ASSERT_EQ(pinf, egg::ovum::Arithmetic::maximum(pinf, ninf, false));
  ASSERT_EQ(pinf, egg::ovum::Arithmetic::maximum(pinf, zero, false));
  ASSERT_EQ(pinf, egg::ovum::Arithmetic::maximum(pinf, pinf, false));
  // NaNs
  ASSERT_TRUE(std::isnan(egg::ovum::Arithmetic::maximum(qnan, qnan, false)));
  ASSERT_EQ(ninf, egg::ovum::Arithmetic::maximum(qnan, ninf, false));
  ASSERT_EQ(zero, egg::ovum::Arithmetic::maximum(qnan, zero, false));
  ASSERT_EQ(pinf, egg::ovum::Arithmetic::maximum(qnan, pinf, false));
  ASSERT_EQ(ninf, egg::ovum::Arithmetic::maximum(ninf, qnan, false));
  ASSERT_EQ(zero, egg::ovum::Arithmetic::maximum(zero, qnan, false));
  ASSERT_EQ(pinf, egg::ovum::Arithmetic::maximum(pinf, qnan, false));
}

TEST(TestArithmetic, MaximumFloatIEEE) {
  // Finites
  ASSERT_EQ(-123.0, egg::ovum::Arithmetic::maximum(-123.0, -123.0, true));
  ASSERT_EQ(0.0, egg::ovum::Arithmetic::maximum(-123.0, 0.0, true));
  ASSERT_EQ(123.0, egg::ovum::Arithmetic::maximum(-123.0, 123.0, true));
  ASSERT_EQ(0.0, egg::ovum::Arithmetic::maximum(0.0, -123.0, true));
  ASSERT_EQ(0.0, egg::ovum::Arithmetic::maximum(0.0, 0.0, true));
  ASSERT_EQ(123.0, egg::ovum::Arithmetic::maximum(0.0, 123.0, true));
  ASSERT_EQ(123.0, egg::ovum::Arithmetic::maximum(123.0, -123.0, true));
  ASSERT_EQ(123.0, egg::ovum::Arithmetic::maximum(123.0, 0.0, true));
  ASSERT_EQ(123.0, egg::ovum::Arithmetic::maximum(123.0, 123.0, true));
  // Infinites
  ASSERT_EQ(ninf, egg::ovum::Arithmetic::maximum(ninf, ninf, true));
  ASSERT_EQ(zero, egg::ovum::Arithmetic::maximum(ninf, zero, true));
  ASSERT_EQ(pinf, egg::ovum::Arithmetic::maximum(ninf, pinf, true));
  ASSERT_EQ(zero, egg::ovum::Arithmetic::maximum(zero, ninf, true));
  ASSERT_EQ(pinf, egg::ovum::Arithmetic::maximum(zero, pinf, true));
  ASSERT_EQ(pinf, egg::ovum::Arithmetic::maximum(pinf, ninf, true));
  ASSERT_EQ(pinf, egg::ovum::Arithmetic::maximum(pinf, zero, true));
  ASSERT_EQ(pinf, egg::ovum::Arithmetic::maximum(pinf, pinf, true));
  // NaNs
  ASSERT_TRUE(std::isnan(egg::ovum::Arithmetic::maximum(qnan, qnan, true)));
  ASSERT_TRUE(std::isnan(egg::ovum::Arithmetic::maximum(qnan, ninf, true)));
  ASSERT_TRUE(std::isnan(egg::ovum::Arithmetic::maximum(qnan, zero, true)));
  ASSERT_TRUE(std::isnan(egg::ovum::Arithmetic::maximum(qnan, pinf, true)));
  ASSERT_TRUE(std::isnan(egg::ovum::Arithmetic::maximum(ninf, qnan, true)));
  ASSERT_TRUE(std::isnan(egg::ovum::Arithmetic::maximum(zero, qnan, true)));
  ASSERT_TRUE(std::isnan(egg::ovum::Arithmetic::maximum(pinf, qnan, true)));
}
