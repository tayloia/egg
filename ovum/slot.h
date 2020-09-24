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
  class Slot : public SoftReferenceCounted<ICollectable> {
    Slot(const Slot& rhs) = delete;
    Slot& operator=(const Slot& rhs) = delete;
  private:
    Atomic<IValue*> ptr;
    virtual ~Slot();
  public:
    // Construction/destruction
    explicit Slot(IAllocator& allocator);
    Slot(Slot&& rhs) noexcept;
    // Atomic access
    Value getValue() const; // 'void' if empty
    IValue* getReference(); // nullptr if empty
    void clobber(const Value& value);
    bool update(IValue* expected, const Value& desired);
    Error mutate(const IType& type, Mutation mutation, const Value& value);
    void clear();
    // Debugging
    bool validate(bool optional) const;
    virtual bool validate() const override {
      return this->validate(true);
    }
    virtual void softVisitLinks(const Visitor&) const override;
  };
}
