namespace egg::ovum {
  class Exception : public std::runtime_error {
  private:
    std::string reason_value;
    std::string where_value;
  public:
    Exception(const std::string& what, const std::string& reason, const std::string& where);
    explicit Exception(const std::string& reason, const std::source_location location = std::source_location::current());
    Exception(const std::string& reason, const std::string& where);
    virtual const std::string& reason() const {
      return this->reason_value;
    }
    virtual const std::string& where() const {
      return this->where_value;
    }
  };

  class SyntaxException : public Exception {
  private:
    std::string token_value;
    std::string resource_value;
    SourceRange range_value;
  public:
    SyntaxException(const std::string& reason, const std::string& resource, const SourceLocation& location, const std::string& token = {});
    SyntaxException(const std::string& reason, const std::string& resource, const SourceRange& range, const std::string& token = {});
    virtual const std::string& token() const {
      return this->token_value;
    }
    virtual const std::string& resource() const {
      return this->resource_value;
    }
    virtual const SourceRange& range() const {
      return this->range_value;
    }
  };
}
