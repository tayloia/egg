namespace egg::ovum {
  namespace os::zip {
    class IZipReader;
    class IZipFileEntry;
  }
  class Zip {
  public:
    using IZipReader = os::zip::IZipReader;
    using IZipFileEntry = os::zip::IZipFileEntry;
    static size_t createFileFromDirectory(const std::string& zipPath, const std::string& directoryPath, uint64_t& compressedSize, uint64_t& uncompressedSize);
    static std::shared_ptr<IZipReader> openEggbox(const std::string& eggbox);
  };
}
