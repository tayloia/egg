namespace egg::ovum {
  class Vanilla {
  public:
    struct IPredicateCallback {
      virtual ~IPredicateCallback() {}
      virtual Value predicateCallback(const INode& node) = 0;
    };

    static Type getArrayType(); // any?[] with well-known properties
    static Type getDictionaryType(); // any?[string] with forwarded properties
    static Type getKeyValueType(); // { string key; any? value; }
  };

  template<typename V>
  class IVanillaArray : public ICollectable {
  public:
    virtual ~IVanillaArray() {}
    virtual bool get(size_t index, V& value) const = 0;
    virtual bool set(size_t index, const V& value) = 0;
    virtual V mut(size_t index, Mutation mutation, const V& value) = 0;
    virtual size_t length() const = 0;
    virtual bool resize(size_t size) = 0;
  };

  template<typename K, typename V>
  class IVanillaMap : public ICollectable {
  public:
    virtual ~IVanillaMap() {}
    virtual bool add(const K& key, const V& value) = 0;
    virtual bool get(const K& key, V& value) const = 0;
    virtual void set(const K& key, const V& value) = 0;
    virtual V mut(const K& key, Mutation mutation, const V& value) = 0;
    virtual bool del(const K& key) = 0;
  };

  class VanillaFactory {
  public:
    static Object createArray(IAllocator& allocator, size_t length);
    static Object createDictionary(IAllocator& allocator);
    static Object createObject(IAllocator& allocator);
    static Object createKeyValue(IAllocator& allocator, const String& key, const Value& value);
    static Object createError(IAllocator& allocator, const LocationSource& location, const String& message);
    static Object createPredicate(IAllocator& allocator, Vanilla::IPredicateCallback& callback, const INode& node);
    static Object createPointer(IAllocator& allocator, ISlot& slot, Modifiability modifiability);
  };
}
