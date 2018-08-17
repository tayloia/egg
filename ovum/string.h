namespace egg::ovum {
  class String : public Memory {
  public:
    String() = default; // implicit
    explicit String(const char* utf8); // fallback to factory
    template<size_t N>
    String(const char (&utf8)[N]) : String(utf8) { // implicit; fallback to factory
    }
    explicit String(const IMemory* rhs) : Memory(rhs) {
    }
    String(const std::string& utf8, size_t codepoints = SIZE_MAX); // implicit; fallback to factory
    size_t length() const {
      auto* memory = this->get();
      if (memory == nullptr) {
        return 0;
      }
      auto codepoints = size_t(memory->tag().u);
      assert(codepoints >= ((memory->bytes() + 3) / 4));
      assert(codepoints <= memory->bytes());
      return codepoints;
    }
    std::string toUTF8() const {
      auto* memory = this->get();
      if (memory == nullptr) {
        return std::string();
      }
      return std::string(memory->begin(), memory->end());
    }

    // WIBBLE helpers
    bool empty() const {
      return this->length() == 0;
    }
    bool equals(const String& other) const;
    bool lessThan(const String& other) const;
    int64_t compareTo(const String& other) const;
    int64_t hashCode() const;
    bool startsWith(const String& other) const;
    bool endsWith(const String& other) const;
    int32_t codePointAt(size_t index) const;
    int64_t indexOfCodePoint(char32_t codepoint, size_t fromIndex = 0) const;
    int64_t indexOfString(const String& needle, size_t fromIndex = 0) const;
    int64_t lastIndexOfCodePoint(char32_t codepoint, size_t fromIndex = SIZE_MAX) const;
    int64_t lastIndexOfString(const String& needle, size_t fromIndex = SIZE_MAX) const;
    String substring(size_t begin, size_t end) const;
    String repeat(size_t count) const;
    String replace(const String& needle, const String& replacement, int64_t occurrences) const;

    bool operator==(const String& rhs) const {
      return this->equals(rhs);
    }

    // These factories use the fallback string allocator; see StringFactory for others
    static String fromCodePoint(char32_t codepoint); // fallback to factory
    static String fromUTF8(const std::string& utf8, size_t codepoints = SIZE_MAX); // fallback to factory
    static String fromUTF32(const std::u32string& utf32); // fallback to factory
  };

  struct StringLess { // WIBBLE
    bool operator()(const String& lhs, const String& rhs) const; // WIBBLE
  };

  template<typename T> // WIBBLE
  using StringMap = std::map<String, T, StringLess>;
}

namespace std {
  template<> struct hash<egg::ovum::String> {
    // String hash specialization for use with std::unordered_map<> etc.
    inline size_t operator()(const egg::ovum::String& s) const {
      return size_t(s.hashCode());
    }
  };
}

std::ostream& operator<<(std::ostream& os, const egg::ovum::String& text);
