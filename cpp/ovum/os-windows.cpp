#include "ovum/ovum.h"
#include "ovum/file.h"
#include "ovum/os-embed.h"
#include "ovum/os-file.h"

#include <windows.h>

namespace {
  using namespace egg::ovum::os::embed;

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
      throw std::runtime_error("Cannot open executable file for resource writing");
    }
    return handle;
  }
  void updateResource(HANDLE handle, const std::string& type, const std::string& label, const void* data, size_t bytes) {
    auto wtype = widen(type);
    auto wlabel = widen(label);
    WORD language = MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL);
    if (!::UpdateResource(handle, wtype.c_str(), wlabel.c_str(), language, LPVOID(data), DWORD(bytes))) {
      throw std::runtime_error("Cannot update resource in executable file");
    }
  }
  void endUpdateResource(HANDLE handle, bool discard) {
    if (!::EndUpdateResource(handle, discard)) {
      throw std::runtime_error("Cannot write resource changes to executable file");
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
      throw std::runtime_error("Cannot free resource library executable file");
    }
  }
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