#if defined(_MSC_VER)

// Micrsoft Visual C++: Keep windows.h inclusions to a minimum
#define WIN32_LEAN_AND_MEAN
// Disable overexuberant warnings:
// warning C4514: '...': unreferenced function has been removed
// warning C4571: Informational: catch(...) semantics changed since Visual C++ 7.1; structured exceptions (SEH) are no longer caught
// warning C4710: '...': function not inlined
// warning C4711: function '...' selected for automatic expansion
// warning C4820 : '...' : '...' bytes padding added after data member '...'
// warning C5045: Compiler will insert Spectre mitigation for memory load if /Qspectre switch specified
#pragma warning(disable: 4514 4571 4710 4711 4820 5045)
// Prepare to include some system headers with lower warning levels here
#pragma warning(push)
// warning C4365: 'argument': conversion from '...' to '...', signed/unsigned mismatch
// warning C4623: '...': default constructor was implicitly defined as deleted because a base class default constructor is inaccessible or deleted
// warning C4625: '...': copy constructor was implicitly defined as deleted
// warning C4626: '...': assignment operator was implicitly defined as deleted
// warning C4774: '...' : format string expected in argument 1 is not a string literal
// warning C5026: '...': move constructor was implicitly defined as deleted
// warning C5027: '...': move assignment operator was implicitly defined as deleted
// warning C5039: '...' : pointer or reference to potentially throwing function passed to extern C function under -EHc
#pragma warning(disable: 4365 4623 4625 4626 4774 5026 5027 5039)

#elif defined(__GNUC__)

// GNU GCC: See https://stackoverflow.com/a/16472469
#ifdef __STRICT_ANSI__
#undef __STRICT_ANSI__
#endif

// For malloc_usable_size()
#include <malloc.h>

#else

// We need to add a new section for this compiler
#error Unknown platform

#endif

// These are system headers (included with a possibly lower warning level)
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <atomic>
#include <functional>
#include <list>
#include <mutex>
#include <set>
#include <shared_mutex>
#include <string>

#if defined(_MSC_VER)
#pragma warning(pop)
#endif

// These are our own headers
#include "ovum/vm.h"
#include "ovum/variant.h"
#include "ovum/factories.h"
