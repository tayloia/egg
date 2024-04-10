namespace egg::ovum {
  class IEggboxFileEntry {
  public:
    // Interface
    virtual ~IEggboxFileEntry() {}
    virtual std::string getName() const = 0;
    virtual std::istream& getReadStream() = 0;
  };

  class IEggbox {
  public:
    // Interface
    virtual ~IEggbox() {}
    virtual std::string getResourcePath() = 0;
    virtual std::shared_ptr<IEggboxFileEntry> findFileEntryByIndex(size_t index) = 0;
    virtual std::shared_ptr<IEggboxFileEntry> findFileEntryBySubpath(const std::string& subpath) = 0;
  };

  class EggboxFactory {
  public:
    static size_t createZipFileFromDirectory(const std::string& zipPath, const std::string& directoryPath, uint64_t& compressedSize, uint64_t& uncompressedSize);
    static std::shared_ptr<IEggbox> openDefault();
    static std::shared_ptr<IEggbox> openEmbedded(const std::string& label);
    static std::shared_ptr<IEggbox> openZipFile(const std::string& path);
    static std::shared_ptr<IEggbox> openDirectory(const std::string& path);
  };
}
