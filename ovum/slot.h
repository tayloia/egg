namespace egg::ovum {
  class ISlot : public ICollectable {
  public:
    virtual ~ISlot() {}
    virtual IValue* get() const = 0; // nullptr if empty
    virtual void set(const Value& value) = 0;
    virtual bool update(IValue* expected, const Value& desired) = 0;
    virtual void clear() = 0;
  };

  // TODO move to implementation .cpp
  class Slot : public SoftReferenceCounted<ISlot> {
    Slot(const Slot&) = delete;
    Slot& operator=(const Slot&) = delete;
  private:
    Atomic<IValue*> ptr;
  public:
    // Construction/destruction
    explicit Slot(IAllocator& allocator);
    Slot(IAllocator& allocator, const Value& value);
    Slot(Slot&& rhs) noexcept;
    virtual ~Slot();
    // Atomic access
    virtual IValue* get() const override; // nullptr if empty
    virtual void set(const Value& value) override;
    virtual bool update(IValue* expected, const Value& desired) override;
    virtual void clear() override;
    // Debugging
    bool validate(bool optional) const;
    virtual bool validate() const override {
      return this->validate(true);
    }
    virtual void softVisitLinks(const Visitor&) const override;
  };

  template<typename K>
  class SlotMap {
    SlotMap(const Slot&) = delete;
    SlotMap& operator=(const Slot&) = delete;
  private:
    std::unordered_map<K, Slot> map;
    std::vector<K> vec; // In insertion order
  public:
    SlotMap() = default;
    bool empty() const {
      return this->vec.empty();
    }
    size_t length() const {
      return this->vec.size();
    }
    bool contains(const K& key) const {
      // Returns true iff the value exists
      return this->map.count(key) > 0;
    }
    const Slot* getOrNull(const K& key) const {
      // Returns the address of the map value or nullptr if not present
      auto found = this->map.find(key);
      if (found != this->map.end()) {
        return &found->second;
      }
      return nullptr;
    }
    bool addOrUpdate(IAllocator& allocator, const K& key, const Value& value) {
      // Updates or adds a new slot
      auto found = this->map.find(key);
      if (found != this->map.end()) {
        found->second.set(value);
        return false;
      }
      // Add a new slot
      Slot slot(allocator, value);
      this->map.insert(std::move(std::make_pair(key, std::move(slot))));
      this->vec.emplace_back(key);
      return true;
    }
    void removeAll() {
      this->map.clear();
      this->vec.clear();
    }
    void foreach(std::function<void(const K& key, const Slot& slot)> visitor) const {
      // Iterate in insertion order
      for (auto& key : this->vec) {
        visitor(key, this->map.at(key));
      }
    }
  };
}
