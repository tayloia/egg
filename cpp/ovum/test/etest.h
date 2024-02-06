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
    template<typename... ARGS>
    egg::ovum::String concat(ARGS&&... args) {
      return egg::ovum::StringBuilder::concat(*this, std::forward<ARGS>(args)...);
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
  public:
    std::string resource;
    std::ostringstream logged;
    Logger() = default;
    virtual void log(Source source, Severity severity, const egg::ovum::String& message) override {
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
      case Severity::None:
        break;
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
      }
      buffer += message.toUTF8();
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
    egg::test::Logger logger;
    egg::ovum::HardPtr<egg::ovum::IVM> vm;
    VM()
      : allocator(),
        vm(egg::ovum::VMFactory::createDefault(allocator, logger)) {
    }
    ~VM() {
      this->vm->getBasket().verify(std::cout);
    }
    egg::ovum::IVM& operator*() {
      return *this->vm;
    }
    egg::ovum::IVM* operator->() {
      return this->vm.get();
    }
    void addBuiltin(egg::ovum::IVMRunner& runner, const std::u8string& name, const egg::ovum::HardObject& instance) {
      runner.addBuiltin(runner.createString(name), runner.createHardValueObject(instance));
    }
    void addBuiltinAssert(egg::ovum::IVMRunner& runner) {
      this->addBuiltin(runner, u8"assert", vm->createBuiltinAssert());
    }
    void addBuiltinPrint(egg::ovum::IVMRunner& runner) {
      this->addBuiltin(runner, u8"print", vm->createBuiltinPrint());
    }
    void addBuiltinExpando(egg::ovum::IVMRunner& runner) {
      this->addBuiltin(runner, u8"expando", vm->createBuiltinExpando());
    }
    void addBuiltinCollector(egg::ovum::IVMRunner& runner) {
      this->addBuiltin(runner, u8"collector", vm->createBuiltinCollector());
    }
    void addBuiltinSymtable(egg::ovum::IVMRunner& runner) {
      this->addBuiltin(runner, u8"symtable", vm->createBuiltinSymtable());
    }
    void addBuiltins(egg::ovum::IVMRunner& runner) {
      this->addBuiltinAssert(runner);
      this->addBuiltinPrint(runner);
      this->addBuiltinExpando(runner);
      this->addBuiltinCollector(runner);
      this->addBuiltinSymtable(runner);
    }
    bool run(egg::ovum::IVMRunner& runner) {
      egg::ovum::HardValue retval;
      auto outcome = runner.run(retval);
      if (retval.hasAnyFlags(egg::ovum::ValueFlags::Throw)) {
        // TODO better location information in the error message
        this->logger.log(egg::ovum::ILogger::Source::Runtime, egg::ovum::ILogger::Severity::Error, this->allocator.concat(retval));
        return false;
      }
      if (!retval->getVoid()) {
        this->logger.log(egg::ovum::ILogger::Source::Runtime, egg::ovum::ILogger::Severity::None, this->allocator.concat("<RETVAL>", retval->getPrimitiveFlag(), ':', retval));
      }
      return outcome == egg::ovum::IVMRunner::RunOutcome::Succeeded;
    }
  };

  inline ::testing::AssertionResult assertValueEQ(const char* lhs_expression, const char* rhs_expression, const egg::ovum::HardValue& lhs, const egg::ovum::HardValue& rhs) {
    if (egg::ovum::SoftKey::compare(lhs.get(), rhs.get()) == 0) {
      return ::testing::AssertionSuccess();
    }
    return ::testing::internal::CmpHelperEQFailure(lhs_expression, rhs_expression, lhs, rhs);
  }
  inline ::testing::AssertionResult assertTypeEQ(const char* lhs_expression, const char* rhs_expression, const egg::ovum::Type& lhs, const egg::ovum::Type& rhs) {
    if (lhs == rhs) {
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
  inline void assertType(const egg::ovum::Type& expected, const egg::ovum::Type& actual) {
    ASSERT_PRED_FORMAT2(assertTypeEQ, expected, actual);
  }
  inline void assertValue(egg::ovum::ValueFlags expected, const egg::ovum::HardValue& value) {
    ASSERT_EQ(expected, value->getPrimitiveFlag());
  }
  inline void assertValue(std::nullptr_t, const egg::ovum::HardValue& value) {
    ASSERT_EQ(egg::ovum::ValueFlags::Null, value->getPrimitiveFlag());
  }
  inline void assertValue(bool expected, const egg::ovum::HardValue& value) {
    ASSERT_EQ(egg::ovum::ValueFlags::Bool, value->getPrimitiveFlag());
    bool actual = false;;
    ASSERT_TRUE(value->getBool(actual));
    ASSERT_EQ(expected, actual);
  }
  inline void assertValue(int expected, const egg::ovum::HardValue& value) {
    ASSERT_EQ(egg::ovum::ValueFlags::Int, value->getPrimitiveFlag());
    egg::ovum::Int actual = ~0;
    ASSERT_TRUE(value->getInt(actual));
    ASSERT_EQ(expected, actual);
  }
  inline void assertValue(double expected, const egg::ovum::HardValue& value) {
    ASSERT_EQ(egg::ovum::ValueFlags::Float, value->getPrimitiveFlag());
    egg::ovum::Float actual = std::nan("");
    ASSERT_TRUE(value->getFloat(actual));
    if (std::isnan(expected)) {
      ASSERT_TRUE(std::isnan(actual));
    } else {
      ASSERT_DOUBLE_EQ(expected, actual);
    }
  }
  inline void assertValue(const char* expected, const egg::ovum::HardValue& value) {
    if (expected == nullptr) {
      ASSERT_EQ(egg::ovum::ValueFlags::Null, value->getPrimitiveFlag());
    } else {
      ASSERT_EQ(egg::ovum::ValueFlags::String, value->getPrimitiveFlag());
      egg::ovum::String actual;
      ASSERT_TRUE(value->getString(actual));
      ASSERT_STREQ(expected, actual.toUTF8().c_str());
    }
  }
  inline void assertValue(const egg::ovum::HardValue& expected, const egg::ovum::HardValue& actual) {
    ASSERT_PRED_FORMAT2(assertValueEQ, expected, actual);
  }
  inline void assertThrown(const char* expected, const egg::ovum::HardValue& actual) {
    ASSERT_TRUE(actual.hasAnyFlags(egg::ovum::ValueFlags::Throw));
    if (expected != nullptr) {
      egg::ovum::HardValue inner;
      ASSERT_TRUE(actual->getInner(inner));
      egg::ovum::String message;
      ASSERT_TRUE(inner->getString(message));
      ASSERT_STREQ(expected, message.toUTF8().c_str());
    }
  }
}

template<>
inline void ::testing::internal::PrintTo(const egg::ovum::Type& value, std::ostream* stream) {
  // Pretty-print the type
  egg::ovum::Printer{ *stream, egg::ovum::Print::Options::DEFAULT } << value;
}

template<>
inline void ::testing::internal::PrintTo(const egg::ovum::ValueFlags& value, std::ostream* stream) {
  // Pretty-print the value flags
  egg::ovum::Printer{ *stream, egg::ovum::Print::Options::DEFAULT } << value;
}

template<>
inline void ::testing::internal::PrintTo(const egg::ovum::HardValue& value, std::ostream* stream) {
  // Pretty-print the value
  egg::ovum::Printer{ *stream, egg::ovum::Print::Options::DEFAULT } << value;
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
#define ASSERT_THROWN(expected, value) egg::test::assertThrown(expected, value)
