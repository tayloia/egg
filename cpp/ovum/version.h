namespace egg::ovum {
  struct Version {
    uint64_t major;
    uint64_t minor;
    uint64_t patch;
    const char* commit;
    const char* timestamp;
    Version();
    static std::string semver();
  };
  std::ostream& operator<<(std::ostream& stream, const Version& version);
}
