#include "ovum/ovum.h"

namespace {
  using namespace egg::ovum;

  class VariantSoft : public SoftReferenceCounted<IVariantSoft> {
    VariantSoft(const VariantSoft&) = delete;
    VariantSoft& operator=(const VariantSoft&) = delete;
  private:
    Variant variant;
  public:
    VariantSoft(IAllocator& allocator, Variant&& value)
      : SoftReferenceCounted(allocator),
        variant(std::move(value)) {
    }
    virtual Variant& getVariant() {
      return this->variant;
    }
    virtual void softVisitLinks(const Visitor&) const override {
      // WIBBLE
    }
  };
}

egg::ovum::HardPtr<egg::ovum::IVariantSoft> egg::ovum::VariantFactory::createVariantSoft(IAllocator& allocator, IBasket& basket, Variant&& value) {
  assert(!value.hasAny(VariantBits::Indirect));
  auto* soften = value.is(VariantBits::Object | VariantBits::Hard) ? value.u.p : nullptr;
  if (soften != nullptr) {
    // Create a soft version of the object
    value.setKind(VariantBits::Object);
  }
  HardPtr<IVariantSoft> created{ allocator.create<VariantSoft>(0, allocator, std::move(value)) };
  assert(created != nullptr);
  basket.take(*created);
  if (soften != nullptr) {
    // Release the hard reference that we masked off earlier
    assert(value.is(VariantBits::Void));
    soften->hardRelease();
  }
  return created;
}
