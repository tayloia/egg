namespace egg::test {
  class Allocator final : public egg::ovum::AllocatorDefault {
    Allocator(const Allocator&) = delete;
    Allocator& operator=(const Allocator&) = delete;
  public:
    enum class Expectation {
      Unknown,
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
      case Expectation::Unknown:
        break;
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

  class Logger final : public egg::ovum::ILogger {
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;
  private:
    std::string resource;
  public:
    std::stringstream logged;
    explicit Logger(const std::string& resource = std::string())
      : resource(resource) {
    }
    virtual void log(Source source, Severity severity, const std::string& message) override {
      std::string buffer;
      switch (source) {
      case Source::Compiler:
        buffer = "<COMPILER>";
        break;
      case Source::Runtime:
        buffer = "<RUNTIME>";
        break;
      case Source::User:
        break;
      }
      switch (severity) {
      case Severity::Debug:
        buffer += "<DEBUG>";
        break;
      case Severity::Verbose:
        buffer += "<VERBOSE>";
        break;
      case Severity::Warning:
        buffer += "<WARNING>";
        break;
      case Severity::Error:
        buffer += "<ERROR>";
        break;
      case Severity::None:
      case Severity::Information:
        break;
      }
      buffer += message;
      std::cout << buffer << std::endl;
      if (!this->resource.empty()) {
        std::string to = "<RESOURCE>";
        for (auto pos = buffer.find(this->resource); pos != std::string::npos; pos = buffer.find(this->resource, pos + to.length())) {
          buffer.replace(pos, this->resource.length(), to);
        }
      }
      this->logged << buffer << '\n';
    }
  };

  inline void assertString(const char* expected, const egg::ovum::String& actual) {
    ASSERT_STREQ(expected, actual.toUTF8().c_str());
  }
  inline void assertString(const egg::ovum::String& expected, const egg::ovum::String& actual) {
    ASSERT_STREQ(expected.toUTF8().c_str(), actual.toUTF8().c_str());
  }
  inline void assertVariant(egg::ovum::VariantBits expected, const egg::ovum::Variant& variant) {
    ASSERT_EQ(expected, variant.getKind());
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

template<>
inline void ::testing::internal::PrintTo(const egg::ovum::VariantBits& value, std::ostream* stream) {
  // Pretty-print the variant kind
  egg::ovum::VariantKind::printTo(*stream, value);
}

#define ASSERT_STRING(expected, string) egg::test::assertString(expected, string)
#define ASSERT_VARIANT(expected, variant) egg::test::assertVariant(expected, variant)
