namespace egg::ovum {
  struct LocationSource {
    String file;
    size_t line;
    size_t column;

    inline LocationSource(const LocationSource& rhs) = default;
    inline LocationSource(const String& file, size_t line, size_t column)
      : file(file), line(line), column(column) {
    }
    void formatSourceString(StringBuilder& sb) const;
    String toSourceString() const;
  };

  struct LocationRuntime : public LocationSource {
    String function;
    const LocationRuntime* parent;

    inline LocationRuntime(const LocationRuntime& rhs)
      : LocationSource(rhs), function(rhs.function), parent(rhs.parent) {
    }
    inline LocationRuntime(const LocationSource& source, const String& function, const LocationRuntime* parent = nullptr)
      : LocationSource(source), function(function), parent(parent) {
    }
    void formatRuntimeString(StringBuilder& sb) const;
    String toRuntimeString() const;
  };

  class IPreparation {
  public:
    virtual ~IPreparation() {}
    virtual void raise(ILogger::Severity severity, const String& message) = 0;

    // Useful helpers
    template<typename... ARGS>
    void raiseWarning(ARGS&&... args) {
      auto message = StringBuilder::concat(std::forward<ARGS>(args)...);
      this->raise(ILogger::Severity::Warning, message);
    }
    template<typename... ARGS>
    void raiseError(ARGS&&... args) {
      auto message = StringBuilder::concat(std::forward<ARGS>(args)...);
      this->raise(ILogger::Severity::Error, message);
    }
  };

  class IExecution {
  public:
    virtual ~IExecution() {}
    virtual IAllocator& getAllocator() const = 0;
    virtual IBasket& getBasket() const = 0;
    virtual Value raise(const String& message) = 0;
    virtual Value assertion(const Value& predicate) = 0;
    virtual void print(const std::string& utf8) = 0;

    // Useful helper
    template<typename... ARGS>
    Value raiseFormat(ARGS&&... args) {
      auto message = StringBuilder::concat(std::forward<ARGS>(args)...);
      return this->raise(message);
    }
  };
}
