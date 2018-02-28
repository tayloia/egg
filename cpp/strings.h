namespace egg::yolk {
  class String {
  public:
    static bool beginsWith(const std::string& haystack, const std::string& needle) {
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
  };
}
