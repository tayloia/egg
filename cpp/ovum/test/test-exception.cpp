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
    auto ending = expected_file + '(' + std::to_string(expected_line) + ')';
    ASSERT_EQ(ending + ": " + expected_message, exception.what());
    ASSERT_EQ(expected_message, exception.reason());
    ASSERT_EQ(ending, exception.where());
  }
}

TEST(TestException, Format) {
  auto exception = egg::ovum::Exception("<REASON>", "<WHERE>");
  exception["alpha"] = "<ALPHA>";
  exception["beta"] = "<BETA>";
  exception["gamma"] = "<GAMMA>";
  exception["curly"] = "{CURLY}";
  ASSERT_EQ("plain text", exception.format("plain text"));
  ASSERT_EQ("<REASON><WHERE>", exception.format("{reason}{where}"));
  ASSERT_EQ("<ALPHA>.<BETA>.<GAMMA>", exception.format("{alpha}.{beta}.{gamma}"));
  ASSERT_EQ(">>>{CURLY}<<<", exception.format(">>>{curly}<<<"));
  ASSERT_EQ(">>>{missing}<<<", exception.format(">>>{missing}<<<"));
  ASSERT_EQ(">>>{}<<<", exception.format(">>>{}<<<"));
  ASSERT_EQ(">>>{alpha<<<", exception.format(">>>{alpha<<<"));
  ASSERT_EQ(">>>gamma}<<<", exception.format(">>>gamma}<<<"));
}

TEST(TestException, Print) {
  auto exception = egg::ovum::Exception("<REASON>", "<WHERE>");
  exception["alpha"] = "<ALPHA>";
  exception["beta"] = "<BETA>";
  exception["gamma"] = "<GAMMA>";
  egg::ovum::StringBuilder sb;
  sb << exception;
  auto actual = sb.toUTF8();
  auto expected = "<WHERE>: <REASON>\n"
                  "  alpha=<ALPHA>\n"
                  "  beta=<BETA>\n"
                  "  gamma=<GAMMA>\n"
                  "  reason=<REASON>\n"
                  "  what=<WHERE>: <REASON>\n"
                  "  where=<WHERE>";
  ASSERT_EQ(expected, actual);
}
