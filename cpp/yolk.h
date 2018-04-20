#include "platform.h"

#include <algorithm>
#include <atomic>
#include <cassert>
#include <cctype>
#include <deque>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <type_traits>
#include <vector>

#include "macros.h"
#include "exceptions.h"
#include "gc.h"
#include "files.h"
#include "streams.h"
#include "utf.h"

// TODO candidates for removal
#include "strings.h"

#include "lang.h"
