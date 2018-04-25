namespace egg::utf {
  template<typename TARGET>
  void utf32_to_utf8(TARGET&& target, char32_t utf32) {
    // See https://en.wikipedia.org/wiki/UTF-8
    assert((utf32 >= 0) && (utf32 <= 0x10FFFF));
    if (utf32 < 0x80) {
      target = char(utf32);
    } else if (utf32 < 0x800) {
      target = char(0xC0 + (utf32 >> 6));
      target = char(0x80 + (utf32 & 0x2F));
    } else if (utf32 < 0x10000) {
      target = char(0xE0 + (utf32 >> 12));
      target = char(0x80 + ((utf32 >> 6) & 0x2F));
      target = char(0x80 + (utf32 & 0x2F));
    } else {
      target = char(0xF0 + (utf32 >> 18));
      target = char(0x80 + ((utf32 >> 12) & 0x2F));
      target = char(0x80 + ((utf32 >> 6) & 0x2F));
      target = char(0x80 + (utf32 & 0x2F));
    }
  }

  template<typename TARGET>
  bool utf8_to_utf32(TARGET&& target, const uint8_t* utf8, size_t bytes) {
    assert(utf8 != nullptr);
    char32_t result;
    switch (bytes) {
    case 1:
      // Fast code path for ASCII
      assert(*utf8 < 0x80);
      target = char32_t(*utf8);
      return true;
    case 2:
      assert((*utf8 & 0xE0) == 0xC0);
      result = char32_t(*utf8 & 0x1F);
      break;
    case 3:
      assert((*utf8 & 0xF0) == 0xE0);
      result = char32_t(*utf8 & 0x0F);
      break;
    case 4:
      assert((*utf8 & 0xF8) == 0xF0);
      result = char32_t(*utf8 & 0x07);
      break;
    default:
      return false;
    }
    do {
      if ((*++utf8 & 0xC0) != 0x80) {
        // Bad continuation byte
        return false;
      }
      result = (result << 6) | char32_t(*utf8 & 0x3F);
    } while (--bytes > 0);
    target = result;
    return true;
  }

  inline std::string to_utf8(char32_t utf32) {
    std::string utf8;
    auto target = std::back_inserter(utf8);
    utf32_to_utf8(target, utf32);
    return utf8;
  }

  inline std::string to_utf8(const std::u32string& utf32) {
    std::string utf8;
    auto target = std::back_inserter(utf8);
    for (auto ch : utf32) {
      utf32_to_utf8(target, ch);
    }
    return utf8;
  }

  class utf8_reader {
    utf8_reader& operator=(const utf8_reader&) = delete;
  private:
    const uint8_t* p;
    const uint8_t* const q;
  public:
    utf8_reader(const void* begin, const void* end)
      : p(static_cast<const uint8_t*>(begin)), q(static_cast<const uint8_t*>(end)) {
      assert(p != nullptr);
      assert(q != nullptr);
      assert(p <= q);
    }
    bool step() {
      if (this->p < this->q) {
        if (*this->p < 0x80) {
          // Fast code path for ASCII
          this->p++;
          return true;
        }
        auto remaining = size_t(this->q - this->p);
        auto length = utf8_reader::bytes(*this->p);
        if (length > remaining) {
          // Truncated
          return false;
        }
        this->p += length;
        return true;
      }
      return false;
    }
    bool read(char32_t& codepoint) {
      if (this->p < this->q) {
        if (*this->p < 0x80) {
          // Fast code path for ASCII
          codepoint = *this->p++;
          return true;
        }
        auto remaining = size_t(this->q - this->p);
        auto length = utf8_reader::bytes(*this->p);
        if (length > remaining) {
          // Truncated
          return false;
        }
        if (utf8_to_utf32(codepoint, this->p, length)) {
          this->p += length;
          return true;
        }
        return true;
      }
      return false;
    }
    static size_t bytes(uint8_t lead) {
      if (lead < 0x80) {
        return 1;
      } else if (lead < 0xC0) {
        return 2;
      } else if (lead < 0xE0) {
        return 3;
      } else if (lead < 0xF0) {
        return 4;
      }
      return SIZE_MAX;
    }
  };
}
