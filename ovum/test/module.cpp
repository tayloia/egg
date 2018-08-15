#include "ovum/test.h"
#include "ovum/node.h"
#include "ovum/module.h"

#define EGG_VM_MAGIC_BYTE(byte) byte,
#define MAGIC EGG_VM_MAGIC(EGG_VM_MAGIC_BYTE)

using namespace egg::ovum;

namespace {
  void expectFailureFromMemory(const uint8_t memory[], size_t bytes, const char* needle) {
    egg::test::Allocator allocator;
    ASSERT_THROW_E(ModuleFactory::fromMemory(allocator, memory, memory + bytes), std::runtime_error, ASSERT_STARTSWITH(e.what(), needle));
  }
  void toModuleArray(ModuleBuilder& builder, const Nodes& avalues, Module& out) {
    // Create a module that just constructs an array of values
    auto array = builder.createValueArray(avalues);
    ASSERT_NE(nullptr, array);
    auto block = builder.createNode(OPCODE_BLOCK, array);
    ASSERT_NE(nullptr, block);
    auto module = builder.createModule(block);
    ASSERT_NE(nullptr, module);
    out = ModuleFactory::fromRootNode(builder.allocator, *module);
  }
  void toModuleMemoryArray(ModuleBuilder& builder, const Nodes& avalues, std::ostream& out) {
    // Create a module memory image that just constructs an array of values
    Module module;
    toModuleArray(builder, avalues, module);
    ModuleFactory::toBinaryStream(*module, out);
  }
  void fromModuleArray(const Module& in, Node& avalue) {
    // Extract an array of values from a module
    ASSERT_NE(nullptr, in);
    Node root{ &in->getRootNode() };
    ASSERT_EQ(OPCODE_MODULE, root->getOpcode());
    ASSERT_EQ(1u, root->getChildren());
    Node child{ &root->getChild(0) };
    ASSERT_EQ(OPCODE_BLOCK, child->getOpcode());
    ASSERT_EQ(1u, child->getChildren());
    avalue.set(&child->getChild(0));
    ASSERT_EQ(OPCODE_AVALUE, avalue->getOpcode());
  }
  void fromModuleMemoryArray(IAllocator& allocator, std::istream& in, Node& avalue) {
    // Extract an array of values from a module memory image
    in.clear();
    ASSERT_TRUE(in.seekg(0).good());
    auto module = ModuleFactory::fromBinaryStream(allocator, in);
    fromModuleArray(module, avalue);
  }
  Node roundTripArray(ModuleBuilder& builder, const Nodes& avalues) {
    // Create a module memory image and then extract the array values
    std::stringstream ss;
    toModuleMemoryArray(builder, avalues, ss);
    Node avalue;
    fromModuleMemoryArray(builder.allocator, ss, avalue);
    return avalue;
  }
}

TEST(TestModule, FromMemoryBad) {
  const uint8_t zero[] = { 0 };
  expectFailureFromMemory(zero, sizeof(zero), "Invalid magic signature in binary module");
  const uint8_t magic[] = { MAGIC 99 }; // This is an invalid section number
  expectFailureFromMemory(magic, 0, "Truncated section in binary module");
  expectFailureFromMemory(magic, 1, "Truncated section in binary module");
  expectFailureFromMemory(magic, sizeof(magic) - 1, "Missing code section in binary module");
  expectFailureFromMemory(magic, sizeof(magic), "Unrecognized section in binary module");
}

TEST(TestModule, FromMemoryMinimal) {
  egg::test::Allocator allocator;
  const uint8_t minimal[] = { MAGIC SECTION_CODE, OPCODE_MODULE, OPCODE_BLOCK, OPCODE_NOOP };
  auto module = ModuleFactory::fromMemory(allocator, std::begin(minimal), std::end(minimal));
  ASSERT_NE(nullptr, module);
  Node root{ &module->getRootNode() };
  ASSERT_NE(nullptr, root);
  ASSERT_EQ(OPCODE_MODULE, root->getOpcode());
  ASSERT_EQ(1u, root->getChildren());
  Node child{ &root->getChild(0) };
  ASSERT_EQ(OPCODE_BLOCK, child->getOpcode());
  ASSERT_EQ(1u, child->getChildren());
  Node grandchild{ &child->getChild(0) };
  ASSERT_EQ(OPCODE_NOOP, grandchild->getOpcode());
  ASSERT_EQ(0u, grandchild->getChildren());
}

TEST(TestModule, ModuleBuilder) {
  egg::test::Allocator allocator;
  ModuleBuilder builder(allocator);
  auto noop = builder.createNode(OPCODE_NOOP);
  auto block = builder.createNode(OPCODE_BLOCK, noop);
  auto original = builder.createModule(block);
  auto module = ModuleFactory::fromRootNode(allocator, *original);
  ASSERT_NE(nullptr, module);
  Node root{ &module->getRootNode() };
  ASSERT_EQ(original.get(), root.get());
  ASSERT_EQ(OPCODE_MODULE, root->getOpcode());
  ASSERT_EQ(1u, root->getChildren());
  Node child{ &root->getChild(0) };
  ASSERT_EQ(OPCODE_BLOCK, child->getOpcode());
  ASSERT_EQ(1u, child->getChildren());
  Node grandchild{ &child->getChild(0) };
  ASSERT_EQ(OPCODE_NOOP, grandchild->getOpcode());
  ASSERT_EQ(0u, grandchild->getChildren());
}

TEST(TestModule, BuildConstantInt) {
  egg::test::Allocator allocator;
  ModuleBuilder builder(allocator);
  auto avalue = roundTripArray(builder, {
    builder.createValueInt(123456789),
    builder.createValueInt(-123456789)
  });
  ASSERT_EQ(2u, avalue->getChildren());
  Node value;
  value.set(&avalue->getChild(0));
  ASSERT_EQ(OPCODE_IVALUE, value->getOpcode());
  ASSERT_EQ(123456789, value->getInt());
  ASSERT_EQ(0u, value->getChildren());
  value.set(&avalue->getChild(1));
  ASSERT_EQ(OPCODE_IVALUE, value->getOpcode());
  ASSERT_EQ(-123456789, value->getInt());
  ASSERT_EQ(0u, value->getChildren());
}

TEST(TestModule, BuildConstantFloat) {
  egg::test::Allocator allocator;
  ModuleBuilder builder(allocator);
  auto avalue = roundTripArray(builder, {
    builder.createValueFloat(123456789),
    builder.createValueFloat(-123456789),
    builder.createValueFloat(-0.125),
    builder.createValueFloat(std::numeric_limits<double>::quiet_NaN())
  });
  ASSERT_EQ(4u, avalue->getChildren());
  Node value;
  value.set(&avalue->getChild(0));
  ASSERT_EQ(OPCODE_FVALUE, value->getOpcode());
  ASSERT_EQ(123456789.0, value->getFloat());
  ASSERT_EQ(0u, value->getChildren());
  value.set(&avalue->getChild(1));
  ASSERT_EQ(OPCODE_FVALUE, value->getOpcode());
  ASSERT_EQ(-123456789.0, value->getFloat());
  ASSERT_EQ(0u, value->getChildren());
  value.set(&avalue->getChild(2));
  ASSERT_EQ(OPCODE_FVALUE, value->getOpcode());
  ASSERT_EQ(-0.125, value->getFloat());
  ASSERT_EQ(0u, value->getChildren());
  value.set(&avalue->getChild(3));
  ASSERT_EQ(OPCODE_FVALUE, value->getOpcode());
  ASSERT_TRUE(std::isnan(value->getFloat()));
  ASSERT_EQ(0u, value->getChildren());
}

TEST(TestModule, BuildConstantString) {
  egg::test::Allocator allocator;
  ModuleBuilder builder(allocator);
  auto avalue = roundTripArray(builder, {
    builder.createValueString(""),
    builder.createValueString("hello")
  });
  ASSERT_EQ(2u, avalue->getChildren());
  Node value;
  value.set(&avalue->getChild(0));
  ASSERT_EQ(OPCODE_SVALUE, value->getOpcode());
  ASSERT_STRING("", value->getString());
  ASSERT_EQ(0u, value->getChildren());
  value.set(&avalue->getChild(1));
  ASSERT_EQ(OPCODE_SVALUE, value->getOpcode());
  ASSERT_STRING("hello", value->getString());
  ASSERT_EQ(0u, value->getChildren());
}

TEST(TestModule, BuildOperator) {
  egg::test::Allocator allocator;
  ModuleBuilder builder(allocator);
  auto avalue = roundTripArray(builder, {
    builder.createOperator(OPCODE_UNARY, 123456789, { builder.createNode(OPCODE_NULL) })
  });
  ASSERT_EQ(1u, avalue->getChildren());
  Node value;
  value.set(&avalue->getChild(0));
  ASSERT_EQ(OPCODE_UNARY, value->getOpcode());
  ASSERT_EQ(123456789, value->getInt());
  ASSERT_EQ(1u, value->getChildren());
  value.set(&value->getChild(0));
  ASSERT_EQ(OPCODE_NULL, value->getOpcode());
  ASSERT_EQ(0u, value->getChildren());
}

TEST(TestModule, BuildAddAttribute) {
  egg::test::Allocator allocator;
  ModuleBuilder builder(allocator);
  auto avalue = roundTripArray(builder, {
    builder.createOperator(OPCODE_UNARY, 123456789,{ builder.createNode(OPCODE_NULL) })
    });
  ASSERT_EQ(1u, avalue->getChildren());
  Node value;
  value.set(&avalue->getChild(0));
  ASSERT_EQ(OPCODE_UNARY, value->getOpcode());
  ASSERT_EQ(123456789, value->getInt());
  ASSERT_EQ(1u, value->getChildren());
  value.set(&value->getChild(0));
  ASSERT_EQ(OPCODE_NULL, value->getOpcode());
  ASSERT_EQ(0u, value->getChildren());
}
