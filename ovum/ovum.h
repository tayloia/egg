#include "ovum/platform.h"

#include <atomic>
#include <cassert>
#include <cmath>
#include <cstring>
#include <functional>
#include <list>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <sstream>

#include "ovum/vm.h"
#include "ovum/interfaces.h"
#include "ovum/utility.h"
#include "ovum/hardptr.h"
#include "ovum/softptr.h"
#include "ovum/memory.h"
#include "ovum/print.h"
#include "ovum/string.h"
#include "ovum/arithmetic.h"
#include "ovum/type.h"
#include "ovum/object.h"
#include "ovum/value.h"
#include "ovum/basket.h"
#include "ovum/context.h"
