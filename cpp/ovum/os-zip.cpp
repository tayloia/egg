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

  class ZipReader : public IZipReader {
    ZipReader(const ZipReader&) = delete;
    ZipReader& operator=(const ZipReader&) = delete;
  private:
    std::shared_ptr<miniz_cpp::zip_file> handle;
    std::vector<miniz_cpp::zip_info> info;
  public:
    ZipReader()
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
    void loadFromFile(const std::filesystem::path& path) {
      assert(this->handle == nullptr);
      this->handle = std::make_shared<miniz_cpp::zip_file>(path.string());
      assert(this->handle != nullptr);
      this->info = this->handle->infolist();
    }
    void loadFromStream(std::istream& stream) {
      assert(this->handle == nullptr);
      this->handle = std::make_shared<miniz_cpp::zip_file>(stream);
      assert(this->handle != nullptr);
      this->info = this->handle->infolist();
    }
  };

  class ZipWriter : public IZipWriter {
    ZipWriter(const ZipWriter&) = delete;
    ZipWriter& operator=(const ZipWriter&) = delete;
  private:
    std::filesystem::path path;
    std::shared_ptr<miniz_cpp::zip_file> handle;
    std::vector<miniz_cpp::zip_info> info;
  public:
    ZipWriter(const std::filesystem::path& path)
      : path(path),
        handle(std::make_shared<miniz_cpp::zip_file>()),
        info() {
    }
    virtual void addFileEntry(const std::string& name, const std::string& content) {
      assert(this->handle != nullptr);
      this->handle->writestr(name, content);
    }
    virtual uint64_t commit() {
      assert(this->handle != nullptr);
      std::ofstream stream{ this->path, std::ios::binary };
      this->handle->save(stream);
      return uint64_t(stream.tellp());
    }
  };

  class ZipFactory : public IZipFactory {
  public:
    virtual std::string getVersion() const override {
      return MZ_VERSION;
    }
    virtual std::shared_ptr<IZipReader> readStream(std::istream& stream) override {
      auto zip = std::make_shared<ZipReader>();
      zip->loadFromStream(stream);
      return zip;
    }
    virtual std::shared_ptr<IZipReader> readZipFile(const std::filesystem::path& zipfile) override {
      auto zip = std::make_shared<ZipReader>();
      try {
        zip->loadFromFile(zipfile);
      }
      catch (std::exception&) {
        auto status = std::filesystem::status(zipfile);
        auto* what = std::filesystem::exists(status) ? "Invalid zip file: '{path}'" : "Zip file not found: '{path}'";
        throw egg::ovum::Exception(what).with("path", egg::ovum::os::file::normalizePath(zipfile.string(), false));
      }
      return zip;
    }
    virtual std::shared_ptr<IZipWriter> writeZipFile(const std::filesystem::path& zipfile) override {
      return std::make_shared<ZipWriter>(zipfile);
    }
  };
}

std::shared_ptr<egg::ovum::os::zip::IZipFactory> egg::ovum::os::zip::createFactory() {
  return std::make_shared<ZipFactory>();
}
