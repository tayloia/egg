namespace egg::ovum::os::embed {
  struct Resource {
    std::string type;
    std::string label;
    size_t bytes;
  };
  struct LockableResource : public Resource {
    virtual ~LockableResource() {}
    virtual const void* lock() = 0;
    virtual void unlock() = 0;
  };
  std::string getExecutableFilename();
  std::string getExecutableStem();
  void cloneExecutable(const std::filesystem::path& target, bool overwrite);
  uint64_t updateResourceFromFile(const std::filesystem::path& executable, const std::string& type, const std::string& label, const std::filesystem::path& datapath);
  uint64_t updateResourceFromMemory(const std::filesystem::path& executable, const std::string& type, const std::string& label, const void* data, size_t bytes);
  std::vector<Resource> findResources(const std::filesystem::path& executable);
  std::vector<Resource> findResourcesByType(const std::filesystem::path& executable, const std::string& type);
  std::shared_ptr<LockableResource> findResourceByName(const std::filesystem::path& executable, const std::string& type, const std::string& label);
}
