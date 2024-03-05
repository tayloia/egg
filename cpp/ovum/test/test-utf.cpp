#include "ovum/test.h"
#include "ovum/utf.h"

namespace {
  // Here are our test cases:
  // NUL          U+0000    0x00                  http://www.fileformat.info/info/unicode/char/0/index.htm
  // DOLLAR SIGN  U+0024    0x24                  http://www.fileformat.info/info/unicode/char/0024/index.htm
  // POUND SIGN   U+00A3    0xC2 0xA3             http://www.fileformat.info/info/unicode/char/00A3/index.htm
  // EURO SIGN    U+20AC    0xE2 0x82 0xAC        http://www.fileformat.info/info/unicode/char/20ac/index.htm
  // EGG EMOJI    U+1F95A   0xF0 0x9F 0xA5 0x9A   http://www.fileformat.info/info/unicode/char/1f95a/index.htm
  // LAST         U+10FFFF  0xF4 0x8F 0xBF 0xBF   http://www.fileformat.info/info/unicode/char/10ffff/index.htm
  const struct TestCase {
    std::string name;
    std::string utf8;
    int32_t utf32;
  } testCases[] = {
    { "Nul", std::string(1, '\0'), 0x0000 }, // Need to force the NUL as part of the string
    { "Dollar", "\x24", 0x0024 },
    { "Pound", "\xC2\xA3", 0x00A3 },
    { "Euro", "\xE2\x82\xAC", 0x20AC },
    { "Egg", "\xF0\x9F\xA5\x9A", 0x1F95A },
    { "Last", "\xF4\x8F\xBF\xBF", 0x10FFFF }
  };

  class TestUTF : public ::testing::TestWithParam<size_t> {
    // You can implement all the usual fixture class members here.
    // To access the test parameter, call GetParam() from class
    // TestWithParam<T>.
  public:
    TestUTF() {}
    static ::testing::internal::ParamGenerator<size_t> generator() {
      // Generate value parameterizations for all the examples
      return ::testing::Range<size_t>(0, EGG_NELEMS(testCases) - 1);
    }
    const TestCase& getTestCase() const {
      auto index = this->GetParam();
      assert(index < EGG_NELEMS(testCases));
      return testCases[index];
    }
    static std::string name(const ::testing::TestParamInfo<size_t>& info) {
      // Format the test case name parameterization nicely
      assert(info.param < EGG_NELEMS(testCases));
      return testCases[info.param].name;
    }
  };
}

TEST_P(TestUTF, UTF32toUTF8) {
  auto& param = this->getTestCase();
  if (param.utf32 >= 0) {
    auto codepoint = char32_t(param.utf32);
    ASSERT_EQ(param.utf8, egg::ovum::UTF32::toUTF8(codepoint));
    std::u32string utf32(1, codepoint);
    ASSERT_EQ(param.utf8, egg::ovum::UTF32::toUTF8(utf32));
  }
}

TEST_P(TestUTF, UTF8toUTF32) {
  const auto& param = this->getTestCase();
  if (param.utf32 >= 0) {
    auto codepoint = char32_t(param.utf32);
    auto utf32 = egg::ovum::UTF8::toUTF32(param.utf8);
    ASSERT_EQ(1u, utf32.length());
    ASSERT_EQ(codepoint, utf32[0]);
  }
}

EGG_INSTANTIATE_TEST_CASE_P(TestUTF)

TEST(TestUTF32, toReadable) {
  ASSERT_EQ("<EOF>", egg::ovum::UTF32::toReadable(-1));
  ASSERT_EQ("U+0000", egg::ovum::UTF32::toReadable(0));
  ASSERT_EQ("' '", egg::ovum::UTF32::toReadable(32));
  ASSERT_EQ("'~'", egg::ovum::UTF32::toReadable(126));
  ASSERT_EQ("U+007F", egg::ovum::UTF32::toReadable(127));
  ASSERT_EQ("U+00A3", egg::ovum::UTF32::toReadable(0xA3));
  ASSERT_EQ("U+20AC", egg::ovum::UTF32::toReadable(0x20AC));
  ASSERT_EQ("U+1F95A", egg::ovum::UTF32::toReadable(0x1F95A));
}
