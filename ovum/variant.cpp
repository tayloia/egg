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
  auto* created = allocator.create<VariantSoft>(0, allocator, std::move(value));
  basket.take(*created);
  return HardPtr<IVariantSoft>(created);
}
