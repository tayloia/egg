#include "yolk.h"

namespace egg::yolk {
  class FunctionSignatureParameter : public egg::lang::IFunctionSignatureParameter {
  private:
    egg::lang::String name; // may be empty
    egg::lang::ITypeRef type;
    size_t position; // may be SIZE_MAX
    Flags flags;
  public:
    FunctionSignatureParameter(const egg::lang::String& name, const egg::lang::ITypeRef& type, size_t position, Flags flags)
      : name(name), type(type), position(position), flags(flags) {
    }
    virtual egg::lang::String getName() const override {
      return this->name;
    }
    virtual const egg::lang::IType& getType() const override {
      return *this->type;
    }
    virtual size_t getPosition() const override {
      return this->position;
    }
    virtual Flags getFlags() const override {
      return this->flags;
    }
  };

  class FunctionSignature : public egg::lang::IFunctionSignature {
    EGG_NO_COPY(FunctionSignature);
  private:
    egg::lang::String name;
    egg::lang::ITypeRef returnType;
    std::vector<FunctionSignatureParameter> parameters;
  public:
    FunctionSignature(const egg::lang::String& name, const egg::lang::ITypeRef& returnType)
      : name(name), returnType(returnType) {
    }
    void addSignatureParameter(const egg::lang::String& parameterName, const egg::lang::ITypeRef& parameterType, size_t position, FunctionSignatureParameter::Flags flags) {
      this->parameters.emplace_back(parameterName, parameterType, position, flags);
    }
    virtual egg::lang::String getFunctionName() const override {
      return this->name;
    }
    virtual const egg::lang::IType& getReturnType() const override {
      return *this->returnType;
    }
    virtual size_t getParameterCount() const override {
      return this->parameters.size();
    }
    virtual const egg::lang::IFunctionSignatureParameter& getParameter(size_t index) const override {
      assert(index < this->parameters.size());
      return this->parameters[index];
    }
  };
}

egg::yolk::FunctionType::FunctionType(const egg::lang::String& name, const egg::lang::ITypeRef& returnType)
  : HardReferenceCounted(0), signature(std::make_unique<FunctionSignature>(name, returnType)) {
}

egg::yolk::FunctionType::~FunctionType() {
  // Must be in this source file due to incomplete types in the header
}

egg::lang::String egg::yolk::FunctionType::toString() const {
  // Do not include names in the signature
  return this->signature->toString();
}

egg::yolk::FunctionType::AssignmentSuccess egg::yolk::FunctionType::canBeAssignedFrom(const IType& rtype) const {
  // We can assign if the signatures are the same or equal
  auto* rsig = rtype.callable();
  if (rsig == nullptr) {
    return AssignmentSuccess::Never;
  }
  auto lsig = this->signature.get();
  if (lsig == rsig) {
    return AssignmentSuccess::Always;
  }
  // TODO fuzzy matching of signatures
  if (lsig->getReturnType().canBeAssignedFrom(rsig->getReturnType()) != AssignmentSuccess::Always) {
    return AssignmentSuccess::Never;
  }
  if (lsig->getParameterCount() != rsig->getParameterCount()) {
    return AssignmentSuccess::Never;
  }
  // WIBBLE
  return AssignmentSuccess::Sometimes; // WIBBLE
}

const egg::lang::IFunctionSignature* egg::yolk::FunctionType::callable() const {
  return this->signature.get();
}

void egg::yolk::FunctionType::addParameter(const egg::lang::String& name, const egg::lang::ITypeRef& type, egg::lang::IFunctionSignatureParameter::Flags flags) {
  this->signature->addSignatureParameter(name, type, this->signature->getParameterCount(), flags);
}
