#include "ovum/test.h"

using Flags = egg::ovum::ValueFlags;

namespace {
  struct TestForge final {
    egg::test::Allocator allocator;
    egg::ovum::HardPtr<egg::ovum::IBasket> basket;
    egg::ovum::HardPtr<egg::ovum::ITypeForge> forge;
    TestForge() {
      this->basket = egg::ovum::BasketFactory::createBasket(this->allocator);
      this->forge = egg::ovum::TypeForgeFactory::createTypeForge(allocator, *this->basket);
      assert(this->forge != nullptr);
    }
    egg::ovum::ITypeForge* operator->() const {
      return this->forge.get();
    }
  };
}

TEST(TestType, ForgeVoid) {
  TestForge forge;
  auto forged = forge->forgePrimitiveType(Flags::Void);
  ASSERT_EQ(egg::ovum::Type::Void, forged);
  ASSERT_TRUE(forged->isPrimitive());
  ASSERT_EQ(egg::ovum::ValueFlags::Void, forged->getPrimitiveFlags());
}
