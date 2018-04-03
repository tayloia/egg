namespace egg::lang {
  class Bits {
  public:
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

  class IObject {
  public:
    virtual ~IObject() {}
    virtual IObject* acquire() const = 0;
    virtual void release() const = 0;
  };

  class Value final {
  private:
    Discriminator tag;
    union {
      bool b;
      int64_t i;
      double f;
      std::string* s;
      IObject* o;
      Value* v;
    };
    explicit Value(Discriminator tag, Value* child = nullptr) : tag(tag) { this->v = child; }
    void copyInternals(const Value& other);
    void moveInternals(Value& other);
  public:
    inline Value() : tag(Discriminator::Void) { this->o = nullptr; }
    inline explicit Value(nullptr_t) : tag(Discriminator::Null) { this->o = nullptr; }
    inline explicit Value(bool value) : tag(Discriminator::Bool) { this->b = value; }
    inline explicit Value(int64_t value) : tag(Discriminator::Int) { this->i = value; }
    inline explicit Value(double value) : tag(Discriminator::Float) { this->f = value; }
    inline explicit Value(const std::string& value) : tag(Discriminator::String) { this->s = new std::string(value); }
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
    inline const std::string& getString() const { assert(this->is(Discriminator::String)); return *this->s; }
    inline IObject& getType() const { assert(this->is(Discriminator::Type)); return *this->o; }
    inline IObject& getObject() const { assert(this->is(Discriminator::Object)); return *this->o; }
    inline Value& getFlowControl() const { assert(this->is(Discriminator::FlowControl)); return *this->v; }
    inline std::string getTagString() const { return Value::getTagString(this->tag); }
    static std::string getTagString(Discriminator tag);
    static bool equal(const Value& lhs, const Value& rhs);
    static Value makeFlowControl(Discriminator tag, Value* value);
    static Value raise(const std::string& exception);
    static const Value Void;
    static const Value Null;
    static const Value False;
    static const Value True;
    static const Value Break;
    static const Value Continue;
    static const Value Rethrow;
  };
}
