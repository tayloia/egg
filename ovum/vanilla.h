namespace egg::ovum {
  class Vanilla {
  public:
    struct IPredicateCallback {
      virtual ~IPredicateCallback() {}
      virtual Value predicateCallback(const INode& node) = 0;
    };

    static Type getArrayType(); // any?[int] with well-known properties
    static Type getDictionaryType(); // any?[string] with forwarded properties
    static Type getKeyValueType(); // { string key; any? value; }
    static Type getObjectType(); // any?[any?] with separate properties
  };

  template<typename K, typename V>
  class IVanillaMap : public ICollectable {
  public:
    // Construction/destruction
    virtual ~IVanillaMap() {}
    virtual bool add(const K& key, const V& value) = 0;
    virtual bool get(const K& key, V& value) const = 0;
    virtual void set(const K& key, const V& value) = 0;
    virtual V mut(const K& key, Mutation mutation, const V& value) = 0;
    virtual bool del(const K& key) = 0;
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
