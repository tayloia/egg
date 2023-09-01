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
    virtual IAllocator& getAllocator() const override {
      return this->allocator;
    }
    virtual IBasket& getBasket() const override {
      return *this->basket;
    }
    virtual String createUTF8(const std::u8string& utf8, size_t codepoints = SIZE_MAX) override {
      return this->allocator.fromUTF8(utf8, codepoints);
    }
    virtual String createUTF32(const std::u32string& utf32) override {
      return this->allocator.fromUTF32(utf32);
    }
  };
}

egg::ovum::VM egg::ovum::VMFactory::createDefault(IAllocator& allocator) {
  return allocator.makeHard<VMDefault>();
}

egg::ovum::VM egg::ovum::VMFactory::createTest(IAllocator& allocator) {
  return allocator.makeHard<VMDefault>();
}
