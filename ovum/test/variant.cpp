#include "ovum/test.h"

using Bits = egg::ovum::VariantBits;

namespace {
  size_t ownedCount(egg::ovum::IBasket& basket) {
    egg::ovum::IBasket::Statistics statistics;
    if (basket.statistics(statistics)) {
      return statistics.currentBlocksOwned;
    }
    return SIZE_MAX;
  }
}

TEST(TestVariant, Kind) {
  egg::ovum::VariantKind kind{ Bits::Throw | Bits::Object };
  // hasAny
  ASSERT_FALSE(kind.hasAny(Bits::Void));
  ASSERT_TRUE(kind.hasAny(Bits::Throw));
  ASSERT_TRUE(kind.hasAny(Bits::Object));
  ASSERT_TRUE(kind.hasAny(Bits::Throw | Bits::Object));
  ASSERT_TRUE(kind.hasAny(Bits::Void | Bits::Throw | Bits::Object));
  // hasAll
  ASSERT_FALSE(kind.hasAll(Bits::Void));
  ASSERT_TRUE(kind.hasAll(Bits::Throw));
  ASSERT_TRUE(kind.hasAll(Bits::Object));
  ASSERT_TRUE(kind.hasAll(Bits::Throw | Bits::Object));
  ASSERT_FALSE(kind.hasAll(Bits::Void | Bits::Throw | Bits::Object));
  // is
  ASSERT_FALSE(kind.is(Bits::Void));
  ASSERT_FALSE(kind.is(Bits::Throw));
  ASSERT_FALSE(kind.is(Bits::Object));
  ASSERT_TRUE(kind.is(Bits::Throw | Bits::Object));
  ASSERT_FALSE(kind.is(Bits::Void | Bits::Throw | Bits::Object));
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
  ASSERT_TRUE(variant.is(Bits::Memory | Bits::Hard));
  ASSERT_EQ(memory.get(), variant.getMemory().get());
}

TEST(TestVariant, ObjectHard) {
  egg::test::Allocator allocator;
  auto object = egg::ovum::ObjectFactory::createVanillaObject(allocator);
  egg::ovum::Variant variant{ object };
  ASSERT_TRUE(variant.is(Bits::Object | Bits::Hard));
  ASSERT_EQ(object.get(), variant.getObject().get());
}

TEST(TestVariant, ObjectSoft) {
  egg::test::Allocator allocator;
  auto object = egg::ovum::ObjectFactory::createVanillaObject(allocator);
  ASSERT_NE(nullptr, object);
  auto basket = egg::ovum::BasketFactory::createBasket(allocator);
  ASSERT_EQ(0u, ownedCount(*basket));
  auto root = egg::ovum::VariantFactory::createVariantSoft(allocator, *basket, object);
  ASSERT_EQ(1u, ownedCount(*basket));
  egg::ovum::Variant variant(Bits::Indirect | Bits::Hard, *root);
  ASSERT_TRUE(variant.is(Bits::Indirect | Bits::Hard));
  auto& pointee = variant.getPointee();
  ASSERT_TRUE(pointee.is(Bits::Object));
  ASSERT_EQ(object.get(), pointee.getObject().get());
  ASSERT_EQ(1u, ownedCount(*basket));
  ASSERT_EQ(0u, basket->collect());
  root = nullptr;
  ASSERT_EQ(1u, ownedCount(*basket));
  ASSERT_EQ(0u, basket->collect());
  ASSERT_EQ(1u, ownedCount(*basket));
  variant = nullptr;
  ASSERT_EQ(1u, ownedCount(*basket));
  ASSERT_EQ(1u, basket->collect());
  ASSERT_EQ(0u, ownedCount(*basket));
}

TEST(TestVariant, IndirectHard) {
  egg::test::Allocator allocator;
  auto object = egg::ovum::ObjectFactory::createVanillaObject(allocator);
  ASSERT_NE(nullptr, object);
  auto basket = egg::ovum::BasketFactory::createBasket(allocator);
  ASSERT_EQ(0u, ownedCount(*basket));
  egg::ovum::Variant variant(Bits::Indirect | Bits::Hard, *egg::ovum::VariantFactory::createVariantSoft(allocator, *basket, object));
  ASSERT_TRUE(variant.is(Bits::Indirect | Bits::Hard));
  ASSERT_EQ(1u, ownedCount(*basket));
  ASSERT_EQ(0u, basket->collect());
  ASSERT_EQ(1u, ownedCount(*basket));
  variant = nullptr;
  ASSERT_EQ(1u, ownedCount(*basket));
  ASSERT_EQ(1u, basket->collect());
  ASSERT_EQ(0u, ownedCount(*basket));
}

TEST(TestVariant, IndirectSoft) {
  egg::test::Allocator allocator;
  auto object = egg::ovum::ObjectFactory::createVanillaObject(allocator);
  ASSERT_NE(nullptr, object);
  auto basket = egg::ovum::BasketFactory::createBasket(allocator);
  ASSERT_EQ(0u, ownedCount(*basket));
  egg::ovum::Variant variant(Bits::Indirect, *egg::ovum::VariantFactory::createVariantSoft(allocator, *basket, object));
  ASSERT_TRUE(variant.is(Bits::Indirect));
  ASSERT_EQ(1u, ownedCount(*basket));
  ASSERT_EQ(1u, basket->collect());
  ASSERT_EQ(0u, ownedCount(*basket));
}

TEST(TestVariant, PointerHard1) {
  egg::test::Allocator allocator;
  auto basket = egg::ovum::BasketFactory::createBasket(allocator);
  ASSERT_EQ(0u, ownedCount(*basket));
  egg::ovum::Variant variant(Bits::Pointer | Bits::Hard, *egg::ovum::VariantFactory::createVariantSoft(allocator, *basket, "hello world"));
  ASSERT_TRUE(variant.is(Bits::Pointer | Bits::Hard));
  ASSERT_VARIANT("hello world", variant.getPointee());
  ASSERT_EQ(1u, ownedCount(*basket));
  ASSERT_EQ(0u, basket->collect());
  ASSERT_EQ(1u, ownedCount(*basket));
  variant = nullptr;
  ASSERT_EQ(1u, ownedCount(*basket));
  ASSERT_EQ(1u, basket->collect());
  ASSERT_EQ(0u, ownedCount(*basket));
}

TEST(TestVariant, PointerHard2) {
  egg::test::Allocator allocator;
  auto basket = egg::ovum::BasketFactory::createBasket(allocator);
  ASSERT_EQ(0u, ownedCount(*basket));
  egg::ovum::Variant variant(Bits::Pointer | Bits::Hard, *egg::ovum::VariantFactory::createVariantSoft(allocator, *basket, "hello world"));
  ASSERT_TRUE(variant.is(Bits::Pointer | Bits::Hard));
  ASSERT_VARIANT("hello world", variant.getPointee());
  ASSERT_EQ(1u, ownedCount(*basket));
  ASSERT_EQ(0u, basket->collect());
  ASSERT_EQ(1u, ownedCount(*basket));
  variant = egg::ovum::Variant(Bits::Pointer | Bits::Hard, *egg::ovum::VariantFactory::createVariantSoft(allocator, *basket, std::move(variant)));
  ASSERT_TRUE(variant.is(Bits::Pointer | Bits::Hard));
  ASSERT_TRUE(variant.getPointee().is(Bits::Pointer)); // softened
  ASSERT_VARIANT("hello world", variant.getPointee().getPointee());
  ASSERT_EQ(2u, ownedCount(*basket));
  ASSERT_EQ(0u, basket->collect());
  ASSERT_EQ(2u, ownedCount(*basket));
  variant = nullptr;
  ASSERT_EQ(2u, ownedCount(*basket));
  ASSERT_EQ(2u, basket->collect());
  ASSERT_EQ(0u, ownedCount(*basket));
}

TEST(TestVariant, PointerSoft1) {
  egg::test::Allocator allocator;
  auto basket = egg::ovum::BasketFactory::createBasket(allocator);
  ASSERT_EQ(0u, ownedCount(*basket));
  egg::ovum::Variant variant(Bits::Pointer, *egg::ovum::VariantFactory::createVariantSoft(allocator, *basket, "hello world"));
  ASSERT_TRUE(variant.is(Bits::Pointer));
  ASSERT_VARIANT("hello world", variant.getPointee());
  ASSERT_EQ(1u, ownedCount(*basket));
  ASSERT_EQ(1u, basket->collect());
  ASSERT_EQ(0u, ownedCount(*basket));
}

TEST(TestVariant, PointerSoft2) {
  egg::test::Allocator allocator;
  auto basket = egg::ovum::BasketFactory::createBasket(allocator);
  ASSERT_EQ(0u, ownedCount(*basket));
  egg::ovum::Variant variant(Bits::Pointer, *egg::ovum::VariantFactory::createVariantSoft(allocator, *basket, "hello world"));
  ASSERT_TRUE(variant.is(Bits::Pointer));
  ASSERT_VARIANT("hello world", variant.getPointee());
  ASSERT_EQ(1u, ownedCount(*basket));
  variant = egg::ovum::Variant(Bits::Pointer, *egg::ovum::VariantFactory::createVariantSoft(allocator, *basket, std::move(variant)));
  ASSERT_TRUE(variant.is(Bits::Pointer));
  ASSERT_TRUE(variant.getPointee().is(Bits::Pointer));
  ASSERT_VARIANT("hello world", variant.getPointee().getPointee());
  ASSERT_EQ(2u, ownedCount(*basket));
  ASSERT_EQ(2u, basket->collect());
  ASSERT_EQ(0u, ownedCount(*basket));
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
