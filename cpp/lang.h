namespace egg::lang {
  enum class TypeStorage {
    Inferred = -1,
    Void = 0,
    Null = 1 << 0,
    Bool = 1 << 1,
    Int = 1 << 2,
    Float = 1 << 3,
    String = 1 << 4,
    Type = 1 << 5,
    Object = 1 << 6,
    Any = Bool | Int | Float | String | Type | Object
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
}