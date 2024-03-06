namespace egg::ovum::os {
  namespace file {
    std::string normalizePath(const std::string& path, bool trailingSlash);
    std::string denormalizePath(const std::string& path, bool trailingSlash);
    std::string getCurrentDirectory();
    std::string getDevelopmentDirectory();
    std::string getExecutableDirectory();
    std::string getExecutablePath();
  }
}
