namespace egg::yolk {
  class Exception : public std::runtime_error {
  private:
    std::string location;
  public:
    Exception(const std::string& what, const std::string& file, size_t line);
    virtual const std::string& where() const {
      return this->location;
    }
  };
}
