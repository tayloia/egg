namespace egg::ovum {
  enum class Mutation {
    Assign,
    Decrement,
    Increment,
    Add,
    BitwiseAnd,
    BitwiseOr,
    BitwiseXor,
    Divide,
    IfNull,
    LogicalAnd,
    LogicalOr,
    Multiply,
    Remainder,
    ShiftLeft,
    ShiftRight,
    ShiftRightUnsigned,
    Subtract
  };

  class ISlot : public ICollectable {
  public:
    virtual ~ISlot() {}
    virtual Value getValue() const = 0; // 'void' if empty
    virtual const IValue* getReference() const = 0; // nullptr if empty
    virtual IValue* getReference() = 0; // nullptr if empty
    virtual void assign(const Value& desired) = 0;
    virtual bool update(IValue* expected, const Value& desired) = 0;
    virtual Error mutate(const IType& type, Mutation mutation, const Value& value) = 0;
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
    Slot(Slot&& rhs) noexcept;
    virtual ~Slot();
    // Atomic access
    virtual Value getValue() const override; // 'void' if empty
    virtual const IValue* getReference() const override; // nullptr if empty
    virtual IValue* getReference() override; // nullptr if empty
    virtual void assign(const Value& desired) override;
    virtual bool update(IValue* expected, const Value& desired) override;
    virtual Error mutate(const IType& type, Mutation mutation, const Value& value) override;
    virtual void clear() override;
    void clobber(const Value& value);
    // Debugging
    bool validate(bool optional) const;
    virtual bool validate() const override {
      return this->validate(true);
    }
    virtual void softVisitLinks(const Visitor&) const override;
  };
}
