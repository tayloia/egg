#include "ovum/test.h"

#include <cmath>

using namespace egg::ovum;

#define TEST_ME(f, m, e) \
  me.fromFloat(f); \
  ASSERT_EQ(m, me.mantissa); \
  ASSERT_EQ(e, me.exponent); \
  ASSERT_EQ(f, me.toFloat())

TEST(TestUtility, MantissaExponent) {
  using Limits = std::numeric_limits<Float>;
  MantissaExponent me;
  // zero = 0 * 2^0
  const auto zero = 0.0;
  TEST_ME(zero, 0, 0);
  TEST_ME(-zero, 0, 0);
  // half = 1 * 2^-1
  const auto half = 0.5;
  TEST_ME(half, 1, -1);
  TEST_ME(-half, -1, -1);
  // one = 1 * 2^0
  const auto one = 1.0;
  TEST_ME(one, 1, 0);
  TEST_ME(-one, -1, 0);
  // ten = 5 * 2^1
  const auto ten = 10.0;
  TEST_ME(ten, 5, 1);
  TEST_ME(-ten, -5, 1);
  // almost one
  auto mantissaBits = Limits::digits;
  auto mantissaMax = Int(1ull << mantissaBits);
  auto almost = Float(mantissaMax - 1) / Float(mantissaMax);
  TEST_ME(almost, mantissaMax - 1, -mantissaBits);
  TEST_ME(-almost, -mantissaMax + 1, -mantissaBits);
  // epsilon
  constexpr auto epsilon = Limits::epsilon();
  TEST_ME(epsilon, 1, 1 - mantissaBits);
  TEST_ME(-epsilon, -1, 1 - mantissaBits);
  // tiny (smallest positive normal)
  constexpr auto exponentMax = Limits::max_exponent;
  constexpr auto tiny = Limits::min();
  TEST_ME(tiny, 1, -exponentMax + 2);
  TEST_ME(-tiny, -1, -exponentMax + 2);
  // lowest (most negative normal)
  constexpr auto lowest = Limits::lowest();
  TEST_ME(lowest, -mantissaMax + 1, exponentMax - mantissaBits);
  TEST_ME(-lowest, +mantissaMax - 1, exponentMax - mantissaBits);
  // highest (most positive normal)
  constexpr auto highest = Limits::max();
  TEST_ME(highest, mantissaMax - 1, exponentMax - mantissaBits);
  TEST_ME(-highest, -mantissaMax + 1, exponentMax - mantissaBits);
  // infinity
  constexpr auto infinity = Limits::infinity();
  TEST_ME(infinity, 0, MantissaExponent::ExponentPositiveInfinity);
  TEST_ME(-infinity, 0, MantissaExponent::ExponentNegativeInfinity);
  // not a number (cannot test equality)
  me.fromFloat(Limits::quiet_NaN());
  ASSERT_EQ(0, me.mantissa);
  ASSERT_EQ(MantissaExponent::ExponentNaN, me.exponent);
  ASSERT_TRUE(std::isnan(me.toFloat()));
}
