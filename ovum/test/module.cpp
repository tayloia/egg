#include "ovum/test.h"

#define EGG_VM_MAGIC_BYTE(byte) byte,
#define MAGIC EGG_VM_MAGIC(EGG_VM_MAGIC_BYTE)

namespace {
  void expectFailureFromMemory(const uint8_t memory[], size_t bytes, const char* needle) {
    egg::test::Allocator allocator;
    ASSERT_THROW_E(egg::ovum::ModuleFactory::fromMemory(allocator, memory, memory + bytes), std::runtime_error, ASSERT_STARTSWITH(e.what(), needle));
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
