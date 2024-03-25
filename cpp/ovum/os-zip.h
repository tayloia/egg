namespace egg::ovum::os::zip {
  class IZip {
  public:
    // Interface
    virtual ~IZip() = 0;
    virtual std::string getComment() = 0;
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
