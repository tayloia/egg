namespace egg::ovum {
  using Bool = bool;
  using Int = int64_t;
  using Float = double;

  template<class T>
  class HardPtr {
  private:
    T * ptr;
  public:
    HardPtr() : ptr(nullptr) {
    }
    explicit HardPtr(const T* rhs) : ptr(HardPtr::hardAcquire(rhs)) {
    }
    HardPtr(const HardPtr& rhs) : ptr(rhs.hardAcquire()) {
    }
    HardPtr(HardPtr&& rhs) : ptr(rhs.get()) {
      rhs.ptr = nullptr;
    }
    template<typename U>
    HardPtr(const HardPtr<U>& rhs) : ptr(rhs.hardAcquire()) {
    }
    HardPtr& operator=(nullptr_t) {
      this->ptr->hardRelease();
      this->ptr = nullptr;
      return *this;
    }
    HardPtr& operator=(const HardPtr& rhs) {
      this->set(rhs.get());
      return *this;
    }
    HardPtr& operator=(HardPtr&& rhs) {
      if (this->ptr != nullptr) {
        this->ptr->hardRelease();
      }
      this->ptr = rhs.get();
      rhs.ptr = nullptr;
      return *this;
    }
    template<typename U>
    HardPtr& operator=(const HardPtr<U>& rhs) {
      this->set(rhs.get());
      return *this;
    }
    ~HardPtr() {
      if (this->ptr != nullptr) {
        this->ptr->hardRelease();
      }
    }
    T* hardAcquire() const {
      return HardPtr::hardAcquire(this->ptr);
    }
    T* get() const {
      return this->ptr;
    }
    void set(T* rhs) {
      auto* old = this->ptr;
      this->ptr = HardPtr::hardAcquire(rhs);
      if (old != nullptr) {
        old->hardRelease();
      }
    }
    T& operator*() const {
      assert(this->ptr != nullptr);
      return *this->ptr;
    }
    T* operator->() const {
      assert(this->ptr != nullptr);
      return this->ptr;
    }
    bool operator==(nullptr_t) const {
      return this->ptr == nullptr;
    }
    bool operator!=(nullptr_t) const {
      return this->ptr != nullptr;
    }
    static T* hardAcquire(const T* ptr) {
      if (ptr != nullptr) {
        return static_cast<T*>(ptr->hardAcquire());
      }
      return nullptr;
    }
  };
  template<typename T>
  bool operator==(nullptr_t, const HardPtr<T>& ptr) {
    // Yoda equality comparison used by GoogleTest
    return ptr == nullptr;
  }
  template<typename T>
  bool operator!=(nullptr_t, const HardPtr<T>& ptr) {
    // Yoda inequality comparison used by GoogleTest
    return ptr != nullptr;
  }

  using Memory = HardPtr<const IMemory>;

  class String : public Memory {
  public:
    explicit String(const IMemory* rhs = nullptr) : Memory(rhs) {
    }
    String(const char* rhs); // implicit; fallback to factory
    String(const std::string& rhs); // implicit; fallback to factory
    String(const std::string& rhs, size_t codepoints); // fallback to factory
    size_t length() const {
      auto* memory = this->get();
      if (memory == nullptr) {
        return 0;
      }
      return size_t(memory->tag().u);
    }
    std::string toUTF8() const {
      auto* memory = this->get();
      if (memory == nullptr) {
        return std::string();
      }
      return std::string(memory->begin(), memory->end());
    }
  };

  class Object : public HardPtr<IObject> {
  public:
    explicit Object(const IObject& rhs) : HardPtr(&rhs) {
      assert(this->get() != nullptr);
    }
  };

  // Helper for converting IEEE to/from mantissa/exponents
  struct MantissaExponent {
    static constexpr int64_t ExponentNaN = 1;
    static constexpr int64_t ExponentPositiveInfinity = 2;
    static constexpr int64_t ExponentNegativeInfinity = -2;
    int64_t mantissa;
    int64_t exponent;
    void fromFloat(Float f);
    Float toFloat() const;
  };

  struct UTF8 {
    inline static size_t sizeFromLeadByte(uint8_t lead) {
      // Fetch the size in bytes of a codepoint given just the lead byte
      if (lead < 0x80) {
        return 1;
      } else if (lead < 0xC0) {
        return SIZE_MAX; // Continuation byte
      } else if (lead < 0xE0) {
        return 2;
      } else if (lead < 0xF0) {
        return 3;
      } else if (lead < 0xF8) {
        return 4;
      }
      return SIZE_MAX;
    }
    inline static size_t measure(const uint8_t* p, const uint8_t* q) {
      // Return SIZE_MAX if this is not valid UTF-8, otherwise the codepoint count
      size_t count = 0;
      while (p < q) {
        if (*p < 0x80) {
          // Fast code path for ASCII
          p++;
        } else {
          auto remaining = size_t(q - p);
          auto length = UTF8::sizeFromLeadByte(*p);
          if (length > remaining) {
            // Truncated
            return SIZE_MAX;
          }
          assert(length > 1);
          for (size_t i = 1; i < length; ++i) {
            if ((p[i] & 0xC0) != 0x80) {
              // Bad continuation byte
              return SIZE_MAX;
            }
          }
          p += length;
        }
        count++;
      }
      return count;
    }
  };
}
