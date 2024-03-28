#include "ovum/test.h"
#include "ovum/exception.h"

TEST(TestException, Throw) {
  ASSERT_THROW(throw egg::ovum::Exception("Hello world"), egg::ovum::Exception);
  ASSERT_THROW_E(throw egg::ovum::Exception("Hello world"), egg::ovum::Exception, ASSERT_EQ("Hello world", e.reason()));
}

TEST(TestException, Catch) {
  std::string expected_message = "Hello world";
  std::string expected_file = "ovum/test/test-exception.cpp";
  size_t expected_line = __LINE__ + 2; // where the throw occurs
  try {
    throw egg::ovum::Exception(expected_message);
  } catch (const egg::ovum::Exception& exception) {
    auto ending = expected_file + "(" + std::to_string(expected_line) + ")";
    ASSERT_ENDSWITH(exception.what(), ending + ": " + expected_message);
    ASSERT_EQ(expected_message, exception.reason());
    ASSERT_ENDSWITH(exception.where(), ending);
  }
}

TEST(TestException, WIBBLE) {
  ASSERT_THROW_E(throw egg::ovum::Exception("Hello world"), egg::ovum::Exception, ASSERT_ENDSWITH(e.what(), "ovum/test/test-exception.cpp(24): Hello world"));
}
