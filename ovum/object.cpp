#include "ovum/ovum.h"
#include "ovum/slot.h"
#include "ovum/vanilla.h"

egg::ovum::Object egg::ovum::ObjectFactory::createEmpty(IAllocator& allocator) {
  return VanillaFactory::createEmpty(allocator);
}

egg::ovum::Object egg::ovum::ObjectFactory::createPointer(IAllocator& allocator, ISlot& slot, const Type& pointee, Modifiability modifiability) {
  auto basket = slot.softGetBasket();
  assert(basket != nullptr);
  return VanillaFactory::createPointer(allocator, *basket, slot, pointee, modifiability);
}
