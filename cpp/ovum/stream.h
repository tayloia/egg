#include <deque>
#include <fstream>
#include <iomanip>

namespace egg::ovum {
  class FileStream : public std::fstream {
    FileStream(FileStream&) = delete;
    FileStream& operator=(FileStream&) = delete;
  public:
    FileStream(const std::string& unresolved, const std::string& resolved, ios_base::openmode mode)
      : std::fstream(resolved, mode) {
      if (this->fail()) {
        throw egg::ovum::Exception("Failed to open file for reading: '{path}'").with("path", unresolved);
      }
    }
    explicit FileStream(const std::string& path, ios_base::openmode mode = ios_base::in | ios_base::binary)
      : FileStream(path, File::resolvePath(path), mode) {
    }
  };

  class ByteStream {
    ByteStream(ByteStream&) = delete;
    ByteStream& operator=(ByteStream&) = delete;
  private:
    std::istream& stream;
    std::string resource;
  public:
    ByteStream(std::istream& stream, const std::string& resource)
      : stream(stream), resource(resource) {
    }
    int get() {
      char ch;
      if (this->stream.get(ch)) {
        return int(uint8_t(ch));
      }
      if (this->stream.bad()) {
        throw egg::ovum::Exception("Failed to read byte from binary file: '{path}'").with("path", this->resource);
      }
      return -1;
    }
    bool rewind() {
      this->stream.clear();
      return this->stream.seekg(0).good();
    }
    const std::string& getResourceName() const {
      return this->resource;
    }
  };

  class FileByteStream : public ByteStream {
    FileByteStream(FileByteStream&) = delete;
    FileByteStream& operator=(FileByteStream&) = delete;
  private:
    FileStream fs;
  public:
    explicit FileByteStream(const std::string& path)
      : ByteStream(fs, path), fs(path) {
    }
  };

  class StringByteStream : public ByteStream {
    StringByteStream(StringByteStream&) = delete;
    StringByteStream& operator=(StringByteStream&) = delete;
  private:
    std::stringstream ss;
  public:
    explicit StringByteStream(const std::string& text, const std::string& resource = std::string())
      : ByteStream(ss, resource), ss(text) {
    }
  };

  class CharStream {
    CharStream(CharStream&) = delete;
    CharStream& operator=(CharStream&) = delete;
  private:
    ByteStream& bytes;
    bool swallowBOM;
  public:
    explicit CharStream(ByteStream& bytes, bool swallowBOM = true)
      : bytes(bytes), swallowBOM(swallowBOM) {
    }
    int get();
    void slurp(std::u32string& text);
    bool rewind();
    const std::string& getResourceName() const {
      return this->bytes.getResourceName();
    }
  };

  class FileCharStream : public CharStream {
    FileCharStream(FileCharStream&) = delete;
    FileCharStream& operator=(FileCharStream&) = delete;
  private:
    FileByteStream fbs;
  public:
    explicit FileCharStream(const std::string& path, bool swallowBOM = true)
      : CharStream(fbs, swallowBOM), fbs(path) {
    }
  };

  class StringCharStream : public CharStream {
    StringCharStream(StringCharStream&) = delete;
    StringCharStream& operator=(StringCharStream&) = delete;
  private:
    StringByteStream sbs;
  public:
    explicit StringCharStream(const std::string& text, const std::string& resource = std::string())
      : CharStream(sbs, false), sbs(text, resource) {
    }
  };

  class TextStream {
    TextStream(TextStream&) = delete;
    TextStream& operator=(TextStream&) = delete;
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
    int peek(size_t index = 0) {
      if (this->ensure(index + 1)) {
        return this->upcoming[index];
      }
      return -1;
    }
    size_t getCurrentLine() {
      this->ensure(1);
      return this->line;
    }
    size_t getCurrentColumn() {
      return this->column;
    }
    const std::string& getResourceName() const {
      return this->chars.getResourceName();
    }
    bool readline(std::string& text);
    bool readline(std::u32string& text);
    void slurp(std::string& text, int eol = -1);
    void slurp(std::u32string& text, int eol = -1);
    bool rewind();
  private:
    bool ensure(size_t count);
  };

  class FileTextStream : public TextStream {
    FileTextStream(FileTextStream&) = delete;
    FileTextStream& operator=(FileTextStream&) = delete;
  private:
    FileCharStream fcs;
  public:
    explicit FileTextStream(const std::string& path, bool swallowBOM = true)
      : TextStream(fcs), fcs(path, swallowBOM) {
    }
  };

  class StringTextStream : public TextStream {
    StringTextStream(StringTextStream&) = delete;
    StringTextStream& operator=(StringTextStream&) = delete;
  private:
    StringCharStream scs;
  public:
    explicit StringTextStream(const std::string& text, const std::string& resource = std::string())
      : TextStream(scs), scs(text, resource) {
    }
  };
}
