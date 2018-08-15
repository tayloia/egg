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

  class Basket : public egg::ovum::BasketDefault {
  public:
    size_t getOwnedCount() const {
      return this->owned.size();
    }
  };

  inline void assertString(const char* expected, const egg::ovum::String& actual) {
    ASSERT_STREQ(expected, actual.toUTF8().c_str());
  }
  inline void assertString(const egg::ovum::String& expected, const egg::ovum::String& actual) {
    ASSERT_STREQ(expected.toUTF8().c_str(), actual.toUTF8().c_str());
  }
  inline void assertVariant(bool expected, const egg::ovum::Variant& variant) {
    ASSERT_EQ(egg::ovum::VariantBits::Bool, variant.getKind());
    ASSERT_EQ(expected, variant.getBool());
  }
  inline void assertVariant(int expected, const egg::ovum::Variant& variant) {
    ASSERT_EQ(egg::ovum::VariantBits::Int, variant.getKind());
    ASSERT_EQ(expected, variant.getInt());
  }
  inline void assertVariant(double expected, const egg::ovum::Variant& variant) {
    ASSERT_EQ(egg::ovum::VariantBits::Float, variant.getKind());
    ASSERT_EQ(expected, variant.getFloat());
  }
  inline void assertVariant(const char* expected, const egg::ovum::Variant& variant) {
    if (expected == nullptr) {
      ASSERT_EQ(egg::ovum::VariantBits::Null, variant.getKind());
    } else {
      ASSERT_EQ(egg::ovum::VariantBits::String | egg::ovum::VariantBits::Hard, variant.getKind());
      ASSERT_STREQ(expected, variant.getString().toUTF8().c_str());
    }
  }
}

#define ASSERT_STRING(expected, string) egg::test::assertString(expected, string)
#define ASSERT_VARIANT(expected, variant) egg::test::assertVariant(expected, variant)
