#include "test.h"

// Here are our test cases:
// NUL          U+0000    0x00                  http://www.fileformat.info/info/unicode/char/0/index.htm
// DOLLAR SIGN  U+0024    0x24                  http://www.fileformat.info/info/unicode/char/0024/index.htm
// POUND SIGN   U+00A3    0xC2 0xA3             http://www.fileformat.info/info/unicode/char/00A3/index.htm
// EURO SIGN    U+20AC    0xE2 0x82 0xAC        http://www.fileformat.info/info/unicode/char/20ac/index.htm
// EGG EMOJI    U+1F95A   0xF0 0x9F 0xA5 0x9A   http://www.fileformat.info/info/unicode/char/1f95a/index.htm
// LAST         U+10FFFF  0xF4 0x8F 0xBF 0xBF   http://www.fileformat.info/info/unicode/char/10ffff/index.htm
namespace {
  struct Param {
    std::string name;
    std::string utf8;
    int32_t utf32;
  };

  class TestUTF : public ::testing::TestWithParam<Param> {
    EGG_NO_COPY(TestUTF);
    // You can implement all the usual fixture class members here.
    // To access the test parameter, call GetParam() from class
    // TestWithParam<T>.
  public:
    TestUTF() {}
    static ::testing::internal::ParamGenerator<Param> generator() {
      // Generate value parameterizations for all the examples
      return ::testing::Values(
        Param{ "Nul", std::string(1, '\0'), 0x0000 }, // Need to force the NUL as part of the string
        Param{ "Dollar", "\x24", 0x0024 },
        Param{ "Pound", "\xC2\xA3", 0x00A3 },
        Param{ "Euro", "\xE2\x82\xAC", 0x20AC },
        Param{ "Egg", "\xF0\x9F\xA5\x9A", 0x1F95A },
        Param{ "Last", "\xF4\x8F\xBF\xBF", 0x10FFFF }
      );
    }
    static std::string name(const ::testing::TestParamInfo<Param>& info) {
      // Format the test case name parameterization nicely
      return info.param.name;
    }
  };
}

TEST_P(TestUTF, UTF32toUTF8) {
  auto& param = this->GetParam();
  if (param.utf32 >= 0) {
    auto codepoint = char32_t(param.utf32);
    ASSERT_EQ(param.utf8, egg::utf::to_utf8(codepoint));
    std::u32string utf32(1, codepoint);
    ASSERT_EQ(param.utf8, egg::utf::to_utf8(utf32));
  }
}

TEST_P(TestUTF, UTF8toUTF32) {
  auto& param = this->GetParam();
  if (param.utf32 >= 0) {
    auto codepoint = char32_t(param.utf32);
    auto utf32 = egg::utf::to_utf32(param.utf8);
    ASSERT_EQ(1u, utf32.length());
    ASSERT_EQ(codepoint, utf32[0]);
  }
}

EGG_INSTANTIATE_TEST_CASE_P(TestUTF)
