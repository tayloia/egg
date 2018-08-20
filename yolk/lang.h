namespace egg::lang {
  // Forward declarations
  class ValueLegacyReferenceCounted;

  class ValueLegacy {
    // Stop type promotion for implicit constructors
    template<typename T> ValueLegacy(T rhs) = delete;
  private:
    using Tag = egg::ovum::VariantBits;
    Tag tag;
    union {
      bool b;
      int64_t i;
      double f;
      egg::ovum::IObject* o;
      const egg::ovum::IMemory* s;
      ValueLegacyReferenceCounted* v;
    };
    explicit ValueLegacy(Tag tag) : tag(tag) {}
    void copyInternals(const ValueLegacy& other);
    void moveInternals(ValueLegacy& other);
    void destroyInternals();
  public:
    ValueLegacy() : tag(Tag::Void) { this->v = nullptr; }
    ValueLegacy(const ValueLegacy& value);
    ValueLegacy(ValueLegacy&& value) noexcept;
    explicit ValueLegacy(std::nullptr_t) : tag(Tag::Null) { this->v = nullptr; }
    explicit ValueLegacy(bool value) : tag(Tag::Bool) { this->b = value; }
    explicit ValueLegacy(int64_t value) : tag(Tag::Int) { this->i = value; }
    explicit ValueLegacy(double value) : tag(Tag::Float) { this->f = value; }
    explicit ValueLegacy(const egg::ovum::String& value) : tag(Tag::String) { this->s = value.hardAcquire(); }
    explicit ValueLegacy(const egg::ovum::Object& value) : tag(Tag::Object) { this->o = value.hardAcquire(); }
    explicit ValueLegacy(const ValueLegacyReferenceCounted& vrc);
    ValueLegacy& operator=(const ValueLegacy& value);
    ValueLegacy& operator=(ValueLegacy&& value) noexcept;
    ~ValueLegacy();
    bool operator==(const ValueLegacy& other) const { return ValueLegacy::equals(*this, other); }
    bool operator!=(const ValueLegacy& other) const { return !ValueLegacy::equals(*this, other); }
    bool hasBasal(egg::ovum::BasalBits bits) const { return egg::ovum::Bits::hasAnySet(this->tag, static_cast<Tag>(bits)); }
    bool hasArithmetic() const { return egg::ovum::Bits::hasAnySet(this->tag, Tag::Arithmetic); }
    bool isVoid() const { return this->tag == Tag::Void; }
    bool isBreak() const { return this->tag == Tag::Break; }
    bool isContinue() const { return this->tag == Tag::Continue; }
    const ValueLegacy& direct() const;
    ValueLegacy& direct();
    bool hasIndirect() const { return egg::ovum::Bits::hasAnySet(this->tag, Tag::Indirect); }
    ValueLegacyReferenceCounted& indirect(egg::ovum::IAllocator& allocator);
    ValueLegacy& soft(egg::ovum::ICollectable& container);
    bool hasNull() const { return egg::ovum::Bits::hasAnySet(this->tag, Tag::Null); }
    bool isNull() const { return this->tag == Tag::Null; }
    bool hasBool() const { return egg::ovum::Bits::hasAnySet(this->tag, Tag::Bool); }
    bool isBool() const { return this->tag == Tag::Bool; }
    bool getBool() const { assert(this->hasBool()); return this->b; }
    bool hasInt() const { return egg::ovum::Bits::hasAnySet(this->tag, Tag::Int); }
    bool isInt() const { return this->tag == Tag::Int; }
    int64_t getInt() const { assert(this->hasInt()); return this->i; }
    bool hasFloat() const { return egg::ovum::Bits::hasAnySet(this->tag, Tag::Float); }
    bool isFloat() const { return this->tag == Tag::Float; }
    double getFloat() const { assert(this->hasFloat()); return this->f; }
    bool hasString() const { return egg::ovum::Bits::hasAnySet(this->tag, Tag::String); }
    bool isString() const { return this->tag == Tag::String; }
    egg::ovum::String getString() const { assert(this->hasString()); return egg::ovum::String(this->s); }
    bool hasObject() const { return egg::ovum::Bits::hasAnySet(this->tag, Tag::Object); }
    egg::ovum::Object getObject() const { assert(this->hasObject()); assert(this->o != nullptr); return egg::ovum::Object(*this->o); }
    bool hasReference() const { return egg::ovum::Bits::hasAnySet(this->tag, Tag::Indirect | Tag::Pointer); }
    bool hasPointer() const { return egg::ovum::Bits::hasAnySet(this->tag, Tag::Pointer); }
    ValueLegacyReferenceCounted& getPointee() const { assert(this->hasPointer() && !this->hasObject()); return *this->v; }
    bool hasAny() const { return egg::ovum::Bits::hasAnySet(this->tag, Tag::Any); }
    bool isSoftReference() const { return this->tag == (Tag::Object | Tag::Pointer); }
    void softVisitLink(const egg::ovum::ICollectable::Visitor& visitor) const;
    bool hasThrow() const { return egg::ovum::Bits::hasAnySet(this->tag, Tag::Throw); }
    bool isRethrow() const { return this->tag == (Tag::Throw | Tag::Void); }
    bool hasFlowControl() const { return egg::ovum::Bits::hasAnySet(this->tag, Tag::FlowControl); }
    void addFlowControl(Tag bits);
    bool stripFlowControl(Tag bits);
    static std::string getBasalString(egg::ovum::BasalBits basal);
    static bool equals(const ValueLegacy& lhs, const ValueLegacy& rhs);
    egg::ovum::String toString() const;
    std::string toUTF8() const;
    egg::ovum::ITypeRef getRuntimeType() const;

    // Constants
    static const ValueLegacy Void;
    static const ValueLegacy Null;
    static const ValueLegacy False;
    static const ValueLegacy True;
    static const ValueLegacy Break;
    static const ValueLegacy EmptyString;
    static const ValueLegacy Continue;
    static const ValueLegacy Rethrow;
    static const ValueLegacy ReturnVoid;

    // Built-ins
    static ValueLegacy builtinString(egg::ovum::IAllocator& allocator);
    static ValueLegacy builtinType(egg::ovum::IAllocator& allocator);
    static ValueLegacy builtinAssert(egg::ovum::IAllocator& allocator);
    static ValueLegacy builtinPrint(egg::ovum::IAllocator& allocator);

    // Factories
    template<typename T, typename... ARGS>
    static ValueLegacy makeObject(egg::ovum::IAllocator& allocator, ARGS&&... args) {
      // Use perfect forwarding
      return ValueLegacy(egg::ovum::Object(*allocator.make<T>(std::forward<ARGS>(args)...)));
    }
  };

  class ValueLegacyReferenceCounted : public egg::ovum::IHardAcquireRelease, public ValueLegacy {
    ValueLegacyReferenceCounted(const ValueLegacyReferenceCounted&) = delete;
    ValueLegacyReferenceCounted& operator=(const ValueLegacyReferenceCounted&) = delete;
  protected:
    explicit ValueLegacyReferenceCounted(ValueLegacy&& value) noexcept : ValueLegacy(std::move(value)) {}
  };
}

template<typename... ARGS>
inline void egg::ovum::IPreparation::raiseWarning(ARGS... args) {
  auto message = StringBuilder::concat(args...);
  this->raise(ILogger::Severity::Warning, message);
}

template<typename... ARGS>
inline void egg::ovum::IPreparation::raiseError(ARGS... args) {
  auto message = StringBuilder::concat(args...);
  this->raise(ILogger::Severity::Error, message);
}

template<typename... ARGS>
inline egg::lang::ValueLegacy egg::ovum::IExecution::raiseFormat(ARGS... args) {
  auto message = StringBuilder::concat(args...);
  return this->raise(message);
}
