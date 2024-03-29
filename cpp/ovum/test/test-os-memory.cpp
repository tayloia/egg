#include "ovum/test.h"

TEST(TestOS_Memory, Alloc) {
  const size_t alignment = alignof(size_t);
  auto* allocated = egg::ovum::os::memory::alloc(256, alignment);
  egg::ovum::os::memory::free(allocated, alignment);
  ASSERT_NE(nullptr, allocated);
}

TEST(TestOS_Memory, Align) {
  const size_t alignment = 256;
  auto* allocated = egg::ovum::os::memory::alloc(256, alignment);
  auto pv = reinterpret_cast<std::uintptr_t>(allocated);
  egg::ovum::os::memory::free(allocated, alignment);
  ASSERT_NE(0u, pv);
  ASSERT_EQ(0u, pv % alignment);
}

TEST(TestOS_Memory, Size) {
  const size_t alignment = 8;
  auto* allocated = egg::ovum::os::memory::alloc(1, alignment);
  auto size = egg::ovum::os::memory::size(allocated, alignment);
  egg::ovum::os::memory::free(allocated, alignment);
  ASSERT_EQ(1u, size);
}

TEST(TestOS_Memory, Snapshot) {
  auto snapshot = egg::ovum::os::memory::snapshot();
  ASSERT_GT(snapshot.codeCurrentBytes, 0u);
  ASSERT_GT(snapshot.dataCurrentBytes, 0u);
  ASSERT_GT(snapshot.heapMinimumBytes, 0u);
  ASSERT_GE(snapshot.heapCurrentBytes, snapshot.heapMinimumBytes);
  ASSERT_GE(snapshot.heapMaximumBytes, snapshot.heapCurrentBytes);
  ASSERT_GT(snapshot.heapTotalDeallocatedBytes, 0u);
  ASSERT_GE(snapshot.heapTotalAllocatedBytes, snapshot.heapTotalDeallocatedBytes);
}

