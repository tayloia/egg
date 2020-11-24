namespace egg::ovum {
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
    static constexpr bool hasAllSet(T value, T bits) {
      auto a = static_cast<std::underlying_type_t<T>>(value);
      auto b = static_cast<std::underlying_type_t<T>>(bits);
      return (a & b) == b;
    }
    template<typename T>
    static constexpr bool hasAnySet(T value) {
      auto a = static_cast<std::underlying_type_t<T>>(value);
      return a != 0;
    }
    template<typename T>
    static constexpr bool hasAnySet(T value, T bits) {
      auto a = static_cast<std::underlying_type_t<T>>(value);
      auto b = static_cast<std::underlying_type_t<T>>(bits);
      return (a & b) != 0;
    }
    template<typename T>
    static constexpr bool hasOneSet(T value) {
      auto a = static_cast<std::underlying_type_t<T>>(value);
      return ((a & (a - 1)) == 0) && (a != 0);
    }
    template<typename T>
    static constexpr bool hasOneSet(T value, T bits) {
      auto a = static_cast<std::underlying_type_t<T>>(value);
      auto b = static_cast<std::underlying_type_t<T>>(bits);
      auto c = a & b;
      return ((c & (c - 1)) == 0) && (c != 0);
    }
    template<typename T>
    static constexpr bool hasZeroOrOneSet(T value) {
      auto a = static_cast<std::underlying_type_t<T>>(value);
      return (a & (a - 1)) == 0;
    }
    template<typename T>
    static constexpr bool hasZeroOrOneSet(T value, T bits) {
      auto a = static_cast<std::underlying_type_t<T>>(value);
      auto b = static_cast<std::underlying_type_t<T>>(bits);
      auto c = a & b;
      return (c & (c - 1)) == 0;
    }
    template<typename T>
    static constexpr T mask(T value, T bits) {
      auto a = static_cast<std::underlying_type_t<T>>(value);
      auto b = static_cast<std::underlying_type_t<T>>(bits);
      return static_cast<T>(a & b);
    }
    template<typename T>
    static constexpr T set(T value, T bits) {
      auto a = static_cast<std::underlying_type_t<T>>(value);
      auto b = static_cast<std::underlying_type_t<T>>(bits);
      return static_cast<T>(a | b);
    }
    template<typename T>
    static constexpr T clear(T value, T bits) {
      auto a = static_cast<std::underlying_type_t<T>>(value);
      auto b = static_cast<std::underlying_type_t<T>>(bits);
      return static_cast<T>(a & ~b);
    }
    template<typename T>
    static constexpr T invert(T value, T bits) {
      auto a = static_cast<std::underlying_type_t<T>>(value);
      auto b = static_cast<std::underlying_type_t<T>>(bits);
      return static_cast<T>(a ^ b);
    }
    template<typename T>
    static T topmost(T value) {
      auto a = static_cast<std::underlying_type_t<T>>(value);
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
}
