#include "test.h"

namespace {
  struct Instance : public egg::gc::Collectable {
    EGG_NO_COPY(Instance);
    explicit Instance(const std::string& name) : Collectable(), name(name), pointers() {}
    std::string name;
    std::vector<egg::gc::SoftRef<Instance>> pointers;
  };

  class BasketCounter : public egg::gc::Basket::IVisitor {
  public:
    size_t count = 0;
    virtual void visit(egg::gc::Collectable&) override {
      this->count++;
    }
  };

  typedef void (egg::gc::Basket::*Visitation)(egg::gc::Basket::IVisitor&);

  size_t basketGetCount(egg::gc::Basket& basket, Visitation visitation) {
    BasketCounter visitor;
    (basket.*visitation)(visitor);
    return visitor.count;
  }

  class BasketDeleter : public egg::gc::Basket::IVisitor {
  public:
    virtual void visit(egg::gc::Collectable& collectable) override {
      delete &collectable;
    }
  };

  void basketDeleteAll(egg::gc::Basket& basket) {
    BasketDeleter visitor;
    basket.visitPurge(visitor);
  }
}

TEST(TestGCSoft, BasketEmpty) {
  auto basket = egg::gc::BasketFactory::createBasket();
  ASSERT_EQ(0u, basketGetCount(*basket, &egg::gc::Basket::visitCollectables));
  ASSERT_EQ(0u, basketGetCount(*basket, &egg::gc::Basket::visitRoots));
}

TEST(TestGCSoft, BasketAdd) {
  auto basket = egg::gc::BasketFactory::createBasket();
  Instance instance("instance");
  ASSERT_EQ(nullptr, instance.getCollectableBasket());
  basket->add(instance, false);
  ASSERT_EQ(1u, basketGetCount(*basket, &egg::gc::Basket::visitCollectables));
  ASSERT_EQ(0u, basketGetCount(*basket, &egg::gc::Basket::visitRoots));
  ASSERT_EQ(&*basket, instance.getCollectableBasket());
  basket->remove(instance);
}

TEST(TestGCSoft, BasketAddRoot) {
  auto basket = egg::gc::BasketFactory::createBasket();
  Instance instance("instance");
  ASSERT_EQ(nullptr, instance.getCollectableBasket());
  basket->add(instance, true);
  ASSERT_EQ(1u, basketGetCount(*basket, &egg::gc::Basket::visitCollectables));
  ASSERT_EQ(1u, basketGetCount(*basket, &egg::gc::Basket::visitRoots));
  ASSERT_EQ(&*basket, instance.getCollectableBasket());
  basket->remove(instance);
}

TEST(TestGCSoft, BasketRemove) {
  auto basket = egg::gc::BasketFactory::createBasket();
  Instance instance("instance");
  ASSERT_EQ(nullptr, instance.getCollectableBasket());
  basket->add(instance, false);
  ASSERT_EQ(1u, basketGetCount(*basket, &egg::gc::Basket::visitCollectables));
  ASSERT_EQ(0u, basketGetCount(*basket, &egg::gc::Basket::visitRoots));
  ASSERT_EQ(&*basket, instance.getCollectableBasket());
  basket->remove(instance);
  ASSERT_EQ(0u, basketGetCount(*basket, &egg::gc::Basket::visitCollectables));
  ASSERT_EQ(0u, basketGetCount(*basket, &egg::gc::Basket::visitRoots));
  ASSERT_EQ(nullptr, instance.getCollectableBasket());
}

TEST(TestGCSoft, BasketRemoveRoot) {
  auto basket = egg::gc::BasketFactory::createBasket();
  Instance instance("instance");
  ASSERT_EQ(nullptr, instance.getCollectableBasket());
  basket->add(instance, true);
  ASSERT_EQ(1u, basketGetCount(*basket, &egg::gc::Basket::visitCollectables));
  ASSERT_EQ(1u, basketGetCount(*basket, &egg::gc::Basket::visitRoots));
  ASSERT_EQ(&*basket, instance.getCollectableBasket());
  basket->remove(instance);
  ASSERT_EQ(0u, basketGetCount(*basket, &egg::gc::Basket::visitCollectables));
  ASSERT_EQ(0u, basketGetCount(*basket, &egg::gc::Basket::visitRoots));
  ASSERT_EQ(nullptr, instance.getCollectableBasket());
}

TEST(TestGCSoft, BasketPoint) {
  auto basket = egg::gc::BasketFactory::createBasket();
  auto* a = new Instance("a");
  basket->add(*a, true);
  auto* b = new Instance("b");
  basket->add(*b, false);
  ASSERT_EQ(2u, basketGetCount(*basket, &egg::gc::Basket::visitCollectables));
  a->pointers.emplace_back(*basket, *a, *b);
  ASSERT_EQ(b, a->pointers[0].get());
  basketDeleteAll(*basket);
}
