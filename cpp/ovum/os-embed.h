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
  std::string getExecutableStub();
  void cloneExecutable(const std::string& target);
  void addResource(const std::string& executable, const std::string& type, const std::string& label, const void* resource, size_t bytes);
  std::vector<Resource> findResources(const std::string& executable);
  std::vector<Resource> findResources(const std::string& executable, const std::string& type);
  std::shared_ptr<LockableResource> findResource(const std::string& executable, const std::string& type, const std::string& label);
}
