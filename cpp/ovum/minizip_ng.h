#if EGG_PLATFORM == EGG_PLATFORM_MSVC

// warning C4127: conditional expression is constant
// warning C4189: '...': local variable is initialized but not referenced
// warning C4191: 'type cast': unsafe conversion from 'FARPROC' to 'LPCREATESYMBOLICLINKW'
// warning C4365: '=': conversion from 'int' to '...', signed/unsigned mismatch
// warning C4668 : '...' is not defined as a preprocessor macro, replacing with '0' for '#if/#elif'
// warning C4706: assignment within conditional expression
// error C4996: 'strncpy': This function or variable may be unsafe. Consider using strncpy_s instead.
#pragma warning(push)
#pragma warning(disable: 4127 4189 4191 4365 4668 4706 4996)

// mz_zip.c(2049): error C2664: 'uint32_t mz_crypt_crc32_update(uint32_t,const uint8_t *,int32_t)': cannot convert argument 2 from 'void *' to 'const uint8_t *'
#include "minizip-ng/mz_crypt.c"
#define mz_crypt_crc32_update(v, b, s) mz_crypt_crc32_update(v, static_cast<const uint8_t*>(b), s)
#include "minizip-ng/mz_os.c"
#include "minizip-ng/mz_os_win32.c"
#include "minizip-ng/mz_strm.c"
#include "minizip-ng/mz_strm_mem.c"
#include "minizip-ng/mz_strm_os_win32.c"
#include "minizip-ng/mz_zip.c"

#pragma warning(pop)

#else

// TODO: The following could be genuine errors in the minizip-ng code
#pragma GCC diagnostic ignored "-Wrestrict"
#pragma GCC diagnostic ignored "-Wstringop-truncation"
#pragma GCC diagnostic ignored "-Wstringop-overflow"
#pragma GCC diagnostic ignored "-Wunused-parameter"

// mz_zip.c:2049: error: invalid conversion from 'void*' to 'const uint8_t*'
#include "minizip-ng/mz_crypt.c"
#define mz_crypt_crc32_update(v, b, s) mz_crypt_crc32_update(v, static_cast<const uint8_t*>(b), s)
#include "minizip-ng/mz_os.c"
#include "minizip-ng/mz_os_posix.c"
#include "minizip-ng/mz_strm.c"
#include "minizip-ng/mz_strm_mem.c"
#include "minizip-ng/mz_strm_os_posix.c"
#include "minizip-ng/mz_zip.c"

#endif
