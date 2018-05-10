namespace egg::yolk {
  // See https://docs.oracle.com/javase/8/docs/api/java/util/Map.html
  template<typename K, typename V>
  class Dictionary {
  public:
    typedef std::vector<K> Keys;
    typedef std::vector<V> Values;
    typedef std::vector<std::pair<K, V>> KeyValues;
  private:
    std::unordered_map<K, V> map;
    std::vector<K> vec; // In insertion order
  public:
    bool isEmpty() const {
      return this->vec.empty();
    }
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
    bool contains(const K& key) const {
      // Returns true iff the value exists
      return this->map.count(key) > 0;
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
    size_t getValues(Values& values) const {
      // Copy the values in key-insertion order
      assert(this->map.size() == this->vec.size());
      values.clear();
      values.reserve(this->vec.size());
      std::transform(this->vec.begin(), this->vec.end(), std::back_inserter(values), [=](const auto& k) {
        return this->map.at(k);
      });
      assert(values.size() == this->vec.size());
      return values.size();
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
