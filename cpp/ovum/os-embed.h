namespace egg::ovum::os::embed {
  struct Resource {
    std::string type;
    std::string name;
    size_t bytes;
  };
  std::string getExecutableFilename();
  std::string getExecutableStub();
  void cloneExecutable(const std::string& target);
  void addResource(const std::string& executable, const std::string& label, const void* resource, size_t bytes);
  std::vector<Resource> findResources(const std::string& executable);
}
