#include "ovum/ovum.h"
#include "ovum/dictionary.h"

namespace egg::yolk {
  template<typename T, size_t N> char(*nelemsHelper(T(&)[N]))[N] {}
}
#define EGG_NELEMS(arr) (sizeof(*egg::yolk::nelemsHelper(arr)))

#include "yolk/exceptions.h"
#include "yolk/files.h"
#include "yolk/streams.h"
