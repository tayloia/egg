#include "ovum/test.h"

using namespace egg::ovum;

namespace {
  ValueFlags getSimpleFlags(TypeFactory& factory, ValueFlags flags) {
    auto type = factory.createSimple(flags);
    return type->getPrimitiveFlags();
  }
}

TEST(TestType, FactorySimpleBasic) {
  egg::test::Allocator allocator{ egg::test::Allocator::Expectation::NoAllocations };
  TypeFactory factory{ allocator };
  ASSERT_EQ(ValueFlags::None, getSimpleFlags(factory, ValueFlags::None));
  ASSERT_EQ(ValueFlags::Void, getSimpleFlags(factory, ValueFlags::Void));
  ASSERT_EQ(ValueFlags::Null, getSimpleFlags(factory, ValueFlags::Null));
  ASSERT_EQ(ValueFlags::Bool, getSimpleFlags(factory, ValueFlags::Bool));
  ASSERT_EQ(ValueFlags::Int, getSimpleFlags(factory, ValueFlags::Int));
  ASSERT_EQ(ValueFlags::Float, getSimpleFlags(factory, ValueFlags::Float));
  ASSERT_EQ(ValueFlags::String, getSimpleFlags(factory, ValueFlags::String));
  ASSERT_EQ(ValueFlags::Arithmetic, getSimpleFlags(factory, ValueFlags::Arithmetic));
  ASSERT_EQ(ValueFlags::Object, getSimpleFlags(factory, ValueFlags::Object));
  ASSERT_EQ(ValueFlags::Any, getSimpleFlags(factory, ValueFlags::Any));
  ASSERT_EQ(ValueFlags::AnyQ, getSimpleFlags(factory, ValueFlags::AnyQ));
}

TEST(TestType, FactorySimpleNonBasic) {
  egg::test::Allocator allocator{ egg::test::Allocator::Expectation::AtLeastOneAllocation };
  TypeFactory factory{ allocator };
  auto flags = ValueFlags::String | ValueFlags::Arithmetic;
  ASSERT_EQ(flags, getSimpleFlags(factory, flags));
}

TEST(TestType, FactorySimpleBasicAddNull) {
  egg::test::Allocator allocator{ egg::test::Allocator::Expectation::NoAllocations };
  TypeFactory factory{ allocator };
  auto any = factory.createSimple(ValueFlags::Any);
  ASSERT_STRING("Value of type 'any'", any.describeValue());
  auto anyq = factory.addNull(any);
  ASSERT_STRING("Value of type 'any?'", anyq.describeValue());
  ASSERT_EQ(ValueFlags::AnyQ, anyq->getPrimitiveFlags());
  auto anyqq = factory.addNull(anyq);
  ASSERT_STRING("Value of type 'any?'", anyqq.describeValue());
  ASSERT_EQ(anyq.get(), anyqq.get());
}

TEST(TestType, FactorySimpleNonBasicAddNull) {
  egg::test::Allocator allocator{ egg::test::Allocator::Expectation::AtLeastOneAllocation };
  TypeFactory factory{ allocator };
  auto arithmetic = factory.createSimple(ValueFlags::Arithmetic);
  ASSERT_STRING("Value of type 'int|float'", arithmetic.describeValue());
  auto arithmeticq = factory.addNull(arithmetic);
  ASSERT_STRING("Value of type 'int|float?'", arithmeticq.describeValue());
  ASSERT_EQ(ValueFlags::Null | ValueFlags::Arithmetic, arithmeticq->getPrimitiveFlags());
  auto arithmeticqq = factory.addNull(arithmeticq);
  ASSERT_STRING("Value of type 'int|float?'", arithmeticqq.describeValue());
  ASSERT_EQ(arithmeticq.get(), arithmeticqq.get());
}

TEST(TestType, FactorySimpleNonBasicAddVoid) {
  egg::test::Allocator allocator{ egg::test::Allocator::Expectation::AtLeastOneAllocation };
  TypeFactory factory{ allocator };
  auto any = factory.createSimple(ValueFlags::Any);
  ASSERT_STRING("Value of type 'any'", any.describeValue());
  auto anyv = factory.addVoid(any);
  ASSERT_STRING("Value of type 'void|any'", anyv.describeValue());
  ASSERT_EQ(ValueFlags::Void | ValueFlags::Any, anyv->getPrimitiveFlags());
  auto anyvv = factory.addVoid(anyv);
  ASSERT_STRING("Value of type 'void|any'", anyvv.describeValue());
  ASSERT_EQ(anyv.get(), anyvv.get());
}

TEST(TestType, FactorySimpleBasicRemoveNull) {
  egg::test::Allocator allocator{ egg::test::Allocator::Expectation::NoAllocations };
  TypeFactory factory{ allocator };
  auto anyq = factory.createSimple(ValueFlags::AnyQ);
  ASSERT_STRING("Value of type 'any?'", anyq.describeValue());
  auto any = factory.removeNull(anyq);
  ASSERT_STRING("Value of type 'any'", any.describeValue());
  ASSERT_EQ(ValueFlags::Any, any->getPrimitiveFlags());
  auto any_ = factory.removeNull(any);
  ASSERT_STRING("Value of type 'any'", any_.describeValue());
  ASSERT_EQ(any.get(), any_.get());
}

TEST(TestType, FactorySimpleNonBasicRemoveNull) {
  egg::test::Allocator allocator{ egg::test::Allocator::Expectation::AtLeastOneAllocation };
  TypeFactory factory{ allocator };
  auto arithmeticq = factory.createSimple(ValueFlags::Null | ValueFlags::Arithmetic);
  ASSERT_STRING("Value of type 'int|float?'", arithmeticq.describeValue());
  auto arithmetic = factory.removeNull(arithmeticq);
  ASSERT_STRING("Value of type 'int|float'", arithmetic.describeValue());
  ASSERT_EQ(ValueFlags::Arithmetic, arithmetic->getPrimitiveFlags());
  auto arithmetic_ = factory.removeNull(arithmetic);
  ASSERT_STRING("Value of type 'int|float'", arithmetic_.describeValue());
  ASSERT_EQ(arithmetic.get(), arithmetic_.get());
}

TEST(TestType, FactorySimpleNonBasicRemoveVoid) {
  egg::test::Allocator allocator{ egg::test::Allocator::Expectation::AtLeastOneAllocation };
  TypeFactory factory{ allocator };
  auto vany = factory.createSimple(ValueFlags::Void | ValueFlags::Any);
  ASSERT_STRING("Value of type 'void|any'", vany.describeValue());
  auto any = factory.removeVoid(vany);
  ASSERT_STRING("Value of type 'any'", any.describeValue());
  ASSERT_EQ(ValueFlags::Any, any->getPrimitiveFlags());
  auto any_ = factory.removeVoid(any);
  ASSERT_STRING("Value of type 'any'", any_.describeValue());
  ASSERT_EQ(any.get(), any_.get());
}

TEST(TestType, FactoryPointer) {
  egg::test::Allocator allocator{ egg::test::Allocator::Expectation::AtLeastOneAllocation };
  TypeFactory factory{ allocator };
  auto modifiability = Modifiability::Read | Modifiability::Write | Modifiability::Mutate;
  auto pointer1 = factory.createPointer(Type::Any, modifiability);
  ASSERT_STRING("Pointer of type 'any*'", pointer1.describeValue());
  ASSERT_EQ(ValueFlags::None, pointer1->getPrimitiveFlags());
  ASSERT_EQ(1u, pointer1->getObjectShapeCount());
  auto* shape = pointer1->getObjectShape(0);
  ASSERT_NE(nullptr, shape->pointable);
  ASSERT_TYPE(Type::Any, shape->pointable->getType());
  ASSERT_EQ(modifiability, shape->pointable->getModifiability());
  auto pointer2 = factory.createPointer(Type::Any, modifiability);
  ASSERT_STRING("Pointer of type 'any*'", pointer2.describeValue());
  ASSERT_EQ(pointer1.get(), pointer2.get());
}

TEST(TestType, FactoryUnion0) {
  egg::test::Allocator allocator{ egg::test::Allocator::Expectation::NoAllocations };
  TypeFactory factory{ allocator };
  auto merged = factory.createUnion({});
  ASSERT_EQ(nullptr, merged);
}

TEST(TestType, FactoryUnionBasic1) {
  egg::test::Allocator allocator{ egg::test::Allocator::Expectation::NoAllocations };
  TypeFactory factory{ allocator };
  auto merged = factory.createUnion({ Type::Arithmetic });
  ASSERT_STRING("Value of type 'int|float'", merged.describeValue());
  ASSERT_EQ(Type::Arithmetic.get(), merged.get());
}

TEST(TestType, FactoryUnionBasic2) {
  egg::test::Allocator allocator{ egg::test::Allocator::Expectation::NoAllocations };
  TypeFactory factory{ allocator };
  auto merged = factory.createUnion({ Type::Int, Type::Float });
  ASSERT_STRING("Value of type 'int|float'", merged.describeValue());
  ASSERT_EQ(Type::Arithmetic.get(), merged.get());
  merged = factory.createUnion({ Type::Float, Type::Int });
  ASSERT_STRING("Value of type 'int|float'", merged.describeValue());
  ASSERT_EQ(Type::Arithmetic.get(), merged.get());
  merged = factory.createUnion({ Type::Float, Type::Arithmetic });
  ASSERT_STRING("Value of type 'int|float'", merged.describeValue());
  ASSERT_EQ(Type::Arithmetic.get(), merged.get());
}

TEST(TestType, FactoryUnionComplex1) {
  egg::test::Allocator allocator{ egg::test::Allocator::Expectation::AtLeastOneAllocation };
  TypeFactory factory{ allocator };
  auto modifiability = Modifiability::Read | Modifiability::Write | Modifiability::Mutate;
  auto pointer = factory.createPointer(Type::Int, modifiability);
  ASSERT_STRING("Pointer of type 'int*'", pointer.describeValue());
  ASSERT_STRING("int*", pointer.toString());
  auto merged = factory.createUnion({ pointer });
  ASSERT_STRING("Pointer of type 'int*'", merged.describeValue());
  ASSERT_STRING("int*", merged.toString());
  ASSERT_EQ(pointer.get(), merged.get());
}

TEST(TestType, FactoryUnionComplex2) {
  egg::test::Allocator allocator{ egg::test::Allocator::Expectation::AtLeastOneAllocation };
  TypeFactory factory{ allocator };
  auto modifiability = Modifiability::Read | Modifiability::Write | Modifiability::Mutate;
  auto pointer1 = factory.createPointer(Type::Int, modifiability);
  ASSERT_STRING("Pointer of type 'int*'", pointer1.describeValue());
  ASSERT_STRING("int*", pointer1.toString());
  auto merged11 = factory.createUnion({ pointer1, pointer1 });
  ASSERT_STRING("Pointer of type 'int*'", merged11.describeValue());
  ASSERT_STRING("int*", merged11.toString());
  ASSERT_EQ(pointer1.get(), merged11.get());
  auto pointer2 = factory.createPointer(Type::Float, modifiability);
  ASSERT_STRING("Pointer of type 'float*'", pointer2.describeValue());
  ASSERT_STRING("float*", pointer2.toString());
  auto merged12 = factory.createUnion({ pointer1, pointer2 });
  ASSERT_STRING("Value of type 'float*|int*'", merged12.describeValue());
  ASSERT_STRING("float*|int*", merged12.toString());
  ASSERT_NE(merged11.get(), merged12.get());
  auto merged21 = factory.createUnion({ pointer2, pointer1 });
  ASSERT_STRING("Value of type 'float*|int*'", merged21.describeValue());
  ASSERT_STRING("float*|int*", merged21.toString());
  ASSERT_EQ(merged12.get(), merged21.get());
}

TEST(TestType, FactoryFunctionBuilderTrivial) {
  egg::test::Allocator allocator;
  TypeFactory factory{ allocator };
  auto builder = factory.createFunctionBuilder(Type::Int, "function");
  auto built = builder->build();
  ASSERT_STRING("int()", built.toString());
  ASSERT_STRING("Function 'int function()'", built->describeValue());
}

TEST(TestType, FactoryFunctionBuilderSimple) {
  egg::test::Allocator allocator;
  TypeFactory factory{ allocator };
  auto builder = factory.createFunctionBuilder(Type::Arithmetic, "function");
  builder->addPositionalParameter(Type::Bool, "arg1", egg::ovum::IFunctionSignatureParameter::Flags::Required);
  builder->addPositionalParameter(Type::String, "arg2", egg::ovum::IFunctionSignatureParameter::Flags::Required);
  auto built = builder->build();
  ASSERT_STRING("(int|float)(bool,string)", built.toString());
  ASSERT_STRING("Function '(int|float) function(bool arg1, string arg2)'", built->describeValue());
}

TEST(TestType, FactoryGeneratorBuilderTrivial) {
  egg::test::Allocator allocator;
  TypeFactory factory{ allocator };
  auto builder = factory.createGeneratorBuilder(Type::Int, "generator");
  auto built = builder->build();
  ASSERT_STRING("int...()", built.toString());
  ASSERT_STRING("Generator 'int... generator()'", built->describeValue());
}

TEST(TestType, FactoryGeneratorBuilderSimple) {
  egg::test::Allocator allocator;
  TypeFactory factory{ allocator };
  auto builder = factory.createGeneratorBuilder(Type::Arithmetic, "generator");
  builder->addPositionalParameter(Type::Bool, "arg1", egg::ovum::IFunctionSignatureParameter::Flags::Required);
  builder->addPositionalParameter(Type::String, "arg2", egg::ovum::IFunctionSignatureParameter::Flags::Required);
  auto built = builder->build();
  ASSERT_STRING("(int|float)...(bool,string)", built.toString());
  ASSERT_STRING("Generator '(int|float)... generator(bool arg1, string arg2)'", built->describeValue());
}

TEST(TestType, FactoryTypeBuilderTrivial) {
  egg::test::Allocator allocator;
  TypeFactory factory{ allocator };
  auto builder = factory.createTypeBuilder("CustomType");
  auto built = builder->build();
  ASSERT_STRING("CustomType", built.toString());
  ASSERT_STRING("Value of type 'CustomType'", built->describeValue());
}

TEST(TestType, FactoryTypeBuilderDotable) {
  egg::test::Allocator allocator;
  TypeFactory factory{ allocator };
  auto builder = factory.createTypeBuilder("CustomDotable");
  builder->defineDotable(nullptr, egg::ovum::Modifiability::None);
  builder->addProperty(Type::String, "name", egg::ovum::Modifiability::Read);
  builder->addProperty(Type::Int, "age", egg::ovum::Modifiability::Read | Modifiability::Delete);
  auto built = builder->build();
  ASSERT_STRING("CustomDotable", built.toString());
  ASSERT_STRING("Value of type 'CustomDotable'", built->describeValue());
}

TEST(TestType, FactoryTypeBuilderIndexable) {
  egg::test::Allocator allocator;
  TypeFactory factory{ allocator };
  auto builder = factory.createTypeBuilder("CustomIndexable");
  builder->defineIndexable(Type::Float, nullptr, egg::ovum::Modifiability::Read | egg::ovum::Modifiability::Write);
  auto built = builder->build();
  ASSERT_STRING("CustomIndexable", built.toString());
  ASSERT_STRING("Value of type 'CustomIndexable'", built->describeValue());
}

TEST(TestType, FactoryTypeBuilderIterable) {
  egg::test::Allocator allocator;
  TypeFactory factory{ allocator };
  auto builder = factory.createTypeBuilder("CustomIterable");
  builder->defineIterable(Type::Any);
  auto built = builder->build();
  ASSERT_STRING("CustomIterable", built.toString());
  ASSERT_STRING("Value of type 'CustomIterable'", built->describeValue());
}
