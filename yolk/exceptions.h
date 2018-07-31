namespace egg::yolk {
  class Exception : public std::runtime_error {
  private:
    std::string reason_value;
    std::string where_value;
  public:
    Exception(const std::string& what, const std::string& reason, const std::string& where);
    Exception(const std::string& reason, const std::string& where);
    Exception(const std::string& reason, const std::string& file, size_t line);
    virtual const std::string& reason() const {
      return this->reason_value;
    }
    virtual const std::string& where() const {
      return this->where_value;
    }
  };

  struct ExceptionLocation {
    size_t line;
    size_t column;
  };

  struct ExceptionLocationRange {
    ExceptionLocation begin;
    ExceptionLocation end;
  };

  class SyntaxException : public Exception {
  private:
    std::string token_value;
    std::string resource_value;
    ExceptionLocationRange location_value;
  public:
    SyntaxException(const std::string& reason, const std::string& resource, const ExceptionLocation& location, const std::string& token = std::string());
    SyntaxException(const std::string& reason, const std::string& resource, const ExceptionLocationRange& location, const std::string& token = std::string());
    virtual const std::string& token() const {
      return this->token_value;
    }
    virtual const std::string& resource() const {
      return this->resource_value;
    }
    virtual const ExceptionLocationRange& location() const {
      return this->location_value;
    }
  };
}
