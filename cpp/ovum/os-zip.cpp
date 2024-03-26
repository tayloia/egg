#include "ovum/ovum.h"
#include "ovum/os-zip.h"
#include "ovum/os-file.h"
#include "ovum/miniz-cpp.h"

namespace {
  using namespace egg::ovum::os::zip;

  class ZipFileEntry : public IZipFileEntry {
    ZipFileEntry(const ZipFileEntry&) = delete;
    ZipFileEntry& operator=(const ZipFileEntry&) = delete;
  private:
    std::shared_ptr<miniz_cpp::zip_file> handle;
    miniz_cpp::zip_info info;
    std::stringstream stream;
  public:
    ZipFileEntry(std::shared_ptr<miniz_cpp::zip_file> handle, miniz_cpp::zip_info&& info)
      : handle(handle),
        info(std::move(info)) {
      assert(this->handle != nullptr);
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
    virtual std::istream& getReadStream() override {
      auto data = this->handle->read(this->info);
      assert(data.size() == this->info.file_size);
      this->stream.clear();
      this->stream << data;
      return this->stream;
    }
  };

  class Zip : public IZip {
    Zip(const Zip&) = delete;
    Zip& operator=(const Zip&) = delete;
  private:
    std::shared_ptr<miniz_cpp::zip_file> handle;
  public:
    Zip()
      : handle(nullptr) {
    }
    virtual std::string getComment() override {
      assert(this->handle != nullptr);
      return this->handle->comment;
    }
    virtual std::shared_ptr<IZipFileEntry> getFileEntry(const std::string& subpath) override {
      assert(this->handle != nullptr);
      miniz_cpp::zip_info info;
      try {
        info = this->handle->getinfo(subpath);
      }
      catch (std::runtime_error& exception) {
        if (std::strcmp(exception.what(), "not found") == 0) {
          return nullptr;
        }
        throw;
      }
      return std::make_shared<ZipFileEntry>(this->handle, std::move(info));
    }
    void openFile(const std::filesystem::path& path) {
      assert(this->handle == nullptr);
      this->handle = std::make_shared<miniz_cpp::zip_file>(path.string());
      assert(this->handle != nullptr);
    }
  };

  class ZipFactory : public IZipFactory {
  public:
    virtual std::string getVersion() const override {
      return MZ_VERSION;
    }
    virtual std::shared_ptr<IZip> openFile(const std::filesystem::path& zipfile) override {
      auto zip = std::make_shared<Zip>();
      try {
        zip->openFile(zipfile);
      }
      catch (std::exception&) {
        auto status = std::filesystem::status(zipfile);
        if (std::filesystem::exists(status)) {
          throw std::runtime_error("Invalid zip file: " + egg::ovum::os::file::normalizePath(zipfile.string(), false));
        }
        throw std::runtime_error("Zip file not found: " + egg::ovum::os::file::normalizePath(zipfile.string(), false));
      }
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
