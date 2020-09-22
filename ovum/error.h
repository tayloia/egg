namespace egg::ovum {
  class Error {
  private:
    String message;
  public:
    Error()
      : message() {
    }
    explicit Error(const String& message)
      : message(message) {
    }
    template<typename... ARGS>
    explicit Error(ARGS&&... args)
      : message(StringBuilder::concat(std::forward<ARGS>(args)...)) {
    }
    bool empty() const {
      return this->message.empty();
    }
    String toString() const {
      return this->message;
    }
  };

  template<typename T>
  class Erratic {
  private:
    T good;
    Error bad;
    Erratic(bool, const String& bad)
      : good(), bad(bad) {
    }
  public:
    /* implicit */ Erratic(const T& good)
      : good(good), bad() {
    }
    bool failed() const {
      return !this->bad.empty();
    }
    const T& success() const {
      assert(this->bad.empty());
      return this->good;
    }
    const Error& failure() const {
      assert(!this->bad.empty());
      return this->bad;
    }
    template<typename... ARGS>
    static Erratic fail(ARGS&&... args) {
      return Erratic(false, StringBuilder::concat(std::forward<ARGS>(args)...));
    }
  };

  template<>
  class Erratic<void> {
  private:
    Error bad;
  public:
    Erratic()
      : bad() {
      assert(!this->failed());
    }
    template<typename... ARGS>
    explicit Erratic(ARGS&&... args)
      : bad(StringBuilder::concat(std::forward<ARGS>(args)...)) {
      assert(this->failed());
    }
    bool failed() const {
      return !this->bad.empty();
    }
    const Error& failure() const {
      assert(!this->bad.empty());
      return this->bad;
    }
    template<typename... ARGS>
    static Erratic fail(ARGS&&... args) {
      return Erratic(std::forward<ARGS>(args)...);
    }
  };
}

inline std::ostream& operator<<(std::ostream& stream, const egg::ovum::Error& value) {
  if (value.empty()) {
    return stream << "<SUCCESS>";
  }
  return stream << "<ERROR:" << value.toString().toUTF8() << ">";
}

template<typename T>
inline std::ostream& operator<<(std::ostream& stream, const egg::ovum::Erratic<T>& value) {
  if (value.failed()) {
    return stream << "<ERROR:" << value.failure().toString().toUTF8() << ">";
  }
  return stream << value.success();
}
