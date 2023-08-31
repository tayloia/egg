#include "ovum/ovum.h"

namespace {
  using namespace egg::ovum;

  class VMDefault : public HardReferenceCounted<IVM> {
    VMDefault(const VMDefault&) = delete;
    VMDefault& operator=(const VMDefault&) = delete;
  private:
    Basket basket;
  public:
    explicit VMDefault(IAllocator& allocator)
      : HardReferenceCounted(allocator, 0),
        basket(BasketFactory::createBasket(allocator)) {
    }
    IAllocator& getAllocator() const override {
      return this->allocator;
    }
    IBasket& getBasket() const override {
      return *this->basket;
    }
  };
}

egg::ovum::VM egg::ovum::VMFactory::createTest(IAllocator& allocator) {
  return allocator.makeHard<VMDefault>();
}
