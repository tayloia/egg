#include "ovum/ovum.h"
#include "ovum/slot.h"
#include "ovum/node.h"

namespace {
  using namespace egg::ovum;

  const char* flagsComponent(ValueFlags flags) {
    EGG_WARNING_SUPPRESS_SWITCH_BEGIN();
      switch (flags) {
      case ValueFlags::None:
        return "var";
#define EGG_OVUM_VALUE_FLAGS_COMPONENT(name, text) case ValueFlags::name: return text;
        EGG_OVUM_VALUE_FLAGS(EGG_OVUM_VALUE_FLAGS_COMPONENT)
#undef EGG_OVUM_VALUE_FLAGS_COMPONENT
      case ValueFlags::Any:
        return "any";
      }
    EGG_WARNING_SUPPRESS_SWITCH_END();
      return nullptr;
  }

  std::string flagsToString(ValueFlags flags) {
    auto* component = flagsComponent(flags);
    if (component != nullptr) {
      return component;
    }
    if (Bits::hasAnySet(flags, ValueFlags::Null)) {
      return flagsToString(Bits::clear(flags, ValueFlags::Null)) + "?";
    }
    auto head = Bits::topmost(flags);
    assert(head != ValueFlags::None);
    assert(head != ValueFlags::Pointer);
    component = flagsComponent(head);
    assert(component != nullptr);
    return flagsToString(Bits::clear(flags, head)) + '|' + component;
  }

  std::pair<std::string, int> flagsToStringPrecedence(ValueFlags flags) {
    // Adjust the precedence of the result if the string has '|' (union) in it
    // This is so that parentheses can be added appropriately to handle "(void|int)*" versus "void|int*"
    auto result = std::make_pair(flagsToString(flags), 0);
    if (result.first.find('|') != std::string::npos) {
      result.second--;
    }
    return result;
  }

  Erratic<bool> assignableFlags(ValueFlags lhs, ValueFlags rhs) {
    if (Bits::hasAllSet(lhs, rhs)) {
      return true; // always
    }
    if (Bits::hasAnySet(lhs, rhs)) {
      return false; // sometimes
    }
    if (Bits::hasAnySet(lhs, ValueFlags::Float) && Bits::hasAnySet(rhs, ValueFlags::Int)) {
      // Float<-Int promotion
      return false; // sometimes
    }
    return Erratic<bool>::fail("Cannot assign values of type '", flagsToString(rhs),"' to targets of type '", flagsToString(lhs), "'");
  }

  Erratic<Value> tryAssignFlags(IAllocator& allocator, ValueFlags lflags, const Value& rhs) {
    auto rflags = rhs->getFlags();
    if (Bits::hasAnySet(lflags, rflags)) {
      return rhs;
    }
    Int i;
    if (Bits::hasAnySet(lflags, ValueFlags::Float) && rhs->getInt(i)) {
      // Float<-Int promotion
      auto f = Float(i);
      if (Int(f) != i) {
        return Erratic<Value>::fail("Cannot convert 'int' to 'float' accurately: ", i);
      }
      return ValueFactory::createFloat(allocator, f);
    }
    return Erratic<Value>::fail("Cannot convert '", rhs->getRuntimeType().toString(), "' to '", flagsToString(lflags), "'");
  }

  Erratic<Value> tryMutateOperation(IAllocator& allocator, ValueFlags lflags, IValue& lhs, Mutation mutation, const Value&) {
    if (mutation == Mutation::Increment) {
      if (Bits::hasAnySet(lflags, ValueFlags::Int)) {
        Int i;
        if (lhs.getInt(i)) {
          return ValueFactory::createInt(allocator, i + 1);
        }
      }
      return Erratic<Value>::fail("Expected increment '++' operator to be applied to a target of type 'int', not '", lhs.getRuntimeType().toString(), "'");
    }
    if (mutation == Mutation::Decrement) {
      if (Bits::hasAnySet(lflags, ValueFlags::Int)) {
        Int i;
        if (lhs.getInt(i)) {
          return ValueFactory::createInt(allocator, i - 1);
        }
      }
      return Erratic<Value>::fail("Expected decrement '--' operator to be applied to a target of type 'int', not '", lhs.getRuntimeType().toString(), "'");
    }
    return Erratic<Value>::fail("WIBBLE WOBBLE: tryMutateOperation() not implemented");
  }

  Error tryMutateFlags(IAllocator& allocator, ValueFlags lflags, Slot& slot, Mutation mutation, const Value& rhs) {
    if (mutation == Mutation::Assign) {
      // Treat assignment specially because the slot may be empty
      auto result = tryAssignFlags(allocator, lflags, rhs);
      if (result.failed()) {
        // The assignment is invalid
        return result.failure();
      }
      slot.clobber(result.success());
      return {};
    }
    for (;;) {
      // Loop until we successfully (atomically) update the slot (or fail)
      auto* lhs = slot.getReference();
      if (lhs == nullptr) {
        return Error("Cannot mutate an empty slot");
      }
      // If successful, 'error' will hold a hard reference to the result
      auto result = tryMutateOperation(allocator, lflags, *lhs, mutation, rhs);
      if (result.failed()) {
        // The mutation is invalid
        return result.failure();
      }
      auto desired = result.success();
      if (lhs == &desired.get()) {
        // No need to update
        break;
      }
      if (slot.update(lhs, desired)) {
        // We atomically updated the slot
        break;
      }
    }
    return {};
  }

  Type makeUnionFlags(IAllocator& allocator, ValueFlags flhs, const IType& lhs, const IType& rhs) {
    // Check for identity early
    if (&lhs == &rhs) {
      return Type(&lhs);
    }
    auto frhs = rhs.getFlags();
    if (Bits::hasAnySet(frhs, ValueFlags::Pointer)) {
      return TypeFactory::createUnionJoin(allocator, lhs, rhs);
    }
    if (Bits::hasAllSet(frhs, flhs)) {
      // The rhs is a superset of the types in lhs, just return rhs
      return Type(&rhs);
    }
    if (Bits::hasAnySet(frhs, ValueFlags::Object)) {
      // The superset check above should ensure this will not result in infinite recursion
      return rhs.makeUnion(allocator, lhs);
    }
    return TypeFactory::createSimple(allocator, flhs | frhs);
  }

  // An 'omni' function looks like this: 'any?(...any?[])'
  class OmniFunctionSignature : public IFunctionSignature {
    OmniFunctionSignature(const OmniFunctionSignature&) = delete;
    OmniFunctionSignature& operator=(const OmniFunctionSignature&) = delete;
  private:
    class Parameter : public IFunctionSignatureParameter {
    public:
      virtual String getName() const override {
        return String();
      }
      virtual Type getType() const override {
        return Type::AnyQ;
      }
      virtual size_t getPosition() const override {
        return 0;
      }
      virtual Flags getFlags() const override {
        return Flags::Variadic;
      }
    };
    Parameter parameter;
  public:
    OmniFunctionSignature() = default;
    virtual String getFunctionName() const override {
      return String();
    }
    virtual Type getReturnType() const override {
      return Type::AnyQ;
    }
    virtual size_t getParameterCount() const override {
      return 1;
    }
    virtual const IFunctionSignatureParameter& getParameter(size_t) const override {
      return this->parameter;
    }
  };
  const OmniFunctionSignature omniFunctionSignature{};

  class TypeBase : public IType {
    TypeBase(const TypeBase&) = delete;
    TypeBase& operator=(const TypeBase&) = delete;
  public:
    TypeBase() = default;
    virtual Erratic<Type> queryPropertyType(const String& property) const override {
      return Erratic<Type>::fail("Values of type '", this->str(), "' do not support properties such as '.", property, "'");
    }
    virtual Erratic<Type> queryIterable() const override {
      // By default, we are not iterable
      return Erratic<Type>::fail("Values of type '", this->str(), "' are not iterable");
    }
    virtual Erratic<Type> queryPointeeType() const override {
      // By default, we are not pointable
      return Erratic<Type>::fail("Values of type '", this->str(), "' do not support the pointer dereferencing '*' operator");
    }
    virtual const IFunctionSignature* queryCallable() const override {
      // By default, we are not callable
      return nullptr;
    }
    virtual const IIndexSignature* queryIndexable() const override {
      // By default, we are not indexable
      return nullptr;
    }
    std::string str() const {
      return this->toStringPrecedence().first;
    }
  };

  template<ValueFlags FLAGS>
  class TypeCommon : public NotReferenceCounted<TypeBase> {
    TypeCommon(const TypeCommon&) = delete;
    TypeCommon& operator=(const TypeCommon&) = delete;
  public:
    TypeCommon() = default;
    virtual ValueFlags getFlags() const override {
      return FLAGS;
    }
    virtual Type makeUnion(IAllocator& allocator, const IType& rhs) const override {
      return makeUnionFlags(allocator, FLAGS, *this, rhs);
    }
    virtual Error tryMutate(IAllocator& allocator, Slot& lhs, Mutation mutation, const Value& rhs) const override {
      return tryMutateFlags(allocator, FLAGS, lhs, mutation, rhs);
    }
    virtual Erratic<bool> queryAssignableAlways(const IType& rhs) const override {
      return assignableFlags(FLAGS, rhs.getFlags());
    }
    virtual std::pair<std::string, int> toStringPrecedence() const override {
      return flagsToStringPrecedence(FLAGS);
    }
  };
  const TypeCommon<ValueFlags::Bool> typeBool{};
  const TypeCommon<ValueFlags::Int> typeInt{};
  const TypeCommon<ValueFlags::Float> typeFloat{};
  const TypeCommon<ValueFlags::Int | ValueFlags::Float> typeArithmetic{};

  class TypeVoid : public TypeCommon<ValueFlags::Void> {
    TypeVoid(const TypeVoid&) = delete;
    TypeVoid& operator=(const TypeVoid&) = delete;
  public:
    TypeVoid() = default;
    virtual Erratic<bool> queryAssignableAlways(const IType&) const override {
      return Erratic<bool>::fail("Cannot assign values to 'void'");
    }
  };
  const TypeVoid typeVoid{};

  class TypeNull : public TypeCommon<ValueFlags::Null> {
    TypeNull(const TypeNull&) = delete;
    TypeNull& operator=(const TypeNull&) = delete;
  public:
    TypeNull() = default;
  };
  const TypeNull typeNull{};

  class TypeString : public TypeCommon<ValueFlags::String> {
    TypeString(const TypeString&) = delete;
    TypeString& operator=(const TypeString&) = delete;
  private:
    class IndexSignature : public IIndexSignature {
      virtual Type getResultType() const override {
        return Type::String;
      }
      virtual Type getIndexType() const override {
        return Type::Int;
      }
    };
  public:
    TypeString() = default;
    virtual Erratic<Type> queryPropertyType(const String& property) const {
      auto type = ValueFactory::queryBuiltinStringProperty(property);
      if (type == nullptr) {
        return Erratic<Type>::fail("Unknown property for string: '.", property, "'");
      }
      return type;
    }
    virtual const IIndexSignature* queryIndexable() const override {
      static const IndexSignature indexSignature{};
      return &indexSignature;
    }
    virtual Erratic<Type> queryIterable() const override {
      return Type::String;
    }
  };
  const TypeString typeString{};

  class TypeObject : public TypeCommon<ValueFlags::Object> {
    TypeObject(const TypeObject&) = delete;
    TypeObject& operator=(const TypeObject&) = delete;
  private:
    class IndexSignature : public IIndexSignature {
      virtual Type getResultType() const override {
        return Type::AnyQ;
      }
      virtual Type getIndexType() const override {
        return Type::AnyQ;
      }
    };
  public:
    TypeObject() = default;
    virtual Erratic<Type> queryPropertyType(const String&) const {
      return Type::AnyQ;
    }
    virtual const IFunctionSignature* queryCallable() const override {
      return &omniFunctionSignature;
    }
    virtual const IIndexSignature* queryIndexable() const override {
      static const IndexSignature indexSignature{};
      return &indexSignature;
    }
  };
  const TypeObject typeObject{};

  class TypeMemory : public TypeCommon<ValueFlags::Memory> {
    TypeMemory(const TypeMemory&) = delete;
    TypeMemory& operator=(const TypeMemory&) = delete;
  public:
    TypeMemory() = default;
  };
  const TypeMemory typeMemory{};

  class TypeAny : public TypeCommon<ValueFlags::Any> {
    TypeAny(const TypeAny&) = delete;
    TypeAny& operator=(const TypeAny&) = delete;
  public:
    TypeAny() = default;
    virtual const IFunctionSignature* queryCallable() const override {
      return &omniFunctionSignature;
    }
  };
  const TypeAny typeAny{};

  class TypeAnyQ : public TypeCommon<ValueFlags::AnyQ> {
    TypeAnyQ(const TypeAnyQ&) = delete;
    TypeAnyQ& operator=(const TypeAnyQ&) = delete;
  public:
    TypeAnyQ() = default;
    virtual const IFunctionSignature* queryCallable() const override {
      return &omniFunctionSignature;
    }
  };
  const TypeAnyQ typeAnyQ{};

  class TypeUnion : public HardReferenceCounted<TypeBase> {
    TypeUnion(const TypeUnion&) = delete;
    TypeUnion& operator=(const TypeUnion&) = delete;
  private:
    Type a;
    Type b;
  public:
    TypeUnion(IAllocator& allocator, const IType& a, const IType& b)
      : HardReferenceCounted(allocator, 0), a(&a), b(&b) {
    }
    virtual ValueFlags getFlags() const override {
      return this->a->getFlags() | this->b->getFlags();
    }
    virtual Type makeUnion(IAllocator& allocator2, const IType& rhs) const override {
      assert(&this->allocator == &allocator2);
      return TypeFactory::createUnionJoin(allocator2, *this, rhs);
    }
    virtual Error tryMutate(IAllocator& allocator2, Slot& lhs, Mutation mutation, const Value& rhs) const override {
      assert(&this->allocator == &allocator2);
      auto qa = a->tryMutate(allocator2, lhs, mutation, rhs);
      if (!qa.empty()) {
        auto qb = b->tryMutate(allocator2, lhs, mutation, rhs);
        if (qb.empty()) {
          return qb;
        }
      }
      return qa;
    }
    virtual Erratic<bool> queryAssignableAlways(const IType& rhs) const override {
      // The logic is:
      //  Assignable to a Assignable to b Result
      //  =============== =============== ======
      //  Never           Never           Never
      //  Never           Sometimes       Sometimes
      //  Never           Always          Always
      //  Sometimes       Never           Sometimes
      //  Sometimes       Sometimes       Sometimes
      //  Sometimes       Always          Always
      //  Always          Never           Always
      //  Always          Sometimes       Always
      //  Always          Always          Always
      auto qa = this->a->queryAssignableAlways(rhs);
      if (qa.failed()) {
        // Never assignable to a
        return this->b->queryAssignableAlways(rhs);
      }
      if (qa.success() == true) {
        // Always assignable to a
        return qa;
      }
      // Sometimes assignable to a
      auto qb = this->b->queryAssignableAlways(rhs);
      if (qb.failed()) {
        // Sometimes assignable to a, but never to b
        return qa;
      }
      // Sometimes assignable to a, and sometimes/always to b
      return qb;
    }
    virtual Erratic<Type> queryPropertyType(const String& property) const {
      auto qa = this->a->queryPropertyType(property);
      if (qa.failed()) {
        auto qb = this->b->queryPropertyType(property);
        if (!qb.failed()) {
          return qb;
        }
      }
      return qa;
    }
    virtual const IFunctionSignature* queryCallable() const override {
      // TODO What if both have signatures?
      auto* signature = this->a->queryCallable();
      if (signature != nullptr) {
        return signature;
      }
      return this->b->queryCallable();
    }
    virtual std::pair<std::string, int> toStringPrecedence() const override {
      // TODO
      auto ab = StringBuilder().add(this->a.toString(0), '|', this->b.toString(0)).toUTF8();
      return std::make_pair(ab, 0);
    }
  };

  class TypeSimple : public HardReferenceCounted<TypeBase> {
    TypeSimple(const TypeSimple&) = delete;
    TypeSimple& operator=(const TypeSimple&) = delete;
  private:
    ValueFlags flags;
  public:
    TypeSimple(IAllocator& allocator, ValueFlags flags)
      : HardReferenceCounted(allocator, 0), flags(flags) {
    }
    virtual ValueFlags getFlags() const override {
      return this->flags;
    }
    virtual Type makeUnion(IAllocator& allocator2, const IType& rhs) const override {
      assert(&this->allocator == &allocator2);
      return makeUnionFlags(allocator2, this->flags, *this, rhs);
    }
    virtual Error tryMutate(IAllocator& allocator2, Slot& lhs, Mutation mutation, const Value& rhs) const override {
      assert(&this->allocator == &allocator2);
      return tryMutateFlags(allocator2, this->flags, lhs, mutation, rhs);
    }
    virtual Erratic<bool> queryAssignableAlways(const IType& rhs) const override {
      return assignableFlags(this->flags, rhs.getFlags());
    }
    virtual const IFunctionSignature* queryCallable() const override {
      if (Bits::hasAnySet(this->flags, ValueFlags::Object)) {
        return &omniFunctionSignature;
      }
      return nullptr;
    }
    virtual std::pair<std::string, int> toStringPrecedence() const override {
      return flagsToStringPrecedence(this->flags);
    }
  };

  class TypePointer : public HardReferenceCounted<TypeBase> {
    TypePointer(const TypePointer&) = delete;
    TypePointer& operator=(const TypePointer&) = delete;
  private:
    Type referenced;
  public:
    TypePointer(IAllocator& allocator, const IType& referenced)
      : HardReferenceCounted(allocator, 0), referenced(&referenced) {
    }
    virtual ValueFlags getFlags() const override {
      return ValueFlags::Pointer;
    }
    virtual Type makeUnion(IAllocator& allocator2, const IType& rhs) const override {
      // TODO elide if similar
      assert(&this->allocator == &allocator2);
      return TypeFactory::createUnionJoin(allocator2, *this, rhs);
    }
    virtual Error tryMutate(IAllocator& allocator2, Slot&, Mutation, const Value&) const override {
      assert(&this->allocator == &allocator2);
      (void)&allocator2;
      return Error("WIBBLE: Pointer modification not yet implemented");
    }
    virtual Erratic<bool> queryAssignableAlways(const IType& rhs) const override {
      return Erratic<bool>::fail("TODO: Assignment of values of type '", Type(&rhs).toString(), "' to '", Type(this).toString(), "'");
    }
    virtual std::pair<std::string, int> toStringPrecedence() const override {
      auto p = StringBuilder().add(this->referenced.toString(0), '*').toUTF8();
      return std::make_pair(p, 0);
    }
  };

  class FunctionSignatureParameter : public IFunctionSignatureParameter {
  private:
    String name; // may be empty
    Type type;
    size_t position; // may be SIZE_MAX
    Flags flags;
  public:
    FunctionSignatureParameter(const String& name, const Type& type, size_t position, Flags flags)
      : name(name), type(type), position(position), flags(flags) {
    }
    virtual String getName() const override {
      return this->name;
    }
    virtual Type getType() const override {
      return this->type;
    }
    virtual size_t getPosition() const override {
      return this->position;
    }
    virtual Flags getFlags() const override {
      return this->flags;
    }
  };

  class FunctionSignature : public IFunctionSignature {
  private:
    String name;
    Type rettype;
    std::vector<FunctionSignatureParameter> parameters;
  public:
    FunctionSignature(const String& name, const Type& rettype)
      : name(name), rettype(rettype) {
    }
    void addSignatureParameter(const String& parameterName, const Type& parameterType, size_t position, IFunctionSignatureParameter::Flags flags) {
      this->parameters.emplace_back(parameterName, parameterType, position, flags);
    }
    virtual String getFunctionName() const override {
      return this->name;
    }
    virtual Type getReturnType() const override {
      return this->rettype;
    }
    virtual size_t getParameterCount() const override {
      return this->parameters.size();
    }
    virtual const IFunctionSignatureParameter& getParameter(size_t index) const override {
      assert(index < this->parameters.size());
      return this->parameters[index];
    }
    // Helpers
    enum class Parts {
      ReturnType = 0x01,
      FunctionName = 0x02,
      ParameterList = 0x04,
      ParameterNames = 0x08,
      NoNames = ReturnType | ParameterList,
      All = ReturnType | FunctionName | ParameterList | ParameterNames
    };
    static void build(StringBuilder& sb, const IFunctionSignature& signature, Parts parts = Parts::All) {
      // TODO better formatting of named/variadic etc.
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
    }
  };

  class FunctionType : public HardReferenceCounted<ITypeFunction> {
    FunctionType(const FunctionType&) = delete;
    FunctionType& operator=(const FunctionType&) = delete;
  protected:
    FunctionSignature signature;
  public:
    FunctionType(IAllocator& allocator, const String& name, const Type& rettype)
      : HardReferenceCounted(allocator, 0),
        signature(name, rettype) {
    }
    // Interface
    virtual ValueFlags getFlags() const override {
      return ValueFlags::Object;
    }
    virtual Type makeUnion(IAllocator& allocator2, const IType& rhs) const override {
      assert(&this->allocator == &allocator2);
      return TypeFactory::createUnionJoin(allocator2, *this, rhs);
    }
    virtual Error tryMutate(IAllocator& allocator2, Slot&, Mutation, const Value&) const override {
      assert(&this->allocator == &allocator2);
      (void)&allocator2;
      return Error("WIBBLE: Function modification not yet implemented");
    }
    virtual Erratic<bool> queryAssignableAlways(const IType& rhs) const override {
      // We can assign if the signatures are the same or equal
      auto* rsig = rhs.queryCallable();
      if (rsig == nullptr) {
        return Erratic<bool>::fail("Cannot assign values of type '", Type(&rhs).toString(), "' to targets expecting functions");
      }
      auto* lsig = &this->signature;
      if (lsig == rsig) {
        return true; // always
      }
      // TODO fuzzy matching of signatures
      auto lnum = lsig->getParameterCount();
      auto rnum = rsig->getParameterCount();
      if (lnum != rnum) {
        return Erratic<bool>::fail("Cannot assign functions with ", rnum, " parameter(s) to targets expecting ", lnum, " function parameter(s)");
      }
      return false; // sometimes
    }
    virtual Erratic<Type> queryPropertyType(const String& property) const override {
      return Erratic<Type>::fail("Unknown property for function: '.", property, "'");
    }
    virtual Erratic<Type> queryIterable() const override {
      return Erratic<Type>::fail("Functions are not iterable");
    }
    virtual Erratic<Type> queryPointeeType() const override {
      return Erratic<Type>::fail("Functions do not support the pointer dereferencing '*' operator");
    }
    virtual const IFunctionSignature* queryCallable() const override {
      return &this->signature;
    }
    virtual const IIndexSignature* queryIndexable() const override {
      return nullptr;
    }
    virtual std::pair<std::string, int> toStringPrecedence() const override {
      // Do not include names in the signature
      StringBuilder sb;
      FunctionSignature::build(sb, this->signature, FunctionSignature::Parts::NoNames);
      return std::make_pair(sb.toUTF8(), 0);
    }
    virtual void addParameter(const String& name, const Type& type, IFunctionSignatureParameter::Flags flags, size_t index = SIZE_MAX) override {
      assert((index == SIZE_MAX) || (index == this->signature.getParameterCount()));
      this->signature.addSignatureParameter(name, type, index, flags);
    }
  };

  class GeneratorFunctionType : public FunctionType {
    GeneratorFunctionType(const GeneratorFunctionType&) = delete;
    GeneratorFunctionType& operator=(const GeneratorFunctionType&) = delete;
  private:
    Type rettype;
  public:
    explicit GeneratorFunctionType(IAllocator& allocator, const Type& rettype)
      : FunctionType(allocator, String(), TypeFactory::createUnion(allocator, *rettype, *Type::Void)),
        rettype(rettype) {
      assert(!Bits::hasAnySet(rettype->getFlags(), ValueFlags::Void));
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

egg::ovum::Type egg::ovum::TypeFactory::createSimple(IAllocator& allocator, ValueFlags flags) {
  // OPTIMIZE for known types
  return allocator.make<TypeSimple, Type>(flags);
}

egg::ovum::Type egg::ovum::TypeFactory::createPointer(IAllocator& allocator, const IType& pointee) {
  // OPTIMIZE with lazy pointer types in values instantiated when address-of is invoked
  return allocator.make<TypePointer, Type>(pointee);
}

egg::ovum::Type egg::ovum::TypeFactory::createUnion(IAllocator& allocator, const IType& a, const IType& b) {
  if (&a == &b) {
    return Type(&a);
  }
  return a.makeUnion(allocator, b);
}

egg::ovum::Type egg::ovum::TypeFactory::createUnionJoin(IAllocator& allocator, const IType& a, const IType& b) {
  if (&a == &b) {
    return Type(&a);
  }
  return allocator.make<TypeUnion, Type>(a, b);
}

egg::ovum::ITypeFunction* egg::ovum::TypeFactory::createFunction(IAllocator& allocator, const String& name, const Type& rettype) {
  return allocator.create<FunctionType>(0, allocator, name, rettype);
}

egg::ovum::ITypeFunction* egg::ovum::TypeFactory::createGenerator(IAllocator& allocator, const String& name, const Type& rettype) {
  // Convert the return type (e.g. 'int') into a generator function 'int...' aka '(void|int)()'
  return allocator.create<FunctionType>(0, allocator, name, allocator.make<GeneratorFunctionType, Type>(rettype));
}

// Common types
const egg::ovum::Type egg::ovum::Type::Void{ &typeVoid };
const egg::ovum::Type egg::ovum::Type::Null{ &typeNull };
const egg::ovum::Type egg::ovum::Type::Bool{ &typeBool };
const egg::ovum::Type egg::ovum::Type::Int{ &typeInt };
const egg::ovum::Type egg::ovum::Type::Float{ &typeFloat };
const egg::ovum::Type egg::ovum::Type::String{ &typeString };
const egg::ovum::Type egg::ovum::Type::Object{ &typeObject };
const egg::ovum::Type egg::ovum::Type::Memory{ &typeMemory };
const egg::ovum::Type egg::ovum::Type::Arithmetic{ &typeArithmetic };
const egg::ovum::Type egg::ovum::Type::Any{ &typeAny };
const egg::ovum::Type egg::ovum::Type::AnyQ{ &typeAnyQ };

