#include "ovum/test.h"

using Flags = egg::ovum::ValueFlags;

TEST(TestValue, Uninitialized) {
  egg::ovum::HardValue value;
  ASSERT_EQ(Flags::Void, value->getFlags());
  ASSERT_TRUE(value->getVoid());
  ASSERT_VALUE(Flags::Void, value);
}

TEST(TestValue, Void) {
  auto value{ egg::ovum::HardValue::Void };
  ASSERT_EQ(Flags::Void, value->getFlags());
  ASSERT_TRUE(value->getVoid());
  ASSERT_VALUE(Flags::Void, value);
}

TEST(TestValue, Null) {
  auto value{ egg::ovum::HardValue::Null };
  ASSERT_EQ(Flags::Null, value->getFlags());
  ASSERT_TRUE(value->getNull());
  ASSERT_VALUE(Flags::Null, value);
}

TEST(TestValue, Bool) {
  auto value{ egg::ovum::HardValue::False };
  ASSERT_EQ(Flags::Bool, value->getFlags());
  bool actual = true;
  ASSERT_TRUE(value->getBool(actual));
  ASSERT_FALSE(actual);
  ASSERT_VALUE(false, value);
  value = egg::ovum::HardValue::True;
  ASSERT_EQ(Flags::Bool, value->getFlags());
  ASSERT_TRUE(value->getBool(actual));
  ASSERT_TRUE(actual);
  ASSERT_VALUE(true, value);
}

TEST(TestValue, Int) {
  egg::test::Allocator allocator;
  auto value = egg::ovum::ValueFactory::createInt(allocator, 0);
  ASSERT_EQ(Flags::Int, value->getFlags());
  egg::ovum::Int actual = -1;
  ASSERT_TRUE(value->getInt(actual));
  ASSERT_EQ(0, actual);
  ASSERT_VALUE(0, value);
  value = egg::ovum::ValueFactory::createInt(allocator, 123456789);
  ASSERT_EQ(Flags::Int, value->getFlags());
  ASSERT_TRUE(value->getInt(actual));
  ASSERT_EQ(123456789, actual);
  ASSERT_VALUE(123456789, value);
  value = egg::ovum::ValueFactory::createInt(allocator, -1);
  ASSERT_EQ(Flags::Int, value->getFlags());
  ASSERT_TRUE(value->getInt(actual));
  ASSERT_EQ(-1, actual);
  ASSERT_VALUE(-1, value);
}

TEST(TestValue, Float) {
  egg::test::Allocator allocator;
  auto value = egg::ovum::ValueFactory::createFloat(allocator, 0.0);
  ASSERT_EQ(Flags::Float, value->getFlags());
  egg::ovum::Float actual = -1.0;
  ASSERT_TRUE(value->getFloat(actual));
  ASSERT_EQ(0.0, actual);
  ASSERT_VALUE(0.0, value);
  value = egg::ovum::ValueFactory::createFloat(allocator, 123456789.0);
  ASSERT_EQ(Flags::Float, value->getFlags());
  ASSERT_TRUE(value->getFloat(actual));
  ASSERT_EQ(123456789.0, actual);
  ASSERT_VALUE(123456789.0, value);
  value = egg::ovum::ValueFactory::createFloat(allocator, -0.5);
  ASSERT_EQ(Flags::Float, value->getFlags());
  ASSERT_TRUE(value->getFloat(actual));
  ASSERT_EQ(-0.5, actual);
  ASSERT_VALUE(-0.5, value);
}

TEST(TestValue, String) {
  egg::test::Allocator allocator;
  auto value = egg::ovum::ValueFactory::createStringLiteral(allocator, "hello world");
  ASSERT_EQ(Flags::String, value->getFlags());
  egg::ovum::String actual;
  ASSERT_TRUE(value->getString(actual));
  ASSERT_STRING("hello world", actual);
  ASSERT_VALUE("hello world", value);
  value = egg::ovum::ValueFactory::createStringLiteral(allocator, "");
  ASSERT_EQ(Flags::String, value->getFlags());
  ASSERT_TRUE(value->getString(actual));
  ASSERT_STRING("", actual);
  ASSERT_VALUE("", value);
  value = egg::ovum::ValueFactory::createStringLiteral(allocator, "goodbye");
  ASSERT_EQ(Flags::String, value->getFlags());
  ASSERT_TRUE(value->getString(actual));
  ASSERT_STRING("goodbye", actual);
  ASSERT_VALUE("goodbye", value);
}

TEST(TestValue, Value) {
  egg::test::Allocator allocator;
  auto a = egg::ovum::ValueFactory::createStringLiteral(allocator, "hello world");
  ASSERT_VALUE("hello world", a);
  auto b = egg::ovum::ValueFactory::createStringLiteral(allocator, "goodbye");
  ASSERT_VALUE("goodbye", b);
  a = b;
  ASSERT_VALUE("goodbye", a);
  ASSERT_VALUE("goodbye", b);
  a = a;
  ASSERT_VALUE("goodbye", a);
  ASSERT_VALUE("goodbye", b);
  a = std::move(b);
  ASSERT_VALUE("goodbye", a);
}

TEST(TestValue, Set) {
  egg::test::Allocator allocator;
  auto a = egg::ovum::ValueFactory::createInt(allocator, 12345);
  auto b = egg::ovum::ValueFactory::createInt(allocator, 54321);
  ASSERT_TRUE(a->set(b.get()));
  ASSERT_VALUE(54321, a);
  ASSERT_FALSE(a->set(egg::ovum::HardValue::True.get()));
  ASSERT_VALUE(54321, a);
}

TEST(TestValue, MutateIntAssign) {
  egg::test::Allocator allocator;
  auto a = egg::ovum::ValueFactory::createInt(allocator, 12345);
  auto b = egg::ovum::ValueFactory::createInt(allocator, 54321);
  ASSERT_VALUE(12345, a->mutate(egg::ovum::Mutation::Assign, b.get()));
  ASSERT_VALUE(54321, a);
  ASSERT_THROWN("TODO: Invalid right-hand value for mutation assignment to integer", a->mutate(egg::ovum::Mutation::Assign, egg::ovum::HardValue::True.get()));
}

TEST(TestValue, MutateIntDecrement) {
  egg::test::Allocator allocator;
  auto a = egg::ovum::ValueFactory::createInt(allocator, 12345);
  ASSERT_VALUE(12345, a->mutate(egg::ovum::Mutation::Decrement, egg::ovum::HardValue::Void.get()));
  ASSERT_VALUE(12344, a);
}

TEST(TestValue, MutateIntIncrement) {
  egg::test::Allocator allocator;
  auto a = egg::ovum::ValueFactory::createInt(allocator, 12345);
  ASSERT_VALUE(12345, a->mutate(egg::ovum::Mutation::Increment, egg::ovum::HardValue::Void.get()));
  ASSERT_VALUE(12346, a);
}

TEST(TestValue, MutateIntAdd) {
  egg::test::Allocator allocator;
  auto a = egg::ovum::ValueFactory::createInt(allocator, 12345);
  auto b = egg::ovum::ValueFactory::createInt(allocator, 10);
  ASSERT_VALUE(12345, a->mutate(egg::ovum::Mutation::Add, b.get()));
  ASSERT_VALUE(12355, a);
}

TEST(TestValue, MutateIntSubtract) {
  egg::test::Allocator allocator;
  auto a = egg::ovum::ValueFactory::createInt(allocator, 12345);
  auto b = egg::ovum::ValueFactory::createInt(allocator, 10);
  ASSERT_VALUE(12345, a->mutate(egg::ovum::Mutation::Subtract, b.get()));
  ASSERT_VALUE(12335, a);
}

TEST(TestValue, MutateIntMultiply) {
  egg::test::Allocator allocator;
  auto a = egg::ovum::ValueFactory::createInt(allocator, 12345);
  auto b = egg::ovum::ValueFactory::createInt(allocator, 10);
  ASSERT_VALUE(12345, a->mutate(egg::ovum::Mutation::Multiply, b.get()));
  ASSERT_VALUE(123450, a);
}

TEST(TestValue, MutateIntDivide) {
  egg::test::Allocator allocator;
  auto a = egg::ovum::ValueFactory::createInt(allocator, 12345);
  auto b = egg::ovum::ValueFactory::createInt(allocator, 10);
  ASSERT_VALUE(12345, a->mutate(egg::ovum::Mutation::Divide, b.get()));
  ASSERT_VALUE(1234, a);
  b = egg::ovum::ValueFactory::createInt(allocator, 0);
  ASSERT_THROWN("TODO: Division by zero in mutation divide", a->mutate(egg::ovum::Mutation::Divide, b.get()));
}

TEST(TestValue, MutateIntRemainder) {
  egg::test::Allocator allocator;
  auto a = egg::ovum::ValueFactory::createInt(allocator, 12345);
  auto b = egg::ovum::ValueFactory::createInt(allocator, 10);
  ASSERT_VALUE(12345, a->mutate(egg::ovum::Mutation::Remainder, b.get()));
  ASSERT_VALUE(5, a);
  b = egg::ovum::ValueFactory::createInt(allocator, 0);
  ASSERT_THROWN("TODO: Division by zero in mutation remainder", a->mutate(egg::ovum::Mutation::Remainder, b.get()));
}

TEST(TestValue, MutateIntBitwiseAnd) {
  egg::test::Allocator allocator;
  auto a = egg::ovum::ValueFactory::createInt(allocator, 12345);
  auto b = egg::ovum::ValueFactory::createInt(allocator, 10);
  ASSERT_VALUE(12345, a->mutate(egg::ovum::Mutation::BitwiseAnd, b.get()));
  ASSERT_VALUE(8, a);
}

TEST(TestValue, MutateIntBitwiseOr) {
  egg::test::Allocator allocator;
  auto a = egg::ovum::ValueFactory::createInt(allocator, 12345);
  auto b = egg::ovum::ValueFactory::createInt(allocator, 10);
  ASSERT_VALUE(12345, a->mutate(egg::ovum::Mutation::BitwiseOr, b.get()));
  ASSERT_VALUE(12347, a);
}

TEST(TestValue, MutateIntBitwiseXor) {
  egg::test::Allocator allocator;
  auto a = egg::ovum::ValueFactory::createInt(allocator, 12345);
  auto b = egg::ovum::ValueFactory::createInt(allocator, 10);
  ASSERT_VALUE(12345, a->mutate(egg::ovum::Mutation::BitwiseXor, b.get()));
  ASSERT_VALUE(12339, a);
}

TEST(TestValue, MutateIntShiftLeft) {
  egg::test::Allocator allocator;
  auto a = egg::ovum::ValueFactory::createInt(allocator, 12345);
  auto b = egg::ovum::ValueFactory::createInt(allocator, 10);
  ASSERT_VALUE(12345, a->mutate(egg::ovum::Mutation::ShiftLeft, b.get()));
  ASSERT_VALUE(12641280, a);
}

TEST(TestValue, MutateIntShiftRight) {
  egg::test::Allocator allocator;
  auto a = egg::ovum::ValueFactory::createInt(allocator, 12345);
  auto b = egg::ovum::ValueFactory::createInt(allocator, 10);
  ASSERT_VALUE(12345, a->mutate(egg::ovum::Mutation::ShiftRight, b.get()));
  ASSERT_VALUE(12, a);
}

TEST(TestValue, MutateIntShiftRightUnsigned) {
  egg::test::Allocator allocator;
  auto a = egg::ovum::ValueFactory::createInt(allocator, 12345);
  auto b = egg::ovum::ValueFactory::createInt(allocator, 10);
  ASSERT_VALUE(12345, a->mutate(egg::ovum::Mutation::ShiftRightUnsigned, b.get()));
  ASSERT_VALUE(12, a);
}
