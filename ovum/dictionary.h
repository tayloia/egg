#include <algorithm>

namespace egg::ovum {
  // See https://docs.oracle.com/javase/8/docs/api/java/util/Map.html
  template<typename K, typename V>
  class DictionaryUnordered {
    DictionaryUnordered(const DictionaryUnordered&) = delete;
    DictionaryUnordered& operator=(const DictionaryUnordered&) = delete;
  private:
    std::unordered_map<K, V> map;
  public:
    DictionaryUnordered() = default;
    bool empty() const {
      return this->map.empty();
    }
    size_t length() const {
      return this->map.size();
    }
    bool tryAdd(const K& key, const V& value) {
      // Returns true iff an insertion occurred
      return this->map.emplace(std::make_pair(key, value)).second;
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
      return this->map.erase(key) > 0;
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
      return this->map.insert_or_assign(key, value).second;
    }
    void addUnique(const K& key, const V& value) {
      // Asserts unless an insertion occurred
      bool inserted = this->map.insert_or_assign(key, value).second;
      assert(inserted);
      (void)inserted;
    }
    void emplaceUnique(const K& key, V&& value) {
      this->map.emplace(key, std::move(value));
    }
    void removeAll() {
      this->map.clear();
    }
    void foreach(std::function<void(const K& key, const V& value)> visitor) const {
      for (auto& entry : this->map) {
        visitor(entry.first, entry.second);
      }
    }
  };

  template<typename K, typename V>
  class Dictionary {
    Dictionary(const Dictionary&) = delete;
    Dictionary& operator=(const Dictionary&) = delete;
  public:
    typedef std::vector<K> Keys;
    typedef std::vector<V> Values;
    typedef std::vector<std::pair<K, V>> KeyValues;
  private:
    std::unordered_map<K, V> map;
    std::vector<K> vec; // In insertion order
  public:
    Dictionary() = default;
    bool empty() const {
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
    const V* getOrNull(const K& key) const {
      // Returns the address of the map value or nullptr if not present
      auto found = this->map.find(key);
      if (found != this->map.end()) {
        return &found->second;
      }
      return nullptr;
    }
    V getOrDefault(const K& key, const V& defval) const {
      // Returns the map value or a default if not present
      auto found = this->map.find(key);
      if (found != this->map.end()) {
        return found->second;
      }
      return defval;
    }
    std::pair<K, V> getByIndex(size_t index) const {
      // Return by insertion order index
      assert(index < this->vec.size());
      auto& key = this->vec.at(index);
      auto& value = this->map.at(key);
      return std::make_pair(key, value);
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
      std::transform(this->vec.begin(), this->vec.end(), std::back_inserter(values), [this](const auto& k) {
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
      std::transform(this->vec.begin(), this->vec.end(), std::back_inserter(keyvalues), [this](const auto& k) {
        // Wow, this is quite ugly, isn't it?!
        return *this->map.find(k);
      });
      assert(keyvalues.size() == this->vec.size());
      return keyvalues.size();
    }
    void removeAll() {
      this->map.clear();
      this->vec.clear();
    }
    void foreach(std::function<void(const K& key, const V& value)> visitor) const {
      // Iterate in insertion order
      for (auto& key : this->vec) {
        visitor(key, this->map.at(key));
      }
    }
  };
}
