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
  };
}
