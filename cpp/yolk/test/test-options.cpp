#include "yolk/test.h"
#include "yolk/options.h"

using Occurrences = egg::yolk::OptionParser::Occurrences;

namespace {
  std::string parseOptionKey(Occurrences occurrences, const std::initializer_list<std::string>& arguments) {
    egg::yolk::OptionParser parser;
    parser.withExtraneousArguments();
    parser.withOption("key", occurrences);
    parser.withArguments(arguments);
    try {
      auto options = parser.parse();
      std::stringstream ss;
      char separator = '\0';
      for (auto& value : options.query("key")) {
        if (separator == '\0') {
          separator = ',';
        } else {
          ss.put(separator);
        }
        ss << value;
      }
      return ss.str();
    } catch (const egg::ovum::Exception& exception) {
      return exception.what();
    }
  }
}

TEST(TestOptions, Empty) {
  egg::yolk::OptionParser parser;
  auto options = parser.parse();
  ASSERT_EQ(0u, options.size());
}

TEST(TestOptions, WithExtraneousArguments) {
  egg::yolk::OptionParser parser;
  parser.withExtraneousArguments();
  parser.withArguments({ "alpha", "beta", "gamma" });
  auto options = parser.parse();
  auto extraneous = options.extraneous();
  ASSERT_EQ(3u, extraneous.size());
  ASSERT_EQ("alpha", extraneous[0]);
  ASSERT_EQ("beta", extraneous[1]);
  ASSERT_EQ("gamma", extraneous[2]);
}

TEST(TestOptions, WithUnexpectedExtraneousArguments) {
  egg::yolk::OptionParser parser;
  parser.withArguments({ "alpha", "beta", "gamma" });
  ASSERT_THROW_E(parser.parse(), egg::ovum::Exception, ASSERT_STREQ("Unexpected argument: 'alpha'", e.what()));
}

TEST(TestOptions, WithTooManyExtraneousArguments) {
  egg::yolk::OptionParser parser;
  parser.withExtraneousArguments(Occurrences::ZeroOrOne);
  parser.withArguments({ "alpha", "beta", "gamma" });
  ASSERT_THROW_E(parser.parse(), egg::ovum::Exception, ASSERT_STREQ("At most one argument was expected", e.what()));
}

TEST(TestOptions, WithTooFewExtraneousArguments) {
  egg::yolk::OptionParser parser;
  parser.withExtraneousArguments(Occurrences::One);
  ASSERT_THROW_E(parser.parse(), egg::ovum::Exception, ASSERT_STREQ("Exactly one argument was expected", e.what()));
}

TEST(TestOptions, WithUnexpectedOption) {
  egg::yolk::OptionParser parser;
  parser.withArguments({ "--unknown" });
  ASSERT_THROW_E(parser.parse(), egg::ovum::Exception, ASSERT_STREQ("Unrecognized option: '--unknown'", e.what()));
}

TEST(TestOptions, WithOption0) {
  std::initializer_list<std::string> arguments{};
  ASSERT_EQ("", parseOptionKey(Occurrences::ZeroOrOne, arguments));
  ASSERT_EQ("", parseOptionKey(Occurrences::ZeroOrMore, arguments));
  ASSERT_EQ("Exactly one occurrence of '--key' was expected", parseOptionKey(Occurrences::One, arguments));
  ASSERT_EQ("At least one occurrence of '--key' was expected", parseOptionKey(Occurrences::OneOrMore, arguments));
}

TEST(TestOptions, WithOption1) {
  std::initializer_list<std::string> arguments{ "--key=a" };
  ASSERT_EQ("a", parseOptionKey(Occurrences::ZeroOrOne, arguments));
  ASSERT_EQ("a", parseOptionKey(Occurrences::ZeroOrMore, arguments));
  ASSERT_EQ("a", parseOptionKey(Occurrences::One, arguments));
  ASSERT_EQ("a", parseOptionKey(Occurrences::OneOrMore, arguments));
}

TEST(TestOptions, WithOption2) {
  std::initializer_list<std::string> arguments{ "--key=z", "--key=a" };
  ASSERT_EQ("At most one occurrence of '--key' was expected", parseOptionKey(Occurrences::ZeroOrOne, arguments));
  ASSERT_EQ("z,a", parseOptionKey(Occurrences::ZeroOrMore, arguments));
  ASSERT_EQ("Exactly one occurrence of '--key' was expected", parseOptionKey(Occurrences::One, arguments));
  ASSERT_EQ("z,a", parseOptionKey(Occurrences::OneOrMore, arguments));
}

TEST(TestOptions, WithOption3) {
  std::initializer_list<std::string> arguments{ "--key=z", "--key", "--key=a" };
  ASSERT_EQ("At most one occurrence of '--key' was expected", parseOptionKey(Occurrences::ZeroOrOne, arguments));
  ASSERT_EQ("z,,a", parseOptionKey(Occurrences::ZeroOrMore, arguments));
  ASSERT_EQ("Exactly one occurrence of '--key' was expected", parseOptionKey(Occurrences::One, arguments));
  ASSERT_EQ("z,,a", parseOptionKey(Occurrences::OneOrMore, arguments));
}
