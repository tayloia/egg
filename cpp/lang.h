namespace egg::lang {
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

  enum class TypeStorage {
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
    Arithmetic = Int | Float
  };

  class VariantTag {
  private:
    TypeStorage bits;
  public:
    VariantTag(TypeStorage bits)
      : bits(bits) {
    }
    operator TypeStorage() const {
      return this->bits;
    }
    TypeStorage mask(TypeStorage bit) const {
      return TypeStorage(underlying(this->bits) & underlying(bit));
    }
    TypeStorage onion(TypeStorage bit) const {
      // Because 'union' is a reserved word
      return TypeStorage(underlying(this->bits) | underlying(bit));
    }
    bool hasBit(TypeStorage bit) const {
      return (underlying(this->bits) & underlying(bit)) != 0;
    }
    static std::underlying_type<TypeStorage>::type underlying(TypeStorage bits) {
      return static_cast<std::underlying_type<TypeStorage>::type>(bits);
    }
  };

  inline VariantTag operator|(TypeStorage lhs, TypeStorage rhs) {
    // See http://blog.bitwigglers.org/using-enum-classes-as-type-safe-bitmasks/
    return static_cast<TypeStorage>(VariantTag::underlying(lhs) | VariantTag::underlying(rhs));
  }

  inline VariantTag operator^(TypeStorage lhs, TypeStorage rhs) {
    return static_cast<TypeStorage>(VariantTag::underlying(lhs) ^ VariantTag::underlying(rhs));
  }
}