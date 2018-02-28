#define EGG_NO_COPY(type) \
  type(const type&) = delete; \
  type& operator=(const type&) = delete

#define EGG_THROW(message) \
  throw egg::yolk::Exception(message, __FILE__, __LINE__)