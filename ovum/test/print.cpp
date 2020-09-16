#include "ovum/test.h"

namespace {
  template<typename T>
  std::string print(T value) {
    std::stringstream ss;
    egg::ovum::Print::write(ss, value);
    return ss.str();
  }
}

#define CHECK(value, expected) \
  ASSERT_EQ(expected, print(value)); \
  ASSERT_STRING(expected, egg::ovum::Print::toString(value))

TEST(TestPrint, Null) {
  CHECK(nullptr, "null");
}

TEST(TestPrint, Bool) {
  CHECK(false, "false");
  CHECK(true, "true");
}

TEST(TestPrint, Int) {
  CHECK(0, "0");
  CHECK(123, "123");
  CHECK(-123, "-123");
}

TEST(TestPrint, UInt) {
  CHECK(0u, "0");
  CHECK(123u, "123");
}

TEST(TestPrint, Float) {
  CHECK(-123.0f, "-123.0");
  CHECK(0.0f, "0.0");
  CHECK(0.5f, "0.5"); // 0.1 rounds badly!
  CHECK(123.0f, "123.0");
  CHECK(std::numeric_limits<float>::quiet_NaN(), "#NAN");
  CHECK(std::numeric_limits<float>::signaling_NaN(), "#NAN");
  CHECK(std::numeric_limits<float>::infinity(), "#+INF");
  CHECK(-std::numeric_limits<float>::infinity(), "#-INF");
}

TEST(TestPrint, Double) {
  CHECK(-123.0, "-123.0");
  CHECK(0.0, "0.0");
  CHECK(0.1, "0.1");
  CHECK(123.0, "123.0");
  CHECK(std::numeric_limits<double>::quiet_NaN(), "#NAN");
  CHECK(std::numeric_limits<double>::signaling_NaN(), "#NAN");
  CHECK(std::numeric_limits<double>::infinity(), "#+INF");
  CHECK(-std::numeric_limits<double>::infinity(), "#-INF");
}

TEST(TestPrint, String) {
  CHECK("", "");
  CHECK("hello", "hello");
  CHECK(std::string(), "");
  CHECK(std::string("hello"), "hello");
  CHECK(egg::ovum::String(), "");
  CHECK(egg::ovum::String("hello"), "hello");
}

TEST(TestPrint, ValueConstants) {
  CHECK(egg::ovum::Value::Void, "void");
  CHECK(egg::ovum::Value::Null, "null");
  CHECK(egg::ovum::Value::False, "false");
  CHECK(egg::ovum::Value::True, "true");
  CHECK(egg::ovum::Value::Break, "break");
  CHECK(egg::ovum::Value::Continue, "continue");
  CHECK(egg::ovum::Value::Rethrow, "throw");
}
