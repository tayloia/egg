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

  class Execution final : public egg::ovum::IExecution {
    Execution(const Execution&) = delete;
    Execution& operator=(const Execution&) = delete;
  private:
    egg::ovum::IAllocator& allocator;
    egg::ovum::Basket basket;
    egg::ovum::ILogger& logger;
  public:
    Execution(egg::ovum::IAllocator& allocator, egg::ovum::ILogger& logger)
      : allocator(allocator),
        basket(egg::ovum::BasketFactory::createBasket(allocator)),
        logger(logger) {
    }
    virtual egg::ovum::IAllocator& getAllocator() const override {
      return this->allocator;
    }
    virtual egg::ovum::IBasket& getBasket() const override {
      return *this->basket;
    }
    virtual egg::ovum::Value raise(const egg::ovum::String& message) override {
      auto value = egg::ovum::ValueFactory::create(this->allocator, message);
      return egg::ovum::ValueFactory::createFlowControl(this->allocator, egg::ovum::ValueFlags::Throw, value);
    }
    virtual egg::ovum::Value assertion(const egg::ovum::Value& predicate) override {
      egg::ovum::Object object;
      if (predicate->getObject(object)) {
        // Predicates can be functions that throw exceptions, as well as 'bool' values
        auto type = object->getRuntimeType();
        if (type.queryCallable() != nullptr) {
          // Call the predicate directly
          return object->call(*this, egg::ovum::Object::ParametersNone);
        }
      }
      egg::ovum::Bool value;
      if (!predicate->getBool(value)) {
        return this->raiseFormat("'assert()' expects its parameter to be a 'bool' or 'void()', but got '", predicate->getRuntimeType().toString(), "' instead");
      }
      if (value) {
        return egg::ovum::Value::Void;
      }
      return this->raise("Assertion is untrue");
    }
    virtual void print(const std::string& utf8) override {
      this->logger.log(egg::ovum::ILogger::Source::User, egg::ovum::ILogger::Severity::None, utf8);
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

  inline ::testing::AssertionResult assertValueEQ(const char* lhs_expression, const char* rhs_expression, const egg::ovum::Value& lhs, const egg::ovum::Value& rhs) {
    if (lhs->equals(rhs.get(), egg::ovum::ValueCompare::Binary)) {
      return ::testing::AssertionSuccess();
    }
    return ::testing::internal::CmpHelperEQFailure(lhs_expression, rhs_expression, lhs, rhs);
  }
  inline void assertString(const char* expected, const egg::ovum::String& actual) {
    ASSERT_STREQ(expected, actual.toUTF8().c_str());
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
    bool actual;
    ASSERT_TRUE(value->getBool(actual));
    ASSERT_EQ(expected, actual);
  }
  inline void assertValue(int expected, const egg::ovum::Value& value) {
    ASSERT_EQ(egg::ovum::ValueFlags::Int, value->getFlags());
    egg::ovum::Int actual;
    ASSERT_TRUE(value->getInt(actual));
    ASSERT_EQ(expected, actual);
  }
  inline void assertValue(double expected, const egg::ovum::Value& value) {
    ASSERT_EQ(egg::ovum::ValueFlags::Float, value->getFlags());
    egg::ovum::Float actual;
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
inline void ::testing::internal::PrintTo(const egg::ovum::ValueFlags& value, std::ostream* stream) {
  // Pretty-print the value flags
  egg::ovum::Print::write(*stream, value);
}

template<>
inline void ::testing::internal::PrintTo(const egg::ovum::Value& value, std::ostream* stream) {
  // Pretty-print the value
  egg::ovum::Print::write(*stream, value);
}

template<>
inline void ::testing::internal::PrintTo(const egg::ovum::ILogger::Severity& value, std::ostream* stream) {
  // Pretty-print the value flags
  egg::ovum::Print::write(*stream, value);
}

template<>
inline void ::testing::internal::PrintTo(const egg::ovum::ILogger::Source& value, std::ostream* stream) {
  // Pretty-print the value flags
  egg::ovum::Print::write(*stream, value);
}

#define ASSERT_STRING(expected, string) egg::test::assertString(expected, string)
#define ASSERT_VALUE(expected, value) egg::test::assertValue(expected, value)
#define ASSERT_VARIANT(expected, variant) egg::test::assertValue(expected, variant)
