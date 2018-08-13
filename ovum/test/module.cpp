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
  const uint8_t minimal[] = { MAGIC SECTION_CODE, OPCODE_MODULE, OPCODE_BLOCK, OPCODE_NOOP };
  egg::test::Allocator allocator;
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
