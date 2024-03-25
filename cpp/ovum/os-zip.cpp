#include "ovum/ovum.h"
#include "ovum/os-zip.h"
#include "ovum/minizip_ng.h"

namespace {
  using namespace egg::ovum::os::zip;

  class ZipFile : public IZip {
    std::filesystem::path zipfile;
    typedef struct UndefinedHandle* Handle;
    Handle handle;
  public:
    explicit ZipFile(const std::filesystem::path& zipfile)
      : zipfile(zipfile),
        handle(nullptr) {
    }
    virtual ~ZipFile() override {
      if (this->handle != nullptr) {
        ZipFile::handleClose(this->handle);
      }
    }
    virtual std::string getComment() override {
      assert(this->handle != nullptr);
      const char* comment = nullptr;
      auto err = mz_zip_get_comment(this->handle, &comment);
      switch (err) {
      case MZ_OK:
        return comment;
      case MZ_EXIST_ERROR:
        break;
      default:
        this->failed("mz_zip_get_comment", err);
        break;
      }
      return {};
    }
    void open() {
      assert(this->handle == nullptr);
      this->handle = ZipFile::handleOpen(this->zipfile.string().c_str());
      if (this->handle == nullptr) {
        throw std::runtime_error("Cannot open zip");
      }
    }
  private:
    void failed(const char* api, int32_t retval) {
      if (retval != MZ_OK) {
        throw std::runtime_error(api);
      }
    }
    static Handle handleOpen(const char* path) {
      // WIBBLE this leaks blocks under Linux
      auto stream = mz_stream_os_create();
      if (stream == nullptr) {
        return nullptr;
      }
      if (mz_stream_open(stream, path, MZ_OPEN_MODE_READ) != MZ_OK) {
        mz_stream_delete(&stream);
        return nullptr;
      }
      auto handle = mz_zip_create();
      if (handle == nullptr) {
        mz_stream_delete(&stream);
        return nullptr;
      }
      auto err = mz_zip_open(handle, stream, MZ_OPEN_MODE_READ);
      if (err != MZ_OK) {
        mz_zip_delete(&handle);
        return nullptr;
      }
      return static_cast<Handle>(handle);
    }
    static void handleClose(void* handle) {
      mz_zip_close(handle);
      mz_zip_delete(&handle);
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

egg::ovum::os::zip::IZip::~IZip() {
}

egg::ovum::os::zip::IZipFactory::~IZipFactory() {
}

std::shared_ptr<egg::ovum::os::zip::IZipFactory> egg::ovum::os::zip::createFactory() {
  return std::make_shared<ZipFactory>();
}
