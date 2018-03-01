#include "test.h"

TEST(TestExceptions, Throw) {
  ASSERT_THROW(EGG_THROW("Hello world"), egg::yolk::Exception);
  ASSERT_THROW_E(EGG_THROW("Hello world"), egg::yolk::Exception, ASSERT_EQ("Hello world", e.reason()));
}

TEST(TestExceptions, Catch) {
  std::string expected_message = "Hello world";
  std::string expected_file = "/exceptions.cpp";
  auto expected_line = __LINE__ + 2; // where the throw occurs
  try {
    EGG_THROW(expected_message);
  } catch (const egg::yolk::Exception& exception) {
    auto ending = expected_file + "(" + std::to_string(expected_line) + ")";
    ASSERT_ENDSWITH(exception.what(), ending + ": " + expected_message);
    ASSERT_EQ(expected_message, exception.reason());
    ASSERT_ENDSWITH(exception.where(), ending);
  }
}
