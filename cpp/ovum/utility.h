namespace egg::ovum {
  struct SourceLocation {
    size_t line = 0;
    size_t column = 0;
    bool empty() const {
      return (this->line == 0) && (this->column == 0);
    }
  };

  struct SourceRange {
    SourceLocation begin;
    SourceLocation end;
    bool empty() const {
      return this->begin.empty();
    }
  };

  template<typename T>
  class Atomic {
    Atomic(Atomic&) = delete;
    Atomic& operator=(Atomic&) = delete;
  public:
    using Underlying = T;
  private:
    std::atomic<Underlying> atomic;
  public:
    explicit Atomic(Underlying value) : atomic(value) {
    }
    Underlying get() const {
      // Get the current value
      return std::atomic_load(&this->atomic);
    }
    void set(Underlying desired) {
      // Atomically set the value
      std::atomic_store(&this->atomic, desired);
    }
    Underlying exchange(Underlying desired) {
      // Atomically swap the values returning the value BEFORE
      return std::atomic_exchange(&this->atomic, desired);
    }
    Underlying update(Underlying expected, Underlying desired) {
      // Atomically swap iff the current value is 'expected' returning the value BEFORE
      (void)std::atomic_compare_exchange_strong(&this->atomic, &expected, desired);
      return expected;
    }
    Underlying add(Underlying value) {
      // Return the value BEFORE the addition
      return std::atomic_fetch_add(&this->atomic, value);
    }
    Underlying sub(Underlying value) {
      // Return the value BEFORE the subtraction
      return std::atomic_fetch_sub(&this->atomic, value);
    }
    Underlying bitwiseAnd(Underlying value) {
      // Return the value BEFORE the operation
      return std::atomic_fetch_and(&this->atomic, value);
    }
    Underlying bitwiseOr(Underlying value) {
      // Return the value BEFORE the operation
      return std::atomic_fetch_or(&this->atomic, value);
    }
    Underlying bitwiseXor(Underlying value) {
      // Return the value BEFORE the operation
      return std::atomic_fetch_xor(&this->atomic, value);
    }
    Underlying increment() {
      // The result should be strictly positive
      auto result = this->add(1) + 1;
      assert(result > 0);
      return result;
    }
    Underlying decrement() {
      // The result should not be negative
      auto result = this->add(-1) - 1;
      assert(result >= 0);
      return result;
    }
  };

  class Bits {
  public:
    template<typename T>
    static constexpr std::underlying_type_t<T> underlying(T value) {
      return static_cast<std::underlying_type_t<T>>(value);
    }
    template<typename T>
    static constexpr bool hasAllSet(T value, T bits) {
      auto a = Bits::underlying(value);
      auto b = Bits::underlying(bits);
      return (a & b) == b;
    }
    template<typename T>
    static constexpr bool hasAnySet(T value) {
      auto a = Bits::underlying(value);
      return a != 0;
    }
    template<typename T>
    static constexpr bool hasAnySet(T value, T bits) {
      auto a = Bits::underlying(value);
      auto b = Bits::underlying(bits);
      return (a & b) != 0;
    }
    template<typename T>
    static constexpr bool hasOneSet(T value) {
      auto a = Bits::underlying(value);
      return ((a & (a - 1)) == 0) && (a != 0);
    }
    template<typename T>
    static constexpr bool hasOneSet(T value, T bits) {
      auto a = Bits::underlying(value);
      auto b = Bits::underlying(bits);
      auto c = a & b;
      return ((c & (c - 1)) == 0) && (c != 0);
    }
    template<typename T>
    static constexpr bool hasZeroOrOneSet(T value) {
      auto a = Bits::underlying(value);
      return (a & (a - 1)) == 0;
    }
    template<typename T>
    static constexpr bool hasZeroOrOneSet(T value, T bits) {
      auto a = Bits::underlying(value);
      auto b = Bits::underlying(bits);
      auto c = a & b;
      return (c & (c - 1)) == 0;
    }
    template<typename T>
    static constexpr bool hasNoneSet(T value) {
      auto a = Bits::underlying(value);
      return a == 0;
    }
    template<typename T>
    static constexpr bool hasNoneSet(T value, T bits) {
      auto a = Bits::underlying(value);
      auto b = Bits::underlying(bits);
      auto c = a & b;
      return c == 0;
    }
    template<typename T>
    static constexpr T mask(T value, T bits) {
      auto a = Bits::underlying(value);
      auto b = Bits::underlying(bits);
      return static_cast<T>(a & b);
    }
    template<typename T>
    static constexpr T set(T value, T bits) {
      auto a = Bits::underlying(value);
      auto b = Bits::underlying(bits);
      return static_cast<T>(a | b);
    }
    template<typename T>
    static constexpr T clear(T value, T bits) {
      auto a = Bits::underlying(value);
      auto b = Bits::underlying(bits);
      return static_cast<T>(a & ~b);
    }
    template<typename T>
    static constexpr T invert(T value, T bits) {
      auto a = Bits::underlying(value);
      auto b = Bits::underlying(bits);
      return static_cast<T>(a ^ b);
    }
    template<typename T>
    static T topmost(T value) {
      auto a = Bits::underlying(value);
      if (a <= 0) {
        return static_cast<T>(0);
      }
      auto b = 1;
      while ((a >>= 1) != 0) {
        b <<= 1;
      }
      return static_cast<T>(b);
    }
  };

  using ReadWriteMutex = std::shared_mutex;
  using WriteLock = std::unique_lock<ReadWriteMutex>;
  using ReadLock = std::shared_lock<ReadWriteMutex>;

  inline constexpr Modifiability operator|(Modifiability lhs, Modifiability rhs) {
    return Bits::set(lhs, rhs);
  }

  template<typename K, typename V>
  class OrderedMap {
    OrderedMap(const OrderedMap&) = delete;
    OrderedMap& operator=(const OrderedMap&) = delete;
  private:
    std::unordered_map<K, V> map;
    std::vector<K> vec; // insertion order
  public:
    OrderedMap() = default;
    void add(const K& key, V&& value) {
      // Add the new entry (asserts if extant)
      auto inserted = this->map.emplace(key, std::move(value));
      assert(inserted.second);
      this->vec.emplace_back(inserted.first->first);
    }
    bool insert(const K& key, V&& value) {
      // Try to add a new entry
      auto inserted = this->map.emplace(key, std::move(value));
      if (inserted) {
        this->vec.emplace_back(inserted.first->first);
      }
      return inserted;
    }
    const V* get(const K& key) const {
      // Fetch an entry by key
      auto found = this->map.find(key);
      if (found == this->map.end()) {
        return nullptr;
      }
      return &found->second;
    }
    const V* get(size_t index, K& key) const {
      // Fetch a key and entry by insertion order
      if (index >= this->vec.size()) {
        return nullptr;
      }
      key = this->vec[index];
      auto found = this->map.find(key);
      assert(found != this->map.end());
      return &found->second;
    }
    size_t size() const {
      // Fetch the number of entries
      return this->vec.size();
    }
  };

  class Hash {
    Hash(const Hash&) = delete;
    Hash& operator=(const Hash&) = delete;
  private:
    size_t seed;
  public:
    explicit Hash(size_t seed = 0)
      : seed(seed) {
    }
    operator size_t() const {
      return this->seed;
    }
    static void combine(uint32_t& dst, uint32_t src) {
      // See https://www.boost.org/doc/libs/1_83_0/libs/container_hash/doc/html/hash.html#notes
      dst ^= src + 0x9E3779B9 + (dst << 6) + (dst >> 2);
    }
    static void combine(uint64_t& dst, uint64_t src) {
      // See https://stackoverflow.com/a/4948967
      dst ^= src + 0x9E3779B97F4A7C15 + (dst << 6) + (dst >> 2);
    }
    Hash& add(size_t value) {
      Hash::combine(this->seed, value);
      return *this;
    }
    template<typename T>
    Hash& add(const T& value) {
      return this->add(Hash::hash(value));
    }
    template<typename T>
    Hash& add(T begin, T end) {
      while (begin != end) {
        this->add(*begin++);
      }
      return *this;
    }
    template<typename T>
    static size_t hash(const T* value) {
      // Note that pointers are hashed directly (identity)
      return reinterpret_cast<size_t>(value);
    }
    template<typename T>
    static std::enable_if_t<std::is_enum_v<T>, size_t> hash(T value) {
      // Enums are hashed according to numeric value
      return static_cast<size_t>(value);
    }
    template<typename T>
    static std::enable_if_t<std::is_member_function_pointer_v<decltype(&T::hash)>, size_t> hash(const T& value) {
      // Use member T::hash() if available
      return static_cast<size_t>(value.hash());
    }
  };
}
