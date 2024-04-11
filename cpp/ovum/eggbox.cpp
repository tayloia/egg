#include "ovum/ovum.h"
#include "ovum/eggbox.h"
#include "ovum/file.h"
#include "ovum/os-embed.h"
#include "ovum/os-file.h"
#include "ovum/os-process.h"
#include "ovum/os-zip.h"

#include <fstream>

namespace {
  using namespace egg::ovum;

  inline static std::string PROGBITS = "PROGBITS";

  class EggboxDirectory : public IEggbox {
    EggboxDirectory(const EggboxDirectory&) = delete;
    EggboxDirectory& operator=(const EggboxDirectory&) = delete;
  private:
    class FileEntry : public IEggboxFileEntry {
      FileEntry(const FileEntry&) = delete;
      FileEntry& operator=(const FileEntry&) = delete;
    private:
      std::filesystem::path root;
      std::filesystem::path subpath;
      std::ifstream stream;
    public:
      FileEntry(const std::filesystem::path& root, const std::filesystem::path& subpath)
        : root(root),
          subpath(subpath) {
        assert(this->root.is_absolute());
        assert(this->subpath.is_relative());
      }
      virtual std::string getSubpath() const override {
        return os::file::normalizePath(this->subpath.string(), false);
      }
      virtual std::string getName() const override {
        return os::file::normalizePath(this->subpath.filename().string(), false);
      }
      virtual std::istream& getReadStream() override {
        assert(!this->stream.is_open());
        this->stream.open(this->root / this->subpath, std::ios::in | std::ios::binary);
        assert(this->stream.is_open());
        return this->stream;
      }
      static std::shared_ptr<IEggboxFileEntry> make(const std::filesystem::path& root, const std::filesystem::path& subpath) {
        return std::make_shared<FileEntry>(root, subpath);
      }
    };
    std::filesystem::path root;
    std::vector<std::filesystem::path> children;
  public:
    explicit EggboxDirectory(const std::filesystem::path& root)
      : root(root) {
    }
    virtual std::string getResourcePath(const std::string* subpath) override {
      if (subpath != nullptr) {
        return os::file::normalizePath(this->root.string(), true) + *subpath;
      }
      return os::file::normalizePath(this->root.string(), true);
    }
    virtual size_t getFileEntryCount() override {
      this->ensureChildren();
      return this->children.size();
    }
    virtual std::shared_ptr<IEggboxFileEntry> findFileEntryByIndex(size_t index) override {
      this->ensureChildren();
      if (index < this->children.size()) {
        return FileEntry::make(this->root, this->children[index]);
      }
      return nullptr;
    }
    virtual std::shared_ptr<IEggboxFileEntry> findFileEntryBySubpath(const std::string& subpath) override {
      std::filesystem::path sub{ os::file::normalizePath(subpath, false) };
      auto full = this->root / sub;
      if (std::filesystem::exists(full)) {
        return FileEntry::make(this->root, sub);
      }
      return nullptr;
    }
    virtual std::shared_ptr<IEggboxFileEntry> getFileEntry(const std::string& subpath) override {
      auto entry = this->findFileEntryBySubpath(subpath);
      if (entry == nullptr) {
        throw Exception("Entry not found in eggbox: '{entry}'")
          .with("entry", subpath)
          .with("eggbox", this->getResourcePath(nullptr))
          .with("native", (this->root / subpath).string());
      }
      return entry;
    }
  private:
    void ensureChildren() {
      if (this->children.empty()) {
        // Use a set to sort the paths
        std::set<std::filesystem::path> found;
        for (auto& entry : std::filesystem::recursive_directory_iterator(this->root)) {
          if (entry.is_regular_file()) {
            found.insert(std::filesystem::relative(entry.path(), this->root));
          }
        }
        std::copy(found.begin(), found.end(), std::back_inserter(this->children));
      }
    }
  };

  class EggboxZip : public IEggbox {
    EggboxZip(const EggboxZip&) = delete;
    EggboxZip& operator=(const EggboxZip&) = delete;
  private:
    class FileEntry : public IEggboxFileEntry {
      FileEntry(const FileEntry&) = delete;
      FileEntry& operator=(const FileEntry&) = delete;
    private:
      std::shared_ptr<os::zip::IZipFileEntry> entry;
    public:
      FileEntry(std::shared_ptr<os::zip::IZipFileEntry>&& entry)
        : entry(std::move(entry)) {
        assert(this->entry != nullptr);
      }
      virtual std::string getSubpath() const override {
        return this->entry->getName();
      }
      virtual std::string getName() const override {
        auto name = this->entry->getName();
        auto slash = name.rfind('/');
        if (++slash < name.size()) {
          name.erase(0, slash);
        }
        return name;
      }
      virtual std::istream& getReadStream() override {
        return this->entry->getReadStream();
      }
      static std::shared_ptr<IEggboxFileEntry> make(std::shared_ptr<os::zip::IZipFileEntry>&& entry) {
        if (entry == nullptr) {
          return nullptr;
        }
        return std::make_shared<FileEntry>(std::move(entry));
      }
    };
    std::shared_ptr<os::zip::IZipReader> reader;
    std::string resource;
  public:
    EggboxZip(const std::shared_ptr<os::zip::IZipReader>& reader, const std::string& resource)
      : reader(reader),
        resource(resource) {
      assert(this->reader != nullptr);
    }
    virtual std::string getResourcePath(const std::string* subpath) override {
      if (subpath != nullptr) {
        return this->resource + "//" + *subpath;
      }
      return this->resource;
    }
    virtual size_t getFileEntryCount() override {
      return this->reader->getFileEntryCount();
    }
    virtual std::shared_ptr<IEggboxFileEntry> findFileEntryByIndex(size_t index) override {
      return FileEntry::make(this->reader->findFileEntryByIndex(index));
    }
    virtual std::shared_ptr<IEggboxFileEntry> findFileEntryBySubpath(const std::string& subpath) override {
      return FileEntry::make(this->reader->findFileEntryBySubpath(subpath));
    }
    virtual std::shared_ptr<IEggboxFileEntry> getFileEntry(const std::string& subpath) override {
      auto entry = this->reader->findFileEntryBySubpath(subpath);
      if (entry == nullptr) {
        throw Exception("Entry not found in eggbox: '{entry}'")
          .with("entry", subpath)
          .with("eggbox", this->getResourcePath(nullptr));
      }
      return FileEntry::make(std::move(entry));
    }
  };

  class EggboxChain : public IEggboxChain {
    EggboxChain(const EggboxChain&) = delete;
    EggboxChain& operator=(const EggboxChain&) = delete;
  private:
    struct Entry {
      IEggbox* eggbox;
      std::shared_ptr<IEggboxFileEntry> file;
    };
    std::map<std::string, Entry> entries;
    std::vector<std::shared_ptr<IEggbox>> subboxes;
  public:
    EggboxChain() {
    }
    virtual std::string getResourcePath(const std::string* subpath) override {
      if (subpath == nullptr) {
        return "~";
      }
      auto found = this->entries.find(*subpath);
      if (found == this->entries.end()) {
        return "~/" + *subpath;
      }
      return found->second.eggbox->getResourcePath(subpath);
    }
    virtual size_t getFileEntryCount() override {
      return this->entries.size();
    }
    virtual std::shared_ptr<IEggboxFileEntry> findFileEntryByIndex(size_t index) override {
      if (index < this->entries.size()) {
        auto i = this->entries.begin();
        std::advance(i, index);
        return i->second.file;
      }
      return nullptr;
    }
    virtual std::shared_ptr<IEggboxFileEntry> findFileEntryBySubpath(const std::string& subpath) override {
      auto found = this->entries.find(subpath);
      if (found == this->entries.end()) {
        return nullptr;
      }
      return found->second.file;
    }
    virtual std::shared_ptr<IEggboxFileEntry> getFileEntry(const std::string& subpath) override {
      auto found = this->entries.find(subpath);
      if (found == this->entries.end()) {
        throw Exception("Entry not found in eggbox: '{entry}'")
          .with("entry", subpath)
          .populate([this](Exception& exception) {
            size_t index = 0;
            for (auto& eggbox : this->subboxes) {
              exception.emplace("eggbox" + std::to_string(++index), eggbox->getResourcePath(nullptr));
            }
          });
      }
      return found->second.file;
    }
    virtual IEggboxChain& with(const std::shared_ptr<IEggbox>& subbox) override {
      this->subboxes.push_back(subbox);
      auto found = subbox->findFileEntryByIndex(0);
      for (size_t index = 0; found != nullptr; ++index) {
        this->entries.emplace(found->getSubpath(), Entry{ subbox.get(), found});
        found = subbox->findFileEntryByIndex(index);
      }
      return *this;
    }
  };

  uint64_t addFile(os::zip::IZipWriter& writer, const std::string& name, const std::filesystem::path& native) {
    std::ifstream ifs{ native, std::ios::binary };
    ifs.seekg(0, std::ios::end);
    auto bytes = ifs.tellg();
    std::string content(size_t(bytes), '\0');
    ifs.seekg(0);
    ifs.read(content.data(), bytes);
    writer.addFileEntry(name, content);
    return uint64_t(bytes);
  }

  uint64_t addDirectoryRecursive(os::zip::IZipWriter& writer, const std::string& prefix, const std::filesystem::path& native, size_t& entries) {
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
      throw Exception("Cannot walk directory: {error}")
        .with("path", native.string())
        .with("error", os::process::format(error));
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

uint64_t egg::ovum::EggboxFactory::createSandwichFromFile(const std::filesystem::path& targetPath, const std::filesystem::path& zipPath, bool overwriteTarget, const std::string& label) {
  os::embed::cloneExecutable(targetPath, overwriteTarget);
  return os::embed::updateResourceFromFile(targetPath, PROGBITS, label, zipPath);
}

size_t egg::ovum::EggboxFactory::createZipFileFromDirectory(const std::filesystem::path& zipPath, const std::filesystem::path& directoryPath, uint64_t& compressedBytes, uint64_t& uncompressedBytes) {
  auto writer = os::zip::openWriteZipFile(zipPath);
  size_t entries = 0;
  uncompressedBytes = addDirectoryRecursive(*writer, "", directoryPath, entries);
  compressedBytes = writer->commit();
  return entries;
}

std::shared_ptr<egg::ovum::IEggbox> egg::ovum::EggboxFactory::openDirectory(const std::filesystem::path& path) {
  auto full = std::filesystem::absolute(path);
  auto status = std::filesystem::status(full);
  if (!std::filesystem::exists(status)) {
    throw Exception("Eggbox directory does not exist: '{path}'").with("path", path.generic_string()).with("native", full.string());
  }
  if (!std::filesystem::is_directory(status)) {
    throw Exception("Eggbox path is not a directory: '{path}'").with("path", path.generic_string()).with("native", full.string());
  }
  return std::make_shared<EggboxDirectory>(full);
}

std::shared_ptr<egg::ovum::IEggbox> egg::ovum::EggboxFactory::openZipFile(const std::filesystem::path& path) {
  auto full = std::filesystem::absolute(path);
  auto status = std::filesystem::status(full);
  if (!std::filesystem::exists(status)) {
    throw Exception("Eggbox zip file does not exist: '{path}'").with("path", path.generic_string()).with("native", full.string());
  }
  if (!std::filesystem::is_regular_file(status)) {
    throw Exception("Eggbox zip is not a regular file: '{path}'").with("path", path.generic_string()).with("native", full.string());
  }
  auto reader = os::zip::openReadZipFile(full);
  return std::make_shared<EggboxZip>(reader, os::file::normalizePath(path.string(), false));
}

std::shared_ptr<egg::ovum::IEggbox> egg::ovum::EggboxFactory::openEmbedded(const std::filesystem::path& executable, const std::string& label) {
  // TODO optimize
  auto lockable = os::embed::findResourceByName(executable, PROGBITS, label);
  if (lockable == nullptr) {
    auto full = std::filesystem::absolute(executable);
    auto status = std::filesystem::status(full);
    if (!std::filesystem::exists(status)) {
      throw Exception("Eggbox executable does not exist: '{executable}'").with("executable", executable.generic_string()).with("native", full.string());
    }
    if (!std::filesystem::is_regular_file(status)) {
      throw Exception("Eggbox executable is not a regular file: '{executable}'").with("executable", executable.generic_string()).with("native", full.string());
    }
    throw Exception("Unable to find eggbox resource in executable: '{executable}'").with("executable", executable.generic_string()).with("native", full.string()).with("label", label);
  }
  auto base = lockable->lock();
  MemoryStreamBuf streambuf{ const_cast<void*>(base), lockable->bytes };
  std::istream stream{ &streambuf };
  auto reader = os::zip::openReadStream(stream);
  return std::make_shared<EggboxZip>(reader, executable.generic_string() + "//~" + label);
}

std::shared_ptr<egg::ovum::IEggboxChain> egg::ovum::EggboxFactory::createChain() {
  return std::make_shared<EggboxChain>();
}

std::shared_ptr<egg::ovum::IEggboxChain> egg::ovum::EggboxFactory::createDefault() {
  auto chain = EggboxFactory::createChain();
  assert(chain != nullptr);
  chain->with(EggboxFactory::openEmbedded(os::file::getExecutablePath()));
  return chain;
}

