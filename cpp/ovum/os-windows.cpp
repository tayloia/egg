#include "ovum/ovum.h"
#include "ovum/os-embed.h"
#include "ovum/os-file.h"

#include <windows.h>
#include <codecvt>

using Resource = egg::ovum::os::embed::Resource;

namespace {
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
  void updateResource(HANDLE handle, const std::string& type, const std::string& name, const void* data, size_t bytes) {
    auto wtype = widen(type);
    auto wname = widen(name);
    WORD language = MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL);
    if (!::UpdateResource(handle, wtype.c_str(), wname.c_str(), language, LPVOID(data), DWORD(bytes))) {
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
    return ::LoadLibraryEx(wexecutable.c_str(), NULL, LOAD_LIBRARY_AS_IMAGE_RESOURCE);
  }
  BOOL enumResourceNamesCallback(HMODULE module, LPCWSTR type, LPWSTR label, LONG_PTR lparam) {
    std::vector<Resource>& resources = *(std::vector<Resource>*)lparam;
    size_t bytes = 0;
    auto handle = ::FindResourceW(module, label, type);
    if (handle != NULL) {
      bytes = ::SizeofResource(module, handle);
    }
    resources.push_back({ name(type), name(label), bytes });
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
  void freeLibrary(HMODULE module) {
    if (!::FreeLibrary(module)) {
      throw std::runtime_error("Cannot free resource library executable file");
    }
  }
}

void egg::ovum::os::embed::addResource(const std::string& executable, const std::string& type, const std::string& label, const void* resource, size_t bytes) {
  auto handle = beginUpdateResource(executable, false);
  try {
    updateResource(handle, type, label, resource, bytes);
  }
  catch (...) {
    endUpdateResource(handle, true);
    throw;
  }
  endUpdateResource(handle, false);
}

std::vector<Resource> egg::ovum::os::embed::findResources(const std::string& executable) {
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
