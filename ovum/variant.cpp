#include "ovum/ovum.h"

class egg::ovum::VariantSoft : public SoftReferenceCounted<IVariantSoft> { // WIBBLE
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
  virtual void softVisitLinks(const Visitor& visitor) const override {
    if (this->variant.is(VariantBits::Object)) {
      // Soft reference to an object
      assert(this->variant.u.o != nullptr);
      visitor(*this->variant.u.o);
    } else if (this->variant.hasAny(VariantBits::Pointer | VariantBits::Indirect)) {
      // Soft pointer or indirect
      assert(this->variant.u.p != nullptr);
      visitor(*this->variant.u.p);
    }
  }
};

egg::ovum::HardPtr<egg::ovum::IVariantSoft> egg::ovum::VariantFactory::createVariantSoft(IAllocator& allocator, IBasket& basket, Variant&& value) {
  assert(!value.hasAny(VariantBits::Indirect));
  IVariantSoft* soften = nullptr;
  if (value.is(VariantBits::Object | VariantBits::Hard)) {
    // We need to soften objects
    soften = value.u.p;
    value.setKind(VariantBits::Object);
  } else if (value.is(VariantBits::Pointer | VariantBits::Hard)) {
    // We need to soften pointers
    soften = value.u.p;
    value.setKind(VariantBits::Pointer);
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
