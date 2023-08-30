namespace egg::ovum {
  class Print {
  public:
    struct Options {
      char quote;
      static const Options DEFAULT;
    };

    // Print to stream
    template<typename T>
    static void write(std::ostream& stream, T value, const Options& options) = delete;
    static void write(std::ostream& stream, std::nullptr_t value, const Options& options);
    static void write(std::ostream& stream, bool value, const Options& options);
    static void write(std::ostream& stream, int32_t value, const Options& options);
    static void write(std::ostream& stream, int64_t value, const Options& options);
    static void write(std::ostream& stream, uint32_t value, const Options& options);
    static void write(std::ostream& stream, uint64_t value, const Options& options);
    static void write(std::ostream& stream, float value, const Options& options);
    static void write(std::ostream& stream, double value, const Options& options);
    static void write(std::ostream& stream, const std::string& value, const Options& options);
    static void write(std::ostream& stream, const String& value, const Options& options);
    static void write(std::ostream& stream, const Type& value, const Options& options);
    static void write(std::ostream& stream, const Value& value, const Options& options);
    static void write(std::ostream& stream, ValueFlags value, const Options& options);
    static void write(std::ostream& stream, ILogger::Severity value, const Options& options);
    static void write(std::ostream& stream, ILogger::Source value, const Options& options);

    // Print string to stream
    static void ascii(std::ostream& stream, const std::string& value, char quote);
    static void escape(std::ostream& stream, const std::string& value, char quote);
  };

  class Printer {
    Printer(const Printer&) = delete;
    Printer& operator=(const Printer&) = delete;
  private:
    std::ostream& os;
    const Print::Options& options;
  public:
    Printer(std::ostream& os, const Print::Options& options)
      : os(os),
        options(options) {
    }
    template<typename T>
    Printer& operator<<(const T& value) {
      this->os << value;
      return *this;
    }
    Printer& operator<<(const String& value);
    template<typename T>
    void write(T value) {
      Print::write(this->os, value, this->options);
    }
    std::ostream& stream() const {
      return this->os;
    }
  };
}
