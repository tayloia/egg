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
      egg::ovum::IAllocator::Statistics stats{};
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
    std::ostringstream logged;
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
      case Severity::Information:
        buffer += "<INFORMATION>";
        break;
      case Severity::Warning:
        buffer += "<WARNING>";
        break;
      case Severity::Error:
        buffer += "<ERROR>";
        break;
      case Severity::None:
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

  class VM final {
  public:
    egg::test::Allocator allocator;
    egg::ovum::HardPtr<egg::ovum::IVM> vm;
    VM()
      : allocator(),
        vm(egg::ovum::VMFactory::createTest(allocator).get()) {
    }
    egg::ovum::IVM* operator->() {
      return this->vm.get();
    }
  };

  inline ::testing::AssertionResult assertValueEQ(const char* lhs_expression, const char* rhs_expression, const egg::ovum::Value& lhs, const egg::ovum::Value& rhs) {
    if (lhs->equals(rhs.get(), egg::ovum::ValueCompare::Binary)) {
      return ::testing::AssertionSuccess();
    }
    return ::testing::internal::CmpHelperEQFailure(lhs_expression, rhs_expression, lhs, rhs);
  }
  inline void assertString(const char* expected, const egg::ovum::String& actual) {
    ASSERT_STREQ(expected, actual.toUTF8().c_str());
  }
  inline void assertString(const char8_t* expected, const egg::ovum::String& actual) {
    ASSERT_STREQ(reinterpret_cast<const char*>(expected), actual.toUTF8().c_str());
  }
  inline void assertString(const egg::ovum::String& expected, const egg::ovum::String& actual) {
    ASSERT_STREQ(expected.toUTF8().c_str(), actual.toUTF8().c_str());
  }
  inline void assertValue(egg::ovum::ValueFlags expected, const egg::ovum::Value& value) {
    ASSERT_EQ(expected, value->getFlags());
  }
  inline void assertValue(std::nullptr_t, const egg::ovum::Value& value) {
    ASSERT_EQ(egg::ovum::ValueFlags::Null, value->getFlags());
  }
  inline void assertValue(bool expected, const egg::ovum::Value& value) {
    ASSERT_EQ(egg::ovum::ValueFlags::Bool, value->getFlags());
    bool actual = false;;
    ASSERT_TRUE(value->getBool(actual));
    ASSERT_EQ(expected, actual);
  }
  inline void assertValue(int expected, const egg::ovum::Value& value) {
    ASSERT_EQ(egg::ovum::ValueFlags::Int, value->getFlags());
    egg::ovum::Int actual = ~0;
    ASSERT_TRUE(value->getInt(actual));
    ASSERT_EQ(expected, actual);
  }
  inline void assertValue(double expected, const egg::ovum::Value& value) {
    ASSERT_EQ(egg::ovum::ValueFlags::Float, value->getFlags());
    egg::ovum::Float actual = std::nan("");
    ASSERT_TRUE(value->getFloat(actual));
    ASSERT_EQ(expected, actual);
  }
  inline void assertValue(const char* expected, const egg::ovum::Value& value) {
    if (expected == nullptr) {
      ASSERT_EQ(egg::ovum::ValueFlags::Null, value->getFlags());
    } else {
      ASSERT_EQ(egg::ovum::ValueFlags::String, value->getFlags());
      egg::ovum::String actual;
      ASSERT_TRUE(value->getString(actual));
      ASSERT_STREQ(expected, actual.toUTF8().c_str());
    }
  }
  inline void assertValue(const egg::ovum::Value& expected, const egg::ovum::Value& actual) {
    ASSERT_PRED_FORMAT2(assertValueEQ, expected, actual);
  }
}

template<>
inline void ::testing::internal::PrintTo(const egg::ovum::Type& value, std::ostream* stream) {
  // Pretty-print the type
  egg::ovum::Print::write(*stream, value, egg::ovum::Print::Options::DEFAULT);
}

template<>
inline void ::testing::internal::PrintTo(const egg::ovum::ValueFlags& value, std::ostream* stream) {
  // Pretty-print the value flags
  egg::ovum::Print::write(*stream, value, egg::ovum::Print::Options::DEFAULT);
}

template<>
inline void ::testing::internal::PrintTo(const egg::ovum::Value& value, std::ostream* stream) {
  // Pretty-print the value
  egg::ovum::Print::write(*stream, value, egg::ovum::Print::Options::DEFAULT);
}

template<>
inline void ::testing::internal::PrintTo(const egg::ovum::ILogger::Severity& value, std::ostream* stream) {
  // Pretty-print the logger severity
  egg::ovum::Print::write(*stream, value, egg::ovum::Print::Options::DEFAULT);
}

template<>
inline void ::testing::internal::PrintTo(const egg::ovum::ILogger::Source& value, std::ostream* stream) {
  // Pretty-print the logger source
  egg::ovum::Print::write(*stream, value, egg::ovum::Print::Options::DEFAULT);
}

#define ASSERT_STRING(expected, string) egg::test::assertString(expected, string)
#define ASSERT_TYPE(expected, string) egg::test::assertType(expected, string)
#define ASSERT_VALUE(expected, value) egg::test::assertValue(expected, value)
