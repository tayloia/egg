namespace egg::ovum {
  using Bool = bool;
  using Int = int64_t;
  using Float = double;

  // Forward declarations
  template<typename T> class HardPtr;
  template<typename T> class SoftPtr;
  enum class ValueFlags;
  struct LocationSource;
  class Printer;
  class String;
  class Type;
  class Value;
  class ICollectable;
  class IExecution;
  class IType;
  class ITypeFactory;

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
    struct Statistics {
      uint64_t currentBlocksOwned;
      uint64_t currentBytesOwned;
    };
    // Interface
    virtual ICollectable* take(const ICollectable& collectable) = 0;
    virtual void drop(const ICollectable& collectable) = 0;
    virtual size_t collect() = 0;
    virtual size_t purge() = 0;
    virtual bool statistics(Statistics& out) const = 0;
    virtual void print(Printer& printer) const = 0;
    // Helpers
    template<typename T>
    T* soften(const T* collectable) {
      if (collectable != nullptr) {
        auto* taken = this->take(*collectable);
        assert(taken != nullptr);
        taken->hardRelease();
        return static_cast<T*>(taken);
      }
      return nullptr;
    }
    bool verify(std::ostream& os, size_t minimum = 0, size_t maximum = SIZE_MAX);
  };

  class ICollectable : public IHardAcquireRelease {
  public:
    enum class SetBasketResult { Exempt, Unaltered, Altered, Failed };
    using Visitor = std::function<void(const ICollectable& target)>;
    // Interface
    virtual bool validate() const = 0;
    virtual bool softIsRoot() const = 0;
    virtual IBasket* softGetBasket() const = 0;
    virtual SetBasketResult softSetBasket(IBasket* desired) const = 0;
    virtual void softVisit(const Visitor& visitor) const = 0;
    virtual void print(Printer& printer) const = 0;
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

  class IType : public IHardAcquireRelease {
  public:
    virtual ValueFlags getPrimitiveFlags() const = 0;
    virtual std::pair<std::string, int> toStringPrecedence() const = 0;
    virtual String describeValue() const = 0;
  };

  class ITypeBuilder : public IHardAcquireRelease {
  public:
    virtual void addPositionalParameter(const Type& type, const String& name, bool optional) = 0;
    virtual void addNamedParameter(const Type& type, const String& name, bool optional) = 0;
    virtual void addVariadicParameter(const Type& type, const String& name, bool optional) = 0;
    virtual void addPredicateParameter(const Type& type, const String& name, bool optional) = 0;
    virtual void addProperty(const Type& type, const String& name, Modifiability modifiability) = 0;
    virtual void defineCallable(const Type& rettype, const Type& gentype) = 0;
    virtual void defineDotable(const Type& unknownType, Modifiability unknownModifiability) = 0;
    virtual void defineIndexable(const Type& resultType, const Type& indexType, Modifiability modifiability) = 0;
    virtual void defineIterable(const Type& resultType) = 0;
    virtual Type build() = 0;
  };
  using TypeBuilder = HardPtr<ITypeBuilder>;

  class ITypeFactory {
  public:
    // Interface
    virtual ~ITypeFactory() {}
    virtual IAllocator& getAllocator() const = 0;
    virtual Type createSimple(ValueFlags flags) = 0;
    virtual Type createPointer(const Type& pointee, Modifiability modifiability) = 0;
    virtual Type createArray(const Type& result, Modifiability modifiability) = 0;
    virtual Type createMap(const Type& result, const Type& index, Modifiability modifiability) = 0;
    virtual Type createUnion(const std::vector<Type>& types) = 0;
    virtual Type createIterator(const Type& element) = 0;
    virtual Type addVoid(const Type& type) = 0;
    virtual Type addNull(const Type& type) = 0;
    virtual Type removeVoid(const Type& type) = 0;
    virtual Type removeNull(const Type& type) = 0;
    virtual TypeBuilder createTypeBuilder(const String& name, const String& description) = 0;
    virtual TypeBuilder createFunctionBuilder(const Type& rettype, const String& name, const String& description) = 0;
    virtual TypeBuilder createGeneratorBuilder(const Type& gentype, const String& name, const String& description) = 0;

    virtual Type getVanillaArray() = 0;
    virtual Type getVanillaDictionary() = 0;
    virtual Type getVanillaError() = 0;
    virtual Type getVanillaKeyValue() = 0;
    virtual Type getVanillaPredicate() = 0;

    const IFunctionSignature* queryCallable(const Type& type);
    const IPropertySignature* queryDotable(const Type& type);
    const IIndexSignature* queryIndexable(const Type& type);
    const IIteratorSignature* queryIterable(const Type& type);
    const IPointerSignature* queryPointable(const Type& type);
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
  };
}
