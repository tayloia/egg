#include "ovum/test.h"
#include "ovum/exception.h"

TEST(TestException, Throw) {
  ASSERT_THROW(throw egg::ovum::Exception("Hello world"), egg::ovum::Exception);
  ASSERT_THROW_E(throw egg::ovum::Exception("Hello world"), egg::ovum::Exception, ASSERT_STREQ("Hello world", e.what()));
}

TEST(TestException, Catch) {
  try {
    throw egg::ovum::Exception("Hello world");
  } catch (const egg::ovum::Exception& exception) {
    ASSERT_STREQ("Hello world", exception.what());
  }
}

TEST(TestException, HereImplicit) {
  try {
    throw egg::ovum::Exception("{where}: Hello world").here();
  } catch (const egg::ovum::Exception& exception) {
    auto where = "ovum/test/test-exception.cpp(" + std::to_string(__LINE__ - 2) + ')';
    ASSERT_EQ(where + ": Hello world", exception.what());
    ASSERT_EQ(where, exception.get("where"));
  }
}

TEST(TestException, HereExplicit) {
  try {
    throw egg::ovum::Exception("{where}: Hello world").here("file", 2, 3);
  } catch (const egg::ovum::Exception& exception) {
    std::string where = "file(2,3)";
    ASSERT_EQ(where + ": Hello world", exception.what());
    ASSERT_EQ(where, exception.get("where"));
  }
}

TEST(TestException, Format) {
  auto exception = egg::ovum::Exception("<FORMAT>");
  exception["alpha"] = "<ALPHA>";
  exception["beta"] = "<BETA>";
  exception["gamma"] = "<GAMMA>";
  exception["curly"] = "{CURLY}";
  ASSERT_EQ("plain text", exception.format("plain text"));
  ASSERT_EQ("<ALPHA>.<BETA>.<GAMMA>", exception.format("{alpha}.{beta}.{gamma}"));
  ASSERT_EQ(">>>{CURLY}<<<", exception.format(">>>{curly}<<<"));
  ASSERT_EQ(">>>{missing}<<<", exception.format(">>>{missing}<<<"));
  ASSERT_EQ(">>>{}<<<", exception.format(">>>{}<<<"));
  ASSERT_EQ(">>>{alpha<<<", exception.format(">>>{alpha<<<"));
  ASSERT_EQ(">>>gamma}<<<", exception.format(">>>gamma}<<<"));
}

TEST(TestException, Print) {
  auto exception = egg::ovum::Exception("<FORMAT>");
  exception["alpha"] = "<ALPHA>";
  exception["beta"] = "<BETA>";
  exception["gamma"] = "<GAMMA>";
  egg::ovum::StringBuilder sb;
  sb << exception;
  auto actual = sb.toUTF8();
  auto expected = "<FORMAT>\n"
                  "  alpha=<ALPHA>\n"
                  "  beta=<BETA>\n"
                  "  gamma=<GAMMA>\n"
                  "  what=<FORMAT>";
  ASSERT_EQ(actual, expected);
}

TEST(TestException, With) {
  try {
    throw egg::ovum::Exception(">>>{greeting}<<<").with("greeting", "hello");
  } catch (const egg::ovum::Exception& exception) {
    ASSERT_STREQ(">>>hello<<<", exception.what());
  }
}

TEST(TestException, Get) {
  try {
    throw egg::ovum::Exception("format").with("key", "value");
  } catch (const egg::ovum::Exception& exception) {
    ASSERT_EQ("value", exception.get("key"));
    ASSERT_EQ("value", exception.get("key", "default"));
    ASSERT_THROW(exception.get("missing"), std::runtime_error);
    ASSERT_EQ("default", exception.get("missing", "default"));
  }
}

TEST(TestException, Query) {
  try {
    throw egg::ovum::Exception("format").with("key", "value");
  } catch (const egg::ovum::Exception& exception) {
    std::string value;
    ASSERT_TRUE(exception.query("key", value));
    ASSERT_EQ("value", value);
    ASSERT_FALSE(exception.query("unknown", value));
    ASSERT_EQ("value", value);
  }
}
