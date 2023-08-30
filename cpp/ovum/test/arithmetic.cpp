#include "ovum/test.h"

namespace {
  inline constexpr double zero = 0.0;
  inline constexpr double half = 0.5;
  inline constexpr double nine = 9.0;
  inline constexpr double snan = std::numeric_limits<double>::quiet_NaN();
  inline constexpr double qnan = std::numeric_limits<double>::signaling_NaN();
  inline constexpr double pinf = std::numeric_limits<double>::infinity();
  inline constexpr double ninf = -pinf;

  std::string format(double value, size_t sigfigs = egg::ovum::Arithmetic::DEFAULT_SIGFIGS) {
    std::stringstream oss;
    egg::ovum::Arithmetic::print(oss, value, sigfigs);
    return oss.str();
  }
}

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
