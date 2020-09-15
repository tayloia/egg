#include "ovum/test.h"

using Flags = egg::ovum::ValueFlags;

TEST(TestSlot, Empty) {
  egg::ovum::Slot slot;
  ASSERT_TRUE(slot.empty());
  slot.set(egg::ovum::ValueFactory::createVoid());
  ASSERT_FALSE(slot.empty());
}

TEST(TestSlot, GetSet) {
  egg::ovum::Slot slot{ egg::ovum::ValueFactory::createBool(true) };
  ASSERT_VALUE(true, slot.get());
  slot.set(egg::ovum::ValueFactory::createBool(false));
  ASSERT_VALUE(false, slot.get());
}

/* WIBBLE
TEST(TestSlot, SoftLink) {
  egg::test::Allocator allocator;
  auto hard = egg::ovum::ValueFactory::createObject(allocator, egg::ovum::ObjectFactory::createVanillaObject(allocator));
  ASSERT_TRUE(hard->validate());
  egg::ovum::Slot slot{ hard };
  hard = egg::ovum::ValueFactory::createVoid();
}
*/
