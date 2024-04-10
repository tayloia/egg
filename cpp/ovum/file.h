namespace egg::ovum {
  class TextStream;
  class File {
  public:
    enum class Kind { Unknown, Directory, File };
    static std::string normalizePath(const std::string& path, bool trailingSlash = false);
    static std::string denormalizePath(const std::string& path, bool trailingSlash = false);
    static std::unique_ptr<TextStream> resolveTextStream(const std::string& path);
    static std::vector<std::string> readDirectory(const std::filesystem::path& path);
    static Kind getKind(const std::filesystem::path& path);
    static std::string slurp(const std::filesystem::path& path);
    static bool removeFile(const std::filesystem::path& path);
  };
}
