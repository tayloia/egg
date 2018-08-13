#include "ovum/test.h"
#include "ovum/ast.h"

#define EGG_VM_MAGIC_BYTE(byte) byte,
#define MAGIC EGG_VM_MAGIC(EGG_VM_MAGIC_BYTE)

namespace {
  void expectFailureFromMemory(const uint8_t memory[], size_t bytes, const char* needle) {
    egg::test::Allocator allocator;
    ASSERT_THROW_E(egg::ovum::ModuleFactory::fromMemory(allocator, memory, memory + bytes), std::runtime_error, ASSERT_STARTSWITH(e.what(), needle));
  }

  enum Section {
#define EGG_VM_SECTIONS_ENUM(section, value) section = value,
    EGG_VM_SECTIONS(EGG_VM_SECTIONS_ENUM)
#undef EGG_VM_SECTIONS_ENUM
  };

  enum Opcode {
#define EGG_VM_OPCODES_TABLE(opcode, minbyte, minargs, maxargs, text) opcode = minbyte,
    EGG_VM_OPCODES(EGG_VM_OPCODES_TABLE)
#undef EGG_VM_OPCODES_TABLE
  };
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
  auto module = egg::ovum::ModuleFactory::fromMemory(allocator, std::begin(minimal), std::end(minimal));
  ASSERT_NE(nullptr, module);
  egg::ovum::ast::Node root{ &module->getRootNode() };
  ASSERT_NE(nullptr, root);
  ASSERT_EQ(Opcode::OPCODE_MODULE, root->getOpcode());
  ASSERT_EQ(1u, root->getChildren());
  egg::ovum::ast::Node child{ &root->getChild(0) };
  ASSERT_EQ(Opcode::OPCODE_BLOCK, child->getOpcode());
  ASSERT_EQ(1u, child->getChildren());
  egg::ovum::ast::Node grandchild{ &child->getChild(0) };
  ASSERT_EQ(Opcode::OPCODE_NOOP, grandchild->getOpcode());
  ASSERT_EQ(0u, grandchild->getChildren());
}
