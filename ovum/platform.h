#define EGG_PLATFORM_MSVC 1
#define EGG_PLATFORM_GCC 2

#if defined(_MSC_VER)

// Micrsoft Visual C++
#define EGG_PLATFORM EGG_PLATFORM_MSVC
// Keep windows.h inclusions to a minimum
#define WIN32_LEAN_AND_MEAN
// Disable overexuberant warnings:
// warning C4514: '...': unreferenced function has been removed
// warning C4571: Informational: catch(...) semantics changed since Visual C++ 7.1; structured exceptions (SEH) are no longer caught
// warning C4710: '...': function not inlined
// warning C4711: function '...' selected for automatic expansion
// warning C4820 : '...' : '...' bytes padding added after data member '...'
// warning C5045: Compiler will insert Spectre mitigation for memory load if /Qspectre switch specified
#pragma warning(disable: 4514 4571 4710 4711 4820 5045)
// Include some base headers with lower warning levels here
#pragma warning(push)
#pragma warning(disable: 4365)
#include <string>
#pragma warning(pop)

// As of 2022-08-16, MSVC IntelliSense does not respect '/Zc:char8_t'
#if defined(__INTELLISENSE__)
#define char8_t uint8_t
#endif

// warning C4061 : enumerator '...' in switch of enum '...' is not explicitly handled by a case label
// warning C4062 : enumerator '...' in switch of enum '...' is not handled
// The do-while loop is purely to stop the MSVC editor from getting the smart indenting all wrong
#define EGG_WARNING_SUPPRESS_SWITCH_BEGIN __pragma(warning(push)) __pragma(warning(disable: 4061 4062)) do {
#define EGG_WARNING_SUPPRESS_SWITCH_END } __pragma(warning(pop)) while (false);

#elif defined(__GNUC__)

// GNU GCC
#define EGG_PLATFORM EGG_PLATFORM_GCC

#define EGG_WARNING_SUPPRESS_SWITCH_BEGIN _Pragma("GCC diagnostic push") _Pragma("GCC diagnostic ignored \"-Wswitch\"")
#define EGG_WARNING_SUPPRESS_SWITCH_END _Pragma("GCC diagnostic pop")

#else

// We need to add a new section for this compiler
#error Unknown platform

#endif
