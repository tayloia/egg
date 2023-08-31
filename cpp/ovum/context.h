namespace egg::ovum {
  struct LocationSource {
    String file;
    size_t line;
    size_t column;

    inline LocationSource(const LocationSource& rhs) = default;
    inline LocationSource& operator=(const LocationSource& rhs) = default;
    inline LocationSource(const String& file, size_t line, size_t column)
      : file(file), line(line), column(column) {
    }
    bool printSource(Printer& printer) const;
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
    bool printRuntime(Printer& printer) const;
  };
}
