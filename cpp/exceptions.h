namespace egg::yolk {
  class Exception : public std::runtime_error {
  private:
    std::string reason_value;
    std::string where_value;
  public:
    Exception(const std::string& reason, const std::string& where);
    Exception(const std::string& reason, const std::string& file, size_t line);
    Exception(const std::string& reason, const std::string& file, size_t line, size_t column);
    virtual const std::string& reason() const {
      return this->reason_value;
    }
    virtual const std::string& where() const {
      return this->where_value;
    }
  };
}
