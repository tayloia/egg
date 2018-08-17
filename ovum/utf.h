namespace egg::ovum {
  class UTF32 {
  public:
    template<typename TARGET>
    static void toUTF8(TARGET&& target, char32_t utf32) {
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
    inline static std::string toUTF8(char32_t utf32) {
      std::string utf8;
      auto target = std::back_inserter(utf8);
      UTF32::toUTF8(target, utf32);
      return utf8;
    }
    inline static std::string toUTF8(const std::u32string& utf32) {
      std::string utf8;
      auto target = std::back_inserter(utf8);
      for (auto ch : utf32) {
        UTF32::toUTF8(target, ch);
      }
      return utf8;
    }
  };

  class UTF8 {
    UTF8& operator=(const UTF8&) = delete;
  public:
    template<typename TARGET>
    static bool toUTF32(TARGET&& target, const uint8_t* utf8, size_t bytes) {
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
  private:
    const uint8_t* const begin; // Points to the first code unit of the string
    const uint8_t* p;           // Points to the code unit immediately after of the current code point
    const uint8_t* const end;   // Points to the code unit after the last one of the string
  public:
    UTF8(const void* begin, const void* end, size_t offset)
      : begin(static_cast<const uint8_t*>(begin)), end(static_cast<const uint8_t*>(end)) {
      assert(this->begin != nullptr);
      assert(this->end >= this->begin);
      this->p = this->begin + offset;
      assert((this->p >= this->begin) && (this->p <= this->end));
    }
    UTF8(const std::string& utf8, size_t offset)
      : UTF8(utf8.data(), utf8.data() + utf8.size(), offset) {
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
        auto length = UTF8::sizeFromLeadByte(*this->p);
        if ((length <= remaining) && UTF8::toUTF32(codepoint, this->p, length)) {
          this->p += length;
          return true;
        }
      }
      return false;
    }
    bool forward() {
      // Return true iff there's a valid codepoint sequence next
      if (this->p < this->end) {
        auto q = this->p + UTF8::sizeFromLeadByte(*this->p);
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
        auto length = this->p - b;
        assert(length > 0);
        (void)UTF8::toUTF32(codepoint, b, size_t(length));
        this->p = b;
        return true;
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
    const uint8_t* get() const {
      assert((this->p >= this->begin) && (this->p <= this->end));
      return this->p;
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
          auto length = UTF8::sizeFromLeadByte(*this->p);
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
      // Fetch the offset used in egg::ovum::StringIteration::internal
      auto internal = this->p - this->begin;
      assert(internal >= 0);
      return size_t(internal);
    }
    static size_t sizeFromLeadByte(uint8_t lead) {
      // OPTIMIZE
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
    inline static std::u32string toUTF32(const std::string& utf8) {
      UTF8 reader(utf8, 0);
      std::u32string utf32;
      char32_t codepoint;
      while (reader.forward(codepoint)) {
        utf32.push_back(codepoint);
      }
      return utf32;
    }
    inline static std::string::const_iterator offsetOfCodePoint(const std::string& utf8, size_t index) {
      auto remaining = utf8.size();
      auto p = utf8.cbegin();
      auto q = utf8.cend();
      while (p < q) {
        if (index-- == 0) {
          return p;
        }
        auto length = UTF8::sizeFromLeadByte(uint8_t(*p));
        if (length > remaining) {
          return p; // Malformed
        }
        remaining -= length;
        std::advance(p, length);
      }
      return q;
    }
    inline static size_t measure(const uint8_t* p, const uint8_t* q) {
      // Return SIZE_MAX if this is not valid UTF-8, otherwise the codepoint count
      size_t count = 0;
      while (p < q) {
        if (*p < 0x80) {
          // Fast code path for ASCII
          p++;
        } else {
          auto remaining = size_t(q - p);
          auto length = UTF8::sizeFromLeadByte(*p);
          if (length > remaining) {
            // Truncated
            return SIZE_MAX;
          }
          assert(length > 1);
          for (size_t i = 1; i < length; ++i) {
            if ((p[i] & 0xC0) != 0x80) {
              // Bad continuation byte
              return SIZE_MAX;
            }
          }
          p += length;
        }
        count++;
      }
      return count;
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
        if ((b + UTF8::sizeFromLeadByte(*b)) == after) {
          return b;
        }
      }
      return nullptr;
    }
  };
}
