#include "ovum/test.h"

using Flags = egg::ovum::ValueFlags;
using Type = egg::ovum::Type;

namespace {
  using namespace egg::ovum;

  struct TestForge final {
    egg::test::Allocator allocator;
    HardPtr<IBasket> basket;
    HardPtr<ITypeForge> forge;
    TestForge() {
      this->basket = BasketFactory::createBasket(this->allocator);
      this->forge = TypeForgeFactory::createTypeForge(allocator, *this->basket);
      assert(this->forge != nullptr);
    }
    ITypeForge* operator->() const {
      return this->forge.get();
    }
    Type makePrimitive(ValueFlags flags) {
      return this->forge->forgePrimitiveType(flags);
    }
    String makeName(const char* name) {
      return this->allocator.concat(name);
    }
    template<typename T>
    String toString(const T& value) {
      StringBuilder sb;
      Type::print(sb, value);
      return sb.build(this->allocator);
    }
  };
}

TEST(TestType, ForgePrimitiveVoid) {
  TestForge forge;
  auto forged = forge->forgePrimitiveType(Flags::Void);
  ASSERT_EQ(Type::Void, forged);
  ASSERT_TRUE(forged->isPrimitive());
  ASSERT_EQ(egg::ovum::ValueFlags::Void, forged->getPrimitiveFlags());
}

TEST(TestType, ForgePrimitiveUnion) {
  TestForge forge;
  auto forged = forge->forgeUnionType(Type::Int, Type::Float);
  ASSERT_EQ(Type::Arithmetic, forged);
  ASSERT_TRUE(forged->isPrimitive());
  ASSERT_EQ(egg::ovum::ValueFlags::Arithmetic, forged->getPrimitiveFlags());
}

TEST(TestType, ForgePrimitiveNullable) {
  TestForge forge;
  auto forged = forge->forgePrimitiveType(Flags::Any);
  forged = forge->forgeNullableType(forged, true);
  ASSERT_EQ(Type::AnyQ, forged);
  forged = forge->forgeNullableType(forged, true);
  ASSERT_EQ(Type::AnyQ, forged);
  forged = forge->forgeNullableType(forged, false);
  ASSERT_EQ(Type::Any, forged);
  forged = forge->forgeNullableType(forged, false);
  ASSERT_EQ(Type::Any, forged);
}

TEST(TestType, ForgePrimitiveVoidable) {
  TestForge forge;
  auto voidable = forge->forgePrimitiveType(Flags::Void | Flags::Int);
  auto forged = forge->forgePrimitiveType(Flags::Int);
  forged = forge->forgeVoidableType(forged, true);
  ASSERT_EQ(voidable, forged);
  forged = forge->forgeVoidableType(forged, true);
  ASSERT_EQ(voidable, forged);
  forged = forge->forgeVoidableType(forged, false);
  ASSERT_EQ(Type::Int, forged);
  forged = forge->forgeVoidableType(forged, false);
  ASSERT_EQ(Type::Int, forged);
}

TEST(TestType, ForgeFunctionSignatureAssignableAlways) {
  TestForge forge;
  auto fb = forge->createFunctionBuilder();
  fb->setReturnType(Type::Void);
  fb->setFunctionName(forge.makeName("f"));
  fb->addRequiredParameter(Type::Int, forge.makeName("a"));
  auto& built1 = fb->build();
  ASSERT_STRING("void f(int a)", forge.toString(built1));
  fb = forge->createFunctionBuilder();
  fb->setReturnType(Type::Void);
  fb->setFunctionName(forge.makeName("f"));
  fb->addRequiredParameter(Type::Int, forge.makeName("b"));
  auto& built2 = fb->build();
  ASSERT_STRING("void f(int b)", forge.toString(built2));
  ASSERT_NE(&built1, &built2);
  ASSERT_EQ(egg::ovum::ITypeForge::Assignability::Always, forge->isFunctionSignatureAssignable(built1, built2));
  ASSERT_EQ(egg::ovum::ITypeForge::Assignability::Always, forge->isFunctionSignatureAssignable(built2, built1));
}

TEST(TestType, ForgeFunctionSignatureAssignableSometimes) {
  TestForge forge;
  auto fb = forge->createFunctionBuilder();
  fb->setReturnType(Type::Void);
  fb->setFunctionName(forge.makeName("f"));
  fb->addRequiredParameter(Type::Int, forge.makeName("a"));
  auto& built1 = fb->build();
  ASSERT_STRING("void f(int a)", forge.toString(built1));
  fb = forge->createFunctionBuilder();
  fb->setReturnType(Type::Void);
  fb->setFunctionName(forge.makeName("f"));
  fb->addRequiredParameter(Type::Arithmetic, forge.makeName("b"));
  auto& built2 = fb->build();
  ASSERT_STRING("void f(float|int b)", forge.toString(built2));
  ASSERT_NE(&built1, &built2);
  ASSERT_EQ(egg::ovum::ITypeForge::Assignability::Sometimes, forge->isFunctionSignatureAssignable(built1, built2));
  ASSERT_EQ(egg::ovum::ITypeForge::Assignability::Always, forge->isFunctionSignatureAssignable(built2, built1));
}

TEST(TestType, ForgeFunctionSignatureAssignableNever) {
  TestForge forge;
  auto fb = forge->createFunctionBuilder();
  fb->setReturnType(Type::Void);
  fb->setFunctionName(forge.makeName("f"));
  fb->addRequiredParameter(Type::Int, forge.makeName("a"));
  auto& built1 = fb->build();
  ASSERT_STRING("void f(int a)", forge.toString(built1));
  fb = forge->createFunctionBuilder();
  fb->setReturnType(Type::Void);
  fb->setFunctionName(forge.makeName("f"));
  fb->addRequiredParameter(Type::String, forge.makeName("b"));
  auto& built2 = fb->build();
  ASSERT_STRING("void f(string b)", forge.toString(built2));
  ASSERT_NE(&built1, &built2);
  ASSERT_EQ(egg::ovum::ITypeForge::Assignability::Never, forge->isFunctionSignatureAssignable(built1, built2));
  ASSERT_EQ(egg::ovum::ITypeForge::Assignability::Never, forge->isFunctionSignatureAssignable(built2, built1));
}

TEST(TestType, ForgeFunction) {
  TestForge forge;
  auto fb = forge->createFunctionBuilder();
  fb->setReturnType(Type::Void);
  fb->setFunctionName(forge.makeName("f"));
  fb->addRequiredParameter(forge.makePrimitive(ValueFlags::Int), forge.makeName("a"));
  fb->addOptionalParameter(forge.makePrimitive(ValueFlags::String | ValueFlags::Null), forge.makeName("b"));
  auto& built = fb->build();
  ASSERT_STRING("void f(int a, string? b = null)", forge.toString(built));
}
