namespace egg::ovum {
  // WIBBLE all interfaces should be pure

  // Forward declarations
  template<typename T> class HardPtr;
  enum class BasalBits;
  struct LocationSource;
  class String;
  class Type;
  class Variant;
  class ICollectable;
  class IExecution;
  class IType;

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
    virtual ~ILogger() {}
    virtual void log(Source source, Severity severity, const std::string& message) = 0;
  };

  class IHardAcquireRelease {
  public:
    virtual ~IHardAcquireRelease() {}
    virtual IHardAcquireRelease* hardAcquireBase() const = 0;
    virtual void hardRelease() const = 0;
    template<typename T>
    T* hardAcquire() const {
      return static_cast<T*>(this->hardAcquireBase());
    }
  };

  class IAllocator {
  public:
    struct Statistics {
      uint64_t totalBlocksAllocated;
      uint64_t totalBytesAllocated;
      uint64_t currentBlocksAllocated;
      uint64_t currentBytesAllocated;
    };

    virtual void* allocate(size_t bytes, size_t alignment) = 0;
    virtual void deallocate(void* allocated, size_t alignment) = 0;
    virtual bool statistics(Statistics& out) const = 0;

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
    inline RETTYPE make(ARGS&&... args);
  };

  class IMemory : public IHardAcquireRelease {
  public:
    union Tag {
      uintptr_t u;
      void* p;
    };
    virtual const uint8_t* begin() const = 0;
    virtual const uint8_t* end() const = 0;
    virtual Tag tag() const = 0;

    size_t bytes() const {
      return size_t(this->end() - this->begin());
    }
    static bool equals(const IMemory* lhs, const IMemory* rhs);
  };

  class IBasket : public IHardAcquireRelease {
  public:
    struct Statistics {
      uint64_t currentBlocksOwned;
      uint64_t currentBytesOwned;
    };

    virtual void take(ICollectable& collectable) = 0;
    virtual void drop(ICollectable& collectable) = 0;
    virtual size_t collect() = 0;
    virtual size_t purge() = 0;
    virtual bool statistics(Statistics& out) const = 0;
  };

  class ICollectable : public IHardAcquireRelease {
  public:
    using Visitor = std::function<void(ICollectable& target)>;
    virtual bool softIsRoot() const = 0;
    virtual IBasket* softGetBasket() const = 0;
    virtual IBasket* softSetBasket(IBasket* basket) = 0;
    virtual bool softLink(ICollectable& target) = 0;
    virtual void softVisitLinks(const Visitor& visitor) const = 0;
  };

  class IParameters {
  public:
    virtual ~IParameters() {}
    virtual size_t getPositionalCount() const = 0;
    virtual Variant getPositional(size_t index) const = 0;
    virtual const LocationSource* getPositionalLocation(size_t index) const = 0; // May be null
    virtual size_t getNamedCount() const = 0;
    virtual String getName(size_t index) const = 0;
    virtual Variant getNamed(const String& name) const = 0;
    virtual const LocationSource* getNamedLocation(const String& name) const = 0; // May be null
  };

  class IFunctionSignatureParameter {
  public:
    enum class Flags {
      None = 0x00,
      Required = 0x01, // Not optional
      Variadic = 0x02, // Zero/one or more repetitions
      Predicate = 0x04 // Used in assertions
    };
    virtual ~IFunctionSignatureParameter() {}
    virtual String getName() const = 0; // May be empty
    virtual Type getType() const = 0;
    virtual size_t getPosition() const = 0; // SIZE_MAX if not positional
    virtual Flags getFlags() const = 0;
  };

  class IFunctionSignature {
  public:
    enum class Parts {
      ReturnType = 0x01,
      FunctionName = 0x02,
      ParameterList = 0x04,
      ParameterNames = 0x08,
      NoNames = ReturnType | ParameterList,
      All = ~0
    };
    virtual ~IFunctionSignature() {}
    virtual String toString(Parts parts) const; // Calls buildStringDefault
    virtual String getFunctionName() const = 0; // May be empty
    virtual Type getReturnType() const = 0;
    virtual size_t getParameterCount() const = 0;
    virtual const IFunctionSignatureParameter& getParameter(size_t index) const = 0;
    virtual bool validateCall(IExecution& execution, const IParameters& runtime, Variant& problem) const; // Calls validateCallDefault

    // Implementation
    void buildStringDefault(class StringBuilder& sb, Parts parts) const; // Default formats as expected
    bool validateCallDefault(IExecution& execution, const IParameters& runtime, Variant& problem) const;
  };

  class IIndexSignature {
  public:
    virtual ~IIndexSignature() {}
    virtual String toString() const; // Default formats as expected
    virtual Type getResultType() const = 0;
    virtual Type getIndexType() const = 0;
  };

  class IType : public IHardAcquireRelease {
  public:
    // TODO new scheme for type management
    enum class AssignmentSuccess { Never, Sometimes, Always };
    virtual AssignmentSuccess canBeAssignedFrom(const IType& rhs) const = 0;
    virtual BasalBits getBasalTypes() const; // Default implementation returns 'Object'
    virtual Variant promoteAssignment(const Variant& rhs) const; // Default implementation calls IType::canBeAssignedFrom()
    virtual const IFunctionSignature* callable() const; // Default implementation returns nullptr
    virtual const IIndexSignature* indexable() const; // Default implementation returns nullptr
    virtual bool dotable(const String* property, Type& type, String& reason) const; // Default implementation returns false
    virtual bool iterable(Type& type) const; // Default implementation returns false
    virtual Type pointeeType() const; // Default implementation returns nullptr
    virtual Type denulledType() const; // Default implementation returns self
    virtual Type unionWith(IAllocator& allocator, const IType& other) const; // Default implementation calls Type::makeUnion()
    virtual std::pair<std::string, int> toStringPrecedence() const = 0;

    // Helpers
    String toString(int precedence = -1) const;
  };

  class IObject : public ICollectable {
  public:
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
