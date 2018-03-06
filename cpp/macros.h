#define EGG_NO_COPY(type) \
  type(const type&) = delete; \
  type& operator=(const type&) = delete

#define EGG_THROW(message) \
  throw egg::yolk::Exception(message, __FILE__, __LINE__)

namespace egg::yolk {
  template<typename T, size_t N> char(*nelemsHelper(T(&)[N]))[N] {}
}
#define EGG_NELEMS(arr) (sizeof(*egg::yolk::nelemsHelper(arr)))
