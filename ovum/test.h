#include "ovum/ovum.h"
#include "ovum/test/gtest.h"

namespace egg::test {
  class Allocator final : public egg::ovum::AllocatorDefault {
    Allocator(const Allocator&) = delete;
    Allocator& operator=(const Allocator&) = delete;
  public:
    enum class Expectation {
      NoAllocations,
      AtLeastOneAllocation
    };
  private:
    Expectation expectation;
  public:
    explicit Allocator(Expectation expectation = Expectation::AtLeastOneAllocation) : expectation(expectation) {}
    ~Allocator() {
      this->validate();
    }
    void validate() const {
      egg::ovum::IAllocator::Statistics stats;
      ASSERT_TRUE(this->statistics(stats));
      ASSERT_EQ(stats.currentBlocksAllocated, 0u);
      ASSERT_EQ(stats.currentBytesAllocated, 0u);
      switch (this->expectation) {
      case Expectation::NoAllocations:
        ASSERT_EQ(stats.totalBlocksAllocated, 0u);
        ASSERT_EQ(stats.totalBytesAllocated, 0u);
        break;
      case Expectation::AtLeastOneAllocation:
        ASSERT_GT(stats.totalBlocksAllocated, 0u);
        ASSERT_GT(stats.totalBytesAllocated, 0u);
        break;
      }
    }
  };
}
