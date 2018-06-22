#include "test.h"

namespace {
  class Instance : public egg::gc::Collectable {
    EGG_NO_COPY(Instance);
  public:
    explicit Instance() : Collectable() {}
  };

  class BasketVisitor : public egg::gc::Basket::IVisitor {
  public:
    size_t count = 0;
    virtual void visit(egg::gc::Collectable&) override {
      this->count++;
    }
  };

  size_t getBasketCollectableCount(egg::gc::Basket& basket) {
    BasketVisitor visitor;
    basket.visitCollectables(visitor);
    return visitor.count;
  }
}

TEST(TestGCSoft, BasketEmpty) {
  auto basket = egg::gc::BasketFactory::createBasket();
  ASSERT_EQ(0u, getBasketCollectableCount(*basket));
}

TEST(TestGCSoft, BasketAdd) {
  auto basket = egg::gc::BasketFactory::createBasket();
  Instance instance;
  ASSERT_EQ(nullptr, instance.getCollectableBasket());
  basket->add(instance, false);
  ASSERT_EQ(1u, getBasketCollectableCount(*basket));
  ASSERT_EQ(&*basket, instance.getCollectableBasket());
}

TEST(TestGCSoft, BasketAddRoot) {
  auto basket = egg::gc::BasketFactory::createBasket();
  Instance instance;
  ASSERT_EQ(nullptr, instance.getCollectableBasket());
  basket->add(instance, true);
  ASSERT_EQ(1u, getBasketCollectableCount(*basket));
  ASSERT_EQ(&*basket, instance.getCollectableBasket());
}

TEST(TestGCSoft, BasketRemove) {
  auto basket = egg::gc::BasketFactory::createBasket();
  Instance instance;
  ASSERT_EQ(nullptr, instance.getCollectableBasket());
  basket->add(instance, false);
  ASSERT_EQ(1u, getBasketCollectableCount(*basket));
  ASSERT_EQ(&*basket, instance.getCollectableBasket());
  basket->remove(instance);
  ASSERT_EQ(0u, getBasketCollectableCount(*basket));
  ASSERT_EQ(nullptr, instance.getCollectableBasket());
}

TEST(TestGCSoft, BasketRemoveRoot) {
  auto basket = egg::gc::BasketFactory::createBasket();
  Instance instance;
  ASSERT_EQ(nullptr, instance.getCollectableBasket());
  basket->add(instance, true);
  ASSERT_EQ(1u, getBasketCollectableCount(*basket));
  ASSERT_EQ(&*basket, instance.getCollectableBasket());
  basket->remove(instance);
  ASSERT_EQ(0u, getBasketCollectableCount(*basket));
  ASSERT_EQ(nullptr, instance.getCollectableBasket());
}
