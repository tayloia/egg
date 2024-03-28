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
    ZipFileEntry(std::shared_ptr<miniz_cpp::zip_file> handle, const miniz_cpp::zip_info& info)
      : handle(handle),
        info(info) {
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
    std::vector<miniz_cpp::zip_info> info;
  public:
    Zip()
      : handle(nullptr),
        info() {
    }
    virtual std::string getComment() override {
      assert(this->handle != nullptr);
      return this->handle->comment;
    }
    virtual size_t getFileEntryCount() override {
      assert(this->handle != nullptr);
      return this->info.size();
    }
    virtual std::shared_ptr<IZipFileEntry> getFileEntryByIndex(size_t index) override {
      assert(this->handle != nullptr);
      if (index < this->info.size()) {
        return std::make_shared<ZipFileEntry>(this->handle, this->info[index]);
      }
      return nullptr;
    }
    virtual std::shared_ptr<IZipFileEntry> getFileEntryByName(const std::string& subpath) override {
      assert(this->handle != nullptr);
      try {
        return std::make_shared<ZipFileEntry>(this->handle, this->handle->getinfo(subpath));
      }
      catch (std::runtime_error& exception) {
        if (std::strcmp(exception.what(), "not found") == 0) {
          return nullptr;
        }
        throw;
      }
    }
    void openFile(const std::filesystem::path& path) {
      this->prepare(std::make_shared<miniz_cpp::zip_file>(path.string()));
    }
  private:
    void prepare(const std::shared_ptr<miniz_cpp::zip_file>& zip) {
      assert(this->handle == nullptr);
      this->handle = zip;
      assert(this->handle != nullptr);
      this->info = this->handle->infolist();
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
        auto* what = std::filesystem::exists(status) ? "Invalid zip file: '{path}'" : "Zip file not found: '{path}'";
        throw egg::ovum::Exception(what).with("path", egg::ovum::os::file::normalizePath(zipfile.string(), false));
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
