namespace egg::ovum {
  class CallArguments : public ICallArguments {
    CallArguments(const CallArguments&) = delete;
    CallArguments& operator=(const CallArguments&) = delete;
  private:
    struct Argument {
      HardValue value;
      String name;
      const SourceRange* source;
    };
    std::vector<Argument> arguments;
  public:
    CallArguments() = default;
    virtual size_t getArgumentCount() const {
      return this->arguments.size();
    }
    virtual bool getArgumentValueByIndex(size_t index, HardValue& value) const {
      if (index < this->arguments.size()) {
        value = this->arguments[index].value;
        return true;
      }
      return false;
    }
    virtual bool getArgumentNameByIndex(size_t index, String& name) const {
      if (index < this->arguments.size()) {
        const auto& value = this->arguments[index].name;
        if (!value.empty()) {
          name = value;
          return true;
        }
      }
      return false;
    }
    virtual bool getArgumentSourceByIndex(size_t index, SourceRange& source) const {
      if (index < this->arguments.size()) {
        const auto& value = this->arguments[index].source;
        if (value != nullptr) {
          source = *value;
          return true;
        }
      }
      return false;
    }
    void addUnnamed(const HardValue& value, const SourceRange* source) {
      this->arguments.emplace_back(value, String(), source);
    }
  };
}
