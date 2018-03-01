namespace egg::yolk {
  class FileStream : public std::fstream {
    EGG_NO_COPY(FileStream);
  public:
    FileStream(const std::string& unresolved, const std::string& resolved, ios_base::openmode mode)
      : std::fstream(resolved, mode) {
      if (this->fail()) {
        EGG_THROW("Failed to open file for reading: " + unresolved);
      }
    }
    explicit FileStream(const std::string& path, ios_base::openmode mode = ios_base::in | ios_base::binary)
      : FileStream(path, File::resolvePath(path), mode) {
    }
  };

  class ByteStream {
    EGG_NO_COPY(ByteStream);
  private:
    std::iostream& stream;
    std::string resource;
  public:
    ByteStream(std::iostream& stream, const std::string& resource)
      : stream(stream), resource(resource) {
    }
    int get() {
      char ch;
      if (this->stream.get(ch)) {
        return int(uint8_t(ch));
      }
      if (this->stream.bad()) {
        EGG_THROW("Failed to read byte from binary file: " + this->resource);
      }
      return -1;
    }
    const std::string& getResourceName() const {
      return this->resource;
    }
  };

  class FileByteStream : public ByteStream {
    EGG_NO_COPY(FileByteStream);
  private:
    FileStream fs;
  public:
    explicit FileByteStream(const std::string& path)
      : ByteStream(fs, path), fs(path) {
    }
  };

  class StringByteStream : public ByteStream {
    EGG_NO_COPY(StringByteStream);
  private:
    std::stringstream ss;
  public:
    explicit StringByteStream(const std::string& text)
      : StringByteStream(text, "<string>") {
    }
    StringByteStream(const std::string& text, const std::string& name)
      : ByteStream(ss, name), ss(text) {
    }
  };

  class CharStream {
    EGG_NO_COPY(CharStream);
  private:
    ByteStream& bytes;
    bool swallowBOM;
  public:
    explicit CharStream(ByteStream& bytes, bool swallowBOM = true)
      : bytes(bytes), swallowBOM(swallowBOM) {
    }
    int get();
    void slurp(std::u32string& text);
    const std::string& getResourceName() const {
      return this->bytes.getResourceName();
    }
  };

  class FileCharStream : public CharStream {
    EGG_NO_COPY(FileCharStream);
  private:
    FileByteStream fbs;
  public:
    explicit FileCharStream(const std::string& path, bool swallowBOM = true)
      : CharStream(fbs, swallowBOM), fbs(path) {
    }
  };

  class StringCharStream : public CharStream {
    EGG_NO_COPY(StringCharStream);
  private:
    StringByteStream sbs;
  public:
    explicit StringCharStream(const std::string& text)
      : CharStream(sbs, false), sbs(text) {
    }
    StringCharStream(const std::string& text, const std::string& name)
      : CharStream(sbs, false), sbs(text, name) {
    }
  };

  class TextStream {
    EGG_NO_COPY(TextStream);
  private:
    CharStream& chars;
    std::deque<int> upcoming;
    size_t line;
    size_t column;
  public:
    explicit TextStream(CharStream& chars)
      : chars(chars), upcoming(), line(1), column(1) {
    }
    int get();
    bool readline(std::string& text);
    bool readline(std::u32string& text);
    void slurp(std::string& text, int eol = -1);
    void slurp(std::u32string& text, int eol = -1);
    int peek(size_t index = 0) {
      if (this->ensure(index + 1)) {
        // Microsoft's std::deque indexer is borked
        return *(this->upcoming.begin() + ptrdiff_t(index));
      }
      return -1;
    }
    const std::string& getResourceName() const {
      return this->chars.getResourceName();
    }
    size_t getCurrentLine() {
      this->ensure(1);
      return this->line;
    }
    size_t getCurrentColumn() {
      return this->column;
    }
  private:
    bool ensure(size_t count);
  };

  class FileTextStream : public TextStream {
    EGG_NO_COPY(FileTextStream);
  private:
    FileCharStream fcs;
  public:
    explicit FileTextStream(const std::string& path, bool swallowBOM = true)
      : TextStream(fcs), fcs(path, swallowBOM) {
    }
  };

  class StringTextStream : public TextStream {
    EGG_NO_COPY(StringTextStream);
  private:
    StringCharStream scs;
  public:
    explicit StringTextStream(const std::string& text)
      : TextStream(scs), scs(text) {
    }
    StringTextStream(const std::string& text, const std::string& name)
      : TextStream(scs), scs(text, name) {
    }
  };
}
