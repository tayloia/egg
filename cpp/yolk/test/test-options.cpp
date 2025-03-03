#include "yolk/test.h"
#include "yolk/options.h"

using Occurrences = egg::yolk::OptionParser::Occurrences;

namespace {
  template<typename T>
  std::string join(char separator, const T& values) {
    std::stringstream ss;
    auto first = true;
    for (const auto& value : values) {
      if (first) {
        first = false;
      } else {
        ss.put(separator);
      }
      ss << '<' << value << '>';
    }
    return ss.str();
  }
  std::string parseStringOptionKey(Occurrences occurrences, const std::initializer_list<std::string>& arguments) {
    egg::yolk::OptionParser parser;
    parser.withStringOption("key", occurrences);
    parser.withArguments(arguments);
    try {
      auto options = parser.parse();
      return join(',', options.query("key"));
    } catch (const egg::ovum::Exception& exception) {
      return exception.what();
    }
  }
  std::string parseValuelessOptionKey(const std::initializer_list<std::string>& arguments) {
    egg::yolk::OptionParser parser;
    parser.withValuelessOption("key");
    parser.withArguments(arguments);
    try {
      auto options = parser.parse();
      return join(',', options.query("key"));
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

TEST(TestOptions, WithStringOptionWithoutValue) {
  ASSERT_EQ("Missing required option value: '--key'", parseStringOptionKey(Occurrences::One, { "--key" }));
}

TEST(TestOptions, WithStringOption0) {
  std::initializer_list<std::string> arguments{};
  ASSERT_EQ("", parseStringOptionKey(Occurrences::ZeroOrOne, arguments));
  ASSERT_EQ("", parseStringOptionKey(Occurrences::ZeroOrMore, arguments));
  ASSERT_EQ("Exactly one occurrence of '--key' was expected", parseStringOptionKey(Occurrences::One, arguments));
  ASSERT_EQ("At least one occurrence of '--key' was expected", parseStringOptionKey(Occurrences::OneOrMore, arguments));
}

TEST(TestOptions, WithStringOption1) {
  std::initializer_list<std::string> arguments{ "--key=a" };
  ASSERT_EQ("<a>", parseStringOptionKey(Occurrences::ZeroOrOne, arguments));
  ASSERT_EQ("<a>", parseStringOptionKey(Occurrences::ZeroOrMore, arguments));
  ASSERT_EQ("<a>", parseStringOptionKey(Occurrences::One, arguments));
  ASSERT_EQ("<a>", parseStringOptionKey(Occurrences::OneOrMore, arguments));
}

TEST(TestOptions, WithStringOption2) {
  std::initializer_list<std::string> arguments{ "--key=z", "--key=a" };
  ASSERT_EQ("At most one occurrence of '--key' was expected", parseStringOptionKey(Occurrences::ZeroOrOne, arguments));
  ASSERT_EQ("<z>,<a>", parseStringOptionKey(Occurrences::ZeroOrMore, arguments));
  ASSERT_EQ("Exactly one occurrence of '--key' was expected", parseStringOptionKey(Occurrences::One, arguments));
  ASSERT_EQ("<z>,<a>", parseStringOptionKey(Occurrences::OneOrMore, arguments));
}

TEST(TestOptions, WithStringOption3) {
  std::initializer_list<std::string> arguments{ "--key=z", "--key=", "--key=a" };
  ASSERT_EQ("At most one occurrence of '--key' was expected", parseStringOptionKey(Occurrences::ZeroOrOne, arguments));
  ASSERT_EQ("<z>,<>,<a>", parseStringOptionKey(Occurrences::ZeroOrMore, arguments));
  ASSERT_EQ("Exactly one occurrence of '--key' was expected", parseStringOptionKey(Occurrences::One, arguments));
  ASSERT_EQ("<z>,<>,<a>", parseStringOptionKey(Occurrences::OneOrMore, arguments));
}

TEST(TestOptions, WithValuelessOption) {
  ASSERT_EQ("", parseValuelessOptionKey({}));
  ASSERT_EQ("<>", parseValuelessOptionKey({ "--key" }));
  ASSERT_EQ("At most one occurrence of '--key' was expected", parseValuelessOptionKey({ "--key", "--key" }));
  ASSERT_EQ("Unexpected option value: '--key=value'", parseValuelessOptionKey({ "--key=value" }));
  ASSERT_EQ("Unexpected option value: '--key='", parseValuelessOptionKey({ "--key=" }));
}
