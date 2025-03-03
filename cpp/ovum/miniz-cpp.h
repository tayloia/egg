// See https://github.com/tfussell/miniz-cpp
#if EGG_PLATFORM == EGG_PLATFORM_MSVC

// warning C4365: 'argument': conversion from 'mz_uint' to 'int', signed/unsigned mismatch
// warning C4548: expression before comma has no effect; expected expression with side-effect
// warning C4625: 'miniz_cpp::zip_file': copy constructor was implicitly defined as deleted
// warning C4626: 'miniz_cpp::zip_file': assignment operator was implicitly defined as deleted
// warning C4668: '__BYTE_ORDER__' is not defined as a preprocessor macro, replacing with '0' for '#if/#elif'
#pragma warning(push)
#pragma warning(disable: 4365 4548 4625 4626 4668)
#include "miniz-cpp/zip_file.hpp"
#pragma warning(pop)

#else

#pragma GCC diagnostic push
#pragma GCC system_header
#include "miniz-cpp/zip_file.hpp"
#pragma GCC diagnostic pop

#endif
