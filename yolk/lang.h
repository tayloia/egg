namespace egg::lang {
  using String = egg::ovum::String; // TODO remove
  using LogSource = egg::ovum::ILogger::Source;
  using LogSeverity = egg::ovum::ILogger::Severity;
  using Basal = egg::ovum::Basal;

  class ValueLegacy;
  class ValueLegacyReferenceCounted;
  struct LocationSource;

  // WIBBLE
  using Value = ValueLegacy;
  using ValueReferenceCounted = ValueLegacyReferenceCounted;

  using ITypeRef = egg::ovum::HardPtr<const class IType>;

#define EGG_VM_BASAL_ENUM(name, value) name = 1 << value,
  enum class Discriminator {
    Inferred = -1,
    None = 0,
    EGG_VM_BASAL(EGG_VM_BASAL_ENUM)
    Arithmetic = Int | Float,
    Any = Bool | Int | Float | String | Object,
    Indirect = 1 << 8,
    Pointer = 1 << 9,
    Break = 1 << 10,
    Continue = 1 << 11,
    Return = 1 << 12,
    Yield = 1 << 13,
    Exception = 1 << 14,
    FlowControl = Break | Continue | Return | Yield | Exception,
    Raw = 1 << 15
  };
#undef EGG_VM_BASAL_ENUM
  inline Discriminator operator|(Discriminator lhs, Discriminator rhs) {
    return egg::ovum::Bits::set(lhs, rhs);
  }

  class IPreparation {
  public:
    virtual ~IPreparation() {}
    virtual void raise(egg::lang::LogSeverity severity, const String& message) = 0;

    // Useful helpers
    template<typename... ARGS>
    void raiseWarning(ARGS... args);
    template<typename... ARGS>
    void raiseError(ARGS... args);
  };

  class IExecution {
  public:
    virtual ~IExecution() {}
    virtual egg::ovum::IAllocator& getAllocator() const = 0;
    virtual ValueLegacy raise(const String& message) = 0;
    virtual ValueLegacy assertion(const ValueLegacy& predicate) = 0;
    virtual void print(const std::string& utf8) = 0;

    // Useful helper
    template<typename... ARGS>
    ValueLegacy raiseFormat(ARGS... args);
  };

  class IParameters {
  public:
    virtual ~IParameters() {}
    virtual size_t getPositionalCount() const = 0;
    virtual ValueLegacy getPositional(size_t index) const = 0;
    virtual const LocationSource* getPositionalLocation(size_t index) const = 0; // May be null
    virtual size_t getNamedCount() const = 0;
    virtual String getName(size_t index) const = 0;
    virtual ValueLegacy getNamed(const String& name) const = 0;
    virtual const LocationSource* getNamedLocation(const String& name) const = 0; // May be null
  };

  class IFunctionSignatureParameter {
  public:
    enum class Flags {
      None = 0x00,
      Required = 0x01, // Not optional
      Variadic = 0x02, // Zero/one or more repetitions
      Predicate = 0x04 // Used in assertions
    };
    virtual ~IFunctionSignatureParameter() {}
    virtual String getName() const = 0; // May be empty
    virtual ITypeRef getType() const = 0;
    virtual size_t getPosition() const = 0; // SIZE_MAX if not positional
    virtual Flags getFlags() const = 0;

    // Flag helpers
    bool isRequired() const { return egg::ovum::Bits::hasAnySet(this->getFlags(), Flags::Required); }
    bool isVariadic() const { return egg::ovum::Bits::hasAnySet(this->getFlags(), Flags::Variadic); }
    bool isPredicate() const { return egg::ovum::Bits::hasAnySet(this->getFlags(), Flags::Predicate); }
  };

  class IFunctionSignature {
  public:
    enum class Parts {
      ReturnType = 0x01,
      FunctionName = 0x02,
      ParameterList = 0x04,
      ParameterNames = 0x08,
      NoNames = ReturnType | ParameterList,
      All = ~0
    };
    virtual ~IFunctionSignature() {}
    virtual String toString(Parts parts) const; // Calls buildStringDefault
    virtual String getFunctionName() const = 0; // May be empty
    virtual ITypeRef getReturnType() const = 0;
    virtual size_t getParameterCount() const = 0;
    virtual const IFunctionSignatureParameter& getParameter(size_t index) const = 0;
    virtual bool validateCall(IExecution& execution, const IParameters& runtime, Value& problem) const; // Calls validateCallDefault

    // Implementation
    void buildStringDefault(egg::ovum::StringBuilder& sb, Parts parts) const; // Default formats as expected
    bool validateCallDefault(IExecution& execution, const IParameters& runtime, Value& problem) const;
  };

  class IIndexSignature {
  public:
    virtual ~IIndexSignature() {}
    virtual String toString() const; // Default formats as expected
    virtual ITypeRef getResultType() const = 0;
    virtual ITypeRef getIndexType() const = 0;
  };

  class IType : public egg::ovum::IHardAcquireRelease {
  public:
    enum class AssignmentSuccess { Never, Sometimes, Always };
    virtual std::pair<std::string, int> toStringPrecedence() const = 0;
    virtual AssignmentSuccess canBeAssignedFrom(const IType& rhs) const = 0;

    virtual ValueLegacy promoteAssignment(IExecution& execution, const ValueLegacy& rhs) const; // Default implementation calls IType::canBeAssignedFrom()
    virtual const IFunctionSignature* callable() const; // Default implementation returns nullptr
    virtual const IIndexSignature* indexable() const; // Default implementation returns nullptr
    virtual bool dotable(const String* property, ITypeRef& type, String& reason) const; // Default implementation returns false
    virtual bool iterable(ITypeRef& type) const; // Default implementation returns false
    virtual Basal getBasalTypes() const; // Default implementation returns 'Object'
    virtual ITypeRef pointerType() const; // Default implementation returns 'Type*'
    virtual ITypeRef pointeeType() const; // Default implementation returns 'Void'
    virtual ITypeRef denulledType() const; // Default implementation returns 'Void'
    virtual ITypeRef unionWith(const IType& other) const; // Default implementation calls Type::makeUnion()

    // Helpers
    String toString(int precedence = -1) const;
    bool hasBasalType(Basal basal) const {
      return egg::ovum::Bits::hasAnySet(this->getBasalTypes(), basal);
    }
  };

  class Type : public ITypeRef {
    Type() = delete;
  private:
    explicit Type(const IType& rhs) : ITypeRef(&rhs) {}
  public:
    static const Type Void;
    static const Type Null;
    static const Type Bool;
    static const Type Int;
    static const Type Float;
    static const Type String;
    static const Type Arithmetic;
    static const Type Any;
    static const Type AnyQ;
    static const Type Type_; // Underscore needed to avoid name clash

    static ITypeRef makeBasal(Basal basal);
    static ITypeRef makeUnion(const IType& lhs, const IType& rhs);
  };

  class IObject : public egg::ovum::ICollectable {
  public:
    virtual ValueLegacy toString() const = 0;
    virtual ITypeRef getRuntimeType() const = 0;
    virtual ValueLegacy call(IExecution& execution, const IParameters& parameters) = 0;
    virtual ValueLegacy getProperty(IExecution& execution, const String& property) = 0;
    virtual ValueLegacy setProperty(IExecution& execution, const String& property, const ValueLegacy& value) = 0;
    virtual ValueLegacy getIndex(IExecution& execution, const ValueLegacy& index) = 0;
    virtual ValueLegacy setIndex(IExecution& execution, const ValueLegacy& index, const ValueLegacy& value) = 0;
    virtual ValueLegacy iterate(IExecution& execution) = 0;
  };

  struct LocationSource {
    String file;
    size_t line;
    size_t column;

    inline LocationSource(const LocationSource& rhs)
      : file(rhs.file), line(rhs.line), column(rhs.column) {
    }
    inline LocationSource(const String& file, size_t line, size_t column)
      : file(file), line(line), column(column) {
    }
    String toSourceString() const;
  };

  struct LocationRuntime : public LocationSource {
    String function;
    const LocationRuntime* parent;

    inline LocationRuntime(const LocationRuntime& rhs)
      : LocationSource(rhs), function(rhs.function), parent(rhs.parent) {
    }
    inline LocationRuntime(const LocationSource& source, const String& function, const LocationRuntime* parent = nullptr)
      : LocationSource(source), function(function), parent(parent) {
    }
    String toRuntimeString() const;
  };

  class ValueLegacy {
    // Stop type promotion for implicit constructors
    template<typename T> ValueLegacy(T rhs) = delete;
  private:
    Discriminator tag;
    union {
      bool b;
      int64_t i;
      double f;
      IObject* o;
      const egg::ovum::IMemory* s;
      const IType* t;
      ValueReferenceCounted* v;
    };
    explicit ValueLegacy(Discriminator tag) : tag(tag) {}
    void copyInternals(const Value& other);
    void moveInternals(Value& other);
    void destroyInternals();
  public:
    ValueLegacy() : tag(Discriminator::Void) { this->v = nullptr; }
    ValueLegacy(const ValueLegacy& value);
    ValueLegacy(ValueLegacy&& value) noexcept;
    explicit ValueLegacy(std::nullptr_t) : tag(Discriminator::Null) { this->v = nullptr; }
    explicit ValueLegacy(bool value) : tag(Discriminator::Bool) { this->b = value; }
    explicit ValueLegacy(int64_t value) : tag(Discriminator::Int) { this->i = value; }
    explicit ValueLegacy(double value) : tag(Discriminator::Float) { this->f = value; }
    explicit ValueLegacy(const String& value) : tag(Discriminator::String) { this->s = value.hardAcquire(); }
    explicit ValueLegacy(const egg::ovum::HardPtr<IObject>& value) : tag(Discriminator::Object) { this->o = value.hardAcquire(); }
    explicit ValueLegacy(const IType& type) : tag(Discriminator::Type) { this->t = type.hardAcquire<IType>(); }
    explicit ValueLegacy(const ValueLegacyReferenceCounted& vrc);
    ValueLegacy& operator=(const ValueLegacy& value);
    ValueLegacy& operator=(ValueLegacy&& value) noexcept;
    ~ValueLegacy();
    bool operator==(const ValueLegacy& other) const { return ValueLegacy::equals(*this, other); }
    bool operator!=(const ValueLegacy& other) const { return !ValueLegacy::equals(*this, other); }
    bool hasLegacy(Discriminator bits) const { return egg::ovum::Bits::hasAnySet(this->tag, bits); }
    bool hasBasal(Basal bits) const { return egg::ovum::Bits::hasAnySet(this->tag, static_cast<Discriminator>(bits)); }
    bool hasArithmetic() const { return egg::ovum::Bits::hasAnySet(this->tag, Discriminator::Arithmetic); }
    bool isVoid() const { return this->tag == Discriminator::Void; }
    bool isBreak() const { return this->tag == Discriminator::Break; }
    bool isContinue() const { return this->tag == Discriminator::Continue; }
    const ValueLegacy& direct() const;
    ValueLegacy& direct();
    bool hasIndirect() const { return egg::ovum::Bits::hasAnySet(this->tag, Discriminator::Indirect); }
    ValueLegacyReferenceCounted& indirect(egg::ovum::IAllocator& allocator);
    ValueLegacy& soft(egg::ovum::ICollectable& container);
    bool isLegacy(Discriminator bits) const { return this->tag == bits; }
    bool hasNull() const { return egg::ovum::Bits::hasAnySet(this->tag, Discriminator::Null); }
    bool isNull() const { return this->tag == Discriminator::Null; }
    bool hasBool() const { return egg::ovum::Bits::hasAnySet(this->tag, Discriminator::Bool); }
    bool isBool() const { return this->tag == Discriminator::Bool; }
    bool getBool() const { assert(this->hasBool()); return this->b; }
    bool hasInt() const { return egg::ovum::Bits::hasAnySet(this->tag, Discriminator::Int); }
    bool isInt() const { return this->tag == Discriminator::Int; }
    int64_t getInt() const { assert(this->hasInt()); return this->i; }
    bool hasFloat() const { return egg::ovum::Bits::hasAnySet(this->tag, Discriminator::Float); }
    bool isFloat() const { return this->tag == Discriminator::Float; }
    double getFloat() const { assert(this->hasFloat()); return this->f; }
    bool hasString() const { return egg::ovum::Bits::hasAnySet(this->tag, Discriminator::String); }
    bool isString() const { return this->tag == Discriminator::String; }
    String getString() const { assert(this->hasString()); return String(this->s); }
    bool hasObject() const { return egg::ovum::Bits::hasAnySet(this->tag, Discriminator::Object); }
    egg::ovum::HardPtr<IObject> getObject() const { assert(this->hasObject()); assert(this->o != nullptr); return egg::ovum::HardPtr<IObject>(this->o); }
    bool hasType() const { return egg::ovum::Bits::hasAnySet(this->tag, Discriminator::Type); }
    const IType& getType() const { assert(this->hasType()); return *this->t; }
    bool hasPointer() const { return egg::ovum::Bits::hasAnySet(this->tag, Discriminator::Pointer); }
    ValueLegacyReferenceCounted& getPointee() const { assert(this->hasPointer() && !this->hasObject()); return *this->v; }
    bool hasAny() const { return egg::ovum::Bits::hasAnySet(this->tag, Discriminator::Any); }
    bool isSoftReference() const { return this->tag == (Discriminator::Object | Discriminator::Pointer); }
    void softVisitLink(const egg::ovum::ICollectable::Visitor& visitor) const;
    bool hasException() const { return egg::ovum::Bits::hasAnySet(this->tag, Discriminator::Exception); }
    bool isRethrow() const { return this->tag == (Discriminator::Exception | Discriminator::Void); }
    bool hasFlowControl() const { return egg::ovum::Bits::hasAnySet(this->tag, Discriminator::FlowControl); }
    void addFlowControl(Discriminator bits);
    bool stripFlowControl(Discriminator bits);
    std::string getDiscriminatorString() const;
    static std::string getDiscriminatorString(Discriminator tag);
    static std::string getBasalString(Basal basal);
    static bool equals(const ValueLegacy& lhs, const ValueLegacy& rhs);
    String toString() const;
    std::string toUTF8() const;
    ITypeRef getRuntimeType() const;

    // Constants
    static const ValueLegacy Void;
    static const ValueLegacy Null;
    static const ValueLegacy False;
    static const ValueLegacy True;
    static const ValueLegacy Break;
    static const ValueLegacy EmptyString;
    static const ValueLegacy Continue;
    static const ValueLegacy Rethrow;
    static const ValueLegacy ReturnVoid;

    // Built-ins
    static ValueLegacy builtinString(egg::ovum::IAllocator& allocator);
    static ValueLegacy builtinType(egg::ovum::IAllocator& allocator);
    static ValueLegacy builtinAssert(egg::ovum::IAllocator& allocator);
    static ValueLegacy builtinPrint(egg::ovum::IAllocator& allocator);

    // Factories
    template<typename T, typename... ARGS>
    static ValueLegacy makeObject(egg::ovum::IAllocator& allocator, ARGS&&... args) {
      // Use perfect forwarding
      return ValueLegacy(egg::ovum::HardPtr<IObject>(allocator.make<T>(std::forward<ARGS>(args)...)));
    }
  };

  class ValueLegacyReferenceCounted : public egg::ovum::IHardAcquireRelease, public ValueLegacy {
    ValueLegacyReferenceCounted(const ValueLegacyReferenceCounted&) = delete;
    ValueLegacyReferenceCounted& operator=(const ValueLegacyReferenceCounted&) = delete;
  protected:
    explicit ValueLegacyReferenceCounted(ValueLegacy&& value) noexcept : ValueLegacy(std::move(value)) {}
  };
}

template<typename... ARGS>
inline void egg::lang::IPreparation::raiseWarning(ARGS... args) {
  auto message = egg::ovum::StringBuilder::concat(args...);
  this->raise(egg::lang::LogSeverity::Warning, message);
}

template<typename... ARGS>
inline void egg::lang::IPreparation::raiseError(ARGS... args) {
  auto message = egg::ovum::StringBuilder::concat(args...);
  this->raise(egg::lang::LogSeverity::Error, message);
}

template<typename... ARGS>
inline egg::lang::ValueLegacy egg::lang::IExecution::raiseFormat(ARGS... args) {
  auto message = egg::ovum::StringBuilder::concat(args...);
  return this->raise(message);
}
