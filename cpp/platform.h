#define EGG_PLATFORM_MSVC 1

#if defined(_MSC_VER)

// Micrsoft Visual C++
#define EGG_PLATFORM EGG_PLATFORM_MSVC
// Keep windows.h inclusions to a minimum
#define WIN32_LEAN_AND_MEAN
// Disable overexuberant warnings:
// warning C4514: '...': unreferenced inline function has been removed
// warning C4571: Informational: catch(...) semantics changed since Visual C++ 7.1; structured exceptions (SEH) are no longer caught
// warning C4710: '...': function not inlined
// warning C4711: function '...' selected for automatic inline expansion
// warning C4820 : '...' : '...' bytes padding added after data member '...'
#pragma warning(disable: 4514 4571 4710 4711 4820)
// Include some base headers with lower warning levels here
#pragma warning(push)
#pragma warning(disable: 4365 4623 4625 4626 4774 5026 5027)
#include <algorithm>
#include <fstream>
#include <functional>
#include <memory>
#include <string>
#include <sstream>
#include <vector>
#pragma warning(pop)

#else

// We need to add a new section for this compiler
#error Unknown platform

#endif
