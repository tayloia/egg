namespace egg::ovum {
  class File {
  public:
    enum class Kind { Unknown, Directory, File };
    static std::string normalizePath(const std::string& path, bool trailingSlash = false);
    static std::string denormalizePath(const std::string& path, bool trailingSlash = false);
    static std::string resolvePath(const std::string& path);
    static std::vector<std::string> readDirectory(const std::string& path);
    static Kind getKind(const std::string& path);
    static std::string slurp(const std::string& path);
    static bool removeFile(const std::string& path);
  };
}
