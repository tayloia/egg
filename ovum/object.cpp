#include "ovum/ovum.h"
#include "ovum/slot.h"
#include "ovum/vanilla.h"

egg::ovum::Object egg::ovum::ObjectFactory::createEmpty(IAllocator& allocator, IBasket& basket) {
  return VanillaFactory::createObject(allocator, basket);
}
