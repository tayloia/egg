#include "ovum/ovum.h"
#include "ovum/dictionary.h"
#include "ovum/exception.h"

namespace egg::yolk {
  template<typename T, size_t N> char(*nelemsHelper(T(&)[N]))[N] {}
}
#define EGG_NELEMS(arr) (sizeof(*egg::yolk::nelemsHelper(arr)))
