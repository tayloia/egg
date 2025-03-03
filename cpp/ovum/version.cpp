#include "ovum/ovum.h"
#include "ovum/version.h"

static constexpr int VERSION_MAJOR = 0;
static constexpr int VERSION_MINOR = 0;
static constexpr int VERSION_PATCH = 0;

#if !defined(EGG_COMMIT) || !defined(EGG_TIMESTAMP)
#include "msvc/transient.h"
#endif

static constexpr const char VERSION_COMMIT[] = EGG_COMMIT;
static constexpr const char VERSION_TIMESTAMP[] = EGG_TIMESTAMP;

egg::ovum::Version::Version()
  : major(VERSION_MAJOR),
    minor(VERSION_MINOR),
    patch(VERSION_PATCH),
    commit(VERSION_COMMIT),
    timestamp(VERSION_TIMESTAMP) {
  assert(this->commit != nullptr);
}

std::ostream& egg::ovum::operator<<(std::ostream& stream, const egg::ovum::Version& version) {
  return stream << "egg v" << version.major << '.' << version.minor << '.' << version.patch << " (" << version.timestamp << ") [" << version.commit << "]";
}
