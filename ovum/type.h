namespace egg::ovum {
  class Type : public HardPtr<const IType> {
  public:
    explicit Type(const IType* rhs = nullptr) : HardPtr(rhs) {
    }
    static const Type Void;
    static const Type Null;
    static const Type Bool;
    static const Type Int;
    static const Type Float;
    static const Type String;
    static const Type Arithmetic;
    static const Type Any;
    static const Type AnyQ;

    static Type makeBasal(IAllocator& allocator, BasalBits basal);
    static Type makeUnion(IAllocator& allocator, const IType& lhs, const IType& rhs);
  };

  class Object : public HardPtr<IObject> {
  public:
    explicit Object(const IObject& rhs) : HardPtr(&rhs) {
      assert(this->get() != nullptr);
    }
  };
}
