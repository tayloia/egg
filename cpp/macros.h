#if !defined(__has_cpp_attribute)
#define EGG_FALLTHROUGH
#elif !__has_cpp_attribute(fallthrough)
#define EGG_FALLTHROUGH
#else
#define EGG_FALLTHROUGH [[fallthrough]];
#endif

#if !defined(__has_cpp_attribute)
#define EGG_NORETURN
#elif !__has_cpp_attribute(fallthrough)
#define EGG_NORETURN
#else
#define EGG_NORETURN [[noreturn]]
#endif

#define EGG_NO_COPY(type) \
  type(const type&) = delete; \
  type& operator=(const type&) = delete

#define EGG_THROW(message) \
  throw egg::yolk::Exception(message, __FILE__, __LINE__)

namespace egg::yolk {
  template<typename T, size_t N> char(*nelemsHelper(T(&)[N]))[N] {}
}
#define EGG_NELEMS(arr) (sizeof(*egg::yolk::nelemsHelper(arr)))
