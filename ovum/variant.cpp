#include "ovum/ovum.h"

namespace {
  struct FloatParts {
    constexpr static size_t MANTISSA_CHARS = 32;

    bool negative;
    const char* special; // null if finite
    int exponent; // radix 10
    char mantissa[MANTISSA_CHARS]; // may not be NUL-terminated

    FloatParts(double value, size_t sigfigs) {
      assert(sigfigs > 0);
      assert(sigfigs <= MANTISSA_CHARS);
      this->negative = std::signbit(value);
      this->special = FloatParts::getSpecial(value);
      if (this->special == nullptr) {
        auto m = FloatParts::getMantissaExponent10(std::abs(value), this->exponent);
        for (size_t i = 0; i < sigfigs; ++i) {
          assert((m >= 0.0) && (m < 1.0));
          double n;
          m = std::modf(m * 10.0, &n);
          this->mantissa[i] = char(n) + '0';
        }
      }
    }
    size_t round(size_t sigfigs) {
      // Returns the number of non-zero fraction digits
      assert(sigfigs > 1);
      assert(sigfigs < MANTISSA_CHARS);
      bool prune = false;
      switch (this->mantissa[sigfigs]) {
      case '0':
        // Round down (truncate) and scan backwards for last non-zero
        while ((sigfigs > 0) && (this->mantissa[sigfigs - 1] == '0')) {
          sigfigs--;
        }
        return sigfigs;
      case '1':
      case '2':
      case '3':
      case '4':
        // Round down (truncate) but leave trailing zeroes
        return sigfigs;
      case '9':
        // Round up and remove trailing zeroes
        prune = true;
        break;
      }
      auto i = sigfigs;
      for (;;) {
        // Round up and carry forward
        if (i == 0) {
          // We've rounded up "99...99" to "00...00" so renormalize
          this->mantissa[0] = '1';
          this->exponent++;
          return prune ? 1 : sigfigs;
        }
        if (this->mantissa[i - 1] != '9') {
          // No need to carry forward
          this->mantissa[i - 1]++;
          return prune ? i : sigfigs;
        }
        i--;
        this->mantissa[i] = '0';
      }
    }
    void writeMantissa(std::ostream& os, size_t begin, size_t end) {
      assert(begin < end);
      assert(end <= MANTISSA_CHARS);
      os.write(this->mantissa + begin, std::streamsize(end - begin));
    }
    void writeZeroes(std::ostream& os, size_t count) {
      std::fill_n(std::ostreambuf_iterator<char>(os), count, '0');
    }
    void writeScientific(std::ostream& os, size_t sigfigs) {
      // Write out in the following format "M.MMMe+EEE"
      assert((sigfigs > 0) && (sigfigs <= MANTISSA_CHARS));
      os.put(this->mantissa[0]);
      os.put('.');
      if (sigfigs < 2) {
        os.put('0');
      } else {
        this->writeMantissa(os, 1, sigfigs);
      }
      int e = this->exponent - 1;
      if (e < 0) {
        os.write("e-", 2);
        e = -e;
      } else {
        os.write("e+", 2);
      }
      assert((e >= 0) && (e <= 999));
      os.put(char(e / 100) + '0');
      os.put(char((e / 10) % 10) + '0');
      os.put(char(e % 10) + '0');
    }
    static const char* getSpecial(double value) {
      // Work out if this is a "special" IEEE value
      if (std::isnan(value)) {
        return "nan";
      }
      if (std::isinf(value)) {
        return "inf";
      }
      if (value == 0) {
        return "0.0";
      }
      return nullptr;
    }
    static double getMantissaExponent10(double v, int& e) {
      // Computes a decimal mantissa and exponent such that
      //  0.1 <= m < 1.0
      // v == m * 10 ^ e
      assert(std::isfinite(v));
      assert(v > 0);
      e = 0;
      if (!std::isnormal(v)) {
        // Handle denormals simplistically by making them normal
        v *= 1.0e100;
        e = -100;
      }
      auto d = std::floor(std::log10(v)) + 1.0;
      e += int(d);
      auto m = v * std::pow(10.0, -d);
      if (m < 0.1) {
        // Handle edge-case rounding errors
        m = 0.1;
      }
      assert((m >= 0.1) && (m < 1.0));
      return m;
    }
  };

  void writeFloat(std::ostream& os, double value, size_t sigfigs, size_t max_before, size_t max_after) {
    assert(sigfigs > 0);
    FloatParts parts(value, sigfigs + 1);
    if (parts.negative) {
      os.put('-');
    }
    if (parts.special != nullptr) {
      os.write(parts.special, std::streamsize(std::strlen(parts.special)));
      return;
    }
    assert((parts.mantissa[0] >= '1') && (parts.mantissa[0] <= '9'));
    assert((parts.exponent > -333) && (parts.exponent < +333));
    if (sigfigs > 1) {
      sigfigs = parts.round(sigfigs);
    }
    assert((sigfigs > 0) && (sigfigs <= FloatParts::MANTISSA_CHARS));
    if (parts.exponent > 0) {
      // There are digits in front of the decimal point
      auto before = size_t(parts.exponent);
      if (before > max_before) {
        parts.writeScientific(os, sigfigs);
      } else if (before >= sigfigs) {
        // We've got something like "mmmmm0.0" or "mmmmm.0"
        parts.writeMantissa(os, 0, sigfigs);
        parts.writeZeroes(os, before - sigfigs);
        os.write(".0", 2);
      } else {
        // We've got something like "mmm.mm"
        parts.writeMantissa(os, 0, before);
        os.put('.');
        parts.writeMantissa(os, before, sigfigs);
      }
    } else {
      // There is nothing before the decimal point
      // We've got something like "0.00mmmmm" or "0.mmmmm"
      auto zeroes = size_t(-parts.exponent);
      auto after = zeroes + sigfigs;
      if (after > max_after) {
        parts.writeScientific(os, sigfigs);
      } else {
        os.write("0.", 2);
        parts.writeZeroes(os, zeroes);
        parts.writeMantissa(os, 0, sigfigs);
      }
    }
  }

  std::string fromFloat(double value, size_t sigfigs = 12) {
    assert(sigfigs > 0);
    std::stringstream ss;
    writeFloat(ss, value, sigfigs, sigfigs + 3, sigfigs + 3);
    return ss.str();
  }
}

class egg::ovum::VariantSoft : public SoftReferenceCounted<IVariantSoft> { 
  VariantSoft(const VariantSoft&) = delete;
  VariantSoft& operator=(const VariantSoft&) = delete;
private:
  Variant variant;
public:
  VariantSoft(IAllocator& allocator, Variant&& value)
    : SoftReferenceCounted(allocator),
      variant(std::move(value)) {
    assert(this->variant.validate()); // VIBBLE validate soft
  }
  virtual Variant& getVariant() override {
    assert(this->variant.validate()); // VIBBLE validate soft
    return this->variant;
  }
  virtual Type getPointerType() const override {
    assert(this->variant.validate()); // VIBBLE validate soft
    return Type::makePointer(this->allocator, *this->variant.getRuntimeType());
  }
  virtual void softVisitLinks(const Visitor&) const override {
    assert(this->variant.validate()); // VIBBLE validate soft
    assert(false); // VIBBLE
  }
};

bool egg::ovum::Variant::validate() const {
  return this->underlying.validate();
}

void egg::ovum::Variant::soften(IBasket&) {
  // VIBBLE
  assert(this->validate());
}

void egg::ovum::Variant::softVisitLink(const egg::ovum::ICollectable::Visitor&) const {
  // VIBBLE
  assert(this->validate());
}

egg::ovum::Type egg::ovum::Variant::getRuntimeType() const {
  assert(this->validate());
  if (this->hasObject()) {
    auto o = this->getObject();
    return o->getRuntimeType();
  }
  if (this->hasPointer()) {
    return Type::makePointer(*this->getPointee().getRuntimeType());
  }
  auto mask = BasalBits::Void | BasalBits::AnyQ;
  auto basal = Bits::mask(static_cast<BasalBits>(this->getKind()), mask);
  assert(Bits::hasOneSet(basal, mask));
  auto* common = Type::getBasalType(basal);
  assert(common != nullptr);
  return Type(common);
}

egg::ovum::String egg::ovum::Variant::toString() const {
  // OPTIMIZE
  assert(this->validate());
  if (this->hasObject()) {
    auto str = this->getObject()->toString();
    if (str.isString()) {
      return str.getString();
    }
    return "<invalid>";
  }
  if (this->hasString()) {
    return this->getString();
  }
  if (this->isFloat()) {
    return fromFloat(this->getFloat());
  }
  if (this->isInt()) {
    return std::to_string(this->getInt());
  }
  if (this->isBool()) {
    return String(this->getBool() ? "true" : "false");
  }
  if (this->isNull()) {
    return "null";
  }
  return StringBuilder::concat('<', this->getRuntimeType().toString(), '>');
}

egg::ovum::Variant egg::ovum::VariantFactory::createException(Variant&& value) {
  assert(!value.isVoid());
  value.addFlowControl(ValueFlags::Throw);
  return value;
}

egg::ovum::Variant egg::ovum::VariantFactory::createPointer(IAllocator& allocator, const Variant& pointee) {
  assert(!pointee.hasFlowControl());
  return ValueFactory::createPointer(allocator, pointee);
}

egg::ovum::Variant egg::ovum::VariantFactory::createFlowControl(IAllocator& allocator, ValueFlags flags, const Variant& pointee) {
  assert(Bits::hasOneSet(flags, ValueFlags::FlowControl));
  assert(!pointee.isVoid());
  assert(!pointee.hasFlowControl());
  return ValueFactory::createFlowControl(allocator, flags, pointee);
}

egg::ovum::Variant egg::ovum::VariantFactory::createReturnVoid(IAllocator&) {
  Variant value{ Variant::Null };
  value.addFlowControl(ValueFlags::Return);
  return value;
}

egg::ovum::ValueFlags egg::ovum::Variant::getKind() const {
  return this->underlying->getFlags();
}

namespace {
  class VariantFallbackAllocator final : public egg::ovum::IAllocator {
    VariantFallbackAllocator(const VariantFallbackAllocator&) = delete;
    VariantFallbackAllocator& operator=(const VariantFallbackAllocator&) = delete;
  private:
    egg::ovum::Atomic<int64_t> atomic;
  public:
    VariantFallbackAllocator() : atomic(0) {
    }
    ~VariantFallbackAllocator() {
      // Make sure all our strings have been destroyed when the process exits
      // VVIBBLE assert(this->atomic.get() == 0);
    }
    virtual void* allocate(size_t bytes, size_t alignment) override {
      this->atomic.increment();
      return egg::ovum::AllocatorDefaultPolicy::memalloc(bytes, alignment);
    }
    virtual void deallocate(void* allocated, size_t alignment) override {
      assert(allocated != nullptr);
      this->atomic.decrement();
      egg::ovum::AllocatorDefaultPolicy::memfree(allocated, alignment);
    }
    virtual bool statistics(Statistics&) const override {
      return false;
    }
  };
  VariantFallbackAllocator fallbackAllocator;
}

egg::ovum::Type egg::ovum::Type::makePointer(const IType& pointee) {
  // TODO Don't use fallback allocator
  return makePointer(fallbackAllocator, pointee);
}

void egg::ovum::Variant::addFlowControl(ValueFlags bits) {
  assert(this->validate());
  assert(Bits::mask(bits, ValueFlags::FlowControl) == bits);
  assert(!this->hasFlowControl());
  *this = VariantFactory::createFlowControl(fallbackAllocator, bits, *this);
  assert(this->hasFlowControl());
  assert(this->validate());
}

bool egg::ovum::Variant::stripFlowControl(ValueFlags bits) {
  assert(this->validate());
  assert(Bits::mask(bits, ValueFlags::FlowControl) == bits);
  if (this->hasAny(bits)) {
    assert(this->hasFlowControl());
    Value child;
    if (this->underlying->getChild(child)) {
      this->underlying = child;
      assert(!this->hasFlowControl());
      return true;
    }
  }
  return false;
}

// Trivial constant values
const egg::ovum::Variant egg::ovum::Variant::Void{ ValueFactory::createVoid() };
const egg::ovum::Variant egg::ovum::Variant::Null{ ValueFactory::createNull() };
const egg::ovum::Variant egg::ovum::Variant::False{ ValueFactory::createBool(false) };
const egg::ovum::Variant egg::ovum::Variant::True{ ValueFactory::createBool(true) };
const egg::ovum::Variant egg::ovum::Variant::Break{ ValueFactory::createFlowControl(ValueFlags::Break) };
const egg::ovum::Variant egg::ovum::Variant::Continue{ ValueFactory::createFlowControl(ValueFlags::Continue) };
const egg::ovum::Variant egg::ovum::Variant::Rethrow{ ValueFactory::createFlowControl(ValueFlags::Throw) };

egg::ovum::Variant::Variant(std::nullptr_t) : underlying(ValueFactory::createNull()) {}
egg::ovum::Variant::Variant(bool value) : underlying(ValueFactory::createBool(value)) {}
egg::ovum::Variant::Variant(int32_t value) : underlying(ValueFactory::createInt(fallbackAllocator, value)) {}
egg::ovum::Variant::Variant(int64_t value) : underlying(ValueFactory::createInt(fallbackAllocator, value)) {}
egg::ovum::Variant::Variant(float value) : underlying(ValueFactory::createFloat(fallbackAllocator, value)) {}
egg::ovum::Variant::Variant(double value) : underlying(ValueFactory::createFloat(fallbackAllocator, value)) {}
egg::ovum::Variant::Variant(const String& value) : underlying(ValueFactory::createString(fallbackAllocator, value)) {}
egg::ovum::Variant::Variant(const std::string& value) : underlying(ValueFactory::createValue(fallbackAllocator, value)) {}
egg::ovum::Variant::Variant(const char* value) : underlying(ValueFactory::createValue(fallbackAllocator, value)) {}
egg::ovum::Variant::Variant(const Object& value) : underlying(ValueFactory::createObject(fallbackAllocator, value)) {}
egg::ovum::Variant::Variant(const Memory& value) : underlying(ValueFactory::createMemory(fallbackAllocator, value)) {}

egg::ovum::Object egg::ovum::Variant::getObject() const {
  static const Object undefined = ObjectFactory::createVanillaObject(fallbackAllocator);
  auto result = undefined;
  if (this->hasFlowControl()) {
    // We need to support compound values like Return|Object
    Value child;
    if (this->underlying->getChild(child) && child->getObject(result)) {
      return result;
    }
  }
  if (this->underlying->getObject(result)) {
    return result;
  }
  assert(false);
  return result;
}

egg::ovum::Variant egg::ovum::Variant::getPointee() const {
  Variant result;
  if (this->hasPointer() && this->underlying->getChild(result)) {
    return result;
  }
  assert(false);
  return result;
}

