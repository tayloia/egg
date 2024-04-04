namespace egg::yolk {
  class Options : public std::multimap<std::string, std::string> {
  public:
    std::vector<std::string> query(const std::string& key) const {
      std::vector<std::string> result;
      auto range = this->equal_range(key);
      for (auto i = range.first; i != range.second; ++i) {
        result.push_back(i->second);
      }
      return result;
    }
    std::string get(const std::string& key) const {
      assert(this->count(key) == 1);
      return this->lower_bound(key)->second;
    }
    std::vector<std::string> extraneous() const {
      return this->query({});
    }
  };

  class OptionParser {
  public:
    enum class Occurrences {
      ZeroOrOne,
      ZeroOrMore,
      One,
      OneOrMore
    };
  private:
    std::vector<std::string> arguments;
    struct Rule {
      Occurrences occurrences = Occurrences::OneOrMore;
    };
    struct Rules : public std::map<std::string, Rule> {
      std::set<std::set<std::string>> incompatibles;
      void parse(Options& options, const std::string& key, const std::string* value);
    };
    Rules rules;
  public:
    OptionParser& withExtraneousArguments(Occurrences occurrences = Occurrences::ZeroOrMore);
    OptionParser& withOption(const std::string& option, Occurrences occurrences);
    OptionParser& withArgument(const std::string& argument) {
      this->arguments.push_back(argument);
      return *this;
    }
    OptionParser& withArguments(std::initializer_list<std::string> list) {
      this->arguments.insert(this->arguments.end(), list.begin(), list.end());
      return *this;
    }
    template<typename T>
    OptionParser& withArguments(T begin, T end) {
      this->arguments.insert(this->arguments.end(), begin, end);
      return *this;
    }
    Options parse();
  };
}
