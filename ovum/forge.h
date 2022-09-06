#include <span>

namespace egg::ovum {
  class Forge {
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
    struct Reserved {
      const IType* forge();
    };
  private:
    class Implementation;
    std::unique_ptr<Implementation> implementation;
  public:
    explicit Forge(IAllocator& allocator);
    ~Forge();
    const IType* reserveType(Reserved& reserved);
    const IType* forgeType(ValueFlags simple, const std::span<const TypeShape*> complex);
    const TypeShape* forgeTypeShape(const IFunctionSignature* callable, const IPropertySignature* dotable, const IIndexSignature* indexable, const IIteratorSignature* iterable, const IPointerSignature* pointable);
    const IFunctionSignature* forgeFunctionSignature(const IType& returnType, const IType* generatorType, String name, const std::span<Parameter>& parameters);
    const IIndexSignature* forgeIndexSignature(const IType& resultType, const IType* indexType, Modifiability modifiability);
    const IIteratorSignature* forgeIteratorSignature(const IType& resultType);
    const IPointerSignature* forgePointerSignature(const IType& pointeeType, Modifiability modifiability);
    const IPropertySignature* forgePropertySignature(const std::span<Property>& properties, const IType* unknownType, Modifiability unknownModifiability);
  };
}
