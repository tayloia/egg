#include "test.h"

namespace {
  struct Instance : public egg::gc::Collectable {
    EGG_NO_COPY(Instance);
    Instance(egg::gc::Basket& basket, const std::string& name) : Collectable(basket), name(name), pointers() {}
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

  class BasketDeleter : public egg::gc::Basket::IVisitor {
  public:
    size_t count = 0;
    virtual void visit(egg::gc::Collectable& collectable) override {
      this->count++;
      delete &collectable;
    }
  };

  typedef void (egg::gc::Basket::*Visitation)(egg::gc::Basket::IVisitor&);

  template<class T = BasketCounter>
  size_t basketCount(egg::gc::Basket& basket, Visitation visitation) {
    T visitor;
    (basket.*visitation)(visitor);
    return visitor.count;
  }
}

TEST(TestGCSoft, BasketEmpty) {
  auto basket = egg::gc::BasketFactory::createBasket();
  ASSERT_EQ(0u, basketCount(*basket, &egg::gc::Basket::visitCollectables));
  ASSERT_EQ(0u, basketCount(*basket, &egg::gc::Basket::visitRoots));
}

TEST(TestGCSoft, BasketAdd) {
  auto basket = egg::gc::BasketFactory::createBasket();
  Instance instance(*basket, "instance");
  basket->add(instance, false);
  ASSERT_EQ(1u, basketCount(*basket, &egg::gc::Basket::visitCollectables));
  ASSERT_EQ(0u, basketCount(*basket, &egg::gc::Basket::visitRoots));
  ASSERT_EQ(1u, basketCount(*basket, &egg::gc::Basket::visitPurge));
}

TEST(TestGCSoft, BasketAddRoot) {
  auto basket = egg::gc::BasketFactory::createBasket();
  Instance instance(*basket, "instance");
  basket->add(instance, true);
  ASSERT_EQ(1u, basketCount(*basket, &egg::gc::Basket::visitCollectables));
  ASSERT_EQ(1u, basketCount(*basket, &egg::gc::Basket::visitRoots));
  ASSERT_EQ(1u, basketCount(*basket, &egg::gc::Basket::visitPurge));
}

TEST(TestGCSoft, BasketPoint) {
  auto basket = egg::gc::BasketFactory::createBasket();
  auto* a = new Instance(*basket, "a");
  basket->add(*a, true);
  auto* b = new Instance(*basket, "b");
  basket->add(*b, false);
  ASSERT_EQ(2u, basketCount(*basket, &egg::gc::Basket::visitCollectables));
  a->pointers.emplace_back(*basket, *a, *b);
  ASSERT_EQ(b, a->pointers[0].get());
  ASSERT_EQ(2u, basketCount<BasketDeleter>(*basket, &egg::gc::Basket::visitPurge));
}

TEST(TestGCSoft, BasketCollect) {
  auto basket = egg::gc::BasketFactory::createBasket();
  auto* a = new Instance(*basket, "a");
  basket->add(*a, true);
  auto* b = new Instance(*basket, "b");
  basket->add(*b, false);
  ASSERT_EQ(2u, basketCount(*basket, &egg::gc::Basket::visitCollectables));
  a->pointers.emplace_back(*basket, *a, *b);
  ASSERT_EQ(0u, basketCount<BasketDeleter>(*basket, &egg::gc::Basket::visitGarbage)); // collects nothing
  a->setCollectableRoot(false);
  ASSERT_EQ(2u, basketCount<BasketDeleter>(*basket, &egg::gc::Basket::visitGarbage)); // collects "a" and "b"
}

TEST(TestGCSoft, BasketCycle1) {
  auto basket = egg::gc::BasketFactory::createBasket();
  auto* a = new Instance(*basket, "a");
  basket->add(*a, true);
  auto* x = new Instance(*basket, "x");
  basket->add(*x, false);
  a->pointers.emplace_back(*basket, *a, *a);
  x->pointers.emplace_back(*basket, *x, *a);
  ASSERT_EQ(1u, basketCount<BasketDeleter>(*basket, &egg::gc::Basket::visitGarbage)); // collects "x"
  a->setCollectableRoot(false);
  ASSERT_EQ(1u, basketCount<BasketDeleter>(*basket, &egg::gc::Basket::visitGarbage)); // collects "a"
}

TEST(TestGCSoft, BasketCycle2) {
  auto basket = egg::gc::BasketFactory::createBasket();
  auto* a = new Instance(*basket, "a");
  basket->add(*a, true);
  auto* b = new Instance(*basket, "b");
  basket->add(*b, false);
  auto* x = new Instance(*basket, "x");
  basket->add(*x, false);
  a->pointers.emplace_back(*basket, *a, *b);
  b->pointers.emplace_back(*basket, *b, *a);
  x->pointers.emplace_back(*basket, *x, *a);
  ASSERT_EQ(1u, basketCount<BasketDeleter>(*basket, &egg::gc::Basket::visitGarbage)); // collects "x"
  a->setCollectableRoot(false);
  ASSERT_EQ(2u, basketCount<BasketDeleter>(*basket, &egg::gc::Basket::visitGarbage)); // collects "a" and "b"
}

TEST(TestGCSoft, BasketCycle3) {
  auto basket = egg::gc::BasketFactory::createBasket();
  auto* a = new Instance(*basket, "a");
  basket->add(*a, true);
  auto* b = new Instance(*basket, "b");
  basket->add(*b, false);
  auto* c = new Instance(*basket, "c");
  basket->add(*c, false);
  auto* x = new Instance(*basket, "x");
  basket->add(*x, false);
  a->pointers.emplace_back(*basket, *a, *b);
  b->pointers.emplace_back(*basket, *b, *c);
  c->pointers.emplace_back(*basket, *c, *a);
  x->pointers.emplace_back(*basket, *x, *a);
  ASSERT_EQ(1u, basketCount<BasketDeleter>(*basket, &egg::gc::Basket::visitGarbage)); // collects "x"
  a->setCollectableRoot(false);
  ASSERT_EQ(3u, basketCount<BasketDeleter>(*basket, &egg::gc::Basket::visitGarbage)); // collects "a", "b" and "c"
}
