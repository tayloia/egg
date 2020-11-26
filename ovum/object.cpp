#include "ovum/ovum.h"
#include "ovum/vanilla.h"

egg::ovum::Object egg::ovum::ObjectFactory::createEmpty(IAllocator& allocator) {
  // TODO optimize
  return VanillaFactory::createObject(allocator);
}
