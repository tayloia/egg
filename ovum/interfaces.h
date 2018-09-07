namespace egg::ovum {
  // Forward declarations
  template<typename T> class HardPtr;
  enum class BasalBits;
  struct LocationSource;
  struct NodeLocation;
  class Node;
  class String;
  class Type;
  class Variant;
  class ICollectable;
  class IExecution;

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
    virtual Variant getPositional(size_t index) const = 0;
    virtual const LocationSource* getPositionalLocation(size_t index) const = 0; // May return null
    virtual size_t getNamedCount() const = 0;
    virtual String getName(size_t index) const = 0;
    virtual Variant getNamed(const String& name) const = 0;
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
  };

  class IType : public IHardAcquireRelease {
  public:
    // Type management
    virtual Variant tryAssign(IExecution& execution, Variant& lvalue, const egg::ovum::Variant& rvalue) const = 0;
    virtual bool hasBasalType(BasalBits basal) const = 0;

    // no-man's land
    enum class AssignmentSuccess { Never, Sometimes, Always };
    virtual AssignmentSuccess canBeAssignedFrom(const IType& rhs) const = 0;
    virtual const IFunctionSignature* callable() const = 0;
    virtual const IIndexSignature* indexable() const = 0;
    virtual Type dotable(const String* property, String& error) const = 0;
    virtual Type iterable() const = 0;
    virtual Type pointeeType() const = 0;
    virtual Type devoidedType() const = 0;
    virtual Type denulledType() const = 0;
    virtual std::pair<std::string, int> toStringPrecedence() const = 0;

    // WIBBLE legacy type interface
    virtual BasalBits getBasalTypesLegacy() const = 0;
    virtual Type unionWithBasalLegacy(IAllocator& allocator, BasalBits other) const = 0; // May return nullptr
    virtual Node compile(IAllocator& allocator, const NodeLocation& location) const = 0;
  };

  class IObject : public ICollectable {
  public:
    // Interface
    virtual Variant toString() const = 0;
    virtual Type getRuntimeType() const = 0;
    virtual Variant call(IExecution& execution, const IParameters& parameters) = 0;
    virtual Variant getProperty(IExecution& execution, const String& property) = 0;
    virtual Variant setProperty(IExecution& execution, const String& property, const Variant& value) = 0;
    virtual Variant getIndex(IExecution& execution, const Variant& index) = 0;
    virtual Variant setIndex(IExecution& execution, const Variant& index, const Variant& value) = 0;
    virtual Variant iterate(IExecution& execution) = 0;
  };
}
