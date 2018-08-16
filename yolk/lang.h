namespace egg::lang {
  class String;
  class StringBuilder;
  class Value;
  class ValueReferenceCounted;
  struct LocationSource;

  using ITypeRef = egg::ovum::HardPtr<const class IType>;

  class Bits {
  public:
    template<typename T>
    static bool hasAllSet(T value, T bits) {
      auto a = static_cast<std::underlying_type_t<T>>(value);
      auto b = static_cast<std::underlying_type_t<T>>(bits);
      return (a & b) == b;
    }
    template<typename T>
    static bool hasAnySet(T value, T bits) {
      auto a = static_cast<std::underlying_type_t<T>>(value);
      auto b = static_cast<std::underlying_type_t<T>>(bits);
      return (a & b) != 0;
    }
    template<typename T>
    static T mask(T value, T bits) {
      auto a = static_cast<std::underlying_type_t<T>>(value);
      auto b = static_cast<std::underlying_type_t<T>>(bits);
      return static_cast<T>(a & b);
    }
    template<typename T>
    static T set(T value, T bits) {
      auto a = static_cast<std::underlying_type_t<T>>(value);
      auto b = static_cast<std::underlying_type_t<T>>(bits);
      return static_cast<T>(a | b);
    }
    template<typename T>
    static T clear(T value, T bits) {
      auto a = static_cast<std::underlying_type_t<T>>(value);
      auto b = static_cast<std::underlying_type_t<T>>(bits);
      return static_cast<T>(a & ~b);
    }
    template<typename T>
    static T invert(T value, T bits) {
      auto a = static_cast<std::underlying_type_t<T>>(value);
      auto b = static_cast<std::underlying_type_t<T>>(bits);
      return static_cast<T>(a ^ b);
    }
  };

  enum class LogSource {
    Compiler = 1 << 0,
    Runtime = 1 << 1,
    User = 1 << 2
  };

  enum class LogSeverity {
    None = 0,
    Debug = 1 << 0,
    Verbose = 1 << 1,
    Information = 1 << 2,
    Warning = 1 << 3,
    Error = 1 << 4
  };

  enum class Discriminator {
    Inferred = -1,
    None = 0,
    Void = 1 << 0,
    Null = 1 << 1,
    Bool = 1 << 2,
    Int = 1 << 3,
    Float = 1 << 4,
    String = 1 << 5,
    Object = 1 << 6,
    Type = 1 << 7,
    Any = Bool | Int | Float | String | Object,
    Arithmetic = Int | Float,
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
  inline Discriminator operator|(Discriminator lhs, Discriminator rhs) {
    return Bits::set(lhs, rhs);
  }

  struct StringIteration {
    char32_t codepoint;
    size_t internal; // For use by implementations only
  };

  class IString : public egg::ovum::IHardAcquireRelease {
  public:
    virtual size_t length() const = 0;
    virtual bool empty() const = 0;
    virtual bool equal(const IString& other) const = 0;
    virtual bool less(const IString& other) const = 0;
    virtual int64_t hashCode() const = 0;
    virtual int64_t compare(const IString& other) const = 0;
    virtual bool startsWith(const IString& other) const = 0;
    virtual bool endsWith(const IString& other) const = 0;
    virtual int32_t codePointAt(size_t index) const = 0;
    virtual int64_t indexOfCodePoint(char32_t codepoint, size_t fromIndex) const = 0;
    virtual int64_t indexOfString(const IString& needle, size_t fromIndex) const = 0;
    virtual int64_t lastIndexOfCodePoint(char32_t codepoint, size_t fromIndex) const = 0;
    virtual int64_t lastIndexOfString(const IString& needle, size_t fromIndex) const = 0;
    virtual const IString* substring(size_t begin, size_t end) const = 0;
    virtual const IString* repeat(size_t count) const = 0;
    virtual const IString* replace(const IString& needle, const IString& replacement, int64_t occurrences) const = 0;
    virtual std::string toUTF8() const = 0;

    // Iteration
    virtual bool iterateFirst(StringIteration& iteration) const = 0;
    virtual bool iterateNext(StringIteration& iteration) const = 0;
    virtual bool iteratePrevious(StringIteration& iteration) const = 0;
    virtual bool iterateLast(StringIteration& iteration) const = 0;
  };
  using IStringRef = egg::ovum::HardPtr<const IString>;

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
    virtual Value raise(const String& message) = 0;
    virtual Value assertion(const Value& predicate) = 0;
    virtual void print(const std::string& utf8) = 0;

    // Useful helper
    template<typename... ARGS>
    Value raiseFormat(ARGS... args);
  };

  class IParameters {
  public:
    virtual ~IParameters() {}
    virtual size_t getPositionalCount() const = 0;
    virtual Value getPositional(size_t index) const = 0;
    virtual const LocationSource* getPositionalLocation(size_t index) const = 0; // May be null
    virtual size_t getNamedCount() const = 0;
    virtual String getName(size_t index) const = 0;
    virtual Value getNamed(const String& name) const = 0;
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
    bool isRequired() const { return Bits::hasAnySet(this->getFlags(), Flags::Required); }
    bool isVariadic() const { return Bits::hasAnySet(this->getFlags(), Flags::Variadic); }
    bool isPredicate() const { return Bits::hasAnySet(this->getFlags(), Flags::Predicate); }
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
    void buildStringDefault(StringBuilder& sb, Parts parts) const; // Default formats as expected
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

    virtual Value promoteAssignment(IExecution& execution, const Value& rhs) const; // Default implementation calls IType::canBeAssignedFrom()
    virtual const IFunctionSignature* callable() const; // Default implementation returns nullptr
    virtual const IIndexSignature* indexable() const; // Default implementation returns nullptr
    virtual bool dotable(const String* property, ITypeRef& type, String& reason) const; // Default implementation returns false
    virtual bool iterable(ITypeRef& type) const; // Default implementation returns false
    virtual Discriminator getSimpleTypes() const; // Default implementation returns 'Object'
    virtual ITypeRef pointerType() const; // Default implementation returns 'Type*'
    virtual ITypeRef pointeeType() const; // Default implementation returns 'Void'
    virtual ITypeRef denulledType() const; // Default implementation returns 'Void'
    virtual ITypeRef unionWith(const IType& other) const; // Default implementation calls Type::makeUnion()

    // Helpers
    String toString(int precedence = -1) const;
    bool hasNativeType(Discriminator native) const {
      return egg::lang::Bits::hasAnySet(this->getSimpleTypes(), native);
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

    static const egg::lang::IType* getNative(egg::lang::Discriminator tag);
    static ITypeRef makeSimple(Discriminator simple);
    static ITypeRef makeUnion(const IType& lhs, const IType& rhs);
  };

  class IObject : public egg::ovum::ICollectable {
  public:
    virtual Value toString() const = 0;
    virtual ITypeRef getRuntimeType() const = 0;
    virtual Value call(IExecution& execution, const IParameters& parameters) = 0;
    virtual Value getProperty(IExecution& execution, const String& property) = 0;
    virtual Value setProperty(IExecution& execution, const String& property, const Value& value) = 0;
    virtual Value getIndex(IExecution& execution, const Value& index) = 0;
    virtual Value setIndex(IExecution& execution, const Value& index, const Value& value) = 0;
    virtual Value iterate(IExecution& execution) = 0;
  };

  class StringBuilder {
    StringBuilder(const StringBuilder&) = delete;
    StringBuilder& operator=(const StringBuilder&) = delete;
  private:
    std::stringstream ss;
  public:
    StringBuilder() {
    }
    template<typename T>
    StringBuilder& add(T value) {
      this->ss << value;
      return *this;
    }
    template<typename T, typename... ARGS>
    StringBuilder& add(T value, ARGS... args) {
      return this->add(value).add(args...);
    }
    bool empty() const {
      return this->ss.rdbuf()->in_avail() == 0;
    }
    std::string toUTF8() const {
      return this->ss.str();
    }
    String str() const;
  };

  class String : public IStringRef {
  private:
    static const IString& emptyBuffer;
  public:
    explicit String(const IString& rhs = emptyBuffer) : IStringRef(&rhs) {
    }
    String(const String& rhs) : IStringRef(rhs.get()) {
    }
    // Properties
    size_t length() const {
      return this->get()->length();
    }
    bool empty() const {
      return this->get()->empty();
    }
    int64_t hashCode() const {
      return this->get()->hashCode();
    }
    int32_t codePointAt(size_t index) const {
      return this->get()->codePointAt(index);
    }
    std::string toUTF8() const {
      return this->get()->toUTF8();
    }
    bool equal(const String& rhs) const {
      return this->get()->equal(*rhs);
    }
    bool less(const String& rhs) const {
      return this->get()->less(*rhs);
    }
    int64_t compare(const String& rhs) const {
      return this->get()->compare(*rhs);
    }
    bool contains(const String& needle) const {
      return this->get()->indexOfString(*needle, 0) >= 0;
    }
    bool startsWith(const String& needle) const {
      return this->get()->startsWith(*needle);
    }
    bool endsWith(const String& needle) const {
      return this->get()->endsWith(*needle);
    }
    int64_t indexOfCodePoint(char32_t needle, size_t fromIndex = 0) const {
      return this->get()->indexOfCodePoint(needle, fromIndex);
    }
    int64_t indexOfString(const String& needle, size_t fromIndex = 0) const {
      return this->get()->indexOfString(*needle, fromIndex);
    }
    int64_t lastIndexOfCodePoint(char32_t needle, size_t fromIndex = SIZE_MAX) const {
      return this->get()->lastIndexOfCodePoint(needle, fromIndex);
    }
    int64_t lastIndexOfString(const String& needle, size_t fromIndex = SIZE_MAX) const {
      return this->get()->lastIndexOfString(*needle, fromIndex);
    }
    String substring(size_t begin, size_t end = SIZE_MAX) const {
      return String(*this->get()->substring(begin, end));
    }
    String slice(int64_t begin, int64_t end = INT64_MAX) const {
      return String(*this->get()->substring(this->signedToIndex(begin), this->signedToIndex(end)));
    }
    String repeat(size_t count) const {
      return String(*this->get()->repeat(count));
    }
    String replace(const String& needle, const String& replacement, int64_t occurrences = INT64_MAX) const {
      return String(*this->get()->replace(*needle, *replacement, occurrences));
    }
    String padLeft(size_t target, const String& padding = String::Space) const;
    String padRight(size_t target, const String& padding = String::Space) const;
    std::vector<String> split(const String& separator, int64_t limit = INT64_MAX) const;
    // Helpers
    size_t signedToIndex(int64_t index) const {
      // Convert a signed index to an absolute one
      // Negative inputs signify offsets from the end of the string
      auto n = this->length();
      if (index < 0) {
        return size_t(std::max(index + int64_t(n), int64_t(0)));
      }
      return std::min(size_t(index), n);
    }
    // Built-ins
    using BuiltinFactory = std::function<Value(egg::ovum::IAllocator&, const String&, const std::string&)>;
    static BuiltinFactory builtinFactory(const String& property);
    Value builtin(IExecution& execution, const String& property) const;
    // Operators
    String& operator=(const String& rhs) {
      this->set(rhs.get());
      return *this;
    }
    bool operator==(const String& rhs) const {
      return this->get()->equal(*rhs);
    }
    bool operator<(const String& rhs) const {
      return this->get()->less(*rhs);
    }
    // Factories
    static String fromCodePoint(char32_t codepoint);
    static String fromUTF8(const std::string& utf8);
    static String fromUTF32(const std::u32string& utf32);
    template<typename... ARGS>
    static String concat(ARGS... args) {
      return StringBuilder().add(args...).str();
    }
    // Constants
    static const String Empty;
    static const String Space;
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

  class Value {
  private:
    Discriminator tag;
    union {
      bool b;
      int64_t i;
      double f;
      IObject* o;
      const IString* s;
      const IType* t;
      ValueReferenceCounted* v;
    };
    explicit Value(Discriminator tag) : tag(tag) {}
    void copyInternals(const Value& other);
    void moveInternals(Value& other);
    void destroyInternals();
  public:
    Value() : tag(Discriminator::Void) { this->v = nullptr; }
    Value(const Value& value);
    Value(Value&& value) noexcept;
    explicit Value(std::nullptr_t) : tag(Discriminator::Null) { this->v = nullptr; }
    explicit Value(bool value) : tag(Discriminator::Bool) { this->b = value; }
    explicit Value(int64_t value) : tag(Discriminator::Int) { this->i = value; }
    explicit Value(double value) : tag(Discriminator::Float) { this->f = value; }
    explicit Value(const String& value) : tag(Discriminator::String) { this->s = value.hardAcquire(); }
    explicit Value(const egg::ovum::HardPtr<IObject>& value) : tag(Discriminator::Object) { this->o = value.hardAcquire(); }
    explicit Value(const IType& type) : tag(Discriminator::Type) { this->t = type.hardAcquire<IType>(); }
    explicit Value(const ValueReferenceCounted& vrc);
    Value& operator=(const Value& value);
    Value& operator=(Value&& value) noexcept;
    ~Value();
    bool operator==(const Value& other) const { return Value::equal(*this, other); }
    bool operator!=(const Value& other) const { return !Value::equal(*this, other); }
    const Value& direct() const;
    Value& direct();
    ValueReferenceCounted& indirect(egg::ovum::IAllocator& allocator);
    Value& soft(egg::ovum::IAllocator& allocator, egg::ovum::ICollectable& container);
    bool is(Discriminator bits) const { return this->tag == bits; }
    bool has(Discriminator bits) const { return Bits::hasAnySet(this->tag, bits); }
    bool getBool() const { assert(this->has(Discriminator::Bool)); return this->b; }
    int64_t getInt() const { assert(this->has(Discriminator::Int)); return this->i; }
    double getFloat() const { assert(this->has(Discriminator::Float)); return this->f; }
    String getString() const { assert(this->has(Discriminator::String)); return String(*this->s); }
    egg::ovum::HardPtr<IObject> getObject() const { assert(this->has(Discriminator::Object)); assert(this->o != nullptr); return egg::ovum::HardPtr<IObject>(this->o); }
    const IType& getType() const { assert(this->has(Discriminator::Type)); return *this->t; }
    ValueReferenceCounted& getPointee() const { assert(this->has(Discriminator::Pointer) && !this->has(Discriminator::Object)); return *this->v; }
    void addFlowControl(Discriminator bits);
    bool stripFlowControl(Discriminator bits);
    std::string getTagString() const;
    static std::string getTagString(Discriminator tag);
    static bool equal(const Value& lhs, const Value& rhs);
    String toString() const;
    std::string toUTF8() const;
    ITypeRef getRuntimeType() const;

    // Constants
    static const Value Void;
    static const Value Null;
    static const Value False;
    static const Value True;
    static const Value Break;
    static const Value EmptyString;
    static const Value Continue;
    static const Value Rethrow;
    static const Value ReturnVoid;

    // Built-ins
    static Value builtinString(egg::ovum::IAllocator& allocator);
    static Value builtinType(egg::ovum::IAllocator& allocator);
    static Value builtinAssert(egg::ovum::IAllocator& allocator);
    static Value builtinPrint(egg::ovum::IAllocator& allocator);
  };

  class ValueReferenceCounted : public egg::ovum::IHardAcquireRelease, public Value {
    ValueReferenceCounted(const ValueReferenceCounted&) = delete;
    ValueReferenceCounted& operator=(const ValueReferenceCounted&) = delete;
  protected:
    explicit ValueReferenceCounted(Value&& value) noexcept : Value(std::move(value)) {}
  };
}

namespace std {
  template<> struct hash<egg::lang::String> {
    // String hash specialization for use with std::unordered_map<> etc.
    size_t operator()(const egg::lang::String& s) const {
      return size_t(s.hashCode());
    }
  };
}

std::ostream& operator<<(std::ostream& os, const egg::lang::String& text);

template<typename... ARGS>
inline void egg::lang::IPreparation::raiseWarning(ARGS... args) {
  auto message = StringBuilder().add(args...).str();
  this->raise(egg::lang::LogSeverity::Warning, message);
}

template<typename... ARGS>
inline void egg::lang::IPreparation::raiseError(ARGS... args) {
  auto message = StringBuilder().add(args...).str();
  this->raise(egg::lang::LogSeverity::Error, message);
}

template<typename... ARGS>
inline egg::lang::Value egg::lang::IExecution::raiseFormat(ARGS... args) {
  auto message = StringBuilder().add(args...).str();
  return this->raise(message);
}
