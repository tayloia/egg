namespace egg::lang {
  class String;
  class Value;

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
    Type = 1 << 6,
    Object = 1 << 7,
    Any = Bool | Int | Float | String | Type | Object,
    Arithmetic = Int | Float,
    Break = 1 << 8,
    Continue = 1 << 9,
    Return = 1 << 10,
    Yield = 1 << 11,
    Exception = 1 << 12,
    FlowControl = Break | Continue | Return | Yield | Exception
  };
  inline Discriminator operator|(Discriminator lhs, Discriminator rhs) {
    return Bits::set(lhs, rhs);
  }

  class IString {
  public:
    virtual ~IString() {}
    virtual IString* acquireHard() const = 0;
    virtual void releaseHard() const = 0;
    virtual size_t length() const = 0;
    virtual bool empty() const = 0;
    virtual bool equal(const IString& other) const = 0;
    virtual bool less(const IString& other) const = 0;
    virtual std::string toUTF8() const = 0;
  };
  typedef egg::gc::HardRef<const IString> IStringRef;

  class IExecution {
  public:
    virtual ~IExecution() {}
    virtual void print(const std::string& utf8) = 0;
  };

  class IParameters {
  public:
    virtual ~IParameters() {}
    virtual size_t getPositionalCount() const = 0;
    virtual Value getPositional(size_t index) const = 0;
    virtual size_t getNamedCount() const = 0;
    virtual String getName(size_t index) const = 0;
    virtual Value getNamed(const String& name) const = 0;
  };

  class IType {
    typedef egg::gc::HardRef<const IType> Ref; // Local typedef
  public:
    virtual ~IType() {}
    virtual IType* acquireHard() const = 0;
    virtual void releaseHard() const = 0;
    virtual String toString() const = 0;
    virtual Value canAlwaysAssignFrom(const IType& rhs) const = 0;
    virtual Value promoteAssignment(const Value& rhs) const = 0;

    virtual Discriminator getSimpleTypes() const; // Default implementation returns 'None'
    virtual Ref referencedType() const; // Default implementation returns 'Type*'
    virtual Ref dereferencedType() const; // Default implementation returns 'Void'
    virtual Ref denulledType() const; // Default implementation returns self 'Type'
    virtual Ref unionWith(const IType& other) const; // Default implementation calls Type::makeUnion()
    typedef std::function<void(const String& name, const IType& type, const Value& value)> Setter;
    virtual Value decantParameters(const IParameters& supplied, Setter setter) const; // Default implementation returns an error

    // Helpers
    inline bool hasNativeType(Discriminator native) const {
      return egg::lang::Bits::hasAllSet(this->getSimpleTypes(), native);
    }
  };
  typedef egg::gc::HardRef<const IType> ITypeRef;

  class Type : public ITypeRef {
    Type() = delete;
  private:
    inline explicit Type(const IType& rhs) : ITypeRef(&rhs) {}
  public:
    static const Type Void;
    static const Type Null;

    static ITypeRef makeUnion(const IType& lhs, const IType& rhs);
  };

  class IObject {
  public:
    virtual ~IObject() {}
    virtual IObject* acquireHard() const = 0;
    virtual void releaseHard() const = 0;
    virtual bool dispose() = 0;
    virtual Value toString() const = 0;
    virtual Value getRuntimeType() const = 0;
    virtual Value call(IExecution& execution, const IParameters& parameters) = 0;
  };
  typedef egg::gc::HardRef<IObject> IObjectRef;

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
    String str() const;
  };

  class String : public IStringRef {
  private:
    static const IString& emptyBuffer;
  public:
    inline explicit String(const IString& rhs = emptyBuffer) : IStringRef(&rhs) {
    }
    String(const String& rhs) : IStringRef(rhs.get()) {
    }
    // Properties
    inline size_t length() const {
      return this->get()->length();
    }
    inline std::string toUTF8() const {
      return this->get()->toUTF8();
    }
    // Operators
    inline String& operator=(const String& rhs) {
      this->set(rhs.get());
      return *this;
    }
    inline bool operator==(const String& rhs) const {
      return this->get()->equal(*rhs);
    }
    inline bool operator<(const String& rhs) const {
      return this->get()->less(*rhs);
    }
    // Factories
    static String fromUTF8(const std::string& str);
    template<typename... ARGS>
    static String concat(ARGS... args) {
      return StringBuilder().add(args...).str();
    }
    // Constants
    static const String Empty;
  };

  class Value final {
  private:
    Discriminator tag;
    union {
      bool b;
      int64_t i;
      double f;
      const IString* s;
      IObject* o;
      IType* t;
      Value* v;
    };
    inline explicit Value(Discriminator tag, Value* child = nullptr) : tag(tag) { this->v = child; }
    void copyInternals(const Value& other);
    void moveInternals(Value& other);
  public:
    inline Value() : tag(Discriminator::Void) { this->o = nullptr; }
    inline explicit Value(nullptr_t) : tag(Discriminator::Null) { this->o = nullptr; }
    inline explicit Value(bool value) : tag(Discriminator::Bool) { this->b = value; }
    inline explicit Value(int64_t value) : tag(Discriminator::Int) { this->i = value; }
    inline explicit Value(double value) : tag(Discriminator::Float) { this->f = value; }
    inline explicit Value(const String& value) : tag(Discriminator::String) { this->s = value.acquireHard(); }
    inline explicit Value(const IType& type) : tag(Discriminator::Type) { this->t = type.acquireHard(); }
    inline explicit Value(IObject& object) : tag(Discriminator::Object) { this->o = object.acquireHard(); }
    Value(const Value& value);
    Value(Value&& value);
    Value& operator=(const Value& value);
    Value& operator=(Value&& value);
    ~Value();
    inline bool operator==(const Value& other) const { return Value::equal(*this, other); }
    inline bool operator!=(const Value& other) const { return Value::equal(*this, other); }
    inline bool is(Discriminator bits) const { return Bits::mask(this->tag, bits) != Discriminator::None; }
    inline bool getBool() const { assert(this->is(Discriminator::Bool)); return this->b; }
    inline int64_t getInt() const { assert(this->is(Discriminator::Int)); return this->i; }
    inline double getFloat() const { assert(this->is(Discriminator::Float)); return this->f; }
    inline String getString() const { assert(this->is(Discriminator::String)); return String(*this->s); }
    inline IObject& getObject() const { assert(this->is(Discriminator::Object)); return *this->o; }
    inline Value& getFlowControl() const { assert(this->is(Discriminator::FlowControl)); return *this->v; }
    inline std::string getTagString() const { return Value::getTagString(this->tag); }
    static std::string getTagString(Discriminator tag);
    static bool equal(const Value& lhs, const Value& rhs);
    static Value makeFlowControl(Discriminator tag, Value* value);
    template<typename... ARGS>
    static Value raise(ARGS... args) {
      auto message = StringBuilder().add(args...).str();
      return Value(Discriminator::Exception, new Value(message));
    }
    std::string toUTF8() const;
    const IType& getRuntimeType() const;

    // Constants
    static const Value Void;
    static const Value Null;
    static const Value False;
    static const Value True;
    static const Value Break;
    static const Value Continue;
    static const Value Rethrow;

    // Built-ins
    static Value Print;
  };
}

std::ostream& operator<<(std::ostream& os, const egg::lang::String& text);
