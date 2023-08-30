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
    Atomic<IValue*> ptr; // SoftPtr
  public:
    // Construction/destruction
    Slot(IAllocator& allocator, IBasket& basket);
    Slot(IAllocator& allocator, IBasket& basket, const Value& value);
    virtual ~Slot();
    // Atomic access
    virtual IValue* get() const override; // nullptr if empty
    virtual void set(const Value& value) override;
    virtual bool update(IValue* expected, const Value& desired) override;
    virtual void clear() override;
    // Helpers
    static Type::Assignment mutate(ISlot& slot, IAllocator& allocator, const Type& type, Mutation mutation, const Value& value, Value& before);
    Type::Assignment mutate(const Type& type, Mutation mutation, const Value& value, Value& before);
    Value value(const Value& empty) const;
    Value reference(ITypeFactory& factory, IBasket& basket, const Type& pointee, Modifiability modifiability);
    // Debugging
    bool validate(bool optional) const;
    virtual bool validate() const override {
      return this->validate(true);
    }
    virtual void softVisit(const Visitor&) const override;
    virtual void print(Printer& printer) const override;
  };

  class SlotArray {
    SlotArray(const SlotArray&) = delete;
    SlotArray& operator=(const SlotArray&) = delete;
  private:
    std::vector<SoftPtr<Slot>> vec; // Address stable
  public:
    explicit SlotArray(size_t size);;
    bool empty() const;
    size_t length() const;
    Slot* get(size_t index) const;
    Slot* set(IAllocator& allocator, IBasket& basket, size_t index, const Value& value);
    void resize(IAllocator& allocator, IBasket& basket, size_t size, const Value& fill);
    void foreach(std::function<void(const Slot* slot)> visitor) const;
    void softVisit(const ICollectable::Visitor& visitor) const;
  };

  template<typename K>
  class SlotMap {
    SlotMap(const SlotMap&) = delete;
    SlotMap& operator=(const SlotMap&) = delete;
  private:
    std::unordered_map<K, SoftPtr<Slot>> map;
    std::vector<K> vec; // In insertion order
  public:
    SlotMap() {
    }
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
    Slot* lookup(size_t idx, K& key) const {
      // Returns the address of the slot or nullptr if not present
      if (idx < this->vec.size()) {
        key = this->vec[idx];
        auto found = this->map.find(key);
        assert(found != this->map.end());
        return found->second.get();
      }
      return nullptr;
    }
    Slot* find(const K& key) const {
      // Returns the address of the slot or nullptr if not present
      auto found = this->map.find(key);
      if (found != this->map.end()) {
        return found->second.get();
      }
      return nullptr;
    }
    bool add(IAllocator& allocator, IBasket& basket, const K& key, const Value& value) {
      // Add a new slot (returns 'true' iff added)
      auto inserted = this->map.emplace(std::piecewise_construct,
                                        std::forward_as_tuple(key),
                                        std::forward_as_tuple(basket, allocator.makeRaw<Slot>(allocator, basket, value)));
      if (inserted.second) {
        this->vec.emplace_back(key);
        return true;
      }
      return false;
    }
    bool set(IAllocator& allocator, IBasket& basket, const K& key, const Value& value) {
      // Updates or adds a new slot (returns 'true' iff added)
      auto found = this->map.find(key);
      if (found != this->map.end()) {
        found->second->set(value);
        return false;
      }
      // Add a new slot
      this->map.emplace(std::piecewise_construct,
                        std::forward_as_tuple(key),
                        std::forward_as_tuple(basket, allocator.makeRaw<Slot>(allocator, basket, value)));
      this->vec.emplace_back(key);
      return true;
    }
    bool remove(const K& key) {
      // Remove a single slot
      auto erased = this->map.erase(key);
      assert(erased <= 1);
      if (erased == 1) {
        this->vec.erase(std::remove(this->vec.begin(), this->vec.end(), key), this->vec.end());
        assert(this->map.size() == this->vec.size());
        return true;
      }
      return false;
    }
    void removeAll() {
      this->map.clear();
      this->vec.clear();
    }
    void foreach(std::function<void(const K& key, const Slot& slot)> visitor) const {
      // Iterate in insertion order
      for (auto& key : this->vec) {
        visitor(key, *this->map.at(key));
      }
    }
    void visit(const ICollectable::Visitor& visitor) const {
      // Iterate through everything as fast as possible
      for (auto& kv : this->map) {
        kv.second.visit(visitor);
      }
    }
  };

  class SlotFactory {
  public:
    static ISlot& createSlot(IAllocator& allocator, IBasket& basket);
    static ISlot& createSlot(IAllocator& allocator, IBasket& basket, const Value& value);
  };
}
