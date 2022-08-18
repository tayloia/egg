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
  auto anyq = factory.addNull(any);
  ASSERT_EQ(ValueFlags::AnyQ, anyq->getPrimitiveFlags());
  auto anyqq = factory.addNull(anyq);
  ASSERT_EQ(anyq.get(), anyqq.get());
}

TEST(TestType, FactorySimpleNonBasicAddNull) {
  egg::test::Allocator allocator{ egg::test::Allocator::Expectation::AtLeastOneAllocation };
  TypeFactory factory{ allocator };
  auto arithmetic = factory.createSimple(ValueFlags::Arithmetic);
  auto arithmeticq = factory.addNull(arithmetic);
  ASSERT_EQ(ValueFlags::Null | ValueFlags::Arithmetic, arithmeticq->getPrimitiveFlags());
  auto arithmeticqq = factory.addNull(arithmeticq);
  ASSERT_EQ(arithmeticq.get(), arithmeticqq.get());
}

TEST(TestType, FactorySimpleNonBasicAddVoid) {
  egg::test::Allocator allocator{ egg::test::Allocator::Expectation::AtLeastOneAllocation };
  TypeFactory factory{ allocator };
  auto any = factory.createSimple(ValueFlags::Any);
  auto anyv = factory.addVoid(any);
  ASSERT_EQ(ValueFlags::Void | ValueFlags::Any, anyv->getPrimitiveFlags());
  auto anyvv = factory.addVoid(anyv);
  ASSERT_EQ(anyv.get(), anyvv.get());
}

TEST(TestType, FactorySimpleBasicRemoveNull) {
  egg::test::Allocator allocator{ egg::test::Allocator::Expectation::NoAllocations };
  TypeFactory factory{ allocator };
  auto anyq = factory.createSimple(ValueFlags::AnyQ);
  auto any = factory.removeNull(anyq);
  ASSERT_EQ(ValueFlags::Any, any->getPrimitiveFlags());
  auto any_ = factory.removeNull(any);
  ASSERT_EQ(any.get(), any_.get());
}

TEST(TestType, FactorySimpleNonBasicRemoveNull) {
  egg::test::Allocator allocator{ egg::test::Allocator::Expectation::AtLeastOneAllocation };
  TypeFactory factory{ allocator };
  auto arithmeticq = factory.createSimple(ValueFlags::Null | ValueFlags::Arithmetic);
  auto arithmetic = factory.removeNull(arithmeticq);
  ASSERT_EQ(ValueFlags::Arithmetic, arithmetic->getPrimitiveFlags());
  auto arithmetic_ = factory.removeNull(arithmetic);
  ASSERT_EQ(arithmetic.get(), arithmetic_.get());
}

TEST(TestType, FactorySimpleNonBasicRemoveVoid) {
  egg::test::Allocator allocator{ egg::test::Allocator::Expectation::AtLeastOneAllocation };
  TypeFactory factory{ allocator };
  auto vany = factory.createSimple(ValueFlags::Void | ValueFlags::Any);
  auto any = factory.removeVoid(vany);
  ASSERT_EQ(ValueFlags::Any, any->getPrimitiveFlags());
  auto any_ = factory.removeVoid(any);
  ASSERT_EQ(any.get(), any_.get());
}
