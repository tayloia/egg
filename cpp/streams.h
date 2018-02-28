namespace egg::yolk {
  class FileStream : public std::fstream {
    EGG_NO_COPY(FileStream);
  public:
    explicit FileStream(const std::string& path, ios_base::openmode mode = ios_base::in | ios_base::binary)
      : FileStream(path, File::resolvePath(path), mode) {
    }
  private:
    FileStream(const std::string& unresolved, const std::string& resolved, ios_base::openmode mode)
      : std::fstream(resolved, mode) {
      if (this->fail()) {
        EGG_THROW("Failed to open file for reading: " + unresolved); // TODO
      }
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
        EGG_THROW("Failed to read byte from binary file: " + this->name); // TODO
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
}
