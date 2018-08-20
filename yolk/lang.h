namespace egg::lang {
  // Forward declarations
  struct LocationSource;
  class ValueLegacy;
  class ValueLegacyReferenceCounted;

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

  class IParameters {
  public:
    virtual ~IParameters() {}
    virtual size_t getPositionalCount() const = 0;
    virtual ValueLegacy getPositional(size_t index) const = 0;
    virtual const LocationSource* getPositionalLocation(size_t index) const = 0; // May be null
    virtual size_t getNamedCount() const = 0;
    virtual egg::ovum::String getName(size_t index) const = 0;
    virtual ValueLegacy getNamed(const egg::ovum::String& name) const = 0;
    virtual const LocationSource* getNamedLocation(const egg::ovum::String& name) const = 0; // May be null
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
    virtual egg::ovum::String getName() const = 0; // May be empty
    virtual egg::ovum::ITypeRef getType() const = 0;
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
    virtual egg::ovum::String toString(Parts parts) const; // Calls buildStringDefault
    virtual egg::ovum::String getFunctionName() const = 0; // May be empty
    virtual egg::ovum::ITypeRef getReturnType() const = 0;
    virtual size_t getParameterCount() const = 0;
    virtual const IFunctionSignatureParameter& getParameter(size_t index) const = 0;
    virtual bool validateCall(egg::ovum::IExecution& execution, const IParameters& runtime, ValueLegacy& problem) const; // Calls validateCallDefault

    // Implementation
    void buildStringDefault(egg::ovum::StringBuilder& sb, Parts parts) const; // Default formats as expected
    bool validateCallDefault(egg::ovum::IExecution& execution, const IParameters& runtime, ValueLegacy& problem) const;
  };

  class IIndexSignature {
  public:
    virtual ~IIndexSignature() {}
    virtual egg::ovum::String toString() const; // Default formats as expected
    virtual egg::ovum::ITypeRef getResultType() const = 0;
    virtual egg::ovum::ITypeRef getIndexType() const = 0;
  };

  class Type : public egg::ovum::ITypeRef {
    Type() = delete;
  private:
    explicit Type(const egg::ovum::IType& rhs) : egg::ovum::ITypeRef(&rhs) {}
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

    static egg::ovum::ITypeRef makeBasal(egg::ovum::Basal basal);
    static egg::ovum::ITypeRef makeUnion(const egg::ovum::IType& lhs, const egg::ovum::IType& rhs);
  };

  struct LocationSource {
    egg::ovum::String file;
    size_t line;
    size_t column;

    inline LocationSource(const LocationSource& rhs)
      : file(rhs.file), line(rhs.line), column(rhs.column) {
    }
    inline LocationSource(const egg::ovum::String& file, size_t line, size_t column)
      : file(file), line(line), column(column) {
    }
    egg::ovum::String toSourceString() const;
  };

  struct LocationRuntime : public LocationSource {
    egg::ovum::String function;
    const LocationRuntime* parent;

    inline LocationRuntime(const LocationRuntime& rhs)
      : LocationSource(rhs), function(rhs.function), parent(rhs.parent) {
    }
    inline LocationRuntime(const LocationSource& source, const egg::ovum::String& function, const LocationRuntime* parent = nullptr)
      : LocationSource(source), function(function), parent(parent) {
    }
    egg::ovum::String toRuntimeString() const;
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
      egg::ovum::IObject* o;
      const egg::ovum::IMemory* s;
      const egg::ovum::IType* t;
      ValueLegacyReferenceCounted* v;
    };
    explicit ValueLegacy(Discriminator tag) : tag(tag) {}
    void copyInternals(const ValueLegacy& other);
    void moveInternals(ValueLegacy& other);
    void destroyInternals();
  public:
    ValueLegacy() : tag(Discriminator::Void) { this->v = nullptr; }
    ValueLegacy(const ValueLegacy& value);
    ValueLegacy(ValueLegacy&& value) noexcept;
    explicit ValueLegacy(std::nullptr_t) : tag(Discriminator::Null) { this->v = nullptr; }
    explicit ValueLegacy(bool value) : tag(Discriminator::Bool) { this->b = value; }
    explicit ValueLegacy(int64_t value) : tag(Discriminator::Int) { this->i = value; }
    explicit ValueLegacy(double value) : tag(Discriminator::Float) { this->f = value; }
    explicit ValueLegacy(const egg::ovum::String& value) : tag(Discriminator::String) { this->s = value.hardAcquire(); }
    explicit ValueLegacy(const egg::ovum::Object& value) : tag(Discriminator::Object) { this->o = value.hardAcquire(); }
    explicit ValueLegacy(const egg::ovum::IType& type) : tag(Discriminator::Type) { this->t = type.hardAcquire<egg::ovum::IType>(); }
    explicit ValueLegacy(const ValueLegacyReferenceCounted& vrc);
    ValueLegacy& operator=(const ValueLegacy& value);
    ValueLegacy& operator=(ValueLegacy&& value) noexcept;
    ~ValueLegacy();
    bool operator==(const ValueLegacy& other) const { return ValueLegacy::equals(*this, other); }
    bool operator!=(const ValueLegacy& other) const { return !ValueLegacy::equals(*this, other); }
    bool hasLegacy(Discriminator bits) const { return egg::ovum::Bits::hasAnySet(this->tag, bits); }
    bool hasBasal(egg::ovum::Basal bits) const { return egg::ovum::Bits::hasAnySet(this->tag, static_cast<Discriminator>(bits)); }
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
    egg::ovum::String getString() const { assert(this->hasString()); return egg::ovum::String(this->s); }
    bool hasObject() const { return egg::ovum::Bits::hasAnySet(this->tag, Discriminator::Object); }
    egg::ovum::Object getObject() const { assert(this->hasObject()); assert(this->o != nullptr); return egg::ovum::Object(*this->o); }
    bool hasType() const { return egg::ovum::Bits::hasAnySet(this->tag, Discriminator::Type); }
    const egg::ovum::IType& getType() const { assert(this->hasType()); return *this->t; }
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
    static std::string getBasalString(egg::ovum::Basal basal);
    static bool equals(const ValueLegacy& lhs, const ValueLegacy& rhs);
    egg::ovum::String toString() const;
    std::string toUTF8() const;
    egg::ovum::ITypeRef getRuntimeType() const;

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
      return ValueLegacy(egg::ovum::Object(*allocator.make<T>(std::forward<ARGS>(args)...)));
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
inline void egg::ovum::IPreparation::raiseWarning(ARGS... args) {
  auto message = egg::ovum::StringBuilder::concat(args...);
  this->raise(ILogger::Severity::Warning, message);
}

template<typename... ARGS>
inline void egg::ovum::IPreparation::raiseError(ARGS... args) {
  auto message = egg::ovum::StringBuilder::concat(args...);
  this->raise(ILogger::Severity::Error, message);
}

template<typename... ARGS>
inline egg::lang::ValueLegacy egg::ovum::IExecution::raiseFormat(ARGS... args) {
  auto message = egg::ovum::StringBuilder::concat(args...);
  return this->raise(message);
}
