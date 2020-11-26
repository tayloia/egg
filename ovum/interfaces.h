namespace egg::ovum {
  using Bool = bool;
  using Int = int64_t;
  using Float = double;

  // Forward declarations
  template<typename T> class Erratic;
  template<typename T> class HardPtr;
  enum class ValueFlags;
  struct LocationSource;
  struct NodeLocation;
  class Error;
  class Node;
  class Printer;
  class String;
  class StringBuilder;
  class Type;
  class Value;
  class ICollectable;
  class IExecution;
  class ISlot;
  class IValue;

  enum class Mutation {
    Assign,
    Noop,
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

  enum class Modifiability {
    None = 0,
    Read = 1 << 0,
    Write = 1 << 1,
    Mutate = 1 << 2,
    Delete = 1 << 3
  };

  class ILogger {
  public:
    enum class Source {
      Compiler = 1 << 0,
      Runtime = 1 << 1,
      User = 1 << 2
    };
    enum class Severity {
      None = 0,
      Debug = 1 << 0,
      Verbose = 1 << 1,
      Information = 1 << 2,
      Warning = 1 << 3,
      Error = 1 << 4
    };
    // Interface
    virtual ~ILogger() {}
    virtual void log(Source source, Severity severity, const std::string& message) = 0;
  };

  class IHardAcquireRelease {
  public:
    // Interface
    virtual ~IHardAcquireRelease() {}
    virtual IHardAcquireRelease* hardAcquire() const = 0;
    virtual void hardRelease() const = 0;
  };

  class IAllocator {
  public:
    struct Statistics {
      uint64_t totalBlocksAllocated;
      uint64_t totalBytesAllocated;
      uint64_t currentBlocksAllocated;
      uint64_t currentBytesAllocated;
    };
    // Interface
    virtual ~IAllocator() {}
    virtual void* allocate(size_t bytes, size_t alignment) = 0;
    virtual void deallocate(void* allocated, size_t alignment) = 0;
    virtual bool statistics(Statistics& out) const = 0;
    // Helpers
    template<typename T, typename... ARGS>
    T* create(size_t extra, ARGS&&... args) {
      // Use perfect forwarding to in-place new
      size_t bytes = sizeof(T) + extra;
      void* allocated = this->allocate(bytes, alignof(T));
      assert(allocated != nullptr);
      return new(allocated) T(std::forward<ARGS>(args)...);
    }
    template<typename T>
    void destroy(const T* allocated) {
      assert(allocated != nullptr);
      allocated->~T();
      this->deallocate(const_cast<T*>(allocated), alignof(T));
    }
    template<typename T, typename RETTYPE = HardPtr<T>, typename... ARGS>
    inline RETTYPE make(ARGS&&... args) {
      // Use perfect forwarding to in-place new
      void* allocated = this->allocate(sizeof(T), alignof(T));
      assert(allocated != nullptr);
      return RETTYPE(new(allocated) T(*this, std::forward<ARGS>(args)...));
    }
  };

  class IMemory : public IHardAcquireRelease {
  public:
    union Tag {
      uintptr_t u;
      void* p;
    };
    // Interface
    virtual const uint8_t* begin() const = 0;
    virtual const uint8_t* end() const = 0;
    virtual Tag tag() const = 0;
    // Helpers
    size_t bytes() const {
      return size_t(this->end() - this->begin());
    }
  };

  class IBasket : public IHardAcquireRelease {
  public:
    struct Statistics {
      uint64_t currentBlocksOwned;
      uint64_t currentBytesOwned;
    };
    // Interface
    virtual void take(ICollectable& collectable) = 0;
    virtual void drop(ICollectable& collectable) = 0;
    virtual size_t collect() = 0;
    virtual size_t purge() = 0;
    virtual bool statistics(Statistics& out) const = 0;
  };

  class ICollectable : public IHardAcquireRelease {
  public:
    using Visitor = std::function<void(ICollectable& target)>;
    // Interface
    virtual bool validate() const = 0;
    virtual bool softIsRoot() const = 0;
    virtual IBasket* softGetBasket() const = 0;
    virtual IBasket* softSetBasket(IBasket* basket) = 0;
    virtual bool softLink(ICollectable& target) = 0;
    virtual void softVisitLinks(const Visitor& visitor) const = 0;
  };

  class IParameters {
  public:
    // Interface
    virtual ~IParameters() {}
    virtual size_t getPositionalCount() const = 0;
    virtual Value getPositional(size_t index) const = 0;
    virtual const LocationSource* getPositionalLocation(size_t index) const = 0; // May return null
    virtual size_t getNamedCount() const = 0;
    virtual String getName(size_t index) const = 0;
    virtual Value getNamed(const String& name) const = 0;
    virtual const LocationSource* getNamedLocation(const String& name) const = 0; // May return null
  };

  class IFunctionSignatureParameter {
  public:
    enum class Flags {
      None = 0x00,
      Required = 0x01, // Not optional
      Variadic = 0x02, // Zero/one or more repetitions
      Predicate = 0x04 // Used in assertions
    };
    // Interface
    virtual ~IFunctionSignatureParameter() {}
    virtual String getName() const = 0; // May be empty
    virtual Type getType() const = 0;
    virtual size_t getPosition() const = 0; // SIZE_MAX if not positional
    virtual Flags getFlags() const = 0;
  };

  class IFunctionSignature {
  public:
    // Interface
    virtual ~IFunctionSignature() {}
    virtual String getFunctionName() const = 0;
    virtual Type getReturnType() const = 0;
    virtual size_t getParameterCount() const = 0;
    virtual const IFunctionSignatureParameter& getParameter(size_t index) const = 0;
  };

  class IIndexSignature {
  public:
    // Interface
    virtual ~IIndexSignature() {}
    virtual Type getResultType() const = 0;
    virtual Type getIndexType() const = 0;
    virtual Modifiability getModifiability() const = 0;
  };

  class IIteratorSignature {
  public:
    // Interface
    virtual ~IIteratorSignature() {}
    virtual Type getType() const = 0;
  };

  class IPropertySignature {
  public:
    // Interface
    virtual ~IPropertySignature() {}
    virtual Type getType(const String& property) const = 0;
    virtual Modifiability getModifiability(const String& property) const = 0;
    virtual String getName(size_t index) const = 0;
    virtual size_t getNameCount() const = 0;
    virtual bool isClosed() const = 0;
  };

  struct IntShape {
    static constexpr Int Minimum = std::numeric_limits<Int>::min();
    static constexpr Int Maximum = std::numeric_limits<Int>::max();
    Int minimum;
    Int maximum;
  };

  struct FloatShape {
    static constexpr Float Minimum = std::numeric_limits<Float>::min();
    static constexpr Float Maximum = std::numeric_limits<Float>::max();
    Float minimum;
    Float maximum;
    bool allowNaN;
    bool allowNInf;
    bool allowPInf;
  };

  struct ObjectShape {
    const IFunctionSignature* callable;
    const IPropertySignature* dotable;
    const IIndexSignature* indexable;
    const IIteratorSignature* iterable;
  };

  class IType : public ICollectable {
  public:
    virtual ValueFlags getFlags() const = 0;
    virtual const IntShape* getIntShape() const = 0;
    virtual const FloatShape* getFloatShape() const = 0;
    virtual const ObjectShape* getStringShape() const = 0;
    virtual const ObjectShape* getObjectShape(size_t index) const = 0;
    virtual size_t getObjectShapeCount() const = 0;
    virtual std::pair<std::string, int> toStringPrecedence() const = 0;
    virtual String describeValue() const = 0;
  };

  class IObject : public ICollectable {
  public:
    // Interface
    virtual Type getRuntimeType() const = 0;
    virtual Value call(IExecution& execution, const IParameters& parameters) = 0;
    virtual Value getProperty(IExecution& execution, const String& property) = 0;
    virtual Value setProperty(IExecution& execution, const String& property, const Value& value) = 0;
    virtual Value mutProperty(IExecution& execution, const String& property, Mutation mutation, const Value& value) = 0;
    virtual Value delProperty(IExecution& execution, const String& property) = 0;
    virtual Value refProperty(IExecution& execution, const String& property) = 0;
    virtual Value getIndex(IExecution& execution, const Value& index) = 0;
    virtual Value setIndex(IExecution& execution, const Value& index, const Value& value) = 0;
    virtual Value mutIndex(IExecution& execution, const Value& index, Mutation mutation, const Value& value) = 0;
    virtual Value delIndex(IExecution& execution, const Value& index) = 0;
    virtual Value refIndex(IExecution& execution, const Value& index) = 0;
    virtual Value getPointee(IExecution& execution) = 0;
    virtual Value setPointee(IExecution& execution, const Value& value) = 0;
    virtual Value mutPointee(IExecution& execution, Mutation mutation, const Value& value) = 0;
    virtual Value iterate(IExecution& execution) = 0;
    virtual void print(Printer& printer) const = 0;
  };
}
