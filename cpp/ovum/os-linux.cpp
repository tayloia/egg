#include "ovum/ovum.h"
#include "ovum/os-embed.h"
#include "ovum/os-file.h"
#include "ovum/os-process.h"

#include <filesystem>
#include <fstream>
#include <regex>
#include <fcntl.h>
#include <sys/mman.h>

namespace {
  void objcopy(const std::string& command) {
    auto exitcode = egg::ovum::os::process::plines(command, [&](const std::string&) {
      // Do nothing
    });
    fflush(stdout);
    if (exitcode != 0) {
      throw std::runtime_error("Cannot spawn objcopy");
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
      auto exitcode = egg::ovum::os::process::plines("readelf -SW " + executable, [&](const std::string& line) {
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
        throw std::runtime_error("Cannot spawn readelf");
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
        throw std::runtime_error("Cannot open ELF resource executable");
      }
      align = this->offset % pagesize;
      auto* mapped = ::mmap(nullptr, this->bytes + align, PROT_READ, MAP_PRIVATE, fd, this->offset - align);
      ::close(fd);
      if (mapped == MAP_FAILED) {
        throw std::runtime_error("Cannot map ELF resource");
      }
      return mapped;
    }
    void unmap(void* mapped) const {
      if (::munmap(mapped, this->bytes)) {
        throw std::runtime_error("Cannot unmap ELF resource");
      }
    }
  };

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
