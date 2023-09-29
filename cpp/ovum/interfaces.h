namespace egg::ovum {
  using Bool = bool;
  using Int = int64_t;
  using Float = double;

  // Forward declarations
  template<typename T> class HardPtr;
  enum class ValueFlags;
  struct LocationSource;
  class Printer;
  class String;
  class Type;
  class HardValue;
  class ICollectable;
  class IMemory;
  class IValue;
  class IType;
  class ITypeFactory;
  class IVMExecution;

  enum class Modifiability {
    None = 0x00,
    Read = 0x1,
    Write = 0x2,
    Mutate = 0x4,
    Delete = 0x8,
    ReadWrite = Read | Write,
    ReadWriteMutate = Read | Write | Mutate,
    All = Read | Write | Mutate | Delete
  };

  enum class Mutation {
    Assign,
    Decrement,
    Increment,
    Add,
    Subtract,
    Multiply,
    Divide,
    Remainder,
    BitwiseAnd,
    BitwiseOr,
    BitwiseXor,
    ShiftLeft,
    ShiftRight,
    ShiftRightUnsigned,
    Noop // TODO: Split into Clone/Cancel?
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
    virtual void log(Source source, Severity severity, const String& message) = 0;
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
    template<typename T, typename... ARGS>
    inline T* makeRaw(ARGS&&... args) {
      // Use perfect forwarding to in-place new
      void* allocated = this->allocate(sizeof(T), alignof(T));
      assert(allocated != nullptr);
      return new(allocated) T(std::forward<ARGS>(args)...);
    }
    template<typename T, typename RETTYPE = HardPtr<T>, typename... ARGS>
    inline RETTYPE makeHard(ARGS&&... args) {
      // Use perfect forwarding
      return RETTYPE(this->makeRaw<T>(*this, std::forward<ARGS>(args)...));
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
    struct Statistics : public IAllocator::Statistics {
      uint64_t currentBlocksOwned;
    };
    // Interface
    virtual ICollectable* take(const ICollectable& collectable) = 0;
    virtual void drop(const ICollectable& collectable) = 0;
    virtual size_t collect() = 0;
    virtual size_t purge() = 0;
    virtual bool statistics(Statistics& out) const = 0;
    virtual void print(Printer& printer) const = 0;
    // Helpers
    bool verify(std::ostream& os, size_t minimum = 0, size_t maximum = SIZE_MAX);
  };

  class ICollectable : public IHardAcquireRelease {
  public:
    class IVisitor {
    public:
      virtual ~IVisitor() {}
      virtual void visit(const ICollectable& target) = 0;
    };
    enum class SetBasketResult { Exempt, Unaltered, Altered, Failed };
    // Interface
    virtual bool validate() const = 0;
    virtual bool softIsRoot() const = 0;
    virtual IBasket* softGetBasket() const = 0;
    virtual SetBasketResult softSetBasket(IBasket* desired) const = 0;
    virtual void softVisit(IVisitor& visitor) const = 0;
    virtual void print(Printer& printer) const = 0;
  };

  class ICallArguments {
  public:
    // Interface
    virtual ~ICallArguments() {}
    virtual size_t getArgumentCount() const = 0;
    virtual bool getArgumentByIndex(size_t index, HardValue& value, String* name = nullptr) const = 0;
  };

  class IObject : public ICollectable {
  public:
    // Interface
    virtual HardValue vmCall(IVMExecution& execution, const ICallArguments& arguments) = 0;
    virtual HardValue vmPropertyGet(IVMExecution& execution, const HardValue& property) = 0;
    virtual HardValue vmPropertySet(IVMExecution& execution, const HardValue& property, const HardValue& value) = 0;
  };

  class IParameters {
  public:
    // Interface
    virtual ~IParameters() {}
    virtual size_t getPositionalCount() const = 0;
    virtual HardValue getPositional(size_t index) const = 0;
    virtual const LocationSource* getPositionalLocation(size_t index) const = 0; // May return null
    virtual size_t getNamedCount() const = 0;
    virtual String getName(size_t index) const = 0;
    virtual HardValue getNamed(const String& name) const = 0;
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
    IFunctionSignatureParameter() = default;
    IFunctionSignatureParameter(const IFunctionSignatureParameter&) = default;
    virtual ~IFunctionSignatureParameter() {}
    virtual String getName() const = 0; // May be empty if positional
    virtual Type getType() const = 0;
    virtual size_t getPosition() const = 0; // SIZE_MAX if not positional
    virtual Flags getFlags() const = 0;
  };

  class IFunctionSignature {
  public:
    // Interface
    virtual ~IFunctionSignature() {}
    virtual String getName() const = 0;
    virtual Type getReturnType() const = 0;
    virtual size_t getParameterCount() const = 0;
    virtual const IFunctionSignatureParameter& getParameter(size_t index) const = 0;
    virtual Type getGeneratorType() const = 0;
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

  class IPointerSignature {
  public:
    // Interface
    virtual ~IPointerSignature() {}
    virtual Type getType() const = 0;
    virtual Modifiability getModifiability() const = 0;
  };

  class IType : public ICollectable {
  public:
    virtual ValueFlags getPrimitiveFlags() const = 0;
    virtual std::pair<std::string, int> toStringPrecedence() const = 0; // TODO remove?
  };
}
