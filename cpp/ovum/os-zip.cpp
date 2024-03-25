#include "ovum/ovum.h"
#include "ovum/os-zip.h"
#include "ovum/miniz-cpp.h"

namespace {
  using namespace egg::ovum::os::zip;

  class ZipFileEntry : public IZipFileEntry {
    ZipFileEntry(const ZipFileEntry&) = delete;
    ZipFileEntry& operator=(const ZipFileEntry&) = delete;
  private:
    miniz_cpp::zip_info info;
  public:
    explicit ZipFileEntry(const miniz_cpp::zip_info& info)
      : info(info) {
    }
    virtual std::string getName() const override {
      return this->info.filename;
    }
    virtual uint64_t getCompressedBytes() const override {
      return this->info.compress_size;
    }
    virtual uint64_t getUncompressedBytes() const override {
      return this->info.file_size;
    }
    virtual uint32_t getCRC32() const override {
      return this->info.crc;
    }
  };

  class ZipFile : public IZip {
    ZipFile(const ZipFile&) = delete;
    ZipFile& operator=(const ZipFile&) = delete;
  private:
    std::filesystem::path path;
    std::unique_ptr<miniz_cpp::zip_file> handle;
  public:
    explicit ZipFile(const std::filesystem::path& path)
      : path(path),
        handle(nullptr) {
    }
    virtual std::string getComment() override {
      assert(this->handle != nullptr);
      return this->handle->comment;
    }
    virtual std::shared_ptr<IZipFileEntry> getFileEntry(const std::string& subpath) override {
      assert(this->handle != nullptr);
      return std::make_shared<ZipFileEntry>(this->handle->getinfo(subpath));
    }
    void open() {
      assert(this->handle == nullptr);
      this->handle = std::make_unique<miniz_cpp::zip_file>(this->path.string());
      assert(this->handle != nullptr);
    }
  };

  class ZipFactory : public IZipFactory {
  public:
    virtual std::string getVersion() const override {
      return MZ_VERSION;
    }
    virtual std::shared_ptr<IZip> openFile(const std::filesystem::path& zipfile) override {
      auto zip = std::make_shared<ZipFile>(zipfile);
      zip->open();
      return zip;
    }
  };
}

egg::ovum::os::zip::IZipFileEntry::~IZipFileEntry() {
}

egg::ovum::os::zip::IZip::~IZip() {
}

egg::ovum::os::zip::IZipFactory::~IZipFactory() {
}

std::shared_ptr<egg::ovum::os::zip::IZipFactory> egg::ovum::os::zip::createFactory() {
  return std::make_shared<ZipFactory>();
}
