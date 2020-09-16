// LEGACY SHIM
namespace egg::test {
  class Compiler {
  public:
    static egg::ovum::Module compileFile(egg::ovum::IAllocator& allocator, egg::ovum::ILogger& logger, const std::string& path);
    static egg::ovum::Module compileText(egg::ovum::IAllocator& allocator, egg::ovum::ILogger& logger, const std::string& source);

    static egg::ovum::Value run(egg::ovum::IAllocator& allocator, egg::ovum::ILogger& logger, const std::string& path);
  };
}
