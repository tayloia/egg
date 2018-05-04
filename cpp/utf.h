namespace egg::utf {
  template<typename TARGET>
  void utf32_to_utf8(TARGET&& target, char32_t utf32) {
    // See https://en.wikipedia.org/wiki/UTF-8
    assert((utf32 >= 0) && (utf32 <= 0x10FFFF));
    if (utf32 < 0x80) {
      target = char(utf32);
    } else if (utf32 < 0x800) {
      target = char(0xC0 + (utf32 >> 6));
      target = char(0x80 + (utf32 & 0x3F));
    } else if (utf32 < 0x10000) {
      target = char(0xE0 + (utf32 >> 12));
      target = char(0x80 + ((utf32 >> 6) & 0x3F));
      target = char(0x80 + (utf32 & 0x3F));
    } else {
      target = char(0xF0 + (utf32 >> 18));
      target = char(0x80 + ((utf32 >> 12) & 0x3F));
      target = char(0x80 + ((utf32 >> 6) & 0x3F));
      target = char(0x80 + (utf32 & 0x3F));
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
    } while (--bytes > 1);
    target = result;
    return true;
  }

  class utf8_reader {
    utf8_reader& operator=(const utf8_reader&) = delete;
  public:
    enum End { First, Last };
  private:
    const uint8_t* const begin; // Points to the first code unit of the string
    const uint8_t* p;           // Points to the code unit immediately after of the current code point
    const uint8_t* const end;   // Points to the code unit after the last one of the string
    utf8_reader(const void* begin, const void* end)
      : begin(static_cast<const uint8_t*>(begin)), end(static_cast<const uint8_t*>(end)) {
      assert(begin != nullptr);
      assert(end != nullptr);
      assert(begin <= end);
    }
  public:
    utf8_reader(const std::string& utf8, End init)
      : utf8_reader(utf8.data(), utf8.data() + utf8.size()) {
      if (init == First) {
        this->p = this->begin;
      } else {
        this->p = this->end;
      }
    }
    utf8_reader(const std::string& utf8, size_t internal)
      : utf8_reader(utf8.data(), utf8.data() + utf8.size()) {
      // Used by egg::lang::StringIteration
      this->p = this->begin + internal;
    }
    bool forward(char32_t& codepoint) {
      // Return true iff there's a valid codepoint sequence next
      // OPTIMIZE
      if (this->p < this->end) {
        if (*this->p < 0x80) {
          // Fast code path for ASCII
          codepoint = *this->p++;
          return true;
        }
        auto remaining = size_t(this->end - this->p);
        auto length = utf8_reader::getSizeFromLead(*this->p);
        if ((length <= remaining) && utf8_to_utf32(codepoint, this->p, length)) {
          this->p += length;
          return true;
        }
      }
      return false;
    }
    bool forward() {
      // Return true iff there's a valid codepoint sequence next
      if (this->p < this->end) {
        auto q = this->p + utf8_reader::getSizeFromLead(*this->p);
        if (q <= this->end) {
          this->p = q;
          return true;
        }
      }
      return false;
    }
    bool skipForward(size_t codepoints) {
      // OPTIMIZE
      while (codepoints-- > 0) {
        if (!this->forward()) {
          return false;
        }
      }
      return true;
    }
    bool backward(char32_t& codepoint) {
      // Return true iff there's a valid codepoint sequence previously
      // OPTIMIZE
      auto b = this->before(this->p);
      if (b != nullptr) {
        // We can find the start of the current code point
        auto a = this->before(b);
        if (a != nullptr) {
          // And also the start of the preceding code point
          auto length = b - a;
          assert(length > 0);
          (void)utf8_to_utf32(codepoint, a, size_t(length));
          this->p = b;
          return true;
        }
      }
      return false;
    }
    bool backward() {
      // Return true iff we can step back to the start of the current codepoint
      auto b = this->before(this->p);
      if (b != nullptr) {
        // We can find the start of the current code point
        this->p = b;
        return true;
      }
      return false;
    }
    bool skipBackward(size_t codepoints) {
      // OPTIMIZE
      while (codepoints-- > 0) {
        if (!this->backward()) {
          return false;
        }
      }
      return true;
    }
    size_t validate() {
      // Return SIZE_MAX if this is not valid UTF-8, otherwise the codepoint count
      size_t count = 0;
      while (this->p < this->end) {
        if (*this->p < 0x80) {
          // Fast code path for ASCII
          this->p++;
        } else {
          auto remaining = size_t(this->end - this->p);
          auto length = utf8_reader::getSizeFromLead(*this->p);
          if (length > remaining) {
            // Truncated
            return SIZE_MAX;
          }
          assert(length > 1);
          for (size_t i = 1; i < length; ++i) {
            if ((this->p[i] & 0xC0) != 0x80) {
              // Bad continuation byte
              return SIZE_MAX;
            }
          }
          this->p += length;
        }
        count++;
      }
      return count;
    }
    size_t getIterationInternal() const {
      // Fetch the offset used in egg::lang::StringIteration::internal
      auto internal = this->p - this->begin;
      assert(internal >= 0);
      return size_t(internal);
    }
    static size_t getSizeFromLead(uint8_t lead) {
      if (lead < 0x80) {
        return 1;
      } else if (lead < 0xC0) {
        return SIZE_MAX; // Continuation byte
      } else if (lead < 0xE0) {
        return 2;
      } else if (lead < 0xF0) {
        return 3;
      } else if (lead < 0xF8) {
        return 4;
      }
      return SIZE_MAX;
    }
  private:
    const uint8_t* before(const uint8_t* after) const {
      if (after > this->begin) {
        auto b = after;
        while ((*--b & 0xC0) == 0x80) {
          // Continuation byte
          if (b <= this->begin) {
            return nullptr;
          }
        }
        if ((b + utf8_reader::getSizeFromLead(*b)) == after) {
          return b;
        }
      }
      return nullptr;
    }
  };

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

  inline std::u32string to_utf32(const std::string& utf8) {
    utf8_reader reader(utf8, utf8_reader::First);
    std::u32string utf32;
    char32_t codepoint;
    while (reader.forward(codepoint)) {
      utf32.push_back(codepoint);
    }
    return utf32;
  }

  inline std::string::const_iterator utf8_offset(const std::string& utf8, size_t codepoint_index) {
    auto p = utf8.begin();
    std::advance(p, codepoint_index); // WIBBLE
    return p;
  }
}
