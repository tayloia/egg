namespace egg::ovum {
  class TextStream;
  class File {
  public:
    inline static const std::string EGGBOX = "EGGBOX";
    enum class Kind { Unknown, Directory, File };
    static std::string normalizePath(const std::string& path, bool trailingSlash = false);
    static std::string denormalizePath(const std::string& path, bool trailingSlash = false);
    static std::string resolvePath(const std::string& path, bool trailingSlash = false);
    static std::unique_ptr<TextStream> resolveTextStream(const std::string& path);
    static std::vector<std::string> readDirectory(const std::string& path);
    static Kind getKind(const std::string& path);
    static std::string slurp(const std::string& path);
    static bool removeFile(const std::string& path);
    static uint64_t createSandwichFromFile(const std::string& targetPath, const std::string& zipPath, bool overwriteTarget = true, const std::string& label = EGGBOX);
  };
}
