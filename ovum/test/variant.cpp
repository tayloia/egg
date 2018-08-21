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
  ASSERT_EQ(Bits::Void, variant.getKind());
}

TEST(TestVariant, Null) {
  egg::ovum::Variant variant{ nullptr };
  ASSERT_EQ(Bits::Null, variant.getKind());
  const char* pnull = nullptr;
  variant = pnull;
  ASSERT_VARIANT(nullptr, variant);
}

TEST(TestVariant, Bool) {
  egg::ovum::Variant variant{ false };
  ASSERT_EQ(Bits::Bool, variant.getKind());
  ASSERT_EQ(false, variant.getBool());
  variant = true;
  ASSERT_EQ(Bits::Bool, variant.getKind());
  ASSERT_EQ(true, variant.getBool());
  ASSERT_VARIANT(true, variant);
}

TEST(TestVariant, Int) {
  egg::ovum::Variant variant{ 0 };
  ASSERT_EQ(Bits::Int, variant.getKind());
  ASSERT_EQ(0, variant.getInt());
  variant = 123456789;
  ASSERT_EQ(Bits::Int, variant.getKind());
  ASSERT_EQ(123456789, variant.getInt());
  variant = -1;
  ASSERT_EQ(Bits::Int, variant.getKind());
  ASSERT_EQ(-1, variant.getInt());
  ASSERT_VARIANT(-1, variant);
}

TEST(TestVariant, Float) {
  egg::ovum::Variant variant{ 0.0 };
  ASSERT_EQ(Bits::Float, variant.getKind());
  ASSERT_EQ(0.0, variant.getFloat());
  variant = 123456789.0;
  ASSERT_EQ(Bits::Float, variant.getKind());
  ASSERT_EQ(123456789.0, variant.getFloat());
  variant = -1.0;
  ASSERT_EQ(Bits::Float, variant.getKind());
  ASSERT_EQ(-1.0, variant.getFloat());
  variant = 0.5;
  ASSERT_EQ(Bits::Float, variant.getKind());
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
  char goodbye[] = "goodbye";
  egg::ovum::IMemory::Tag tag;
  tag.p = goodbye;
  auto memory = egg::ovum::MemoryFactory::createImmutable(allocator, "hello world", 11, tag);
  ASSERT_NE(nullptr, memory);
  ASSERT_EQ(goodbye, memory->tag().p);
  egg::ovum::Variant variant{ *memory };
  ASSERT_VARIANT(egg::ovum::VariantBits::Memory | egg::ovum::VariantBits::Hard, variant);
  ASSERT_EQ(memory.get(), variant.getMemory().get());
}

TEST(TestVariant, ObjectHard) {
  egg::test::Allocator allocator;
  auto object = egg::ovum::ObjectFactory::createVanillaObject(allocator);
  egg::ovum::Variant variant{ object };
  ASSERT_EQ(Bits::Object | Bits::Hard, variant.getKind());
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
  ASSERT_EQ(Bits::Indirect | Bits::Hard, variant.getKind());
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
  ASSERT_EQ(Bits::Indirect | Bits::Hard, variant.getKind());
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
  ASSERT_EQ(Bits::Indirect, variant.getKind());
  ASSERT_EQ(1u, ownedCount(*basket));
  ASSERT_EQ(1u, basket->collect());
  ASSERT_EQ(0u, ownedCount(*basket));
}

TEST(TestVariant, IndirectConvert) {
  egg::test::Allocator allocator;
  auto object = egg::ovum::ObjectFactory::createVanillaObject(allocator);
  ASSERT_NE(nullptr, object);
  egg::ovum::Variant variant(object);
  ASSERT_EQ(Bits::Object | Bits::Hard, variant.getKind());
  auto basket = egg::ovum::BasketFactory::createBasket(allocator);
  ASSERT_EQ(0u, ownedCount(*basket));
  variant.indirect(allocator, *basket);
  ASSERT_EQ(Bits::Indirect, variant.getKind());
  ASSERT_EQ(Bits::Object, variant.getPointee().getKind());
  ASSERT_EQ(1u, ownedCount(*basket));
  variant.indirect(allocator, *basket); // should be idempotent
  ASSERT_EQ(Bits::Indirect, variant.getKind());
  ASSERT_EQ(Bits::Object, variant.getPointee().getKind());
  ASSERT_EQ(1u, ownedCount(*basket));
  variant = nullptr;
  ASSERT_EQ(1u, basket->collect());
  ASSERT_EQ(0u, ownedCount(*basket));
}

TEST(TestVariant, PointerHard1) {
  egg::test::Allocator allocator;
  auto basket = egg::ovum::BasketFactory::createBasket(allocator);
  ASSERT_EQ(0u, ownedCount(*basket));
  egg::ovum::Variant variant(Bits::Pointer | Bits::Hard, *egg::ovum::VariantFactory::createVariantSoft(allocator, *basket, "hello world"));
  ASSERT_EQ(Bits::Pointer | Bits::Hard, variant.getKind());
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
  ASSERT_EQ(Bits::Pointer | Bits::Hard, variant.getKind());
  ASSERT_VARIANT("hello world", variant.getPointee());
  ASSERT_EQ(1u, ownedCount(*basket));
  ASSERT_EQ(0u, basket->collect());
  ASSERT_EQ(1u, ownedCount(*basket));
  variant = egg::ovum::Variant(Bits::Pointer | Bits::Hard, *egg::ovum::VariantFactory::createVariantSoft(allocator, *basket, std::move(variant)));
  ASSERT_EQ(Bits::Pointer | Bits::Hard, variant.getKind());
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
  ASSERT_EQ(Bits::Pointer, variant.getKind());
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
  ASSERT_EQ(Bits::Pointer, variant.getKind());
  ASSERT_VARIANT("hello world", variant.getPointee());
  ASSERT_EQ(1u, ownedCount(*basket));
  variant = egg::ovum::Variant(Bits::Pointer, *egg::ovum::VariantFactory::createVariantSoft(allocator, *basket, std::move(variant)));
  ASSERT_EQ(Bits::Pointer, variant.getKind());
  ASSERT_TRUE(variant.getPointee().is(Bits::Pointer));
  ASSERT_VARIANT("hello world", variant.getPointee().getPointee());
  ASSERT_EQ(2u, ownedCount(*basket));
  ASSERT_EQ(2u, basket->collect());
  ASSERT_EQ(0u, ownedCount(*basket));
}

TEST(TestVariant, PointerConvert) {
  egg::test::Allocator allocator;
  auto object = egg::ovum::ObjectFactory::createVanillaObject(allocator);
  ASSERT_NE(nullptr, object);
  egg::ovum::Variant variant(object);
  ASSERT_EQ(Bits::Object | Bits::Hard, variant.getKind());
  auto basket = egg::ovum::BasketFactory::createBasket(allocator);
  ASSERT_EQ(0u, ownedCount(*basket));
  variant.indirect(allocator, *basket);
  ASSERT_EQ(Bits::Indirect, variant.getKind());
  ASSERT_EQ(Bits::Object, variant.getPointee().getKind());
  ASSERT_EQ(1u, ownedCount(*basket));
  auto pointer = variant.address();
  ASSERT_EQ(Bits::Pointer | Bits::Hard, pointer.getKind());
  ASSERT_EQ(Bits::Object, pointer.getPointee().getKind());
  ASSERT_EQ(Bits::Indirect, variant.getKind());
  ASSERT_EQ(Bits::Object, variant.getPointee().getKind());
  ASSERT_EQ(1u, ownedCount(*basket));
  variant = nullptr;
  ASSERT_EQ(0u, basket->collect()); // pointer maintains the reference
  pointer = nullptr;
  ASSERT_EQ(1u, basket->collect());
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
