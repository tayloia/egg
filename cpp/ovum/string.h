namespace egg::ovum {
  class String : public Memory {
  public:
    String() = default; // implicit
    explicit String(const IMemory* rhs) : Memory(rhs) {
    }
    bool validate() const;
    void swap(String& that) {
      String::hardSwap(*this, that);
    }

    // See http://chilliant.blogspot.co.uk/2018/05/egg-strings.html
    size_t length() const;
    int32_t codePointAt(size_t index) const;
    bool equals(const String& other) const;
    int64_t hash() const;
    int64_t compareTo(const String& other) const;
    bool contains(const String& needle) const;
    bool startsWith(const String& needle) const;
    bool endsWith(const String& needle) const;
    int64_t indexOfCodePoint(char32_t codepoint, size_t fromIndex = 0) const;
    int64_t indexOfString(const String& needle, size_t fromIndex = 0) const;
    int64_t lastIndexOfCodePoint(char32_t codepoint, size_t fromIndex = SIZE_MAX) const;
    int64_t lastIndexOfString(const String& needle, size_t fromIndex = SIZE_MAX) const;
    String replace(IAllocator& allocator, const String& needle, const String& replacement, int64_t occurrences = INT64_MAX) const;
    String substring(IAllocator& allocator, size_t begin, size_t end = SIZE_MAX) const;
    String repeat(IAllocator& allocator, size_t count) const;

    // Convenience helpers
    bool empty() const;
    bool lessThan(const String& other) const;
    String slice(IAllocator& allocator, int64_t begin, int64_t end = INT64_MAX) const;
    std::vector<String> split(IAllocator& allocator, const String& separator, int64_t limit = INT64_MAX) const;
    String join(IAllocator& allocator, const std::vector<String>& parts) const;
    String padLeft(IAllocator& allocator, size_t target) const;
    String padLeft(IAllocator& allocator, size_t target, const String& padding) const;
    String padRight(IAllocator& allocator, size_t target) const;
    String padRight(IAllocator& allocator, size_t target, const String& padding) const;
    std::string toUTF8() const;

    // Equality operators
    bool equals(const char* utf8) const;
    bool operator==(const String& rhs) const {
      return this->equals(rhs);
    }
    bool operator!=(const String& rhs) const {
      return !this->equals(rhs);
    }

    // Factories
    static String fromUTF8(IAllocator& allocator, const void* utf8, size_t bytes = SIZE_MAX, size_t codepoints = SIZE_MAX);
    static String fromUTF32(IAllocator& allocator, const void* utf32, size_t codepoints = SIZE_MAX);
  };

  class StringBuilder : public Printer {
    StringBuilder(const StringBuilder&) = delete;
    StringBuilder& operator=(const StringBuilder&) = delete;
  private:
    std::stringstream ss;
  public:
    explicit StringBuilder(const Print::Options& options = Print::Options::DEFAULT)
      : Printer(ss, options) {
    }
    template<typename T>
    StringBuilder& add(const T& value) {
      this->write(value);
      return *this;
    }
    template<typename T, typename... ARGS>
    StringBuilder& add(const T& value, ARGS&&... args) {
      return this->add(value).add(std::forward<ARGS>(args)...);
    }
    bool empty() const {
      return this->ss.rdbuf()->in_avail() == 0;
    }
    std::string toUTF8() const {
      return this->ss.str();
    }
    String build(IAllocator& allocator) const;
    template<typename... ARGS>
    static String concat(IAllocator& allocator, ARGS&&... args) {
      StringBuilder sb;
      return sb.add(std::forward<ARGS>(args)...).build(allocator);
    }
  };
}

namespace std {
  template<> struct equal_to<egg::ovum::String> {
    // String hash specialization for use with std::map<> etc.
    inline bool operator()(const egg::ovum::String& a, const egg::ovum::String& b) const {
      return a.equals(b);
    }
  };
  template<> struct less<egg::ovum::String> {
    // String hash specialization for use with std::map<> etc.
    inline bool operator()(const egg::ovum::String& a, const egg::ovum::String& b) const {
      return a.lessThan(b);
    }
  };
  template<> struct hash<egg::ovum::String> {
    // String hash specialization for use with std::unordered_map<> etc.
    inline size_t operator()(const egg::ovum::String& s) const {
      return size_t(s.hash());
    }
  };
}
