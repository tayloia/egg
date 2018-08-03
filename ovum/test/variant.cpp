#include "ovum/test.h"

using Bits = egg::ovum::VariantBits;

TEST(TestVariant, Kind) {
  egg::ovum::VariantKind kind{ Bits::Exception | Bits::Object };
  // hasAny
  ASSERT_FALSE(kind.hasAny(Bits::Void));
  ASSERT_TRUE(kind.hasAny(Bits::Exception));
  ASSERT_TRUE(kind.hasAny(Bits::Object));
  ASSERT_TRUE(kind.hasAny(Bits::Exception | Bits::Object));
  ASSERT_TRUE(kind.hasAny(Bits::Void | Bits::Exception | Bits::Object));
  // hasAll
  ASSERT_FALSE(kind.hasAll(Bits::Void));
  ASSERT_TRUE(kind.hasAll(Bits::Exception));
  ASSERT_TRUE(kind.hasAll(Bits::Object));
  ASSERT_TRUE(kind.hasAll(Bits::Exception | Bits::Object));
  ASSERT_FALSE(kind.hasAll(Bits::Void | Bits::Exception | Bits::Object));
  // is
  ASSERT_FALSE(kind.is(Bits::Void));
  ASSERT_FALSE(kind.is(Bits::Exception));
  ASSERT_FALSE(kind.is(Bits::Object));
  ASSERT_TRUE(kind.is(Bits::Exception | Bits::Object));
  ASSERT_FALSE(kind.is(Bits::Void | Bits::Exception | Bits::Object));
}

TEST(TestVariant, Void) {
  egg::ovum::Variant variant;
  ASSERT_TRUE(variant.is(Bits::Void));
}

TEST(TestVariant, Null) {
  egg::ovum::Variant variant{ nullptr };
  ASSERT_TRUE(variant.is(Bits::Null));
  const char* pnull = nullptr;
  variant = pnull;
  ASSERT_VARIANT(nullptr, variant);
}

TEST(TestVariant, Bool) {
  egg::ovum::Variant variant{ false };
  ASSERT_TRUE(variant.is(Bits::Bool));
  ASSERT_EQ(false, variant.getBool());
  variant = true;
  ASSERT_TRUE(variant.is(Bits::Bool));
  ASSERT_EQ(true, variant.getBool());
  ASSERT_VARIANT(true, variant);
}

TEST(TestVariant, Int) {
  egg::ovum::Variant variant{ 0 };
  ASSERT_TRUE(variant.is(Bits::Int));
  ASSERT_EQ(0, variant.getInt());
  variant = 123456789;
  ASSERT_TRUE(variant.is(Bits::Int));
  ASSERT_EQ(123456789, variant.getInt());
  variant = -1;
  ASSERT_TRUE(variant.is(Bits::Int));
  ASSERT_EQ(-1, variant.getInt());
  ASSERT_VARIANT(-1, variant);
}

TEST(TestVariant, Float) {
  egg::ovum::Variant variant{ 0.0 };
  ASSERT_TRUE(variant.is(Bits::Float));
  ASSERT_EQ(0.0, variant.getFloat());
  variant = 123456789.0;
  ASSERT_TRUE(variant.is(Bits::Float));
  ASSERT_EQ(123456789.0, variant.getFloat());
  variant = -1.0;
  ASSERT_TRUE(variant.is(Bits::Float));
  ASSERT_EQ(-1.0, variant.getFloat());
  variant = 0.5;
  ASSERT_TRUE(variant.is(Bits::Float));
  ASSERT_EQ(0.5, variant.getFloat());
  ASSERT_VARIANT(0.5, variant);
}

TEST(TestVariant, String) {
  egg::ovum::Variant variant{ "hello world" };
  ASSERT_VARIANT("hello world", variant);
  ASSERT_STREQ("hello world", variant.getString().toUTF8().c_str());
  std::string goodbye{ "goodbye" };
  variant = goodbye;
  ASSERT_VARIANT("goodbye", variant);
  variant = egg::ovum::String();
  ASSERT_VARIANT("", variant);
  egg::ovum::String fallback{ "fallback" };
  variant = fallback;
  ASSERT_VARIANT("fallback", variant);
}

TEST(TestVariant, Memory) {
  egg::test::Allocator allocator;
  egg::ovum::IMemory::Tag tag;
  tag.p = &allocator;
  auto memory = egg::ovum::MemoryFactory::createImmutable(allocator, "hello world", 11, tag);
  egg::ovum::Variant variant{ memory };
  ASSERT_TRUE(variant.is(Bits::Memory));
  ASSERT_EQ(memory.get(), variant.getMemory().get());
}

TEST(TestVariant, Object) {
  egg::test::Allocator allocator;
  auto object = egg::ovum::ObjectFactory::createVanillaObject(allocator);
  egg::ovum::Variant variant{ object };
  ASSERT_TRUE(variant.is(Bits::Object));
  ASSERT_EQ(object.get(), variant.getObject().get());
}

TEST(TestVariant, Variant) {
  egg::ovum::Variant a{ "hello world" };
  ASSERT_VARIANT("hello world", a);
  egg::ovum::Variant b{ "goodbye" };
  ASSERT_VARIANT("goodbye", b);
  a = b;
  ASSERT_VARIANT("goodbye", a);
  ASSERT_VARIANT("goodbye", b);
  a = a;
  ASSERT_VARIANT("goodbye", a);
  ASSERT_VARIANT("goodbye", b);
  a = std::move(b);
  ASSERT_VARIANT("goodbye", a);
  ASSERT_TRUE(b.is(Bits::Void));
  a = std::move(a);
  ASSERT_VARIANT("goodbye", a);
  ASSERT_TRUE(b.is(Bits::Void));
}
