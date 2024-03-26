namespace egg::ovum::os::zip {
  class IZipFileEntry {
  public:
    // Interface
    virtual ~IZipFileEntry() = 0;
    virtual std::string getName() const = 0;
    virtual uint64_t getCompressedBytes() const = 0;
    virtual uint64_t getUncompressedBytes() const = 0;
    virtual uint32_t getCRC32() const = 0;
    virtual std::istream& getReadStream() = 0;
  };
  class IZip {
  public:
    // Interface
    virtual ~IZip() = 0;
    virtual std::string getComment() = 0;
    virtual size_t getFileEntryCount() = 0;
    virtual std::shared_ptr<IZipFileEntry> getFileEntryByIndex(size_t index) = 0;
    virtual std::shared_ptr<IZipFileEntry> getFileEntryByName(const std::string& path) = 0;
  };
  class IZipFactory {
  public:
    // Interface
    virtual ~IZipFactory() = 0;
    virtual std::string getVersion() const = 0;
    virtual std::shared_ptr<IZip> openFile(const std::filesystem::path& zipfile) = 0;
  };
  std::shared_ptr<IZipFactory> createFactory();
}
