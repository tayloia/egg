namespace egg::ovum {
  class Zip {
  public:
    static size_t createFileFromDirectory(const std::string& zipPath, const std::string& directoryPath, uint64_t& compressedSize, uint64_t& uncompressedSize);
  };
}
