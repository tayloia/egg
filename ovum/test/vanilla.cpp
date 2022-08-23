#include "ovum/test.h"
#include "ovum/vanilla.h"
#include "ovum/node.h"
#include "ovum/slot.h"

using namespace egg::ovum;

namespace {
  class TestPredicateCallback : public Vanilla::IPredicateCallback {
  public:
    virtual Value predicateCallback(const INode&) override {
      return Value::Void;
    }
  };
}
TEST(TestVanilla, CreateArray) {
  egg::test::Allocator allocator;
  auto basket = BasketFactory::createBasket(allocator);
  {
    auto value = VanillaFactory::createArray(allocator, *basket, 10);
    auto type = value->getRuntimeType();
    ASSERT_STRING("any?[]", type.toString());
    ASSERT_STRING("Array", type.describeValue());
  }
  ASSERT_TRUE(basket->verify(std::cout));
}

TEST(TestVanilla, CreateDictionary) {
  egg::test::Allocator allocator;
  auto basket = BasketFactory::createBasket(allocator);
  {
    auto value = VanillaFactory::createDictionary(allocator, *basket);
    auto type = value->getRuntimeType();
    ASSERT_STRING("any?[string]", type.toString());
    ASSERT_STRING("Dictionary", type.describeValue());
  }
  ASSERT_TRUE(basket->verify(std::cout));
}

TEST(TestVanilla, CreateObject) {
  egg::test::Allocator allocator;
  auto basket = BasketFactory::createBasket(allocator);
  {
    auto value = VanillaFactory::createObject(allocator, *basket);
    auto type = value->getRuntimeType();
    ASSERT_STRING("object", type.toString());
    ASSERT_STRING("Value of type 'object'", type.describeValue());
  }
  ASSERT_TRUE(basket->verify(std::cout));
}

TEST(TestVanilla, CreateKeyValue) {
  egg::test::Allocator allocator;
  auto basket = BasketFactory::createBasket(allocator);
  {
    auto value = VanillaFactory::createKeyValue(allocator, *basket, "Key", Value::True);
    auto type = value->getRuntimeType();
    ASSERT_STRING("object", type.toString());
    ASSERT_STRING("Key-value pair", type.describeValue());
  }
  ASSERT_TRUE(basket->verify(std::cout));
}

TEST(TestVanilla, CreateError) {
  egg::test::Allocator allocator;
  auto basket = BasketFactory::createBasket(allocator);
  {
    LocationSource location{ "bang.egg", 24u, 40u };
    auto value = VanillaFactory::createError(allocator, *basket, location, "Bang!");
    auto type = value->getRuntimeType();
    ASSERT_STRING("object", type.toString());
    ASSERT_STRING("Error", type.describeValue());
  }
  ASSERT_TRUE(basket->verify(std::cout));
}

TEST(TestVanilla, CreatePredicate) {
  egg::test::Allocator allocator;
  auto basket = BasketFactory::createBasket(allocator);
  {
    TestPredicateCallback callback;
    auto node = NodeFactory::createValue(allocator, nullptr);
    auto value = VanillaFactory::createPredicate(allocator, *basket, callback, *node);
    auto type = value->getRuntimeType();
    ASSERT_STRING("void()", type.toString());
    ASSERT_STRING("Predicate", type.describeValue());
  }
  ASSERT_TRUE(basket->verify(std::cout));
}

TEST(TestVanilla, CreatePointer) {
  egg::test::Allocator allocator;
  auto basket = BasketFactory::createBasket(allocator);
  TypeFactory typeFactory{ allocator };
  {
    auto& slot = SlotFactory::createSlot(allocator, *basket, ValueFactory::createASCIIZ(allocator, "Sisters"));
    auto ptype = typeFactory.createPointer(Type::String, Modifiability::Read);
    auto value = VanillaFactory::createPointer(allocator, *basket, slot, ptype, Modifiability::Read);
    auto type = value->getRuntimeType();
    ASSERT_STRING("string*", type.toString());
    ASSERT_STRING("Pointer of type 'string*'", type.describeValue());
  }
  ASSERT_TRUE(basket->verify(std::cout));
}
