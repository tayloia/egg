#include "ovum/ovum.h"
#include "ovum/file.h"
#include "ovum/os-embed.h"
#include "ovum/os-file.h"

#include <windows.h>
#include <psapi.h>

namespace {
  using namespace egg::ovum::os::embed;
  using namespace egg::ovum::os::memory;

  struct WindowsLockableResource : public LockableResource {
    HMODULE module;
    HRSRC handle;
    HGLOBAL locked;
    WindowsLockableResource(HMODULE rmodule, const std::string& rtype, const std::string& rlabel, HRSRC rhandle) {
      this->type = rtype;
      this->label = rlabel;
      this->bytes = ::SizeofResource(rmodule, rhandle);
      this->module = rmodule;
      this->handle = rhandle;
      this->locked = NULL;
    }
    ~WindowsLockableResource() {
      this->unlock();
      ::FreeLibrary(this->module);
    }
    virtual const void* lock() override {
      if (this->locked == NULL) {
        this->locked = ::LoadResource(this->module, this->handle);
        if (this->locked == NULL) {
          return nullptr;
        }
      }
      return ::LockResource(this->locked);
    }
    virtual void unlock() override {
      if (this->locked != NULL) {
        ::FreeResource(this->locked);
        this->locked = NULL;
      }
    }
  };
  std::string narrow(const std::wstring& wide) {
    auto count = ::WideCharToMultiByte(CP_UTF8, 0, wide.data(), int(wide.size()), NULL, NULL, NULL, NULL);
    assert(count >= 0);
    std::string slim(size_t(count), '\0');
    count = ::WideCharToMultiByte(CP_UTF8, 0, wide.data(), int(wide.size()), slim.data(), count, NULL, NULL);
    assert(count >= 0);
    return slim;
  }
  std::wstring widen(const std::string& narrow) {
    auto count = ::MultiByteToWideChar(CP_UTF8, 0, narrow.data(), int(narrow.size()), NULL, 0);
    assert(count >= 0);
    std::wstring wide(size_t(count), L'\0');
    count = ::MultiByteToWideChar(CP_UTF8, 0, narrow.data(), int(narrow.size()), wide.data(), count);
    assert(count >= 0);
    return wide;
  }
  std::string name(LPCWSTR wide) {
    if (IS_INTRESOURCE(wide)) {
      return "#" + std::to_string(size_t(wide));
    }
    return narrow(wide);
  }
  HANDLE beginUpdateResource(const std::string& executable, bool deleteExisting) {
    auto wexecutable = widen(egg::ovum::os::file::denormalizePath(executable, false));
    auto handle = ::BeginUpdateResource(wexecutable.c_str(), deleteExisting);
    if (handle == NULL) {
      throw egg::ovum::Exception("Cannot open executable file for resource writing: '{path}'").with("path", executable);
    }
    return handle;
  }
  void updateResource(HANDLE handle, const std::string& type, const std::string& label, const void* data, size_t bytes) {
    auto wtype = widen(type);
    auto wlabel = widen(label);
    WORD language = MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL);
    if (!::UpdateResource(handle, wtype.c_str(), wlabel.c_str(), language, LPVOID(data), DWORD(bytes))) {
      throw egg::ovum::Exception("Cannot update resource in executable file: '{type}/{label}'").with("type", type).with("label", label);
    }
  }
  void endUpdateResource(HANDLE handle, bool discard) {
    if (!::EndUpdateResource(handle, discard)) {
      throw egg::ovum::Exception("Cannot commit resource changes to executable file");
    }
  }
  HMODULE loadLibrary(const std::string& executable) {
    auto wexecutable = widen(egg::ovum::os::file::denormalizePath(executable, false));
    return ::LoadLibraryEx(wexecutable.c_str(), NULL, LOAD_LIBRARY_AS_DATAFILE);
  }
  BOOL enumResourceNamesCallback(HMODULE module, LPCWSTR type, LPWSTR label, LONG_PTR lparam) {
    auto handle = ::FindResourceW(module, label, type);
    if (handle != NULL) {
      auto& resources = *(std::vector<Resource>*)lparam;
      resources.push_back({ name(type), name(label), ::SizeofResource(module, handle) });
    }
    return TRUE;
  }
  BOOL enumResourceTypesCallback(HMODULE module, LPWSTR type, LONG_PTR lparam) {
    ::EnumResourceNamesW(module, type, enumResourceNamesCallback, lparam);
    return TRUE;
  }
  std::vector<Resource> enumResourceTypes(HMODULE module) {
    std::vector<Resource> resources;
    ::EnumResourceTypesW(module, enumResourceTypesCallback, (LONG_PTR)&resources);
    return resources;
  }
  std::vector<Resource> enumResourceNames(HMODULE module, const std::string& type) {
    std::vector<Resource> resources;
    auto wtype = widen(type);
    ::EnumResourceNamesW(module, wtype.c_str(), enumResourceNamesCallback, (LONG_PTR)&resources);
    return resources;
  }
  std::shared_ptr<LockableResource> findResourceName(HMODULE module, const std::string& type, const std::string& label) {
    auto wtype = widen(type);
    auto wlabel = widen(label);
    auto handle = ::FindResourceW(module, wlabel.c_str(), wtype.c_str());
    if (handle == NULL) {
      return nullptr;
    }
    return std::make_shared<WindowsLockableResource>(module, type, label, handle);
  }
  void freeLibrary(HMODULE module) {
    if (!::FreeLibrary(module)) {
      throw egg::ovum::Exception("Cannot free resource library handle");
    }
  }
  struct InternalSnapshot_PWSI {
    uint64_t executable;
    uint64_t readonly;
    uint64_t writable;
    uint64_t other;
    bool take(const PSAPI_WORKING_SET_INFORMATION& pwsi) {
      this->executable = 0;
      this->readonly = 0;
      this->writable = 0;
      this->other = 0;
      for (size_t i = 0; i < pwsi.NumberOfEntries; ++i) {
        const PSAPI_WORKING_SET_BLOCK& pwsb = pwsi.WorkingSetInfo[i];
        switch (pwsb.Protection) {
        case 0:
          // The page is not accessed
          this->other++;
          break;
        case 1:
          // Read-only
          this->readonly++;
          break;
        case 2:
          // Executable
          this->executable++;
          break;
        case 3:
          // Executable and read-only
          this->executable++;
          break;
        case 4:
          // Read/write
          this->writable++;
          break;
        case 5:
          // Copy-on-write
          this->writable++;
          break;
        case 6:
          // Executable and read/write
          this->executable++;
          break;
        case 7:
          // Executable and copy-on-write
          this->executable++;
          break;
        case 8:
          // The page is not accessed
          this->other++;
          break;
        case 9:
          // Non-cacheable and read-only
          this->readonly++;
          break;
        case 10:
          // Non-cacheable and executable
          this->executable++;
          break;
        case 11:
          // Non-cacheable, executable, and read-only
          this->executable++;
          break;
        case 12:
          // Non-cacheable and read/write
          this->writable++;
          break;
        case 13:
          // Non-cacheable and copy-on-write
          this->writable++;
          break;
        case 14:
          // Non-cacheable, executable, and read/write
          this->executable++;
          break;
        case 15:
          // Non-cacheable, executable, and copy-on-write
          this->executable++;
          break;
        case 16:
          // The page is not accessed
          this->other++;
          break;
        case 17:
          // Guard page and read-only
          this->readonly++;
          break;
        case 18:
          // Guard page and executable
          this->executable++;
          break;
        case 19:
          // Guard page, executable, and read-only
          this->executable++;
          break;
        case 20:
          // Guard page and read/write
          this->writable++;
          break;
        case 21:
          // Guard page and copy-on-write
          this->writable++;
          break;
        case 22:
          // Guard page, executable, and read/write
          this->executable++;
          break;
        case 23:
          // Guard page, executable, and copy-on-write
          this->executable++;
          break;
        case 24:
          // The page is not accessed
          this->other++;
          break;
        case 25:
          // Non-cacheable, guard page, and read-only
          this->readonly++;
          break;
        case 26:
          // Non-cacheable, guard page, and executable
          this->executable++;
          break;
        case 27:
          // Non-cacheable, guard page, executable, and read-only
          this->executable++;
          break;
        case 28:
          // Non-cacheable, guard page, and read/write
          this->writable++;
          break;
        case 29:
          // Non-cacheable, guard page, and copy-on-write
          this->writable++;
          break;
        case 30:
          // Non-cacheable, guard page, executable, and read/write
          this->executable++;
          break;
        case 31:
          // Non-cacheable, guard page, executable, and copy-on-write
          this->executable++;
          break;
        }
      }
      this->executable *= 0x1000;
      this->readonly *= 0x1000;
      this->writable *= 0x1000;
      this->other *= 0x1000;
      return true;
    }
    static bool take(InternalSnapshot_PWSI& snapshot) {
      std::vector<uint8_t> buffer;
      size_t entries = 1;
      do {
        auto bytes = offsetof(PSAPI_WORKING_SET_INFORMATION, WorkingSetInfo[entries]);
        buffer.resize(bytes);
        auto* base = reinterpret_cast<PSAPI_WORKING_SET_INFORMATION*>(buffer.data());
        if (::QueryWorkingSet(::GetCurrentProcess(), base, DWORD(bytes))) {
          return snapshot.take(*base);
        }
        entries = base->NumberOfEntries;
      } while (::GetLastError() == ERROR_BAD_LENGTH);
      return false;
    }
  };
  struct InternalSnapshot_PMCE {
    PROCESS_MEMORY_COUNTERS_EX pcme;
    static bool take(InternalSnapshot_PMCE& snapshot) {
      return ::GetProcessMemoryInfo(::GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&snapshot.pcme), sizeof(snapshot.pcme));
    }
  };
  struct InternalSnapshot : public InternalSnapshot_PMCE {
    InternalSnapshot_PWSI pwsi;
  };
  bool takeSnapshot(InternalSnapshot& snapshot) {
    return InternalSnapshot_PMCE::take(snapshot) && InternalSnapshot_PWSI::take(snapshot.pwsi);
  }
  // Filled by 'egg::ovum::os::memory::initialize()'
  InternalSnapshot initialSnapshot{};
}

void egg::ovum::os::embed::updateResourceFromMemory(const std::string& executable, const std::string& type, const std::string& label, const void* data, size_t bytes) {
  auto handle = beginUpdateResource(executable, false);
  try {
    updateResource(handle, type, label, data, bytes);
  }
  catch (...) {
    endUpdateResource(handle, true);
    throw;
  }
  endUpdateResource(handle, false);
}

void egg::ovum::os::embed::updateResourceFromFile(const std::string& executable, const std::string& type, const std::string& label, const std::string& datapath) {
  auto slurped = egg::ovum::File::slurp(datapath);
  auto data = slurped.empty() ? nullptr : slurped.data();
  auto bytes = slurped.size();
  auto handle = beginUpdateResource(executable, false);
  try {
    updateResource(handle, type, label, data, bytes);
  }
  catch (...) {
    endUpdateResource(handle, true);
    throw;
  }
  endUpdateResource(handle, false);
}

std::vector<egg::ovum::os::embed::Resource> egg::ovum::os::embed::findResources(const std::string& executable) {
  auto handle = loadLibrary(executable);
  try {
    auto resources = enumResourceTypes(handle);
    freeLibrary(handle);
    return resources;
  }
  catch (...) {
    freeLibrary(handle);
    throw;
  }
}

std::vector<egg::ovum::os::embed::Resource> egg::ovum::os::embed::findResourcesByType(const std::string& executable, const std::string& type) {
  auto handle = loadLibrary(executable);
  try {
    auto resources = enumResourceNames(handle, type);
    freeLibrary(handle);
    return resources;
  }
  catch (...) {
    freeLibrary(handle);
    throw;
  }
}

std::shared_ptr<egg::ovum::os::embed::LockableResource> egg::ovum::os::embed::findResourceByName(const std::string& executable, const std::string& type, const std::string& label) {
  // The library handle is freed by RAII in ~LockableResource()
  auto handle = loadLibrary(executable);
  return findResourceName(handle, type, label);
}

#include <iostream>
void WIBBLE() {
  // Open current process
  HANDLE hProcess = ::OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, ::GetCurrentProcessId());
  if (hProcess) {
    PROCESS_MEMORY_COUNTERS ProcessMemoryCounters;

    memset(&ProcessMemoryCounters, 0, sizeof(ProcessMemoryCounters));

    // Set size of structure
    ProcessMemoryCounters.cb = sizeof(ProcessMemoryCounters);

    // Get memory usage
    if (::GetProcessMemoryInfo(hProcess,
      &ProcessMemoryCounters,
      sizeof(ProcessMemoryCounters)) == TRUE) {
      std::cout << std::setfill('0') << std::hex
        << "PageFaultCount: 0x" << std::setw(8)
        << ProcessMemoryCounters.PageFaultCount << std::endl
        << "PeakWorkingSetSize: 0x" << std::setw(8)
        << ProcessMemoryCounters.PeakWorkingSetSize << std::endl
        << "WorkingSetSize: 0x" << std::setw(8)
        << ProcessMemoryCounters.WorkingSetSize << std::endl
        << "QuotaPeakPagedPoolUsage: 0x" << std::setw(8)
        << ProcessMemoryCounters.QuotaPeakPagedPoolUsage << std::endl
        << "QuotaPagedPoolUsage: 0x" << std::setw(8)
        << ProcessMemoryCounters.QuotaPagedPoolUsage << std::endl
        << "QuotaPeakNonPagedPoolUsage: 0x" << std::setw(8)
        << ProcessMemoryCounters.QuotaPeakNonPagedPoolUsage << std::endl
        << "QuotaNonPagedPoolUsage: 0x" << std::setw(8)
        << ProcessMemoryCounters.QuotaNonPagedPoolUsage << std::endl
        << "PagefileUsage: 0x" << std::setw(8)
        << ProcessMemoryCounters.PagefileUsage << std::endl
        << "PeakPagefileUsage: 0x" << std::setw(8)
        << ProcessMemoryCounters.PeakPagefileUsage << std::endl;
    } else
      std::cout << "Could not get memory usage (Error: "
      << ::GetLastError() << ")" << std::endl;

    // Close process
    ::CloseHandle(hProcess);
  } else {
    std::cout << "Could not open process (Error " << ::GetLastError() << ")" << std::endl;
  }
}

void egg::ovum::os::memory::initialize() noexcept {
  //WIBBLE();
  assert(initialSnapshot.pwsi.executable == 0);
  if (!takeSnapshot(initialSnapshot)) {
    initialSnapshot.pwsi.executable = 0;
  }
}

egg::ovum::os::memory::Snapshot egg::ovum::os::memory::snapshot() {
  // See https://stackoverflow.com/a/33228050
  if (initialSnapshot.pwsi.executable == 0) {
    throw egg::ovum::Exception("Failed to get initial process working set information");
  }
  InternalSnapshot internal;
  if (!takeSnapshot(internal)) {
    throw egg::ovum::Exception("Cannot get current process working set information");
  }
  Snapshot pwsi{ 0 };
  pwsi.codeCurrentBytes = internal.pwsi.executable;
  pwsi.dataCurrentBytes = internal.pwsi.readonly;
  pwsi.heapCurrentBytes = internal.pwsi.writable + internal.pwsi.other; // WIBBLE
  Snapshot pcme{ 0 };
  pcme.heapCurrentBytes = internal.pcme.PrivateUsage;
  pcme.dataCurrentBytes = internal.pwsi.executable + internal.pwsi.readonly + internal.pwsi.writable;
  return {};
}