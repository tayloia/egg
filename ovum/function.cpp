#include "ovum/ovum.h"
#include "ovum/node.h"
#include "ovum/function.h"

namespace {
  using namespace egg::ovum;

  class ParametersNone : public IParameters {
    ParametersNone(const ParametersNone&) = delete;
    ParametersNone& operator=(const ParametersNone&) = delete;
  public:
    ParametersNone() = default;
    virtual size_t getPositionalCount() const override {
      return 0;
    }
    virtual Variant getPositional(size_t) const override {
      return Variant::Void;
    }
    virtual const LocationSource* getPositionalLocation(size_t) const override {
      return nullptr;
    }
    virtual size_t getNamedCount() const override {
      return 0;
    }
    virtual String getName(size_t) const override {
      return String();
    }
    virtual Variant getNamed(const String&) const override {
      return Variant::Void;
    }
    virtual const LocationSource* getNamedLocation(const String&) const override {
      return nullptr;
    }
  };
  const ParametersNone parametersNone{};

  class GeneratorFunctionType : public FunctionType {
    GeneratorFunctionType(const GeneratorFunctionType&) = delete;
    GeneratorFunctionType& operator=(const GeneratorFunctionType&) = delete;
  private:
    Type rettype;
  public:
    explicit GeneratorFunctionType(IAllocator& allocator, const Type& rettype)
      : FunctionType(allocator, String(), Type::makeUnion(allocator, *rettype, *Type::Void)),
      rettype(rettype) {
      // No name or parameters in the signature
      assert(!rettype->hasBasalType(BasalBits::Void));
    }
    virtual std::pair<std::string, int> toStringPrecedence() const override {
      // Format a string along the lines of '<rettype>...'
      return std::make_pair(this->rettype.toString(0).toUTF8() + "...", 0);
    }
    virtual Type iterable() const {
      // We are indeed iterable
      return this->rettype;
    }
  };
}

egg::ovum::String egg::ovum::Function::signatureToString(const IFunctionSignature& signature, Parts parts) {
  // TODO better formatting of named/variadic etc.
  StringBuilder sb;
  if (Bits::hasAnySet(parts, Parts::ReturnType)) {
    // Use precedence zero to get any necessary parentheses
    sb.add(signature.getReturnType().toString(0));
  }
  if (Bits::hasAnySet(parts, Parts::FunctionName)) {
    auto name = signature.getFunctionName();
    if (!name.empty()) {
      sb.add(' ', name);
    }
  }
  if (Bits::hasAnySet(parts, Parts::ParameterList)) {
    sb.add('(');
    auto n = signature.getParameterCount();
    for (size_t i = 0; i < n; ++i) {
      if (i > 0) {
        sb.add(", ");
      }
      auto& parameter = signature.getParameter(i);
      assert(parameter.getPosition() != SIZE_MAX);
      if (Bits::hasAnySet(parameter.getFlags(), IFunctionSignatureParameter::Flags::Variadic)) {
        sb.add("...");
      } else {
        sb.add(parameter.getType().toString());
        if (Bits::hasAnySet(parts, Parts::ParameterNames)) {
          auto pname = parameter.getName();
          if (!pname.empty()) {
            sb.add(' ', pname);
          }
        }
        if (!Bits::hasAnySet(parameter.getFlags(), IFunctionSignatureParameter::Flags::Required)) {
          sb.add(" = null");
        }
      }
    }
    sb.add(')');
  }
  return sb.str();
}

const egg::ovum::IParameters& egg::ovum::Function::NoParameters = parametersNone;

egg::ovum::FunctionType::FunctionType(IAllocator& allocator, const String& name, const Type& rettype)
  : HardReferenceCounted(allocator, 0),
  signature(name, rettype) {
}

std::pair<std::string, int> egg::ovum::FunctionType::toStringPrecedence() const {
  // Do not include names in the signature
  auto sig = Function::signatureToString(this->signature, Function::Parts::NoNames);
  return std::make_pair(sig.toUTF8(), 0);
}

egg::ovum::Node egg::ovum::FunctionType::compile(IAllocator& memallocator, const NodeLocation& location) const {
  return NodeFactory::createFunctionType(memallocator, location, this->signature);
}

egg::ovum::FunctionType::AssignmentSuccess egg::ovum::FunctionType::canBeAssignedFrom(const IType& rtype) const {
  // We can assign if the signatures are the same or equal
  auto* rsig = rtype.callable();
  if (rsig == nullptr) {
    return FunctionType::AssignmentSuccess::Never;
  }
  auto* lsig = &this->signature;
  if (lsig == rsig) {
    return FunctionType::AssignmentSuccess::Always;
  }
  // TODO fuzzy matching of signatures
  if (lsig->getParameterCount() != rsig->getParameterCount()) {
    return FunctionType::AssignmentSuccess::Never;
  }
  return FunctionType::AssignmentSuccess::Sometimes; // TODO
}

const egg::ovum::IFunctionSignature* egg::ovum::FunctionType::callable() const {
  return &this->signature;
}

void egg::ovum::FunctionType::addParameter(const String& name, const Type& type, IFunctionSignatureParameter::Flags flags, size_t index) {
  assert((index == SIZE_MAX) || (index == this->signature.getParameterCount()));
  this->signature.addSignatureParameter(name, type, index, flags);
}

void egg::ovum::FunctionType::addParameter(const String& name, const Type& type, IFunctionSignatureParameter::Flags flags) {
  this->signature.addSignatureParameter(name, type, this->signature.getParameterCount(), flags);
}

egg::ovum::FunctionType* egg::ovum::FunctionType::createFunctionType(IAllocator& allocator, const String& name, const Type& rettype) {
  return allocator.create<FunctionType>(0, allocator, name, rettype);
}

egg::ovum::FunctionType* egg::ovum::FunctionType::createGeneratorType(IAllocator& allocator, const String& name, const Type& rettype) {
  // Convert the return type (e.g. 'int') into a generator function 'int..' aka '(void|int)()'
  return allocator.create<FunctionType>(0, allocator, name, allocator.make<GeneratorFunctionType, Type>(rettype));
}
