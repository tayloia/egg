#include "ovum/ovum.h"
#include "ovum/os-process.h"

FILE* egg::ovum::os::process::popen(const std::string& command) {
#if EGG_PLATFORM == EGG_PLATFORM_MSVC
  return ::_popen(command.c_str(), "r");
#else
  return ::popen(command.c_str(), "r");
#endif
}

void egg::ovum::os::process::pclose(FILE* fp) {
#if EGG_PLATFORM == EGG_PLATFORM_MSVC
  ::_pclose(fp);
#else
  ::pclose(fp);
#endif
}
