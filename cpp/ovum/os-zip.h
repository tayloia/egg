namespace egg::ovum::os::zip {
  class IZipFileEntry {
  public:
    // Interface
    virtual ~IZipFileEntry() {}
    virtual std::string getName() const = 0;
    virtual uint64_t getCompressedBytes() const = 0;
    virtual uint64_t getUncompressedBytes() const = 0;
    virtual uint32_t getCRC32() const = 0;
    virtual std::istream& getReadStream() = 0;
  };

  class IZipReader {
  public:
    // Interface
    virtual ~IZipReader() {}
    virtual std::string getComment() = 0;
    virtual size_t getFileEntryCount() = 0;
    virtual std::shared_ptr<IZipFileEntry> findFileEntryByIndex(size_t index) = 0;
    virtual std::shared_ptr<IZipFileEntry> findFileEntryBySubpath(const std::string& subpath) = 0;
  };

  class IZipWriter {
  public:
    // Interface
    virtual ~IZipWriter() {}
    virtual void addFileEntry(const std::string& name, const std::string& content) = 0;
    virtual uint64_t commit() = 0;
  };

  std::string getVersion();
  std::shared_ptr<IZipReader> openReadStream(std::istream& stream);
  std::shared_ptr<IZipReader> openReadZipFile(const std::filesystem::path& zipfile);
  std::shared_ptr<IZipWriter> openWriteZipFile(const std::filesystem::path& zipfile);
}
