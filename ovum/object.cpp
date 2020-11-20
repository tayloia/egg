#include "ovum/ovum.h"
#include "ovum/vanilla.h"

egg::ovum::String egg::ovum::IObject::toString() const {
  StringBuilder sb;
  this->toStringBuilder(sb);
  return sb.str();
}

egg::ovum::Object egg::ovum::ObjectFactory::createEmpty(IAllocator& allocator) {
  // TODO optimize
  return VanillaFactory::createObject(allocator);
}
