namespace egg::ovum {
  class Vanilla {
  public:
    struct IPredicateCallback {
      virtual ~IPredicateCallback() {}
      virtual Value predicateCallback(const INode& node) = 0;
    };

    static const Type Object;
    static const Type Array;
    static const IFunctionSignature& FunctionSignature;
  };

  class IVanillaDictionary : public ICollectable {
  public:
    // Construction/destruction
    virtual ~IVanillaDictionary() {}
    virtual bool get(const String& key, Value& value) const = 0;
    virtual HardPtr<Slot> ref(const String& key) const = 0;
    virtual void add(const String& key, const Value& value) = 0;
    virtual void set(const String& key, const Value& value) = 0;
  };

  class VanillaFactory {
  public:
    static Object createArray(IAllocator& allocator, size_t fixed);
    static Object createError(IAllocator& allocator, const LocationSource& location, const String& message);
    static Object createObject(IAllocator& allocator);
    static Object createPredicate(IAllocator& allocator, Vanilla::IPredicateCallback& callback, const INode& node);
    static HardPtr<IVanillaDictionary> createDictionary(IAllocator& allocator);
  };
}
