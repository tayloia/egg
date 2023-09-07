namespace egg::ovum {
  class CallArguments : public ICallArguments {
    CallArguments(const CallArguments&) = delete;
    CallArguments& operator=(const CallArguments&) = delete;
  private:
    struct Argument {
      String name;
      Value value;
    };
    std::vector<Argument> arguments;
  public:
    CallArguments() = default;
    virtual size_t getArgumentCount() const {
      return this->arguments.size();
    }
    virtual bool getArgumentByIndex(size_t index, Value& value, String* name) const {
      if (index < this->arguments.size()) {
        if (name != nullptr) {
          *name = this->arguments[index].name;
        }
        value = this->arguments[index].value;
        return true;
      }
      return false;
    }
    void addUnnamed(const Value& value) {
      this->arguments.emplace_back(String(), value);
    }
  };
}
