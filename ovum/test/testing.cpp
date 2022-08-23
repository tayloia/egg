#include "ovum/test.h"

TEST(TestTesting, Empty) {
}

TEST(TestTesting, Truth) {
  ASSERT_EQ(6 * 7, 42);
}

TEST(TestTesting, Fails) {
  ASSERT_FAILS(ASSERT_EQ(6 * 7, 41));
}

TEST(TestTesting, String) {
  egg::ovum::String greeting{ "Hello" };
  ASSERT_STRING("Hello", greeting);
}

TEST(TestTesting, Value) {
  egg::test::Allocator allocator;
  egg::ovum::Value greeting{ egg::ovum::ValueFactory::createASCIIZ(allocator, "Hello") };
  ASSERT_VALUE("Hello", greeting);
}

TEST(TestTesting, Contains) {
  // This is our own addition to the assert macros
  ASSERT_CONTAINS("haystack", "ta");
}

TEST(TestTesting, NotContains) {
  // This is our own addition to the assert macros
  ASSERT_NOTCONTAINS("haystack", "needle");
}

TEST(TestTesting, StartsWith) {
  // This is our own addition to the assert macros
  ASSERT_STARTSWITH("haystack", "hay");
}

TEST(TestTesting, EndsWith) {
  // This is our own addition to the assert macros
  ASSERT_ENDSWITH("haystack", "stack");
}

TEST(TestTesting, ContainsNegative) {
  // This is our own addition to the assert macros
  ASSERT_FAILS(ASSERT_CONTAINS("haystack", "needle"));
}

TEST(TestTesting, NotContainsNegative) {
  // This is our own addition to the assert macros
  ASSERT_FAILS(ASSERT_NOTCONTAINS("haystack", "ta"));
}

TEST(TestTesting, StartsWithNegative) {
  // This is our own addition to the assert macros
  ASSERT_FAILS(ASSERT_STARTSWITH("haystack", "stack"));
}

TEST(TestTesting, EndsWithNegative) {
  // This is our own addition to the assert macros
  ASSERT_FAILS(ASSERT_ENDSWITH("haystack", "hay"));
}

TEST(TestTesting, Throws) {
  // This are our own addition to the assert macros
  ASSERT_THROW_E(throw std::runtime_error("reason"), std::runtime_error, ASSERT_STREQ("reason", e.what()));
}

TEST(TestTesting, Logger) {
  using Source = egg::ovum::ILogger::Source;
  using Severity = egg::ovum::ILogger::Severity;
  egg::test::Logger logger;
  logger.log(Source::Compiler, Severity::Debug, "alpha");
  logger.log(Source::Runtime, Severity::Verbose, "beta");
  logger.log(Source::User, Severity::Information, "gamma");
  logger.log(Source::Compiler, Severity::Warning, "delta");
  logger.log(Source::Runtime, Severity::Error, "epsilon");
  logger.log(Source::User, Severity::None, "zeta");
  ASSERT_STRING("<COMPILER><DEBUG>alpha\n<RUNTIME><VERBOSE>beta\n<INFORMATION>gamma\n<COMPILER><WARNING>delta\n<RUNTIME><ERROR>epsilon\nzeta\n", logger.logged.str());
}