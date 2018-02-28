namespace egg::yolk {
  class File {
  public:
    static std::string normalizePath(const std::string& path, bool trailingSlash = false);
    static std::string denormalizePath(const std::string& path, bool trailingSlash = false);
    static std::string getTildeDirectory();
    static std::string resolvePath(const std::string& path);
  };
}
