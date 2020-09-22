namespace egg::ovum {
  class Slot {
    // Stop type promotion for implicit constructors
    template<typename T> Slot(T rhs) = delete;
    Slot(const Slot& rhs) = delete;
    Slot& operator=(const Slot& rhs) = delete;
  private:
    Atomic<IValue*> ptr;
  public:
    // Construction/destruction
    Slot();
    Slot(const Value& rhs);
    Slot(Slot&& rhs) noexcept;
    ~Slot();
    // Atomic access
    Value get() const;
    void set(const Value& value);
    void clear();
    bool empty() const {
      return this->ptr.get() == nullptr;
    }
    // Garbage collection
    void softVisitLink(const ICollectable::Visitor& visitor) const;
    // Debugging and test
    bool validate(bool optional) const;
  };
}
