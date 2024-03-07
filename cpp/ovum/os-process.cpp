#include "ovum/ovum.h"
#include "ovum/os-process.h"

namespace {
  void writeErrno(std::ostream& os, int error) {
#if EGG_PLATFORM == EGG_PLATFORM_MSVC
    char buffer[1024];
    if (strerror_s(buffer, error) == 0) {
      os << buffer;
    } else {
      os << "errno=" << error;
    }
#else
    auto buffer = strerror(error);
    if (buffer != nullptr) {
      os << buffer;
    } else {
      os << "errno=" << error;
    }
#endif
  }
}

FILE* egg::ovum::os::process::popen(const char* command, const char* mode) {
  assert(command != nullptr);
  assert(mode != nullptr);
#if EGG_PLATFORM == EGG_PLATFORM_MSVC
  return ::_popen(command, mode);
#else
  return ::popen(command, mode);
#endif
}

int egg::ovum::os::process::pclose(FILE* fp) {
#if EGG_PLATFORM == EGG_PLATFORM_MSVC
  return ::_pclose(fp);
#else
  auto status = ::pclose(fp);
  return WIFEXITED(status) ? WEXITSTATUS(status) : status;
#endif
}

int egg::ovum::os::process::pexec(std::ostream& os, const std::string& command) {
  int retval = -1;
  auto redirect = "2>&1 " + command;
  auto* fp = egg::ovum::os::process::popen(redirect.c_str(), "r");
  if (fp != nullptr) {
    for (auto ch = std::fgetc(fp); ch != EOF; ch = std::fgetc(fp)) {
      os.put(char(ch));
    }
    retval = egg::ovum::os::process::pclose(fp);
  }
  if (retval < 0) {
    writeErrno(os, errno);
  }
  return retval;
}

int egg::ovum::os::process::plines(const std::string& command, std::function<void(const std::string&)> callback) {
  int retval = -1;
  auto redirect = "2>&1 " + command;
  auto* fp = egg::ovum::os::process::popen(redirect.c_str(), "r");
  if (fp != nullptr) {
    std::string line;
    int last = -1;
    for (int ch = std::fgetc(fp); ch != EOF; ch = std::fgetc(fp)) {
      switch (ch) {
      case '\n':
        callback(line);
        line.clear();
        break;
      case '\r':
        if (last == '\r') {
          callback(line);
          line.clear();
        }
        break;
      default:
        line.push_back(char(ch));
        break;
      }
      last = ch;
    }
    if (last == '\r') {
      callback(line);
    }
    retval = egg::ovum::os::process::pclose(fp);
  }
  return retval;
}
