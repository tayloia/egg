namespace egg::utf {
  template<typename TARGET>
  inline void utf32_to_utf8(TARGET&& target, char32_t utf32) {
    // See https://en.wikipedia.org/wiki/UTF-8
    assert((utf32 >= 0) && (utf32 <= 0x10FFFF));
    if (utf32 < 0x80) {
      target = char(utf32);
    } else if (utf32 < 0x800) {
      target = char(0xC0 + (utf32 >> 6));
      target = char(0x80 + (utf32 & 0x2F));
    } else if (utf32 < 0x10000) {
      target = char(0xE0 + (utf32 >> 12));
      target = char(0x80 + (utf32 >> 6) & 0x2F);
      target = char(0x80 + (utf32 & 0x2F));
    } else {
      target = char(0xF0 + (utf32 >> 18));
      target = char(0x80 + (utf32 >> 12) & 0x2F);
      target = char(0x80 + (utf32 >> 6) & 0x2F);
      target = char(0x80 + (utf32 & 0x2F));
    }
  }

  inline std::string to_utf8(const std::u32string& utf32) {
    std::string utf8;
    auto target = std::back_inserter(utf8);
    for (auto ch : utf32) {
      utf32_to_utf8(target, ch);
    }
    return utf8;
  }
}
