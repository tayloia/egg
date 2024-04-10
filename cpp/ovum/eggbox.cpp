#include "ovum/ovum.h"
#include "ovum/eggbox.h"
#include "ovum/file.h"
#include "ovum/os-embed.h"
#include "ovum/os-file.h"
#include "ovum/os-process.h"
#include "ovum/os-zip.h"

#include <fstream>

namespace {
  using namespace egg::ovum::os::zip;


  class EggboxEmbedded : public egg::ovum::IEggbox {
    EggboxEmbedded(const EggboxEmbedded&) = delete;
    EggboxEmbedded& operator=(const EggboxEmbedded&) = delete;
  private:
    class FileEntry : public egg::ovum::IEggboxFileEntry {
      FileEntry(const FileEntry&) = delete;
      FileEntry& operator=(const FileEntry&) = delete;
    private:
      std::shared_ptr<IZipFileEntry> entry;
    public:
      explicit FileEntry(std::shared_ptr<IZipFileEntry>&& entry)
        : entry(std::move(entry)) {
        assert(this->entry != nullptr);
      }
      virtual std::string getName() const override {
        return std::string();
      }
      virtual std::istream& getReadStream() override {
        return this->entry->getReadStream();
      }
      static std::shared_ptr<IEggboxFileEntry> make(std::shared_ptr<IZipFileEntry>&& entry) {
        if (entry == nullptr) {
          return nullptr;
        }
        return std::make_shared<FileEntry>(std::move(entry));
      }
    };
    std::shared_ptr<IZipReader> reader;
  public:
    explicit EggboxEmbedded(const std::shared_ptr<IZipReader>& reader)
      : reader(reader) {
      assert(this->reader != nullptr);
    }
    virtual std::string getResourcePath() override {
      return std::string();
    }
    virtual std::shared_ptr<egg::ovum::IEggboxFileEntry> findFileEntryByIndex(size_t index) override {
      // WIBBLE
      return FileEntry::make(this->reader->findFileEntryByIndex(index));
    }
    virtual std::shared_ptr<egg::ovum::IEggboxFileEntry> findFileEntryBySubpath(const std::string& subpath) override {
      // WIBBLE
      return FileEntry::make(this->reader->findFileEntryBySubpath(subpath));
    }
  };

  uint64_t addFile(IZipWriter& writer, const std::string& name, const std::filesystem::path& native) {
    std::ifstream ifs{ native, std::ios::binary };
    ifs.seekg(0, std::ios::end);
    auto bytes = ifs.tellg();
    std::string content(size_t(bytes), '\0');
    ifs.seekg(0);
    ifs.read(content.data(), bytes);
    writer.addFileEntry(name, content);
    return uint64_t(bytes);
  }

  uint64_t addDirectoryRecursive(IZipWriter& writer, const std::string& prefix, const std::filesystem::path& native, size_t& entries) {
    uint64_t uncompressed = 0;
    std::error_code error;
    for (auto& entry : std::filesystem::directory_iterator(native, error)) {
      auto name = prefix + entry.path().filename().string();
      if (entry.is_directory()) {
        uncompressed += addDirectoryRecursive(writer, name + '/', entry.path(), entries);
      } else {
        uncompressed += addFile(writer, name, entry.path());
        ++entries;
      }
    }
    if (error) {
      throw egg::ovum::Exception("Cannot walk directory: {error}")
        .with("path", native.string())
        .with("error", egg::ovum::os::process::format(error));
    }
    return uncompressed;
  }

  // See https://stackoverflow.com/a/13586356
  struct MemoryStreamBuf : public std::streambuf {
    MemoryStreamBuf(const void* base, size_t size) {
      this->setg((char*)base, (char*)base, (char*)base + size);
    }
  };
}

size_t egg::ovum::EggboxFactory::createZipFileFromDirectory(const std::string& zipPath, const std::string& directoryPath, uint64_t& compressedBytes, uint64_t& uncompressedBytes) {
  std::filesystem::path zipNative = egg::ovum::os::file::denormalizePath(zipPath, false);
  std::filesystem::path directoryNative = egg::ovum::os::file::denormalizePath(directoryPath, false);
  auto writer = os::zip::openWriteZipFile(zipNative);
  size_t entries = 0;
  uncompressedBytes = addDirectoryRecursive(*writer, "", directoryNative, entries);
  compressedBytes = writer->commit();
  return entries;
}

std::shared_ptr<egg::ovum::IEggbox> egg::ovum::EggboxFactory::openDefault() {
  // WIBBLE
  return EggboxFactory::openEmbedded("EGGBOX");
}

std::shared_ptr<egg::ovum::IEggbox> egg::ovum::EggboxFactory::openEmbedded(const std::string& label) {
  // TODO optimize
  auto executable = os::file::getExecutablePath();
  auto lockable = os::embed::findResourceByName(executable, "PROGBITS", label);
  if (lockable == nullptr) {
    throw Exception("Unable to find eggbox resource in current executable").with("executable", executable).with("label", label);
  }
  auto base = lockable->lock();
  MemoryStreamBuf streambuf{ const_cast<void*>(base), lockable->bytes };
  std::istream stream{ &streambuf };
  auto reader = os::zip::openReadStream(stream);
  return std::make_shared<EggboxEmbedded>(reader);
}
