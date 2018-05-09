namespace egg::yolk {
  // See https://docs.oracle.com/javase/8/docs/api/java/util/Map.html
  /*
      void clear()
        Removes all of the mappings from this map (optional operation).
      default V compute(K key, BiFunction<? super K,? super V,? extends V> remappingFunction)
        Attempts to compute a mapping for the specified key and its current mapped value (or null if there is no current mapping).
      default V computeIfAbsent(K key, Function<? super K,? extends V> mappingFunction)
        If the specified key is not already associated with a value (or is mapped to null), attempts to compute its value using the given mapping function and enters it into this map unless null.
      default V computeIfPresent(K key, BiFunction<? super K,? super V,? extends V> remappingFunction)
        If the value for the specified key is present and non-null, attempts to compute a new mapping given the key and its current mapped value.
      boolean containsKey(Object key)
        Returns true if this map contains a mapping for the specified key.
      boolean containsValue(Object value)
        Returns true if this map maps one or more keys to the specified value.
      Set<Map.Entry<K,V>> entrySet()
        Returns a Set view of the mappings contained in this map.
      boolean equals(Object o)
        Compares the specified object with this map for equality.
      default void forEach(BiConsumer<? super K,? super V> action)
        Performs the given action for each entry in this map until all entries have been processed or the action throws an exception.
      V get(Object key)
        Returns the value to which the specified key is mapped, or null if this map contains no mapping for the key.
      default V getOrDefault(Object key, V defaultValue)
        Returns the value to which the specified key is mapped, or defaultValue if this map contains no mapping for the key.
      int hashCode()
        Returns the hash code value for this map.
      boolean isEmpty()
        Returns true if this map contains no key-value mappings.
      Set<K> keySet()
        Returns a Set view of the keys contained in this map.
      default V merge(K key, V value, BiFunction<? super V,? super V,? extends V> remappingFunction)
        If the specified key is not already associated with a value or is associated with null, associates it with the given non-null value.
      V put(K key, V value)
        Associates the specified value with the specified key in this map (optional operation).
      void putAll(Map<? extends K,? extends V> m)
        Copies all of the mappings from the specified map to this map (optional operation).
      default V putIfAbsent(K key, V value)
        If the specified key is not already associated with a value (or is mapped to null) associates it with the given value and returns null, else returns the current value.
      V remove(Object key)
        Removes the mapping for a key from this map if it is present (optional operation).
      default boolean remove(Object key, Object value)
        Removes the entry for the specified key only if it is currently mapped to the specified value.
      default V replace(K key, V value)
        Replaces the entry for the specified key only if it is currently mapped to some value.
      default boolean replace(K key, V oldValue, V newValue)
        Replaces the entry for the specified key only if currently mapped to the specified value.
      default void replaceAll(BiFunction<? super K,? super V,? extends V> function)
        Replaces each entry's value with the result of invoking the given function on that entry until all entries have been processed or the function throws an exception.
      int size()
        Returns the number of key-value mappings in this map.
      Collection<V> values()
        Returns a Collection view of the values contained in this map.
  */
  template<typename K, typename V>
  class Dictionary {
  public:
    typedef std::vector<K> Keys;
    typedef std::vector<std::pair<K, V>> KeyValues;
  private:
    std::unordered_map<K, V> map;
    std::vector<K> vec; // In insertion order
  public:
    size_t length() const {
      return this->vec.size();
    }
    bool tryAdd(const K& key, const V& value) {
      // Returns true iff an insertion occurred
      bool inserted = this->map.emplace(std::make_pair(key, value)).second;
      if (inserted) {
        this->vec.emplace_back(key);
      }
      assert(this->map.size() == this->vec.size());
      return inserted;
    }
    bool tryGet(const K& key, V& value) const {
      // Returns true iff the value is present
      auto found = this->map.find(key);
      if (found != this->map.end()) {
        value = found->second;
        return true;
      }
      return false;
    }
    bool tryRemove(const K& key) {
      // Returns true iff the value was removed
      auto removed = this->map.erase(key) > 0;
      if (removed) {
        // Remove the key from the insertion order vector too
        this->vec.erase(std::remove(this->vec.begin(), this->vec.end(), key), this->vec.end());
      }
      assert(this->map.size() == this->vec.size());
      return removed;
    }
    V getOrDefault(const K& key, const V& defval) const {
      // Returns the map value or a default if not present
      auto found = this->map.find(key);
      if (found != this->map.end()) {
        return found->second;
      }
      return defval;
    }
    bool addOrUpdate(const K& key, const V& value) {
      // Returns true iff an insertion occurred
      bool inserted = this->map.insert_or_assign(key, value).second;
      if (inserted) {
        this->vec.emplace_back(key);
      }
      assert(this->map.size() == this->vec.size());
      return inserted;
    }
    size_t getKeys(Keys& keys) const {
      // Copy the keys in insertion order
      keys = this->vec;
      return keys.size();
    }
    size_t getKeyValues(KeyValues& keyvalues) const {
      // Copy the keys in insertion order
      assert(this->map.size() == this->vec.size());
      keyvalues.clear();
      keyvalues.reserve(this->vec.size());
      std::transform(this->vec.begin(), this->vec.end(), std::back_inserter(keyvalues), [=](const auto& k) {
        // Wow, this is quite ugly, isn't it?!
        return *this->map.find(k);
      });
      assert(keyvalues.size() == this->vec.size());
      return keyvalues.size();
    }
  };
}
