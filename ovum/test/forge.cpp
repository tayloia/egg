#include "ovum/test.h"
#include "ovum/forge.h"

using namespace egg::ovum;

TEST(TestForge, Empty) {
  egg::test::Allocator allocator{ egg::test::Allocator::Expectation::NoAllocations };
  Forge forge{ allocator };
  ASSERT_NE(nullptr, &forge);
}

TEST(TestForge, FunctionSignatureEmpty) {
  egg::test::Allocator allocator;
  Forge forge{ allocator };
  auto* callable = forge.forgeFunctionSignature(*Type::Void, nullptr, "fname", {});
  ASSERT_TYPE(Type::Void, callable->getReturnType());
  ASSERT_TYPE(nullptr, callable->getGeneratorType());
  ASSERT_STRING("fname", callable->getName());
  ASSERT_EQ(0u, callable->getParameterCount());
}

TEST(TestForge, FunctionSignatureParametersPositional) {
  egg::test::Allocator allocator;
  Forge forge{ allocator };
  std::vector<Forge::Parameter> parameters = {
    { "b", Type::Int, false, Forge::Parameter::Kind::Positional },
    { "c", Type::Float, false, Forge::Parameter::Kind::Positional },
    { "a", Type::String, true, Forge::Parameter::Kind::Positional }
  };
  auto* callable = forge.forgeFunctionSignature(*Type::Void, nullptr, "fname", parameters);
  ASSERT_TYPE(Type::Void, callable->getReturnType());
  ASSERT_TYPE(nullptr, callable->getGeneratorType());
  ASSERT_STRING("fname", callable->getName());
  ASSERT_EQ(3u, callable->getParameterCount());
  auto& p0 = callable->getParameter(0);
  ASSERT_STRING("b", p0.getName());
  ASSERT_TYPE(Type::Int, p0.getType());
  ASSERT_EQ(0u, p0.getPosition());
  ASSERT_EQ(IFunctionSignatureParameter::Flags::Required, p0.getFlags());
  auto& p1 = callable->getParameter(1);
  ASSERT_STRING("c", p1.getName());
  ASSERT_TYPE(Type::Float, p1.getType());
  ASSERT_EQ(1u, p1.getPosition());
  ASSERT_EQ(IFunctionSignatureParameter::Flags::Required, p1.getFlags());
  auto& p2 = callable->getParameter(2);
  ASSERT_STRING("a", p2.getName());
  ASSERT_TYPE(Type::String, p2.getType());
  ASSERT_EQ(2u, p2.getPosition());
  ASSERT_EQ(IFunctionSignatureParameter::Flags::None, p2.getFlags());
}

TEST(TestForge, FunctionSignatureParametersNamed) {
  egg::test::Allocator allocator;
  Forge forge{ allocator };
  std::vector<Forge::Parameter> parameters = {
    { "b", Type::Int, false, Forge::Parameter::Kind::Named },
    { "c", Type::Float, false, Forge::Parameter::Kind::Named },
    { "a", Type::String, true, Forge::Parameter::Kind::Named}
  };
  auto* callable = forge.forgeFunctionSignature(*Type::Void, nullptr, "fname", parameters);
  ASSERT_TYPE(Type::Void, callable->getReturnType());
  ASSERT_TYPE(nullptr, callable->getGeneratorType());
  ASSERT_STRING("fname", callable->getName());
  ASSERT_EQ(3u, callable->getParameterCount());
  auto& p0 = callable->getParameter(0);
  ASSERT_STRING("a", p0.getName());
  ASSERT_TYPE(Type::String, p0.getType());
  ASSERT_EQ(SIZE_MAX, p0.getPosition());
  ASSERT_EQ(IFunctionSignatureParameter::Flags::None, p0.getFlags());
  auto& p1 = callable->getParameter(1);
  ASSERT_STRING("b", p1.getName());
  ASSERT_TYPE(Type::Int, p1.getType());
  ASSERT_EQ(SIZE_MAX, p1.getPosition());
  ASSERT_EQ(IFunctionSignatureParameter::Flags::Required, p1.getFlags());
  auto& p2 = callable->getParameter(2);
  ASSERT_STRING("c", p2.getName());
  ASSERT_TYPE(Type::Float, p2.getType());
  ASSERT_EQ(SIZE_MAX, p2.getPosition());
  ASSERT_EQ(IFunctionSignatureParameter::Flags::Required, p2.getFlags());
}

TEST(TestForge, FunctionSignatureParametersMixed) {
  egg::test::Allocator allocator;
  Forge forge{ allocator };
  std::vector<Forge::Parameter> parameters = {
    { "b", Type::Int, true, Forge::Parameter::Kind::Named },
    { "a", Type::Float, false, Forge::Parameter::Kind::Positional },
  };
  auto* callable = forge.forgeFunctionSignature(*Type::Void, nullptr, "fname", parameters);
  ASSERT_TYPE(Type::Void, callable->getReturnType());
  ASSERT_TYPE(nullptr, callable->getGeneratorType());
  ASSERT_STRING("fname", callable->getName());
  ASSERT_EQ(2u, callable->getParameterCount());
  auto& p0 = callable->getParameter(0);
  ASSERT_STRING("a", p0.getName());
  ASSERT_TYPE(Type::Float, p0.getType());
  ASSERT_EQ(0u, p0.getPosition());
  ASSERT_EQ(IFunctionSignatureParameter::Flags::Required, p0.getFlags());
  auto& p1 = callable->getParameter(1);
  ASSERT_STRING("b", p1.getName());
  ASSERT_TYPE(Type::Int, p1.getType());
  ASSERT_EQ(SIZE_MAX, p1.getPosition());
  ASSERT_EQ(IFunctionSignatureParameter::Flags::None, p1.getFlags());
}

TEST(TestForge, FunctionSignatureGenerator) {
  egg::test::Allocator allocator;
  Forge forge{ allocator };
  std::vector<Forge::Parameter> parameters = {
    { "a", Type::Int, false, Forge::Parameter::Kind::Positional },
    { "b", Type::Float, true, Forge::Parameter::Kind::Positional },
  };
  auto* callable = forge.forgeFunctionSignature(*Type::Any, Type::String.get(), "gname", parameters);
  ASSERT_TYPE(Type::Any, callable->getReturnType());
  ASSERT_TYPE(Type::String, callable->getGeneratorType());
  ASSERT_STRING("gname", callable->getName());
  ASSERT_EQ(2u, callable->getParameterCount());
  auto& p0 = callable->getParameter(0);
  ASSERT_STRING("a", p0.getName());
  ASSERT_TYPE(Type::Int, p0.getType());
  ASSERT_EQ(0u, p0.getPosition());
  ASSERT_EQ(IFunctionSignatureParameter::Flags::Required, p0.getFlags());
  auto& p1 = callable->getParameter(1);
  ASSERT_STRING("b", p1.getName());
  ASSERT_TYPE(Type::Float, p1.getType());
  ASSERT_EQ(1u, p1.getPosition());
  ASSERT_EQ(IFunctionSignatureParameter::Flags::None, p1.getFlags());
}

TEST(TestForge, IndexSignatureArray) {
  egg::test::Allocator allocator;
  Forge forge{ allocator };
  auto* indexable = forge.forgeIndexSignature(*Type::String, nullptr, Modifiability::READ_WRITE);
  ASSERT_TYPE(Type::String, indexable->getResultType());
  ASSERT_TYPE(nullptr, indexable->getIndexType());
  ASSERT_EQ(Modifiability::READ_WRITE, indexable->getModifiability());
}

TEST(TestForge, IndexSignatureMap) {
  egg::test::Allocator allocator;
  Forge forge{ allocator };
  auto* indexable = forge.forgeIndexSignature(*Type::Float, Type::String.get(), Modifiability::READ);
  ASSERT_TYPE(Type::Float, indexable->getResultType());
  ASSERT_TYPE(Type::String, indexable->getIndexType());
  ASSERT_EQ(Modifiability::READ, indexable->getModifiability());
}

TEST(TestForge, IteratorSignature) {
  egg::test::Allocator allocator;
  Forge forge{ allocator };
  auto* iterable = forge.forgeIteratorSignature(*Type::Int);
  ASSERT_TYPE(Type::Int, iterable->getType());
}

TEST(TestForge, PointerSignature) {
  egg::test::Allocator allocator;
  Forge forge{ allocator };
  auto* pointable = forge.forgePointerSignature(*Type::String, Modifiability::READ);
  ASSERT_TYPE(Type::String, pointable->getType());
  ASSERT_EQ(Modifiability::READ, pointable->getModifiability());
}

TEST(TestForge, PropertySignatureClosed) {
  egg::test::Allocator allocator;
  Forge forge{ allocator };
  std::vector<Forge::Property> properties = {
    { "age", Type::Int, Modifiability::READ }
  };
  auto* dotable = forge.forgePropertySignature(properties, nullptr, Modifiability::NONE);
  ASSERT_EQ(true, dotable->isClosed());
  ASSERT_EQ(1u, dotable->getNameCount());
  ASSERT_STRING("age", dotable->getName(0));
  ASSERT_TYPE(Type::Int, dotable->getType("age"));
  ASSERT_EQ(Modifiability::READ, dotable->getModifiability("age"));
  ASSERT_TYPE(nullptr, dotable->getType("unknown"));
  ASSERT_EQ(Modifiability::NONE, dotable->getModifiability("unknown"));
}

TEST(TestForge, PropertySignatureOpen) {
  egg::test::Allocator allocator;
  Forge forge{ allocator };
  std::vector<Forge::Property> properties = {
    { "name", Type::String, Modifiability::READ_WRITE },
    { "cost", Type::Float, Modifiability::READ_WRITE_MUTATE }
  };
  auto* dotable = forge.forgePropertySignature(properties, Type::Int.get(), Modifiability::READ);
  ASSERT_EQ(false, dotable->isClosed());
  ASSERT_EQ(2u, dotable->getNameCount());
  ASSERT_STRING("cost", dotable->getName(0));
  ASSERT_TYPE(Type::Float, dotable->getType("cost"));
  ASSERT_EQ(Modifiability::READ_WRITE_MUTATE, dotable->getModifiability("cost"));
  ASSERT_STRING("name", dotable->getName(1));
  ASSERT_TYPE(Type::String, dotable->getType("name"));
  ASSERT_EQ(Modifiability::READ_WRITE, dotable->getModifiability("name"));
  ASSERT_TYPE(Type::Int, dotable->getType("unknown"));
  ASSERT_EQ(Modifiability::READ, dotable->getModifiability("unknown"));
}

TEST(TestForge, TypeShapeEmpty) {
  egg::test::Allocator allocator;
  Forge forge{ allocator };
  auto* shape = forge.forgeTypeShape(nullptr, nullptr, nullptr, nullptr, nullptr);
  ASSERT_EQ(nullptr, shape->callable);
  ASSERT_EQ(nullptr, shape->dotable);
  ASSERT_EQ(nullptr, shape->indexable);
  ASSERT_EQ(nullptr, shape->iterable);
  ASSERT_EQ(nullptr, shape->pointable);
}

TEST(TestForge, TypeShapeRepeated) {
  egg::test::Allocator allocator;
  Forge forge{ allocator };
  auto* indexable = forge.forgeIndexSignature(*Type::Float, Type::String.get(), Modifiability::READ);
  auto* shape1 = forge.forgeTypeShape(nullptr, nullptr, indexable, nullptr, nullptr);
  ASSERT_EQ(nullptr, shape1->callable);
  ASSERT_EQ(nullptr, shape1->dotable);
  ASSERT_EQ(indexable, shape1->indexable);
  ASSERT_EQ(nullptr, shape1->iterable);
  ASSERT_EQ(nullptr, shape1->pointable);
  auto* shape2 = forge.forgeTypeShape(nullptr, nullptr, indexable, nullptr, nullptr);
  ASSERT_EQ(shape1, shape2);
}

TEST(TestForge, TypePrimitive) {
  egg::test::Allocator allocator{ egg::test::Allocator::Expectation::NoAllocations };
  Forge forge{ allocator };
  Type none{ forge.forgeSimple(ValueFlags::None) };
  ASSERT_TYPE(Type::None, none);
  ASSERT_STRING("var", none.toString());
  Type arithmetic{ forge.forgeSimple(ValueFlags::Arithmetic) };
  ASSERT_TYPE(Type::Arithmetic, arithmetic);
  ASSERT_STRING("int|float", arithmetic.toString());
  Type anyq{ forge.forgeSimple(ValueFlags::AnyQ) };
  ASSERT_TYPE(Type::AnyQ, anyq);
  ASSERT_STRING("any?", anyq.toString());
}

TEST(TestForge, TypeSimple) {
  egg::test::Allocator allocator;
  Forge forge{ allocator };
  Type type1{ forge.forgeSimple(ValueFlags::Int | ValueFlags::String) };
  ASSERT_STRING("int|string", type1.toString());
  Type type2{ forge.forgeSimple(ValueFlags::Void | ValueFlags::AnyQ) };
  ASSERT_STRING("void|any?", type2.toString());
}

TEST(TestForge, TypeComplex) {
  egg::test::Allocator allocator;
  Forge forge{ allocator };
  auto* indexable = forge.forgeIndexSignature(*Type::Float, Type::String.get(), Modifiability::READ);
  std::set<const TypeShape*> shapes{ forge.forgeTypeShape(nullptr, nullptr, indexable, nullptr, nullptr) };
  Type complex{ forge.forgeComplex(ValueFlags::Int | ValueFlags::String, std::move(shapes)) };
  ASSERT_STRING("int|string|float[string]", complex.toString());
}
