#include "yolk.h"

TEST(TestStrings, BeginsWith) {
  EXPECT_TRUE(egg::yolk::String::beginsWith("Hello World", "Hello"));
  EXPECT_TRUE(egg::yolk::String::beginsWith("Hello World", "Hello World"));
  EXPECT_FALSE(egg::yolk::String::beginsWith("Hello World", "World"));
  EXPECT_FALSE(egg::yolk::String::beginsWith("Hello", "Hello World"));
}

TEST(TestStrings, EndsWith) {
  EXPECT_FALSE(egg::yolk::String::endsWith("Hello World", "Hello"));
  EXPECT_TRUE(egg::yolk::String::endsWith("Hello World", "Hello World"));
  EXPECT_TRUE(egg::yolk::String::endsWith("Hello World", "World"));
  EXPECT_FALSE(egg::yolk::String::endsWith("Hello", "Hello World"));
}

TEST(TestStrings, ToLower) {
  EXPECT_EQ("hello world!", egg::yolk::String::toLower("Hello World!"));
}

TEST(TestStrings, ToUpper) {
  EXPECT_EQ("HELLO WORLD!", egg::yolk::String::toUpper("Hello World!"));
}

TEST(TestStrings, Replace) {
  EXPECT_EQ("Hell0 W0rld!", egg::yolk::String::replace("Hello World!", 'o', '0'));
}

TEST(TestStrings, Terminate) {
  std::string str = "Hello World";
  egg::yolk::String::terminate(str, '!');
  EXPECT_EQ("Hello World!", str);
  egg::yolk::String::terminate(str, '!');
  EXPECT_EQ("Hello World!", str);
}
