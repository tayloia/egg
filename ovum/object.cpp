#include "ovum/ovum.h"
#include "ovum/vanilla.h"

egg::ovum::Object egg::ovum::ObjectFactory::createEmpty(IAllocator& allocator) {
  return VanillaFactory::createObject(allocator);
}

egg::ovum::Object egg::ovum::ObjectFactory::createPointer(IAllocator& allocator, ISlot& slot, Modifiability modifiability) {
  return VanillaFactory::createPointer(allocator, slot, modifiability);
}
