namespace egg::ovum {
  class Print {
  public:
    // Print to stream
    template<typename T>
    static void write(std::ostream& stream, T value) = delete;
    static void write(std::ostream& stream, std::nullptr_t value);
    static void write(std::ostream& stream, bool value);
    static void write(std::ostream& stream, int32_t value);
    static void write(std::ostream& stream, int64_t value);
    static void write(std::ostream& stream, uint32_t value);
    static void write(std::ostream& stream, uint64_t value);
    static void write(std::ostream& stream, float value);
    static void write(std::ostream& stream, double value);
    static void write(std::ostream& stream, const char* value);
    static void write(std::ostream& stream, const std::string& value);
    static void write(std::ostream& stream, const String& value);
    static void write(std::ostream& stream, const Value& value);
    static void write(std::ostream& stream, ValueFlags value);

    // Print to string builder
    template<typename T>
    static void add(StringBuilder& sb, T value) {
      Print::write(sb.ss, value);
    }

    // Print to string
    template<typename T>
    static String toString(T value) {
      StringBuilder sb;
      Print::add(sb, value);
      return sb.str();
    }
    static String toString(const String& value) {
      return value;
    }
  };
}
