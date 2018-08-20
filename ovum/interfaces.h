// WIBBLE retire
namespace egg::lang {
  class ValueLegacy;
  class IParameters;
  class IFunctionSignature;
  class IIndexSignature;
}

namespace egg::ovum {
  // WIBBLE
  enum class Basal;
  class IExecution;
  class String;
  class Variant;

  // Forward declarations
  template<typename T> class HardPtr;
  class ICollectable;
  class IType;

  // Useful aliases
  using ITypeRef = HardPtr<const IType>;

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

    template<typename T, typename... ARGS>
    inline HardPtr<T> make(ARGS&&... args);
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

  class IType : public IHardAcquireRelease {
  public:
    enum class AssignmentSuccess { Never, Sometimes, Always };
    virtual std::pair<std::string, int> toStringPrecedence() const = 0;
    virtual AssignmentSuccess canBeAssignedFrom(const IType& rhs) const = 0;

    virtual egg::lang::ValueLegacy promoteAssignment(IExecution& execution, const egg::lang::ValueLegacy& rhs) const; // WIBBLE IExecution? // Default implementation calls IType::canBeAssignedFrom()
    virtual const egg::lang::IFunctionSignature* callable() const; // Default implementation returns nullptr
    virtual const egg::lang::IIndexSignature* indexable() const; // Default implementation returns nullptr
    virtual bool dotable(const String* property, ITypeRef& type, String& reason) const; // Default implementation returns false
    virtual bool iterable(ITypeRef& type) const; // Default implementation returns false
    virtual Basal getBasalTypes() const; // Default implementation returns 'Object'
    virtual ITypeRef pointerType() const; // Default implementation returns 'Type*'
    virtual ITypeRef pointeeType() const; // Default implementation returns 'Void'
    virtual ITypeRef denulledType() const; // Default implementation returns 'Void'
    virtual ITypeRef unionWith(const IType& other) const; // Default implementation calls Type::makeUnion()

    // Helpers
    String toString(int precedence = -1) const; // WIBBLE
  };

  class IObject : public ICollectable {
  public:
    virtual egg::lang::ValueLegacy toString() const = 0;
    virtual ITypeRef getRuntimeType() const = 0;
    virtual egg::lang::ValueLegacy call(IExecution& execution, const egg::lang::IParameters& parameters) = 0;
    virtual egg::lang::ValueLegacy getProperty(IExecution& execution, const String& property) = 0;
    virtual egg::lang::ValueLegacy setProperty(IExecution& execution, const String& property, const egg::lang::ValueLegacy& value) = 0;
    virtual egg::lang::ValueLegacy getIndex(IExecution& execution, const egg::lang::ValueLegacy& index) = 0;
    virtual egg::lang::ValueLegacy setIndex(IExecution& execution, const egg::lang::ValueLegacy& index, const egg::lang::ValueLegacy& value) = 0;
    virtual egg::lang::ValueLegacy iterate(IExecution& execution) = 0;
  };
}
