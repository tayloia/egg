namespace egg::ovum {
  class BuiltinFactory {
  public:
    static const IntShape IntShape;
    static const FloatShape FloatShape;
    static const ObjectShape StringShape;
    static const ObjectShape ObjectShape;

    static Value createAssert(IAllocator& allocator);
    static Value createPrint(IAllocator& allocator);
    static Value createType(IAllocator& allocator);
    static Value createString(IAllocator& allocator);
  };
}
