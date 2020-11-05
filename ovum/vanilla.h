namespace egg::ovum {
  class Vanilla {
  public:
    struct IPredicateCallback {
      virtual ~IPredicateCallback() {}
      virtual Value predicateCallback(const INode& node) = 0;
    };

    static const Type Array;
    static const Type Dictionary;
    static const Type Object;
  };

  template<typename K, typename V>
  class IVanillaMap : public ICollectable {
  public:
    // Construction/destruction
    virtual ~IVanillaMap() {}
    virtual bool get(const K& key, V& value) const = 0;
    virtual void add(const K& key, const V& value) = 0;
    virtual void set(const K& key, const V& value) = 0;
    virtual HardPtr<ISlot> slot(const K& key) const = 0;
  };

  class VanillaFactory {
  public:
    static Object createArray(IAllocator& allocator, size_t fixed);
    static Object createDictionary(IAllocator& allocator);
    static Object createObject(IAllocator& allocator);
    static Object createError(IAllocator& allocator, const LocationSource& location, const String& message);
    static Object createPredicate(IAllocator& allocator, Vanilla::IPredicateCallback& callback, const INode& node);
    static HardPtr<IVanillaMap<String, Value>> createStringValueMap(IAllocator& allocator);
  };
}
