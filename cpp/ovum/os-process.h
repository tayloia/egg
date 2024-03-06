namespace egg::ovum::os::process {
  FILE* popen(const std::string& command);
  void pclose(FILE* fp);
}
