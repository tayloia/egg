#include "ovum/test.h"
#include "ovum/ast.h"

#define EGG_VM_MAGIC_BYTE(byte) byte,
#define MAGIC EGG_VM_MAGIC(EGG_VM_MAGIC_BYTE)

using namespace egg::ovum;
using namespace egg::ovum::ast;

namespace {
  void expectFailureFromMemory(const uint8_t memory[], size_t bytes, const char* needle) {
    egg::test::Allocator allocator;
    ASSERT_THROW_E(ModuleFactory::fromMemory(allocator, memory, memory + bytes), std::runtime_error, ASSERT_STARTSWITH(e.what(), needle));
  }

  void toModuleArray(ModuleBuilder& builder, Nodes&& avalues, Module& out) {
    auto array = builder.createValueArray(std::move(avalues));
    ASSERT_NE(nullptr, array);
    auto block = builder.createBlock({ std::move(array) });
    ASSERT_NE(nullptr, block);
    auto module = builder.createModule(std::move(block));
    ASSERT_NE(nullptr, module);
    out = ModuleFactory::fromRootNode(builder.allocator, *module);
  }
  void toModuleMemoryArray(ModuleBuilder& builder, Nodes&& avalues, std::ostream& out) {
    Module module;
    toModuleArray(builder, std::move(avalues), module);
    ModuleFactory::toBinaryStream(*module, out);
  }
  void fromModuleArray(const Module& in, Node& avalue) {
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
    in.clear();
    ASSERT_TRUE(in.seekg(0).good());
    auto module = ModuleFactory::fromBinaryStream(allocator, in);
    fromModuleArray(module, avalue);
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
  auto noop = builder.createNoop();
  auto block = builder.createBlock({ std::move(noop) });
  auto original = builder.createModule(std::move(block));
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
  Module module;
  toModuleArray(builder, { builder.createValueInt(123456789), builder.createValueInt(-123456789) }, module);
  ASSERT_NE(nullptr, module);
  Node avalue;
  fromModuleArray(module, avalue);
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
