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
      return String::fromUTF8(this->allocator, name);
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
  fb->addRequiredParameter(forge.makeName("a"), Type::Int);
  auto& built1 = fb->build();
  ASSERT_STRING("void f(int a)", forge.toTypeString(built1));
  fb = forge->createFunctionBuilder();
  fb->setReturnType(Type::Void);
  fb->setFunctionName(forge.makeName("f"));
  fb->addRequiredParameter(forge.makeName("b"), Type::Int);
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
  fb->addRequiredParameter(forge.makeName("a"), Type::Int);
  auto& built1 = fb->build();
  ASSERT_STRING("void f(int a)", forge.toTypeString(built1));
  fb = forge->createFunctionBuilder();
  fb->setReturnType(Type::Void);
  fb->setFunctionName(forge.makeName("f"));
  fb->addRequiredParameter(forge.makeName("b"), Type::Arithmetic);
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
  fb->addRequiredParameter(forge.makeName("a"), Type::Int);
  auto& built1 = fb->build();
  ASSERT_STRING("void f(int a)", forge.toTypeString(built1));
  fb = forge->createFunctionBuilder();
  fb->setReturnType(Type::Void);
  fb->setFunctionName(forge.makeName("f"));
  fb->addRequiredParameter(forge.makeName("b"), Type::String);
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
  fb->addRequiredParameter(forge.makeName("a"), forge.makePrimitive(ValueFlags::Int));
  fb->addOptionalParameter(forge.makeName("b"), forge.makePrimitive(ValueFlags::String | ValueFlags::Null));
  auto& built = fb->build();
  ASSERT_STRING("void f(int a, string? b = null)", forge.toTypeString(built));
}

TEST(TestType, ForgePropertySignatureClosed) {
  TestForge forge;
  auto pb = forge->createPropertyBuilder();
  pb->addProperty(forge.makeName("alpha"), Type::Int, Accessability::Get | Accessability::Set);
  auto& built = pb->build();
  ASSERT_EQ(1u, built.getNameCount());
  ASSERT_TRUE(built.isClosed());
  auto name = built.getName(0);
  ASSERT_STRING("alpha", name);
  ASSERT_TYPE(Type::Int, built.getType(name));
  ASSERT_EQ(Accessability::Get | Accessability::Set, built.getAccessability(name));
  name = forge.allocator.concat("omega");
  ASSERT_TYPE(nullptr, built.getType(name));
  ASSERT_EQ(Accessability::None, built.getAccessability(name));
}

TEST(TestType, ForgePropertySignatureOpen) {
  TestForge forge;
  auto pb = forge->createPropertyBuilder();
  pb->addProperty(forge.makeName("alpha"), Type::Int, Accessability::Get);
  pb->addProperty(forge.makeName("beta"), Type::Arithmetic, Accessability::Get | Accessability::Set);
  pb->setUnknownProperty(Type::String, Accessability::All);
  auto& built = pb->build();
  ASSERT_EQ(2u, built.getNameCount());
  ASSERT_FALSE(built.isClosed());
  auto name = built.getName(0);
  ASSERT_STRING("alpha", name);
  ASSERT_TYPE(Type::Int, built.getType(name));
  ASSERT_EQ(Accessability::Get, built.getAccessability(name));
  name = forge.allocator.concat("beta");
  ASSERT_TYPE(Type::Arithmetic, built.getType(name));
  ASSERT_EQ(Accessability::Get | Accessability::Set, built.getAccessability(name));
  name = forge.allocator.concat("omega");
  ASSERT_TYPE(Type::String, built.getType(name));
  ASSERT_EQ(Accessability::All, built.getAccessability(name));
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
  cb->addShape(forge->forgeArrayShape(Type::Int, Accessability::All));
  auto built = cb->build();
  ASSERT_FALSE(built->isPrimitive());
  ASSERT_EQ(ValueFlags::None, built->getPrimitiveFlags());
  ASSERT_EQ(1u, built->getShapeCount());
  ASSERT_STRING("int[]", forge.toTypeString(built));
}

TEST(TestType, ForgeComplexArithmeticArray) {
  TestForge forge;
  auto cb = forge->createComplexBuilder();
  cb->addShape(forge->forgeArrayShape(Type::Arithmetic, Accessability::All));
  auto built = cb->build();
  ASSERT_FALSE(built->isPrimitive());
  ASSERT_EQ(ValueFlags::None, built->getPrimitiveFlags());
  ASSERT_EQ(1u, built->getShapeCount());
  ASSERT_STRING("(float|int)[]", forge.toTypeString(built));
}

TEST(TestType, ForgeComplexIntPointer) {
  TestForge forge;
  auto cb = forge->createComplexBuilder();
  cb->addShape(forge->forgePointerShape(Type::Int, Modifiability::All));
  auto built = cb->build();
  ASSERT_FALSE(built->isPrimitive());
  ASSERT_EQ(ValueFlags::None, built->getPrimitiveFlags());
  ASSERT_EQ(1u, built->getShapeCount());
  ASSERT_STRING("int*", forge.toTypeString(built));
}

TEST(TestType, ForgeComplexArithmeticPointer) {
  TestForge forge;
  auto cb = forge->createComplexBuilder();
  cb->addShape(forge->forgePointerShape(Type::Arithmetic, Modifiability::All));
  auto built = cb->build();
  ASSERT_FALSE(built->isPrimitive());
  ASSERT_EQ(ValueFlags::None, built->getPrimitiveFlags());
  ASSERT_EQ(1u, built->getShapeCount());
  ASSERT_STRING("(float|int)*", forge.toTypeString(built));
}

TEST(TestType, ForgeFunctionTwice) {
  TestForge forge;
  auto fb1 = forge->createFunctionBuilder();
  fb1->setReturnType(forge->forgeNullableType(Type::Int, true));
  auto& fs1 = fb1->build();
  ASSERT_STRING("int?()", forge.toTypeString(fs1));
  auto fb2 = forge->createFunctionBuilder();
  fb2->setFunctionName(forge.makeName("fname")); // shouldn't be involved in matching
  fb2->setReturnType(forge->forgeNullableType(Type::Int, true));
  auto& fs2 = fb2->build();
  ASSERT_STRING("int? fname()", forge.toTypeString(fs2));
  ASSERT_NE(&fs1, &fs2);
  ASSERT_EQ(Assignability::Always, forge->isFunctionSignatureAssignable(fs1, fs2));
  ASSERT_EQ(Assignability::Always, forge->isFunctionSignatureAssignable(fs2, fs1));
  auto shape1 = forge->forgeFunctionShape(fs1);
  auto shape2 = forge->forgeFunctionShape(fs2);
  ASSERT_NE(&shape1.get(), &shape2.get());
  auto type1 = forge->forgeFunctionType(fs1);
  ASSERT_STRING("int?()", forge.toTypeString(type1));
  auto type2 = forge->forgeFunctionType(fs2);
  ASSERT_STRING("int?()", forge.toTypeString(type2));
  ASSERT_NE(type1.get(), type2.get());
  ASSERT_NE(type1, type2);
  ASSERT_EQ(Assignability::Always, forge->isTypeAssignable(type1, type2));
  ASSERT_EQ(Assignability::Always, forge->isTypeAssignable(type2, type1));
}
