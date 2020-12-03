#include "ovum/ovum.h"
#include "ovum/node.h"
#include "ovum/module.h"
#include "ovum/program.h"
#include "ovum/builtin.h"
#include "ovum/function.h"
#include "ovum/slot.h"

#include <map>

namespace {
  using namespace egg::ovum;

  template<typename T, typename... ARGS>
  static Value createBuiltinValueHard(IAllocator& allocator, ARGS&&... args) {
    Object object{ *allocator.make<T>(std::forward<ARGS>(args)...) };
    return ValueFactory::createObjectHard(allocator, object);
  }

  // WEBBLE conflate?
  template<typename T, typename... ARGS>
  static Value createBuiltinValueSoft(IAllocator& allocator, IBasket& basket, ARGS&&... args) {
    Object object{ *allocator.make<T>(basket, std::forward<ARGS>(args)...) };
    return ValueFactory::createObjectHard(allocator, object);
  }

  class ParametersNone : public IParameters {
  public:
    virtual size_t getPositionalCount() const override {
      return 0;
    }
    virtual Value getPositional(size_t) const override {
      return Value::Void;
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
    virtual Value getNamed(const String&) const override {
      return Value::Void;
    }
    virtual const LocationSource* getNamedLocation(const String&) const override {
      return nullptr;
    }
  };
  const ParametersNone parametersNone{};

  class StringProperty_FunctionType : public NotSoftReferenceCounted<IType>, public IFunctionSignature {
    StringProperty_FunctionType(const StringProperty_FunctionType&) = delete;
    StringProperty_FunctionType& operator=(const StringProperty_FunctionType&) = delete;
  private:
    struct Parameter : public IFunctionSignatureParameter {
      String name;
      Type type;
      size_t position = SIZE_MAX;
      Flags flags = Flags::None;
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
    static const size_t MAX_PARAMETERS = 3;
    String name;
    Type rettype;
    size_t parameters;
    Parameter parameter[MAX_PARAMETERS];
    ObjectShape shape;
  public:
    StringProperty_FunctionType(const String& name, const Type& rettype)
      : name(name),
        rettype(rettype),
        parameters(0),
        shape({}) {
      this->shape.callable = this;
    }
    virtual ValueFlags getPrimitiveFlags() const override {
      return ValueFlags::None;
    }
    virtual const ObjectShape* getObjectShape(size_t index) const override {
      return (index == 0) ? &this->shape : nullptr;
    }
    virtual size_t getObjectShapeCount() const override {
      return 1;
    }
    virtual const PointerShape* getPointerShape(size_t) const override {
      return nullptr;
    }
    virtual size_t getPointerShapeCount() const override {
      return 0;
    }
    virtual std::pair<std::string, int> toStringPrecedence() const override {
      return FunctionSignature::toStringPrecedence(*this);
    }
    virtual String describeValue() const override {
      return StringBuilder::concat("Built-in 'string.", this->name, "'");
    }
    virtual String getFunctionName() const override {
      return this->name;
    }
    virtual Type getReturnType() const override {
      return this->rettype;
    }
    virtual size_t getParameterCount() const override {
      return this->parameters;
    }
    virtual const IFunctionSignatureParameter& getParameter(size_t index) const override {
      assert(index < this->parameters);
      return this->parameter[index];
    }
    void addParameter(const String& pname, const Type& ptype, IFunctionSignatureParameter::Flags pflags) {
      assert(this->parameters < MAX_PARAMETERS);
      auto index = this->parameters++;
      auto& entry = this->parameter[index];
      entry.name = pname;
      entry.type = ptype;
      entry.position = index;
      entry.flags = pflags;
    }
  };

  class StringProperty_Length final : public BuiltinFactory::StringProperty {
    StringProperty_Length(const StringProperty_Length&) = delete;
    StringProperty_Length& operator=(const StringProperty_Length&) = delete;
  public:
    StringProperty_Length() {}
    virtual Value createInstance(IAllocator& allocator, const String& string) const override {
      return ValueFactory::createInt(allocator, Int(string.length()));
    }
    virtual String getName() const override {
      return "length";
    }
    virtual Type getType() const override {
      return Type::Int;
    }
  };

  class StringProperty_Member : public BuiltinFactory::StringProperty {
    StringProperty_Member(const StringProperty_Member&) = delete;
    StringProperty_Member& operator=(const StringProperty_Member&) = delete;
  private:
    StringProperty_FunctionType ftype;
  public:
    StringProperty_Member(const String& name, const Type& rettype)
      : ftype(name, rettype) {
    }
    virtual Value createInstance(IAllocator& allocator, const String& string) const override;
    virtual String getName() const override {
      return this->ftype.getFunctionName();
    }
    virtual Type getType() const override {
      return Type(&this->ftype);
    }
    virtual Value invoke(IExecution& execution, const String& string, ParameterChecker& checker) const = 0;
    void addRequiredParameter(const String& pname, const Type& ptype) {
      this->ftype.addParameter(pname, ptype, IFunctionSignatureParameter::Flags::Required);
    }
    void addOptionalParameter(const String& pname, const Type& ptype) {
      this->ftype.addParameter(pname, ptype, IFunctionSignatureParameter::Flags::None);
    }
    void addVariadicParameter(const String& pname, const Type& ptype) {
      this->ftype.addParameter(pname, ptype, IFunctionSignatureParameter::Flags::Variadic);
    }
    Value call(IExecution& execution, const String& string, const IParameters& parameters) const {
      if (parameters.getNamedCount() > 0) {
        return this->raise(execution, "does not accept named parameters"); // TODO
      }
      ParameterChecker checker{ parameters, &this->ftype };
      if (!checker.validateCount()) {
        return this->raise(execution, checker.error);
      }
      return this->invoke(execution, string, checker);
    }
    template<typename... ARGS>
    Value raise(IExecution& execution, ARGS&&... args) const {
      return execution.raiseFormat("String property function '", this->getName(), "' ", std::forward<ARGS>(args)...);
    }
  };

  class StringProperty_Instance : public SoftReferenceCounted<IObject> {
    StringProperty_Instance(const StringProperty_Instance&) = delete;
    StringProperty_Instance& operator=(const StringProperty_Instance&) = delete;
  private:
    const StringProperty_Member& member;
    String string;
  public:
    StringProperty_Instance(IAllocator& allocator, const StringProperty_Member& member, const String& string)
      : SoftReferenceCounted(allocator),
        member(member),
        string(string) {
    }
    virtual void softVisit(const Visitor&) const override {
      // Nothing to visit
    }
    virtual Type getRuntimeType() const override {
      return this->member.getType();
    }
    virtual Value call(IExecution& execution, const IParameters& parameters) override {
      return this->member.call(execution, this->string, parameters);
    }
    virtual Value getProperty(IExecution& execution, const String& property) override {
      return this->unsupported(execution, "properties such as '", property, "'");
    }
    virtual Value setProperty(IExecution& execution, const String& property, const Value&) override {
      return this->unsupported(execution, "properties such as '", property, "'");
    }
    virtual Value mutProperty(IExecution& execution, const String& property, Mutation, const Value&) override {
      return this->unsupported(execution, "properties such as '", property, "'");
    }
    virtual Value delProperty(IExecution& execution, const String& property) override {
      return this->unsupported(execution, "properties such as '", property, "'");
    }
    virtual Value refProperty(IExecution& execution, const String& property) override {
      return this->unsupported(execution, "properties such as '", property, "'");
    }
    virtual Value getIndex(IExecution& execution, const Value&) override {
      return this->unsupported(execution, "indexing with '[]'");
    }
    virtual Value setIndex(IExecution& execution, const Value&, const Value&) override {
      return this->unsupported(execution, "indexing with '[]'");
    }
    virtual Value mutIndex(IExecution& execution, const Value&, Mutation, const Value&) override {
      return this->unsupported(execution, "indexing with '[]'");
    }
    virtual Value delIndex(IExecution& execution, const Value&) override {
      return this->unsupported(execution, "indexing with '[]'");
    }
    virtual Value refIndex(IExecution& execution, const Value&) override {
      return this->unsupported(execution, "indexing with '[]'");
    }
    virtual Value getPointee(IExecution& execution) override {
      return this->unsupported(execution, "pointer dereferencing with '*'");
    }
    virtual Value setPointee(IExecution& execution, const Value&) override {
      return this->unsupported(execution, "pointer dereferencing with '*'");
    }
    virtual Value mutPointee(IExecution& execution, Mutation, const Value&) override {
      return this->unsupported(execution, "pointer dereferencing with '*'");
    }
    virtual Value iterate(IExecution& execution) override {
      return this->unsupported(execution, "iteration");
    }
    virtual void print(Printer& printer) const override {
      printer << "<string-" << this->member.getName() << '>';
    }
  private:
    template<typename... ARGS>
    Value unsupported(IExecution& execution, ARGS&&... args) {
      return this->member.raise(execution, "does not support ", std::forward<ARGS>(args)...);
    }
  };

  Value StringProperty_Member::createInstance(IAllocator& allocator, const String& string) const {
    return createBuiltinValueHard<StringProperty_Instance>(allocator, *this, string);
  }

  class StringProperty_CompareTo final : public StringProperty_Member {
    StringProperty_CompareTo(const StringProperty_CompareTo&) = delete;
    StringProperty_CompareTo& operator=(const StringProperty_CompareTo&) = delete;
  public:
    StringProperty_CompareTo() : StringProperty_Member("compareTo", Type::Int) {
      this->addRequiredParameter("other", Type::String);
    }
    virtual Value invoke(IExecution& execution, const String& string, ParameterChecker& checker) const override {
      String other;
      if (checker.validateParameter(0, other)) {
        auto result = string.compareTo(other);
        return execution.makeValue(result);
      }
      return this->raise(execution, checker.error);
    }
  };

  class StringProperty_Contains final : public StringProperty_Member {
    StringProperty_Contains(const StringProperty_Contains&) = delete;
    StringProperty_Contains& operator=(const StringProperty_Contains&) = delete;
  public:
    StringProperty_Contains() : StringProperty_Member("contains", Type::Bool) {
      this->addRequiredParameter("needle", Type::String);
    }
    virtual Value invoke(IExecution& execution, const String& string, ParameterChecker& checker) const override {
      String needle;
      if (checker.validateParameter(0, needle)) {
        auto result = string.contains(needle);
        return execution.makeValue(result);
      }
      return this->raise(execution, checker.error);
    }
  };

  class StringProperty_EndsWith final : public StringProperty_Member {
    StringProperty_EndsWith(const StringProperty_EndsWith&) = delete;
    StringProperty_EndsWith& operator=(const StringProperty_EndsWith&) = delete;
  public:
    StringProperty_EndsWith() : StringProperty_Member("endsWith", Type::Bool) {
      this->addRequiredParameter("needle", Type::String);
    }
    virtual Value invoke(IExecution& execution, const String& string, ParameterChecker& checker) const override {
      String needle;
      if (checker.validateParameter(0, needle)) {
        auto result = string.endsWith(needle);
        return execution.makeValue(result);
      }
      return this->raise(execution, checker.error);
    }
  };

  class StringProperty_Hash final : public StringProperty_Member {
    StringProperty_Hash(const StringProperty_Hash&) = delete;
    StringProperty_Hash& operator=(const StringProperty_Hash&) = delete;
  public:
    StringProperty_Hash() : StringProperty_Member("hash", Type::Int) {
    }
    virtual Value invoke(IExecution& execution, const String& string, ParameterChecker&) const override {
      auto result = string.hash();
      return execution.makeValue(result);
    }
  };

  class StringProperty_IndexOf final : public StringProperty_Member {
    StringProperty_IndexOf(const StringProperty_IndexOf&) = delete;
    StringProperty_IndexOf& operator=(const StringProperty_IndexOf&) = delete;
  public:
    StringProperty_IndexOf() : StringProperty_Member("indexOf", Type::IntQ) {
      this->addRequiredParameter("needle", Type::String);
      this->addOptionalParameter("fromIndex", Type::Int);
    }
    virtual Value invoke(IExecution& execution, const String& string, ParameterChecker& checker) const override {
      String needle;
      std::optional<Int> fromIndex;
      if (checker.validateParameter(0, needle) && checker.validateParameter(1, fromIndex)) {
        Int result;
        if (fromIndex) {
          if (*fromIndex < 0) {
            return this->raise(execution, "expected optional second parameter 'fromIndex' to be a non-negative integer, but got ", *fromIndex);
          }
          result = string.indexOfString(needle, size_t(*fromIndex));
        } else {
          result = string.indexOfString(needle);
        }
        return (result < 0) ? Value::Null : execution.makeValue(result);
      }
      return this->raise(execution, checker.error);
    }
  };

  class StringProperty_Join final : public StringProperty_Member {
    StringProperty_Join(const StringProperty_Join&) = delete;
    StringProperty_Join& operator=(const StringProperty_Join&) = delete;
  public:
    StringProperty_Join() : StringProperty_Member("join", Type::String) {
      this->addVariadicParameter("parts", Type::String);
    }
    virtual Value invoke(IExecution& execution, const String& string, ParameterChecker& checker) const override {
      auto separator = string.toUTF8();
      auto count = checker.parameters->getPositionalCount();
      StringBuilder sb;
      for (size_t index = 0; index < count; ++index) {
        auto value = checker.parameters->getPositional(index);
        if (index > 0) {
          sb.add(separator);
        }
        value->print(sb);
      }
      return execution.makeValue(sb.str());
    }
  };

  class StringProperty_LastIndexOf final : public StringProperty_Member {
    StringProperty_LastIndexOf(const StringProperty_LastIndexOf&) = delete;
    StringProperty_LastIndexOf& operator=(const StringProperty_LastIndexOf&) = delete;
  public:
    StringProperty_LastIndexOf() : StringProperty_Member("lastIndexOf", Type::IntQ) {
      this->addRequiredParameter("needle", Type::String);
      this->addOptionalParameter("fromIndex", Type::Int);
    }
    virtual Value invoke(IExecution& execution, const String& string, ParameterChecker& checker) const override {
      String needle;
      std::optional<Int> fromIndex;
      if (checker.validateParameter(0, needle) && checker.validateParameter(1, fromIndex)) {
        Int result;
        if (fromIndex) {
          if (*fromIndex < 0) {
            return this->raise(execution, "expected optional second parameter 'fromIndex' to be a non-negative integer, but got ", *fromIndex);
          }
          result = string.lastIndexOfString(needle, size_t(*fromIndex));
        } else {
          result = string.lastIndexOfString(needle);
        }
        return (result < 0) ? Value::Null : execution.makeValue(result);
      }
      return this->raise(execution, checker.error);
    }
  };

  class StringProperty_PadLeft final : public StringProperty_Member {
    StringProperty_PadLeft(const StringProperty_PadLeft&) = delete;
    StringProperty_PadLeft& operator=(const StringProperty_PadLeft&) = delete;
  public:
    StringProperty_PadLeft() : StringProperty_Member("padLeft", Type::String) {
      this->addRequiredParameter("target", Type::Int);
      this->addOptionalParameter("padding", Type::String);
    }
    virtual Value invoke(IExecution& execution, const String& string, ParameterChecker& checker) const override {
      Int target;
      if (checker.validateParameter(0, target)) {
        if (target < 0) {
          return this->raise(execution, "expected first parameter 'target' to be a non-negative integer, but got ", target);
        }
        std::optional<String> padding;
        if (checker.validateParameter(1, padding)) {
          auto result = padding ? string.padLeft(size_t(target), *padding) : string.padLeft(size_t(target));
          return execution.makeValue(result);
        }
      }
      return this->raise(execution, checker.error);
    }
  };

  class StringProperty_PadRight final : public StringProperty_Member {
    StringProperty_PadRight(const StringProperty_PadRight&) = delete;
    StringProperty_PadRight& operator=(const StringProperty_PadRight&) = delete;
  public:
    StringProperty_PadRight() : StringProperty_Member("padRight", Type::String) {
      this->addRequiredParameter("target", Type::Int);
      this->addOptionalParameter("padding", Type::String);
    }
    virtual Value invoke(IExecution& execution, const String& string, ParameterChecker& checker) const override {
      Int target;
      if (checker.validateParameter(0, target)) {
        if (target < 0) {
          return this->raise(execution, "expected first parameter 'target' to be a non-negative integer, but got ", target);
        }
        std::optional<String> padding;
        if (checker.validateParameter(1, padding)) {
          auto result = padding ? string.padRight(size_t(target), *padding) : string.padRight(size_t(target));
          return execution.makeValue(result);
        }
      }
      return this->raise(execution, checker.error);
    }
  };

  class StringProperty_Slice final : public StringProperty_Member {
    StringProperty_Slice(const StringProperty_Slice&) = delete;
    StringProperty_Slice& operator=(const StringProperty_Slice&) = delete;
  public:
    StringProperty_Slice() : StringProperty_Member("slice", Type::String) {
      this->addRequiredParameter("begin", Type::Int);
      this->addOptionalParameter("end", Type::Int);
    }
    virtual Value invoke(IExecution& execution, const String& string, ParameterChecker& checker) const override {
      Int begin;
      std::optional<Int> end;
      if (checker.validateParameter(0, begin) && checker.validateParameter(1, end)) {
        auto result = end ? string.slice(begin, *end) : string.slice(begin);
        return execution.makeValue(result);
      }
      return this->raise(execution, checker.error);
    }
  };

  class StringProperty_Repeat final : public StringProperty_Member {
    StringProperty_Repeat(const StringProperty_Repeat&) = delete;
    StringProperty_Repeat& operator=(const StringProperty_Repeat&) = delete;
  public:
    StringProperty_Repeat() : StringProperty_Member("repeat", Type::String) {
      this->addRequiredParameter("count", Type::Int);
    }
    virtual Value invoke(IExecution& execution, const String& string, ParameterChecker& checker) const override {
      Int count;
      if (checker.validateParameter(0, count)) {
        if (count < 0) {
          return this->raise(execution, "expected parameter 'count' to be a non-negative integer, but got ", count);
        }
        auto result = string.repeat(size_t(count));
        return execution.makeValue(result);
      }
      return this->raise(execution, checker.error);
    }
  };

  class StringProperty_Replace final : public StringProperty_Member {
    StringProperty_Replace(const StringProperty_Replace&) = delete;
    StringProperty_Replace& operator=(const StringProperty_Replace&) = delete;
  public:
    StringProperty_Replace() : StringProperty_Member("replace", Type::String) {
      this->addRequiredParameter("needle", Type::String);
      this->addRequiredParameter("replacement", Type::String);
      this->addOptionalParameter("occurrences", Type::Int);
    }
    virtual Value invoke(IExecution& execution, const String& string, ParameterChecker& checker) const override {
      String needle;
      String replacement;
      std::optional<Int> occurrences;
      if (checker.validateParameter(0, needle) && checker.validateParameter(1, replacement) && checker.validateParameter(2, occurrences)) {
        auto result = occurrences ? string.replace(needle, replacement, *occurrences) : string.replace(needle, replacement);
        return execution.makeValue(result);
      }
      return this->raise(execution, checker.error);
    }
  };

  class StringProperty_StartsWith final : public StringProperty_Member {
    StringProperty_StartsWith(const StringProperty_StartsWith&) = delete;
    StringProperty_StartsWith& operator=(const StringProperty_StartsWith&) = delete;
  public:
    StringProperty_StartsWith() : StringProperty_Member("startsWith", Type::Bool) {
      this->addRequiredParameter("needle", Type::String);
    }
    virtual Value invoke(IExecution& execution, const String& string, ParameterChecker& checker) const override {
      String needle;
      if (checker.validateParameter(0, needle)) {
        auto result = string.startsWith(needle);
        return execution.makeValue(result);
      }
      return this->raise(execution, checker.error);
    }
  };

  class StringProperty_ToString final : public StringProperty_Member {
    StringProperty_ToString(const StringProperty_ToString&) = delete;
    StringProperty_ToString& operator=(const StringProperty_ToString&) = delete;
  public:
    StringProperty_ToString() : StringProperty_Member("toString", Type::String) {
    }
    virtual Value invoke(IExecution& execution, const String& string, ParameterChecker&) const override {
      return execution.makeValue(string);
    }
  };

  class StringProperties : public OrderedMap<String, std::unique_ptr<BuiltinFactory::StringProperty>> {
    StringProperties(const StringProperties&) = delete;
    StringProperties& operator=(const StringProperties&) = delete;
    typedef Type (*MemberBuilder)();
  public:
    StringProperties() {
      this->addMember<StringProperty_Length>();
      this->addMember<StringProperty_CompareTo>();
      this->addMember<StringProperty_Contains>();
      this->addMember<StringProperty_EndsWith>();
      this->addMember<StringProperty_Hash>();
      this->addMember<StringProperty_IndexOf>();
      this->addMember<StringProperty_Join>();
      this->addMember<StringProperty_LastIndexOf>();
      this->addMember<StringProperty_PadLeft>();
      this->addMember<StringProperty_PadRight>();
      this->addMember<StringProperty_Repeat>();
      this->addMember<StringProperty_Replace>();
      this->addMember<StringProperty_Slice>();
      this->addMember<StringProperty_StartsWith>();
      this->addMember<StringProperty_ToString>();
    };
    static const StringProperties& instance() {
      static const StringProperties result;
      return result;
    }
  private:
    template<typename T>
    void addMember() {
      auto property = std::make_unique<T>();
      auto name = property->getName();
      this->add(name, std::move(property));
    }
  };

  class StringShapeDotable : public IPropertySignature {
  public:
    virtual Type getType(const String& property) const override {
      auto* found = StringProperties::instance().get(property);
      return (found == nullptr) ? Type::Void : (*found)->getType();
    }
    virtual Modifiability getModifiability(const String& property) const override {
      auto* found = StringProperties::instance().get(property);
      return (found == nullptr) ? Modifiability::None : Modifiability::Read;
    }
    virtual String getName(size_t index) const override {
      String key;
      auto* found = StringProperties::instance().get(index, key);
      return (found == nullptr) ? String() : key;
    }
    virtual size_t getNameCount() const override {
      return StringProperties::instance().size();
    }
    virtual bool isClosed() const override {
      return true;
    }
  };
  const StringShapeDotable stringShapeDotable{};

  class StringShapeIndexable : public IIndexSignature {
  public:
    virtual Type getResultType() const override {
      return Type::String;
    }
    virtual Type getIndexType() const override {
      return Type::Int;
    }
    virtual Modifiability getModifiability() const override {
      return Modifiability::Read;
    }
  };
  const StringShapeIndexable stringShapeIndexable{};

  class StringShapeIterable : public IIteratorSignature {
  public:
    virtual Type getType() const override {
      return Type::String;
    }
  };
  const StringShapeIterable stringShapeIterable{};

  // An "omni" function looks like this: 'any?(...any?[])'
  class ObjectShapeCallable : public IFunctionSignature {
  private:
    class Parameter : public IFunctionSignatureParameter {
    public:
      virtual String getName() const override {
        return "params";
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
    Parameter params;
  public:
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
      return this->params;
    }
  };
  const ObjectShapeCallable objectShapeCallable{};

  class ObjectShapeDotable : public IPropertySignature {
  public:
    virtual Type getType(const String&) const override {
      return Type::AnyQ;
    }
    virtual Modifiability getModifiability(const String&) const override {
      return Modifiability::Read | Modifiability::Write | Modifiability::Mutate | Modifiability::Delete;
    }
    virtual String getName(size_t) const override {
      return String();
    }
    virtual size_t getNameCount() const override {
      return 0;
    }
    virtual bool isClosed() const override {
      return false;
    }
  };
  const ObjectShapeDotable objectShapeDotable{};

  class ObjectShapeIndexable : public IIndexSignature {
  public:
    virtual Type getResultType() const override {
      return Type::AnyQ;
    }
    virtual Type getIndexType() const override {
      return Type::AnyQ;
    }
    virtual Modifiability getModifiability() const override {
      return Modifiability::Read | Modifiability::Write | Modifiability::Mutate | Modifiability::Delete;
    }
  };
  const ObjectShapeIndexable objectShapeIndexable{};

  class ObjectShapeIterable : public IIteratorSignature {
  public:
    virtual Type getType() const override {
      return Type::AnyQ;
    }
  };
  const ObjectShapeIterable objectShapeIterable{};

  class BuiltinBase : public SoftReferenceCounted<IObject> {
    BuiltinBase(const BuiltinBase&) = delete;
    BuiltinBase& operator=(const BuiltinBase&) = delete;
  protected:
    TypeBuilder builder;
    SoftPtr<const IType> type;
    String name;
    SlotMap<String> properties;
    BuiltinBase(IAllocator& allocator, IBasket& basket, TypeBuilder&& builder, const String& name)
      : SoftReferenceCounted(allocator),
        builder(std::move(builder)),
        type(),
        name(name),
        properties() {
      // 'type' will be initialized by derived class constructors
      basket.take(*this);
    }
  public:
    BuiltinBase(IAllocator& allocator, IBasket& basket, const String& name, const String& description)
      : BuiltinBase(allocator, basket, TypeFactory::createTypeBuilder(allocator, name, description), name) {
    }
    BuiltinBase(IAllocator& allocator, IBasket& basket, const String& name, const String& description, const Type& rettype)
      : BuiltinBase(allocator, basket, TypeFactory::createFunctionBuilder(allocator, rettype, name, description), name) {
    }
    virtual void softVisit(const Visitor& visitor) const override {
      // We have taken ownership of 'builder' which has no soft links
      this->type.visit(visitor);
      this->properties.visit(visitor);
    }
    virtual Value call(IExecution& execution, const IParameters&) override {
      return this->unsupported(execution, "function calling with '()'");
    }
    virtual Value getProperty(IExecution& execution, const String& property) override {
      if (this->properties.empty()) {
        return this->unsupported(execution, "properties such as '", property, "'");
      }
      auto slot = this->properties.find(property);
      if (slot == nullptr) {
        return this->unsupported(execution, "property '", property, "'");
      }
      return slot->value(Value::Void);
    }
    virtual Value setProperty(IExecution& execution, const String& property, const Value&) override {
      auto value = this->getProperty(execution, property);
      if (value.hasFlowControl()) {
        return value;
      }
      return this->unsupported(execution, "modification of property '", property, "'");
    }
    virtual Value mutProperty(IExecution& execution, const String& property, Mutation, const Value&) override {
      auto value = this->getProperty(execution, property);
      if (value.hasFlowControl()) {
        return value;
      }
      return this->unsupported(execution, "modification of property '", property, "'");
    }
    virtual Value delProperty(IExecution& execution, const String& property) override {
      return this->unsupported(execution, "deletion of properties such as '", property, "'");
    }
    virtual Value refProperty(IExecution& execution, const String& property) override {
      return this->unsupported(execution, "properties such as '", property, "'");
    }
    virtual Value getIndex(IExecution& execution, const Value&) override {
      return this->unsupported(execution, "indexing with '[]'");
    }
    virtual Value setIndex(IExecution& execution, const Value&, const Value&) override {
      return this->unsupported(execution, "indexing with '[]'");
    }
    virtual Value mutIndex(IExecution& execution, const Value&, Mutation, const Value&) override {
      return this->unsupported(execution, "indexing with '[]'");
    }
    virtual Value delIndex(IExecution& execution, const Value&) override {
      return this->unsupported(execution, "indexing with '[]'");
    }
    virtual Value refIndex(IExecution& execution, const Value&) override {
      return this->unsupported(execution, "indexing with '[]'");
    }
    virtual Value getPointee(IExecution& execution) override {
      return this->unsupported(execution, "pointer dereferencing with '*'");
    }
    virtual Value setPointee(IExecution& execution, const Value&) override {
      return this->unsupported(execution, "pointer dereferencing with '*'");
    }
    virtual Value mutPointee(IExecution& execution, Mutation, const Value&) override {
      return this->unsupported(execution, "pointer dereferencing with '*'");
    }
    virtual Value iterate(IExecution& execution) override {
      return this->unsupported(execution, "iteration");
    }
    virtual void print(Printer& printer) const override {
      printer.write(this->name);
    }
    virtual Type getRuntimeType() const override {
      return Type(this->type.get());
    }
  protected:
    template<typename... ARGS>
    Value unsupported(IExecution& execution, ARGS&&... args) {
      // TODO i18n
      return execution.raiseFormat(this->type->describeValue(), " does not support ", std::forward<ARGS>(args)...);
    }
    template<typename T>
    Value makeValue(T value) const {
      return ValueFactory::create(this->allocator, value);
    }
    template<typename T>
    void addMember(const String& member) {
      assert(this->basket != nullptr);
      auto value = createBuiltinValueHard<T>(this->allocator, *this->basket);
      if (this->properties.empty()) {
        // We have members, but it's a closed set
        this->builder->defineDotable(Type::Void, Modifiability::None);
      }
      this->builder->addProperty(value->getRuntimeType(), member, Modifiability::Read);
      this->properties.add(this->allocator, *this->basket, member, value);
    }
    void build() {
      auto built = this->builder->build(this->basket);
      this->type.set(*this->basket, built.get());
    }
  };

  class Builtin_Assert final : public BuiltinBase {
    Builtin_Assert(const Builtin_Assert&) = delete;
    Builtin_Assert& operator=(const Builtin_Assert&) = delete;
  public:
    Builtin_Assert(IAllocator& allocator, IBasket& basket)
      : BuiltinBase(allocator, basket, "assert", "Built-in 'assert'", Type::Void) {
      this->builder->addPositionalParameter(Type::Any, "predicate", Bits::set(IFunctionSignatureParameter::Flags::Required, IFunctionSignatureParameter::Flags::Predicate));
      this->build();
    }
    virtual Value call(IExecution& execution, const IParameters& parameters) override {
      if (parameters.getNamedCount() > 0) {
        return execution.raiseFormat("Built-in function 'assert' does not accept named parameters");
      }
      if (parameters.getPositionalCount() != 1) {
        return execution.raiseFormat("Built-in function 'assert' accepts only 1 parameter, not ", parameters.getPositionalCount());
      }
      return execution.assertion(parameters.getPositional(0));
    }
  };

  class Builtin_Print final : public BuiltinBase {
    Builtin_Print(const Builtin_Print&) = delete;
    Builtin_Print& operator=(const Builtin_Print&) = delete;
  public:
    Builtin_Print(IAllocator& allocator, IBasket& basket)
      : BuiltinBase(allocator, basket, "print", "Built-in 'print'", Type::Void) {
      this->builder->addPositionalParameter(Type::AnyQ, "values", IFunctionSignatureParameter::Flags::Variadic);
      this->build();
    }
    virtual Value call(IExecution& execution, const IParameters& parameters) override {
      if (parameters.getNamedCount() > 0) {
        return execution.raiseFormat("Built-in function 'print' does not accept named parameters");
      }
      StringBuilder sb;
      auto n = parameters.getPositionalCount();
      for (size_t i = 0; i < n; ++i) {
        parameters.getPositional(i)->print(sb);
      }
      execution.print(sb.toUTF8());
      return Value::Void;
    }
  };

  class Builtin_StringFrom final : public BuiltinBase {
    Builtin_StringFrom(const Builtin_StringFrom&) = delete;
    Builtin_StringFrom& operator=(const Builtin_StringFrom&) = delete;
  public:
    Builtin_StringFrom(IAllocator& allocator, IBasket& basket)
      : BuiltinBase(allocator, basket, "string.from", "Built-in function 'string.from'", Type::String) {
      this->builder->addPositionalParameter(Type::AnyQ, "value", IFunctionSignatureParameter::Flags::Required);
      this->build();
      printf("--- WUBBLE: Builtin_StringFrom::Builtin_StringFrom()\n");
    }
    virtual ~Builtin_StringFrom() override {
      printf("--- WUBBLE: Builtin_StringFrom::~Builtin_StringFrom()\n");
    }
    virtual IObject* hardAcquire() const override {
      printf("--- WUBBLE: Builtin_StringFrom::hardAcquire()\n");
      return BuiltinBase::hardAcquire();
    }
    virtual void hardRelease() const override {
      printf("--- WUBBLE: Builtin_StringFrom::hardRelease()\n");
      BuiltinBase::hardRelease();
    }

    virtual Value call(IExecution& execution, const IParameters& parameters) override {
      if (parameters.getNamedCount() > 0) {
        return execution.raiseFormat("Built-in function 'string.from' does not accept named parameters");
      }
      if (parameters.getPositionalCount() != 1) {
        return execution.raiseFormat("Built-in function 'string.from' accepts only 1 parameter, not ", parameters.getPositionalCount());
      }
      auto parameter = parameters.getPositional(0);
      std::ostringstream oss;
      Printer printer{ oss, Print::Options::DEFAULT };
      parameter->print(printer);
      return ValueFactory::createString(this->allocator, oss.str());
    }
  };

  class Builtin_String final : public BuiltinBase {
    Builtin_String(const Builtin_String&) = delete;
    Builtin_String& operator=(const Builtin_String&) = delete;
  public:
    Builtin_String(IAllocator& allocator, IBasket& basket)
      : BuiltinBase(allocator, basket, "string", "Type 'string'", Type::String) {
      this->builder->addPositionalParameter(Type::AnyQ, "values", IFunctionSignatureParameter::Flags::Variadic);
      this->addMember<Builtin_StringFrom>("from");
      this->build();
    }
    virtual Value call(IExecution& execution, const IParameters& parameters) override {
      if (parameters.getNamedCount() > 0) {
        return execution.raiseFormat("Built-in function 'string' does not accept named parameters");
      }
      StringBuilder sb;
      auto n = parameters.getPositionalCount();
      for (size_t i = 0; i < n; ++i) {
        parameters.getPositional(i)->print(sb);
      }
      return ValueFactory::createString(allocator, sb.str());
    }
  };

  class Builtin_TypeOf final : public BuiltinBase {
    Builtin_TypeOf(const Builtin_TypeOf&) = delete;
    Builtin_TypeOf& operator=(const Builtin_TypeOf&) = delete;
  public:
    Builtin_TypeOf(IAllocator& allocator, IBasket& basket)
      : BuiltinBase(allocator, basket, "type.of", "Built-in function 'type.of'", Type::String) {
      this->builder->addPositionalParameter(Type::AnyQ, "value", IFunctionSignatureParameter::Flags::Required);
      this->build();
    }
    virtual Value call(IExecution& execution, const IParameters& parameters) override {
      if (parameters.getNamedCount() > 0) {
        return execution.raiseFormat("Built-in function 'type.of' does not accept named parameters");
      }
      if (parameters.getPositionalCount() != 1) {
        return execution.raiseFormat("Built-in function 'type.of' accepts only 1 parameter, not ", parameters.getPositionalCount());
      }
      auto parameter = parameters.getPositional(0);
      return ValueFactory::createString(this->allocator, parameter->getRuntimeType().toString());
    }
  };

  class Builtin_Type final : public BuiltinBase {
    Builtin_Type(const Builtin_Type&) = delete;
    Builtin_Type& operator=(const Builtin_Type&) = delete;
  public:
    Builtin_Type(IAllocator& allocator, IBasket& basket)
      : BuiltinBase(allocator, basket, "type", "Type 'type'") {
      this->addMember<Builtin_TypeOf>("of");
      this->build();
    }
  };
}

const egg::ovum::IParameters& egg::ovum::Object::ParametersNone{ parametersNone };
const egg::ovum::IFunctionSignature& egg::ovum::Object::OmniFunctionSignature{ objectShapeCallable };

const egg::ovum::ObjectShape& egg::ovum::BuiltinFactory::getStringShape() {
  static const ObjectShape instance{
    nullptr,
    &stringShapeDotable,
    &stringShapeIndexable,
    &stringShapeIterable
  };
  return instance;
}

const egg::ovum::ObjectShape& egg::ovum::BuiltinFactory::getObjectShape() {
  static const ObjectShape instance{
    &objectShapeCallable,
    &objectShapeDotable,
    &objectShapeIndexable,
    &objectShapeIterable
  };
  return instance;
}

const egg::ovum::BuiltinFactory::StringProperty* egg::ovum::BuiltinFactory::getStringPropertyByName(const String& property) {
  auto* entry = StringProperties::instance().get(property);
  return (entry == nullptr) ? nullptr : entry->get();
}

const egg::ovum::BuiltinFactory::StringProperty* egg::ovum::BuiltinFactory::getStringPropertyByIndex(size_t index) {
  String key;
  auto* entry = StringProperties::instance().get(index, key);
  return (entry == nullptr) ? nullptr : entry->get();
}

size_t egg::ovum::BuiltinFactory::getStringPropertyCount() {
  return StringProperties::instance().size();
}

egg::ovum::Value egg::ovum::BuiltinFactory::createAssertInstance(IAllocator& allocator, IBasket& basket) {
  return createBuiltinValueHard<Builtin_Assert>(allocator, basket);
}

egg::ovum::Value egg::ovum::BuiltinFactory::createPrintInstance(IAllocator& allocator, IBasket& basket) {
  return createBuiltinValueHard<Builtin_Print>(allocator, basket);
}

egg::ovum::Value egg::ovum::BuiltinFactory::createStringInstance(IAllocator& allocator, IBasket& basket) {
  return createBuiltinValueHard<Builtin_String>(allocator, basket);
}

egg::ovum::Value egg::ovum::BuiltinFactory::createTypeInstance(IAllocator& allocator, IBasket& basket) {
  return createBuiltinValueHard<Builtin_Type>(allocator, basket);
}
