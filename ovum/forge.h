#include <span>

namespace egg::ovum {
  class IForgedType : public IType {
  public:
    virtual const IType* forge(ValueFlags simple, std::set<const TypeShape*>&& complex, const std::string* description = nullptr) = 0;
  };

  class Forge final {
    Forge(const Forge&) = delete;
    Forge& operator=(const Forge&) = delete;
  public:
    struct Property {
      String name;
      Type type;
      Modifiability modifiability;
    };
    struct Parameter {
      String name;
      Type type;
      bool optional;
      enum class Kind { Positional, Named, Variadic, Predicate } kind;
    };
  private:
    class Implementation;
    std::unique_ptr<Implementation> implementation;
  public:
    explicit Forge(IAllocator& allocator);
    ~Forge();

    IAllocator& getAllocator() const;
    IForgedType* reserveType();
    const IType* forgeSimple(ValueFlags simple);
    const IType* forgeComplex(ValueFlags simple, std::set<const TypeShape*>&& complex, const std::string* description = nullptr);
    const IType* forgeModified(const IType& original, ValueFlags flags, const std::string* description = nullptr);
    const TypeShape* forgeTypeShape(const IFunctionSignature* callable, const IPropertySignature* dotable, const IIndexSignature* indexable, const IIteratorSignature* iterable, const IPointerSignature* pointable);
    const IFunctionSignature* forgeFunctionSignature(const IType& returnType, const IType* generatorType, String name, const std::span<Parameter>& parameters);
    const IIndexSignature* forgeIndexSignature(const IType& resultType, const IType* indexType, Modifiability modifiability);
    const IIteratorSignature* forgeIteratorSignature(const IType& resultType);
    const IPointerSignature* forgePointerSignature(const IType& pointeeType, Modifiability modifiability);
    const IPropertySignature* forgePropertySignature(const std::span<Property>& properties, const IType* unknownType, Modifiability unknownModifiability);

    void mergeTypeShapes(std::set<const TypeShape*>& shapes, const IType& other);

    static std::pair<std::string, int> simpleToStringPrecedence(ValueFlags flags);
    static std::pair<std::string, int> complexToStringPrecedence(ValueFlags flags, const std::set<const TypeShape*>& shapes);
  };
}
