#include "ovum/ovum.h"

egg::ovum::String egg::ovum::IType::toString(int priority) const {
  auto pair = this->toStringPrecedence();
  if (pair.second < priority) {
    return StringBuilder::concat("(", pair.first, ")");
  }
  return pair.first;
}

egg::ovum::ITypeRef egg::ovum::IType::pointeeType() const {
  // The default implementation is to return 'Void' indicating that this is NOT dereferencable
  return Type::Void;
}

egg::ovum::ITypeRef egg::ovum::IType::denulledType() const {
  // The default implementation is to return 'Void'
  return Type::Void;
}

egg::ovum::String egg::ovum::IFunctionSignature::toString(Parts parts) const {
  StringBuilder sb;
  this->buildStringDefault(sb, parts);
  return sb.str();
}

bool egg::ovum::IFunctionSignature::validateCall(egg::ovum::IExecution& execution, const IParameters& runtime, egg::lang::ValueLegacy& problem) const {
  // The default implementation just calls validateCallDefault()
  return this->validateCallDefault(execution, runtime, problem);
}

egg::ovum::String egg::ovum::IIndexSignature::toString() const {
  return StringBuilder::concat(this->getResultType()->toString(), "[", this->getIndexType()->toString(), "]");
}

const egg::ovum::IFunctionSignature* egg::ovum::IType::callable() const {
  // The default implementation is to say we don't support calling with '()'
  return nullptr;
}

const egg::ovum::IIndexSignature* egg::ovum::IType::indexable() const {
  // The default implementation is to say we don't support indexing with '[]'
  return nullptr;
}

bool egg::ovum::IType::dotable(const String*, ITypeRef&, String& reason) const {
  // The default implementation is to say we don't support properties with '.'
  reason = StringBuilder::concat("Values of type '", this->toString(), "' do not support the '.' operator for property access");
  return false;
}

bool egg::ovum::IType::iterable(ITypeRef&) const {
  // The default implementation is to say we don't support iteration
  return false;
}

egg::ovum::BasalBits egg::ovum::IType::getBasalTypes() const {
  // The default implementation is to say we are an object
  return BasalBits::Object;
}

egg::ovum::ITypeRef egg::ovum::IType::unionWith(const IType& other) const {
  // The default implementation is to simply make a new union (native and basal types can be more clever)
  return Type::makeUnion(*this, other);
}
