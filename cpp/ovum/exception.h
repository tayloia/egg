namespace egg::ovum {
  class Exception : public std::runtime_error, public std::map<std::string, std::string> {
  public:
    explicit Exception(const char* fmt, const std::source_location location = std::source_location::current());
    explicit Exception(const std::string& fmt, const std::source_location location = std::source_location::current());
    Exception(const std::string& reason, const std::string& file, size_t line, size_t column = 0);
    virtual const std::string& reason() const {
      return this->get("reason");
    }
    virtual const std::string& where() const {
      return this->get("where");
    }
    virtual const char* what() const noexcept override;
    Exception& with(const std::string& key, const std::string& value) {
      this->emplace(key, value);
      return *this;
    }
    const std::string& get(const std::string& key) const {
      auto found = this->find(key);
      if (found != this->end()) {
        return found->second;
      }
      throw std::runtime_error("key not found: " + key);
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

  class SyntaxException : public Exception {
  private:
    SourceRange range_value;
  public:
    SyntaxException(const std::string& reason, const std::string& resource, const SourceLocation& location, const std::string& token = {});
    SyntaxException(const std::string& reason, const std::string& resource, const SourceRange& range, const std::string& token = {});
    virtual const std::string& token() const {
      return this->get("token");
    }
    virtual const std::string& resource() const {
      return this->get("resource");
    }
    virtual const SourceRange& range() const {
      return this->range_value;
    }
  };
}
