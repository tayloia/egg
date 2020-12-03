namespace egg::ovum {
  class BuiltinFactory {
  public:
    static const ObjectShape& getStringShape();
    static const ObjectShape& getObjectShape();

    class StringProperty {
    public:
      virtual ~StringProperty() {}
      virtual Value createInstance(IAllocator& allocator, const String& string) const = 0;
      virtual String getName() const = 0;
      virtual Type getType() const = 0;
    };
    static const StringProperty* getStringPropertyByName(const String& property);
    static const StringProperty* getStringPropertyByIndex(size_t index);
    static size_t getStringPropertyCount();

    static Value createAssertInstance(IAllocator& allocator, IBasket& basket);
    static Value createPrintInstance(IAllocator& allocator, IBasket& basket);
    static Value createTypeInstance(IAllocator& allocator, IBasket& basket);
    static Value createStringInstance(IAllocator& allocator, IBasket& basket);
  };
}
