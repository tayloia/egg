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
    String toTypeString(const IFunctionSignature& value) {
      StringBuilder sb;
      Type::print(sb, value);
      return sb.build(this->allocator);
    }
    String toTypeString(const Type& value) {
      return this->allocator.concat(value);
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
  ASSERT_STRING("void f(int a)", forge.toTypeString(built1));
  fb = forge->createFunctionBuilder();
  fb->setReturnType(Type::Void);
  fb->setFunctionName(forge.makeName("f"));
  fb->addRequiredParameter(Type::Int, forge.makeName("b"));
  auto& built2 = fb->build();
  ASSERT_STRING("void f(int b)", forge.toTypeString(built2));
  ASSERT_NE(&built1, &built2);
  ASSERT_EQ(egg::ovum::Assignability::Always, forge->isFunctionSignatureAssignable(built1, built2));
  ASSERT_EQ(egg::ovum::Assignability::Always, forge->isFunctionSignatureAssignable(built2, built1));
}

TEST(TestType, ForgeFunctionSignatureAssignableSometimes) {
  TestForge forge;
  auto fb = forge->createFunctionBuilder();
  fb->setReturnType(Type::Void);
  fb->setFunctionName(forge.makeName("f"));
  fb->addRequiredParameter(Type::Int, forge.makeName("a"));
  auto& built1 = fb->build();
  ASSERT_STRING("void f(int a)", forge.toTypeString(built1));
  fb = forge->createFunctionBuilder();
  fb->setReturnType(Type::Void);
  fb->setFunctionName(forge.makeName("f"));
  fb->addRequiredParameter(Type::Arithmetic, forge.makeName("b"));
  auto& built2 = fb->build();
  ASSERT_STRING("void f(float|int b)", forge.toTypeString(built2));
  ASSERT_NE(&built1, &built2);
  ASSERT_EQ(egg::ovum::Assignability::Sometimes, forge->isFunctionSignatureAssignable(built1, built2));
  ASSERT_EQ(egg::ovum::Assignability::Always, forge->isFunctionSignatureAssignable(built2, built1));
}

TEST(TestType, ForgeFunctionSignatureAssignableNever) {
  TestForge forge;
  auto fb = forge->createFunctionBuilder();
  fb->setReturnType(Type::Void);
  fb->setFunctionName(forge.makeName("f"));
  fb->addRequiredParameter(Type::Int, forge.makeName("a"));
  auto& built1 = fb->build();
  ASSERT_STRING("void f(int a)", forge.toTypeString(built1));
  fb = forge->createFunctionBuilder();
  fb->setReturnType(Type::Void);
  fb->setFunctionName(forge.makeName("f"));
  fb->addRequiredParameter(Type::String, forge.makeName("b"));
  auto& built2 = fb->build();
  ASSERT_STRING("void f(string b)", forge.toTypeString(built2));
  ASSERT_NE(&built1, &built2);
  ASSERT_EQ(egg::ovum::Assignability::Never, forge->isFunctionSignatureAssignable(built1, built2));
  ASSERT_EQ(egg::ovum::Assignability::Never, forge->isFunctionSignatureAssignable(built2, built1));
}

TEST(TestType, ForgeFunctionSignature) {
  TestForge forge;
  auto fb = forge->createFunctionBuilder();
  fb->setReturnType(Type::Void);
  fb->setFunctionName(forge.makeName("f"));
  fb->addRequiredParameter(forge.makePrimitive(ValueFlags::Int), forge.makeName("a"));
  fb->addOptionalParameter(forge.makePrimitive(ValueFlags::String | ValueFlags::Null), forge.makeName("b"));
  auto& built = fb->build();
  ASSERT_STRING("void f(int a, string? b = null)", forge.toTypeString(built));
}

TEST(TestType, ForgePropertySignatureClosed) {
  TestForge forge;
  auto pb = forge->createPropertyBuilder();
  pb->addProperty(forge.makeName("alpha"), Type::Int, Modifiability::ReadWrite);
  auto& built = pb->build();
  ASSERT_EQ(1u, built.getNameCount());
  ASSERT_TRUE(built.isClosed());
  auto name = built.getName(0);
  ASSERT_STRING("alpha", name);
  ASSERT_TYPE(Type::Int, built.getType(name));
  ASSERT_EQ(Modifiability::ReadWrite, built.getModifiability(name));
  name = forge.allocator.concat("omega");
  ASSERT_TYPE(nullptr, built.getType(name));
  ASSERT_EQ(Modifiability::None, built.getModifiability(name));
}

TEST(TestType, ForgePropertySignatureOpen) {
  TestForge forge;
  auto pb = forge->createPropertyBuilder();
  pb->addProperty(forge.makeName("alpha"), Type::Int, Modifiability::Read);
  pb->addProperty(forge.makeName("beta"), Type::Arithmetic, Modifiability::ReadWrite);
  pb->setUnknownProperty(Type::String, Modifiability::All);
  auto& built = pb->build();
  ASSERT_EQ(2u, built.getNameCount());
  ASSERT_FALSE(built.isClosed());
  auto name = built.getName(0);
  ASSERT_STRING("alpha", name);
  ASSERT_TYPE(Type::Int, built.getType(name));
  ASSERT_EQ(Modifiability::Read, built.getModifiability(name));
  name = forge.allocator.concat("beta");
  ASSERT_TYPE(Type::Arithmetic, built.getType(name));
  ASSERT_EQ(Modifiability::ReadWrite, built.getModifiability(name));
  name = forge.allocator.concat("omega");
  ASSERT_TYPE(Type::String, built.getType(name));
  ASSERT_EQ(Modifiability::All, built.getModifiability(name));
}

TEST(TestType, ForgeComplexNone) {
  TestForge forge;
  auto cb = forge->createComplexBuilder();
  auto built = cb->build();
  ASSERT_EQ(nullptr, built);
}

TEST(TestType, ForgeComplexArithmetic) {
  TestForge forge;
  auto cb = forge->createComplexBuilder();
  cb->addFlags(ValueFlags::Arithmetic);
  auto built = cb->build();
  ASSERT_TRUE(built->isPrimitive());
  ASSERT_EQ(ValueFlags::Arithmetic, built->getPrimitiveFlags());
  ASSERT_EQ(0u, built->getShapeCount());
  ASSERT_STRING("float|int", forge.toTypeString(built));
}

TEST(TestType, ForgeComplexIntArray) {
  TestForge forge;
  auto cb = forge->createComplexBuilder();
  cb->addShape(forge->forgeArrayShape(Type::Int, Modifiability::All));
  auto built = cb->build();
  ASSERT_FALSE(built->isPrimitive());
  ASSERT_EQ(ValueFlags::None, built->getPrimitiveFlags());
  ASSERT_EQ(1u, built->getShapeCount());
  ASSERT_STRING("int[]", forge.toTypeString(built));
}

TEST(TestType, ForgeComplexArithmeticArray) {
  TestForge forge;
  auto cb = forge->createComplexBuilder();
  cb->addShape(forge->forgeArrayShape(Type::Arithmetic, Modifiability::All));
  auto built = cb->build();
  ASSERT_FALSE(built->isPrimitive());
  ASSERT_EQ(ValueFlags::None, built->getPrimitiveFlags());
  ASSERT_EQ(1u, built->getShapeCount());
  ASSERT_STRING("(float|int)[]", forge.toTypeString(built));
}

TEST(TestType, ForgeComplexIntPointer) {
  TestForge forge;
  auto cb = forge->createComplexBuilder();
  cb->addShape(forge->forgePointerShape(Type::Int, Modifiability::ReadWriteMutate));
  auto built = cb->build();
  ASSERT_FALSE(built->isPrimitive());
  ASSERT_EQ(ValueFlags::None, built->getPrimitiveFlags());
  ASSERT_EQ(1u, built->getShapeCount());
  ASSERT_STRING("int*", forge.toTypeString(built));
}

TEST(TestType, ForgeComplexArithmeticPointer) {
  TestForge forge;
  auto cb = forge->createComplexBuilder();
  cb->addShape(forge->forgePointerShape(Type::Arithmetic, Modifiability::ReadWriteMutate));
  auto built = cb->build();
  ASSERT_FALSE(built->isPrimitive());
  ASSERT_EQ(ValueFlags::None, built->getPrimitiveFlags());
  ASSERT_EQ(1u, built->getShapeCount());
  ASSERT_STRING("(float|int)*", forge.toTypeString(built));
}

