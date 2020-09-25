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
    Slot(const Slot& rhs) = delete;
    Slot& operator=(const Slot& rhs) = delete;
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
}
