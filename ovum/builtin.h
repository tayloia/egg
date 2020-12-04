namespace egg::ovum {
  class BuiltinFactory {
  public:
    static const ObjectShape& getStringShape();
    static const ObjectShape& getObjectShape();

    class StringProperty {
    public:
      virtual ~StringProperty() {}
      virtual Value createInstance(TypeFactory& factory, IBasket& basket, const String& string) const = 0;
      virtual String getName() const = 0;
      virtual Type getType() const = 0;
    };
    static const StringProperty* getStringPropertyByName(const String& property);
    static const StringProperty* getStringPropertyByIndex(size_t index);
    static size_t getStringPropertyCount();

    static Value createAssertInstance(TypeFactory& factory, IBasket& basket);
    static Value createPrintInstance(TypeFactory& factory, IBasket& basket);
    static Value createTypeInstance(TypeFactory& factory, IBasket& basket);
    static Value createStringInstance(TypeFactory& factory, IBasket& basket);
  };
}
