#include "yolk/test.h"

namespace {
  struct Instance : public egg::gc::Collectable {
    EGG_NO_COPY(Instance);
    Instance(egg::ovum::IAllocator& allocator, const std::string& name)
      : Collectable(allocator), name(name), pointers() {
    }
    std::string name;
    std::vector<egg::gc::SoftRef<Instance>> pointers;
    void addPointer(const egg::gc::HardRef<Instance>& pointer) {
      this->pointers.emplace_back(*this, pointer.get());
    }
  };

  class BasketCounter : public egg::gc::Basket::IVisitor {
  public:
    size_t count = 0;
    virtual void visit(egg::gc::Collectable&) override {
      this->count++;
    }
  };

  typedef void (egg::gc::Basket::*Visitation)(egg::gc::Basket::IVisitor&);

  size_t basketCount(egg::gc::Basket& basket, Visitation visitation) {
    BasketCounter visitor;
    (basket.*visitation)(visitor);
    return visitor.count;
  }
}

TEST(TestGCSoft, BasketEmpty) {
  egg::test::Allocator allocator;
  auto basket = egg::gc::BasketFactory::createBasket(allocator);
  ASSERT_EQ(0u, basketCount(*basket, &egg::gc::Basket::visitCollectables));
  ASSERT_EQ(0u, basketCount(*basket, &egg::gc::Basket::visitRoots));
}

TEST(TestGCSoft, BasketAdd) {
  egg::test::Allocator allocator;
  auto basket = egg::gc::BasketFactory::createBasket(allocator);
  {
    egg::gc::HardRef<Instance> instance{ allocator.make<Instance>("instance") };
    basket->add(*instance);
    ASSERT_EQ(1u, basketCount(*basket, &egg::gc::Basket::visitCollectables));
    ASSERT_EQ(1u, basketCount(*basket, &egg::gc::Basket::visitRoots));
  }
  ASSERT_EQ(0u, basketCount(*basket, &egg::gc::Basket::visitRoots));
  ASSERT_EQ(1u, basketCount(*basket, &egg::gc::Basket::visitPurge));
}

TEST(TestGCSoft, BasketAddRoot) {
  egg::test::Allocator allocator;
  auto basket = egg::gc::BasketFactory::createBasket(allocator);
  auto instance = basket->make<Instance>("instance");
  ASSERT_EQ(1u, basketCount(*basket, &egg::gc::Basket::visitCollectables));
  ASSERT_EQ(1u, basketCount(*basket, &egg::gc::Basket::visitRoots));
  ASSERT_EQ(1u, basketCount(*basket, &egg::gc::Basket::visitPurge));
}

TEST(TestGCSoft, BasketPoint) {
  egg::test::Allocator allocator;
  auto basket = egg::gc::BasketFactory::createBasket(allocator);
  auto a = basket->make<Instance>("a");
  auto b = basket->make<Instance>("b");
  ASSERT_EQ(2u, basketCount(*basket, &egg::gc::Basket::visitCollectables));
  a->pointers.emplace_back(*a, b.get());
  ASSERT_EQ(b.get(), a->pointers[0].get());
  ASSERT_EQ(2u, basketCount(*basket, &egg::gc::Basket::visitPurge));
}

TEST(TestGCSoft, BasketCollect) {
  egg::test::Allocator allocator;
  auto basket = egg::gc::BasketFactory::createBasket(allocator);
  {
    auto a = basket->make<Instance>("a");
    {
      auto b = basket->make<Instance>("b");
      ASSERT_EQ(2u, basketCount(*basket, &egg::gc::Basket::visitCollectables));
      a->addPointer(b);
    }
    ASSERT_EQ(0u, basketCount(*basket, &egg::gc::Basket::visitGarbage)); // evicts nothing
  }
  ASSERT_EQ(2u, basketCount(*basket, &egg::gc::Basket::visitGarbage)); // evicts "a" and "b"
}

TEST(TestGCSoft, BasketCycle1) {
  egg::test::Allocator allocator;
  auto basket = egg::gc::BasketFactory::createBasket(allocator);
  {
    auto a = basket->make<Instance>("a");
    {
      auto x = basket->make<Instance>("x");
      a->addPointer(a);
      x->addPointer(a);
    }
    ASSERT_EQ(1u, basketCount(*basket, &egg::gc::Basket::visitGarbage)); // evicts "x"
  }
  ASSERT_EQ(1u, basketCount(*basket, &egg::gc::Basket::visitGarbage)); // evicts "a"
}

TEST(TestGCSoft, BasketCycle2) {
  egg::test::Allocator allocator;
  auto basket = egg::gc::BasketFactory::createBasket(allocator);
  {
    auto a = basket->make<Instance>("a");
    {
      auto b = basket->make<Instance>("b");
      auto x = basket->make<Instance>("x");
      a->addPointer(b);
      b->addPointer(a);
      x->addPointer(a);
    }
    ASSERT_EQ(1u, basketCount(*basket, &egg::gc::Basket::visitGarbage)); // evicts "x"
  }
  ASSERT_EQ(2u, basketCount(*basket, &egg::gc::Basket::visitGarbage)); // evicts "a" and "b"
}

TEST(TestGCSoft, BasketCycle3) {
  egg::test::Allocator allocator;
  auto basket = egg::gc::BasketFactory::createBasket(allocator);
  {
    auto a = basket->make<Instance>("a");
    {
      auto b = basket->make<Instance>("b");
      auto c = basket->make<Instance>("c");
      auto x = basket->make<Instance>("x");
      a->addPointer(b);
      b->addPointer(c);
      c->addPointer(a);
      x->addPointer(a);
    }
    ASSERT_EQ(1u, basketCount(*basket, &egg::gc::Basket::visitGarbage)); // evicts "x"
  }
  ASSERT_EQ(3u, basketCount(*basket, &egg::gc::Basket::visitGarbage)); // evicts "a", "b" and "c"
}
