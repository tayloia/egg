namespace egg::ovum {
  class BuiltinFactory {
  public:
    class StringProperty {
    public:
      virtual ~StringProperty() {}
      virtual Value createInstance(ITypeFactory& factory, IBasket& basket, const String& string) const = 0;
      virtual String getName() const = 0;
      virtual Type getType() const = 0;
    };
    static const StringProperty* getStringPropertyByName(ITypeFactory& factory, const String& property);
    static const StringProperty* getStringPropertyByIndex(ITypeFactory& factory, size_t index);
    static size_t getStringPropertyCount(ITypeFactory& factory);

    static Value createAssertInstance(ITypeFactory& factory, IBasket& basket);
    static Value createPrintInstance(ITypeFactory& factory, IBasket& basket);
    static Value createTypeInstance(ITypeFactory& factory, IBasket& basket);
    static Value createStringInstance(ITypeFactory& factory, IBasket& basket);
  };
}
