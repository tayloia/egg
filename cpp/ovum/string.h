namespace egg::ovum {
  class String : public Memory {
  public:
    String() = default; // implicit
    explicit String(const IMemory* rhs) : Memory(rhs) {
    }
    explicit String(const char* utf8) // fallback to factory
      : String(String::fromUTF8(nullptr, utf8)) {
    }
    template<size_t N>
    String(const char(&utf8)[N]) // implicit; fallback to factory
      : String(String::fromUTF8(nullptr, utf8, N - 1, N - 1)) {
    }
    String(const std::string& utf8, size_t codepoints = SIZE_MAX) // implicit; fallback to factory
      : String(String::fromUTF8(nullptr, utf8.data(), utf8.size(), codepoints)) {
    }
    bool validate() const;

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
    String replace(const String& needle, const String& replacement, int64_t occurrences = INT64_MAX) const;
    String substring(size_t begin, size_t end = SIZE_MAX) const;
    String repeat(size_t count) const;

    // Convenience helpers
    bool empty() const;
    bool lessThan(const String& other) const;
    String slice(int64_t begin, int64_t end = INT64_MAX) const;
    std::vector<String> split(const String& separator, int64_t limit = INT64_MAX) const;
    String join(const std::vector<String>& parts) const;
    String padLeft(size_t target) const;
    String padLeft(size_t target, const String& padding) const;
    String padRight(size_t target) const;
    String padRight(size_t target, const String& padding) const;
    std::string toUTF8() const;

    // Equality operators
    bool equals(const char* utf8) const;
    bool operator==(const String& rhs) const {
      return this->equals(rhs);
    }
    bool operator!=(const String& rhs) const {
      return !this->equals(rhs);
    }

    // These factories can take null as their first argument to use the fallback string allocator
    static String fromUTF8(IAllocator* allocator, const void* utf8, size_t bytes = SIZE_MAX, size_t codepoints = SIZE_MAX);
    static String fromUTF32(IAllocator* allocator, const void* utf32, size_t codepoints = SIZE_MAX);
  };

  class StringBuilder : public Printer {
    StringBuilder(const StringBuilder&) = delete;
    StringBuilder& operator=(const StringBuilder&) = delete;
  private:
    IAllocator* allocator;
    std::stringstream ss;
  public:
    explicit StringBuilder(IAllocator* allocator = nullptr)
      : Printer(ss, Print::Options::DEFAULT),
        allocator(allocator) {
    }
    template<typename T>
    StringBuilder& add(T value) {
      this->write(value);
      return *this;
    }
    StringBuilder& add(char value) {
      this->ss.put(value);
      return *this;
    }
    StringBuilder& add(const char* value) {
      assert(value != nullptr);
      this->ss << value;
      return *this;
    }
    template<typename T, typename... ARGS>
    StringBuilder& add(T value, ARGS&&... args) {
      return this->add(value).add(std::forward<ARGS>(args)...);
    }
    bool empty() const {
      return this->ss.rdbuf()->in_avail() == 0;
    }
    std::string toUTF8() const {
      return this->ss.str();
    }
    String build() const;
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
