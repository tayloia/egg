#include "ovum/ovum.h"
#include "ovum/os-embed.h"
#include "ovum/os-file.h"
#include "ovum/os-process.h"

#include <fstream>
#include <regex>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/times.h>

namespace {
  auto clockStart = ::times(nullptr);
  void objcopy(const std::string& command) {
    auto exitcode = egg::ovum::os::process::plines(command, [&](const std::string&) {
      // Do nothing
    });
    fflush(stdout);
    if (exitcode != 0) {
      throw egg::ovum::Exception("Cannot spawn objcopy: '{command}'").with("command", command);
    }
  }
  struct ReadElf {
    // See https://github.com/lirongyuan/ELF-Reader-and-Loader/blob/master/elfinfo-mmap.c
    std::string name;
    std::string type;
    size_t address;
    size_t offset;
    size_t size;
    static void foreach(const std::string& executable, std::function<void(const ReadElf&)> callback) {
      std::regex pattern{ "\\s+\\[\\s*\\d+\\]\\s(\\S+)\\s+(\\S+)\\s+(\\S+)\\s+(\\S+)\\s+(\\S+).*" };
      auto command = "readelf -SW " + executable;
      auto exitcode = egg::ovum::os::process::plines(command, [&](const std::string& line) {
        std::smatch match;
        if (std::regex_match(line, match, pattern)) {
          ReadElf elf;
          elf.name = match[1].str();
          elf.type = match[2].str();
          elf.address = ReadElf::hex(match[3].str());
          elf.offset = ReadElf::hex(match[4].str());
          elf.size = ReadElf::hex(match[5].str());
          callback(elf);
        }
        });
      if (exitcode != 0) {
        throw egg::ovum::Exception("Cannot spawn readelf: '{command}'").with("command", command);
      }
    }
    static size_t hex(const std::string& text) {
      return size_t(std::stoull(text, nullptr, 16));
    }
  };
  struct ElfLockableResource : public egg::ovum::os::embed::LockableResource {
    std::string path;
    size_t offset;
    void* locked;
    size_t skip;
    ElfLockableResource(const std::string& path, const ReadElf& elf) {
      this->type = elf.type;
      this->label = elf.name;
      this->bytes = elf.size;
      this->path = path;
      this->offset = elf.offset;
      this->locked = nullptr;
      this->skip = 0;
    }
    ~ElfLockableResource() {
      this->unlock();
    }
    virtual const void* lock() override {
      if (this->locked == nullptr) {
        this->locked = this->map(this->skip);
      }
      return (char*)this->locked + this->skip;
    }
    virtual void unlock() override {
      auto* mapped = this->locked;
      if (mapped != nullptr) {
        this->locked = nullptr;
        this->skip = 0;
        this->unmap(mapped);
      }
    }
    void* map(size_t& align) const {
      size_t pagesize = ::getpagesize();
      auto fd = ::open(this->path.c_str(), O_RDONLY);
      if (fd < 0) {
        throw egg::ovum::Exception("Cannot open ELF resource executable: '{path}'").with("path", this->path);
      }
      align = this->offset % pagesize;
      auto* mapped = ::mmap(nullptr, this->bytes + align, PROT_READ, MAP_PRIVATE, fd, this->offset - align);
      ::close(fd);
      if (mapped == MAP_FAILED) {
        throw egg::ovum::Exception("Cannot map ELF resource: '{path}'").with("path", this->path);
      }
      return mapped;
    }
    void unmap(void* mapped) const {
      if (::munmap(mapped, this->bytes)) {
        throw egg::ovum::Exception("Cannot unmap ELF resource: '{path}'").with("path", this->path);
      }
    }
  };
  void extractStatus(const std::string& line, const std::string& label, uint64_t& value, uint64_t scale) {
    if (line.starts_with(label)) {
      value = std::atoll(line.data() + label.size()) * scale;
    }
  }
  uint64_t extractMicroseconds(clock_t clock) {
    uint64_t clockTicksPerSecond = ::sysconf(_SC_CLK_TCK);
    return (uint64_t(clock) * 1000000 + (clockTicksPerSecond / 2)) / clockTicksPerSecond;
  }
}

void egg::ovum::os::embed::updateResourceFromMemory(const std::string& executable, const std::string& type, const std::string& label, const void* data, size_t bytes) {
  if ((data == nullptr) || (bytes == 0)) {
    objcopy("objcopy --remove-section " + label + " " + executable);
  } else {
    auto path = egg::ovum::os::file::createTemporaryFile("os-embed-", ".tmp", 100);
    std::ofstream ofs{ path, std::ios::trunc | std::ios::binary };
    if (data != nullptr) {
      ofs.write(static_cast<const char*>(data), bytes);
    }
    ofs.close();
    egg::ovum::os::embed::updateResourceFromFile(executable, type, label, path);
  }
}

void egg::ovum::os::embed::updateResourceFromFile(const std::string& executable, const std::string& type, const std::string& label, const std::string& datapath) {
  std::string option = "--add-section";
  ReadElf::foreach(executable, [&](const ReadElf& elf) {
    if ((elf.type == type) && (elf.name == label)) {
      option = "--update-section";
    }
  });
  objcopy("objcopy " + option + " " + label + "=" + datapath + " --set-section-flags " + label + "=contents,noload,readonly " + executable);
}

std::vector<egg::ovum::os::embed::Resource> egg::ovum::os::embed::findResources(const std::string& executable) {
  std::vector<Resource> resources;
  ReadElf::foreach(executable, [&](const ReadElf& elf) {
    resources.push_back({ elf.type, elf.name, elf.size });
  });
  return resources;
}

std::vector<egg::ovum::os::embed::Resource> egg::ovum::os::embed::findResourcesByType(const std::string& executable, const std::string& type) {
  std::vector<Resource> resources;
  ReadElf::foreach(executable, [&](const ReadElf& elf) {
    if (elf.type == type) {
      resources.push_back({ elf.type, elf.name, elf.size });
    }
  });
  return resources;
}

std::shared_ptr<egg::ovum::os::embed::LockableResource> egg::ovum::os::embed::findResourceByName(const std::string& executable, const std::string& type, const std::string& label) {
  std::shared_ptr<LockableResource> found = nullptr;
  ReadElf::foreach(executable, [&](const ReadElf& elf) {
    if ((elf.type == type) && (elf.name == label)) {
      assert(found == nullptr);
      found = std::make_shared<ElfLockableResource>(executable, elf);
    }
  });
  return found;
}

egg::ovum::os::memory::Snapshot egg::ovum::os::memory::snapshot() {
  // Try 'memusage /mnt/c/Project/egg/bin/wsl/gcc/release/egg-stub.exe --profile=memory'
  // See https://stackoverflow.com/a/64166
  Snapshot snapshot{ 0, 0, 0, 0 };
  std::ifstream ifs{ "/proc/self/status" };
  std::string line;
  while (std::getline(ifs, line)) {
    extractStatus(line, "VmPeak:", snapshot.peakBytesTotal, 1024);
    extractStatus(line, "VmSize:", snapshot.currentBytesTotal, 1024);
    extractStatus(line, "VmHWM:", snapshot.peakBytesData, 1024);
    extractStatus(line, "VmRSS:", snapshot.currentBytesData, 1024);
  }
  return snapshot;
}

egg::ovum::os::process::Snapshot egg::ovum::os::process::snapshot() {
  tms tms;
  auto clockNow = ::times(&tms);
  Snapshot snapshot;
  snapshot.microsecondsUser = extractMicroseconds(tms.tms_utime);
  snapshot.microsecondsSystem = extractMicroseconds(tms.tms_stime);
  snapshot.microsecondsElapsed = extractMicroseconds(clockNow - clockStart);
  return snapshot;
}
