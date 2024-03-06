namespace egg::ovum::os::file {
  std::string normalizePath(const std::string& path, bool trailingSlash);
  std::string denormalizePath(const std::string& path, bool trailingSlash);
  std::string getCurrentDirectory();
  std::string getDevelopmentDirectory();
  std::string getExecutableDirectory();
  std::string getExecutablePath();
  std::string createTemporaryDirectory(const std::string& prefix, size_t attempts);
  char slash();
}
