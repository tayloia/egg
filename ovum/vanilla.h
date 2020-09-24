namespace egg::ovum {
  class IProgram;

  class Vanilla {
  public:
    struct IPredicateCallback {
      virtual ~IPredicateCallback() {}
      virtual Value predicateCallback(const INode& node) = 0;
    };

    static const Type Object;
    static const Type Array;
    static const IFunctionSignature& FunctionSignature;
    static const IIndexSignature& IndexSignature;
  };

  class VanillaFactory {
  public:
    static Object createArray(IAllocator& allocator, size_t fixed);
    static Object createError(IAllocator& allocator, const LocationSource& location, const String& message);
    static Object createObject(IAllocator& allocator);
    static Object createPredicate(IAllocator& allocator, Vanilla::IPredicateCallback& callback, const INode& node);
  };
}
