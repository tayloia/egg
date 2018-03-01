#include "test.h"

TEST(TestStrings, StartsWith) {
  ASSERT_TRUE(egg::yolk::String::startsWith("Hello World", "Hello"));
  ASSERT_TRUE(egg::yolk::String::startsWith("Hello World", "Hello World"));
  ASSERT_FALSE(egg::yolk::String::startsWith("Hello World", "World"));
  ASSERT_FALSE(egg::yolk::String::startsWith("Hello", "Hello World"));
}

TEST(TestStrings, EndsWith) {
  ASSERT_FALSE(egg::yolk::String::endsWith("Hello World", "Hello"));
  ASSERT_TRUE(egg::yolk::String::endsWith("Hello World", "Hello World"));
  ASSERT_TRUE(egg::yolk::String::endsWith("Hello World", "World"));
  ASSERT_FALSE(egg::yolk::String::endsWith("Hello", "Hello World"));
}

TEST(TestStrings, AssertMacros) {
  ASSERT_STARTSWITH("Hello World", "Hello");
  ASSERT_ENDSWITH("Hello World", "World");
}

TEST(TestStrings, ToLower) {
  ASSERT_EQ("hello world!", egg::yolk::String::toLower("Hello World!"));
}

TEST(TestStrings, ToUpper) {
  ASSERT_EQ("HELLO WORLD!", egg::yolk::String::toUpper("Hello World!"));
}

TEST(TestStrings, Replace) {
  ASSERT_EQ("Hell0 W0rld!", egg::yolk::String::replace("Hello World!", 'o', '0'));
}

TEST(TestStrings, Terminate) {
  std::string str = "Hello World";
  egg::yolk::String::terminate(str, '!');
  ASSERT_EQ("Hello World!", str);
  egg::yolk::String::terminate(str, '!');
  ASSERT_EQ("Hello World!", str);
}
