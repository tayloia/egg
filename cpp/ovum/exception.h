namespace egg::ovum {
  class Exception : public std::runtime_error, public std::map<std::string, std::string> {
  public:
    explicit Exception(const char* what)
      : std::runtime_error(what) {
    }
    virtual const char* what() const noexcept override;
    Exception& here(const std::source_location location = std::source_location::current());
    Exception& here(const std::string& file, size_t line, size_t column = 0);
    Exception& with(const std::string& key, const std::string& value) {
      this->emplace(key, value);
      return *this;
    }
    const std::string& get(const std::string& key) const {
      auto found = this->find(key);
      if (found != this->end()) {
        return found->second;
      }
      // Don't throw another egg::ovum exception!
      throw std::runtime_error("exception key not found: " + key);
    }
    std::string get(const std::string& key, const std::string& defval) const {
      auto found = this->find(key);
      if (found != this->end()) {
        return found->second;
      }
      return defval;
    }
    bool query(const std::string& key, std::string& value) const {
      auto found = this->find(key);
      if (found != this->end()) {
        value = found->second;
        return true;
      }
      return false;
    }
    std::string format(const char* fmt) const;
  };

  class InternalException : public Exception {
  public:
    explicit InternalException(const std::string& reason, const std::source_location location = std::source_location::current());
  };

  class SyntaxException : public Exception {
  private:
    SourceRange range_value;
  public:
    SyntaxException(const std::string& reason, const std::string& resource, const SourceLocation& location, const std::string& token = {});
    SyntaxException(const std::string& reason, const std::string& resource, const SourceRange& range, const std::string& token = {});
    const SourceRange& range() const {
      return this->range_value;
    }
  };
}
