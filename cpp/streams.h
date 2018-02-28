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
    std::string name;
  public:
    ByteStream(std::iostream& stream, const std::string& name)
      : stream(stream), name(name) {
    }
    int get() {
      char ch;
      if (this->stream.get(ch)) {
        return int(uint8_t(ch));
      }
      if (this->stream.bad()) {
        EGG_THROW("Failed to read byte from binary file: " + this->name);
      }
      return -1;
    }
    const std::string& filename() const {
      return this->name;
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
    const std::string& filename() const {
      return this->bytes.filename();
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

  class TextStream {
    EGG_NO_COPY(TextStream);
  private:
    CharStream& chars;
    int next;
    size_t line;
    size_t column;
  public:
    explicit TextStream(CharStream& chars)
      : chars(chars), next(0), line(0), column(1) {
    }
    int get();
    int peek() {
      this->initialize();
      return this->next;
    }
    const std::string& filename() const {
      return this->chars.filename();
    }
    size_t getCurrentLine() {
      this->initialize();
      return this->line;
    }
    size_t getCurrentColumn() {
      return this->column;
    }
  private:
    void initialize();
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
}
