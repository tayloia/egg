namespace egg::ovum::os::process {
  FILE* popen(const char* command, const char* mode);
  int pclose(FILE* fp);
  int pexec(std::ostream& os, const std::string& command);
  int plines(const std::string& command, std::function<void(const std::string&)> callback);
}
