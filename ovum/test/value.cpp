#include "ovum/test.h"

using Flags = egg::ovum::ValueFlags;

TEST(TestValue, Uninitialized) {
  egg::ovum::Value value;
  ASSERT_EQ(Flags::Void, value->getFlags());
  ASSERT_TRUE(value->getVoid());
  ASSERT_VALUE(Flags::Void, value);
}

TEST(TestValue, Void) {
  auto value = egg::ovum::Value::Void;
  ASSERT_EQ(Flags::Void, value->getFlags());
  ASSERT_TRUE(value->getVoid());
  ASSERT_VALUE(Flags::Void, value);
}

TEST(TestValue, Null) {
  auto value = egg::ovum::Value::Null;
  ASSERT_EQ(Flags::Null, value->getFlags());
  ASSERT_TRUE(value->getNull());
  ASSERT_VALUE(Flags::Null, value);
}

TEST(TestValue, Bool) {
  auto value = egg::ovum::Value::False;
  ASSERT_EQ(Flags::Bool, value->getFlags());
  bool actual = true;
  ASSERT_TRUE(value->getBool(actual));
  ASSERT_FALSE(actual);
  ASSERT_VALUE(false, value);
  value = egg::ovum::Value::True;
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
  auto value = egg::ovum::ValueFactory::createString(allocator, "hello world");
  ASSERT_EQ(Flags::String, value->getFlags());
  egg::ovum::String actual;
  ASSERT_TRUE(value->getString(actual));
  ASSERT_STRING("hello world", actual);
  ASSERT_VALUE("hello world", value);
  value = egg::ovum::ValueFactory::createString(allocator, "");
  ASSERT_EQ(Flags::String, value->getFlags());
  ASSERT_TRUE(value->getString(actual));
  ASSERT_STRING("", actual);
  ASSERT_VALUE("", value);
  value = egg::ovum::ValueFactory::createString(allocator, "goodbye");
  ASSERT_EQ(Flags::String, value->getFlags());
  ASSERT_TRUE(value->getString(actual));
  ASSERT_STRING("goodbye", actual);
  ASSERT_VALUE("goodbye", value);
}

TEST(TestValue, Object) {
  egg::test::Allocator allocator;
  auto object = egg::ovum::ObjectFactory::createEmpty(allocator);
  auto value = egg::ovum::ValueFactory::createObject(allocator, object);
  ASSERT_EQ(Flags::Object, value->getFlags());
  egg::ovum::Object actual;
  ASSERT_TRUE(value->getObject(actual));
  ASSERT_EQ(object.get(), actual.get());
}

TEST(TestValue, Pointer) {
  egg::test::Allocator allocator;
  auto pointee = egg::ovum::ValueFactory::create(allocator, "hello world");
  ASSERT_VALUE(Flags::String, pointee);
  auto pointer = egg::ovum::ValueFactory::createPointer(allocator, pointee);
  egg::ovum::Value actual;
  ASSERT_TRUE(pointer->getChild(actual));
  ASSERT_VALUE(pointee, actual);
}

TEST(TestValue, Value) {
  egg::test::Allocator allocator;
  auto a = egg::ovum::ValueFactory::createString(allocator, "hello world");
  ASSERT_VALUE("hello world", a);
  auto b = egg::ovum::ValueFactory::createString(allocator, "goodbye");
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
