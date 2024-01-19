namespace egg::ovum {
  class HardObject;
  class HardValue;
  enum class ValueUnaryOp;
  enum class ValueBinaryOp;
  enum class ValueTernaryOp;
  enum class ValueMutationOp;
  enum class TypeUnaryOp;
  enum class TypeBinaryOp;

  class Print {
  public:
    struct Options {
      char quote = '\0';
      bool concise = false;
      bool names = true;
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
    static void write(std::ostream& stream, const ICollectable& value, const Options& options);
    static void write(std::ostream& stream, const IObject& value, const Options& options);
    static void write(std::ostream& stream, const IValue& value, const Options& options);
    static void write(std::ostream& stream, const IType& value, const Options& options);
    static void write(std::ostream& stream, const Type& value, const Options& options);
    static void write(std::ostream& stream, const HardObject& value, const Options& options);
    static void write(std::ostream& stream, const HardValue& value, const Options& options);
    static void write(std::ostream& stream, ValueFlags value, const Options& options);
    static void write(std::ostream& stream, ILogger::Severity value, const Options& options);
    static void write(std::ostream& stream, ILogger::Source value, const Options& options);
    static void write(std::ostream& stream, const SourceRange& value, const Options& options);
    static void write(std::ostream& stream, ValueUnaryOp value, const Options& options);
    static void write(std::ostream& stream, ValueBinaryOp value, const Options& options);
    static void write(std::ostream& stream, ValueTernaryOp value, const Options& options);
    static void write(std::ostream& stream, ValueMutationOp value, const Options& options);
    static void write(std::ostream& stream, TypeUnaryOp value, const Options& options);
    static void write(std::ostream& stream, TypeBinaryOp value, const Options& options);
    template<typename T>
    static void write(std::ostream& stream, const HardPtr<T>& value, const Options& options) {
      Print::write(stream, static_cast<const ICollectable*>(value.get()), options);
    }
    template<typename T>
    static void write(std::ostream& stream, const T* value, const Options& options) {
      if (value == nullptr) {
        stream << "null";
      } else {
        Print::write(stream, *value, options);
      }
    }

    // Print string to stream
    static void ascii(std::ostream& stream, const std::string& value, char quote);
    static void escape(std::ostream& stream, const std::string& value, char quote);
  };

  class Printer {
    Printer(const Printer&) = delete;
    Printer& operator=(const Printer&) = delete;
  public:
    std::ostream& stream;
    const Print::Options& options;
    Printer(std::ostream& stream, const Print::Options& options)
      : stream(stream),
        options(options) {
    }
    template<typename T>
    Printer& operator<<(const T& value) {
      Print::write(this->stream, value, this->options);
      return *this;
    }
    Printer& operator<<(const char* value) {
      this->stream << value;
      return *this;
    }
    Printer& operator<<(char value) {
      this->stream << value;
      return *this;
    }
    template<typename T>
    void write(const T& value) {
      Print::write(this->stream, value, this->options);
    }
    int describe(ValueFlags value); // returns precedence
    void describe(const IType& value);
    void describe(const IValue& value);
    void quote() {
      if (this->options.quote != '\0') {
        this->stream.put(this->options.quote);
      }
    }
  };
}
