namespace egg::yolk {
  class String {
  public:
    static bool contains(const std::string& haystack, const std::string& needle) {
      return haystack.find(needle) != std::string::npos;
    }
    static bool startsWith(const std::string& haystack, const std::string& needle) {
      return (haystack.size() >= needle.size()) && std::equal(needle.begin(), needle.end(), haystack.begin());
    }
    static bool endsWith(const std::string& haystack, const std::string& needle) {
      return (haystack.size() >= needle.size()) && std::equal(needle.rbegin(), needle.rend(), haystack.rbegin());
    }
    static std::string transform(const std::string& src, std::function<char(char)> lambda) {
      std::string dst;
      dst.reserve(src.size());
      std::transform(src.begin(), src.end(), std::back_inserter(dst), lambda);
      return dst;
    }
    static std::string toLower(const std::string& src) {
      return String::transform(src, [](char x) { return char(std::tolower(x)); });
    }
    static std::string toUpper(const std::string& src) {
      return String::transform(src, [](char x) { return char(std::toupper(x)); });
    }
    static std::string replace(const std::string& src, char from, char to) {
      return String::transform(src, [from, to](char x) { return (x == from) ? to : x; });
    }
    static void terminate(std::string& str, char terminator) {
      if (str.empty() || (str.back() != terminator)) {
        str.push_back(terminator);
      }
    }
    static bool tryParseSigned(int64_t& dst, const std::string& src, int base = 10) {
      if (!src.empty()) {
        errno = 0;
        auto* str = src.c_str();
        char* end = nullptr;
        auto value = std::strtoll(str, &end, base);
        if ((errno == 0) && (end == (str + src.size()))) {
          dst = value;
          return true;
        }
      }
      return false;
    }
    static bool tryParseUnsigned(uint64_t& dst, const std::string& src, int base = 10) {
      if (!src.empty()) {
        errno = 0;
        auto* str = src.c_str();
        char* end = nullptr;
        auto value = std::strtoull(str, &end, base);
        if ((errno == 0) && (end == (str + src.size()))) {
          dst = value;
          return true;
        }
      }
      return false;
    }
    static bool tryParseFloat(double& dst, const std::string& src) {
      if (!src.empty()) {
        errno = 0;
        auto* str = src.c_str();
        char* end = nullptr;
        auto value = std::strtod(str, &end);
        if ((errno == 0) && (end == (str + src.size()))) {
          dst = value;
          return true;
        }
      }
      return false;
    }
    static void push_utf8(std::string& str, int utf8) {
      // See https://en.wikipedia.org/wiki/UTF-8
      assert((utf8 >= 0) && (utf8 <= 0x10FFFF));
      if (utf8 < 0x80) {
        str.push_back(char(utf8));
      } else if (utf8 < 0x800) {
        str.push_back(char(0xC0 + (utf8 >> 6)));
        str.push_back(char(0x80 + (utf8 & 0x2F)));
      } else if (utf8 < 0x10000) {
        str.push_back(char(0xE0 + (utf8 >> 12)));
        str.push_back(char(0x80 + (utf8 >> 6) & 0x2F));
        str.push_back(char(0x80 + (utf8 & 0x2F)));
      } else {
        str.push_back(char(0xF0 + (utf8 >> 18)));
        str.push_back(char(0x80 + (utf8 >> 12) & 0x2F));
        str.push_back(char(0x80 + (utf8 >> 6) & 0x2F));
        str.push_back(char(0x80 + (utf8 & 0x2F)));
      }
    }
    static std::string unicodeToString(int ch) {
      std::stringstream ss;
      if ((ch >= 32) && (ch <= 126)) {
        ss << '\'' << char(ch) << '\'';
      } else if (ch < 0) {
        ss << "<EOF>";
      } else {
        ss << "U+" << std::uppercase << std::setfill('0') << std::setw(4) << std::hex << ch;
      }
      return ss.str();
    }
    struct StringFromEnum {
      int value;
      const char* text;
    };
    template<typename E, size_t N>
    static std::string fromEnum(E value, const StringFromEnum (&table)[N]) {
      return String::fromEnum(static_cast<int>(value), table, table + N);
    }
    static std::string fromEnum(int value, const StringFromEnum* tableBegin, const StringFromEnum* tableEnd);
  };
}
