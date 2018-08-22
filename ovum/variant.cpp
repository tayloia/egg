#include "ovum/ovum.h"

#include <cmath>

namespace {
  bool arithmeticEqual(double a, int64_t b) {
    // TODO
    return a == b;
  }

  bool clearBit(egg::ovum::VariantBits& bits, egg::ovum::VariantBits mask) {
    if (egg::ovum::Bits::hasOneSet(bits, mask)) {
      bits = egg::ovum::Bits::clear(bits, mask);
      return true;
    }
    return false;
  }

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

void egg::ovum::VariantKind::printTo(std::ostream& stream, VariantBits kind) {
  // This is used by GoogleTest's ::testing::internal::PrintTo
  size_t found = 0;
#define EGG_OVUM_VARIANT_PRINT(name, text) if (Bits::hasAnySet(kind, VariantBits::name)) { if (++found > 1) { stream << '|'; } stream << #name; }
  EGG_OVUM_VARIANT(EGG_OVUM_VARIANT_PRINT)
#undef EGG_OVUM_VARIANT_PRINT
  if (found == 0) {
    stream << '(' << int(kind) << ')';
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
    assert(this->variant.validate(true));
  }
  virtual Variant& getVariant() override {
    assert(this->variant.validate(true));
    return this->variant;
  }
  virtual Type getPointerType() const override {
    assert(this->variant.validate(true));
    return Type::makePointer(this->allocator, *this->variant.getRuntimeType());
  }
  virtual void softVisitLinks(const Visitor& visitor) const override {
    assert(this->variant.validate(true));
    if (!this->variant.hasAny(VariantBits::Hard)) {
      if (this->variant.hasAny(VariantBits::Object)) {
        // Soft reference to an object
        assert(this->variant.u.o != nullptr);
        visitor(*this->variant.u.o);
      } else if (this->variant.hasAny(VariantBits::Pointer | VariantBits::Indirect)) {
        // Soft pointer or soft indirect
        assert(this->variant.u.p != nullptr);
        visitor(*this->variant.u.p);
      }
    }
  }
};

// Trivial constant values
const egg::ovum::Variant egg::ovum::Variant::Void{ VariantBits::Void };
const egg::ovum::Variant egg::ovum::Variant::Null{ VariantBits::Null };
const egg::ovum::Variant egg::ovum::Variant::False{ false };
const egg::ovum::Variant egg::ovum::Variant::True{ true };
const egg::ovum::Variant egg::ovum::Variant::EmptyString{ String() };
const egg::ovum::Variant egg::ovum::Variant::Break{ VariantBits::Break };
const egg::ovum::Variant egg::ovum::Variant::Continue{ VariantBits::Continue };
const egg::ovum::Variant egg::ovum::Variant::Rethrow{ VariantBits::Throw | VariantBits::Void };
const egg::ovum::Variant egg::ovum::Variant::ReturnVoid{ VariantBits::Return | VariantBits::Void };

#if !defined(NDEBUG)
bool egg::ovum::Variant::validate(bool soft) const {
  const auto zero = VariantBits(0);
  auto bits = this->kind;
  if (clearBit(bits, VariantBits::Break | VariantBits::Continue)) {
    // These flow controls have no parameters
    assert(bits == zero);
    return true;
  }
  if (clearBit(bits, VariantBits::Return | VariantBits::Yield | VariantBits::Throw)) {
    // These flow controls have additional data
    assert(bits != zero);
  }
  if (clearBit(bits, VariantBits::Hard)) {
    if (clearBit(bits, VariantBits::Memory)) {
      // Memory is always hard but may not be null
      assert(this->u.s != nullptr);
      assert(bits == zero);
      return true;
    }
    if (clearBit(bits, VariantBits::String)) {
      // Strings are always hard and may be null/empty
      assert(bits == zero);
      return true;
    }
    assert(!soft);
    assert((bits == VariantBits::Pointer) || (bits == VariantBits::Indirect) || (bits == VariantBits::Object));
  }
  if (clearBit(bits, VariantBits::Pointer | VariantBits::Indirect)) {
    // Pointers/indirections cannot be null
    assert(this->u.p != nullptr);
    assert(bits == zero);
    return true;
  }
  if (clearBit(bits, VariantBits::Object)) {
    // Object cannot be null
    assert(this->u.o != nullptr);
    assert(bits == zero);
    return true;
  }
  if (clearBit(bits, VariantBits::Bool)) {
    // Check that there isn't garbage in a bool
    assert((this->u.b == false) || (this->u.b == true));
    assert(bits == zero);
    return true;
  }
  if (clearBit(bits, VariantBits::Void | VariantBits::Null | VariantBits::Int | VariantBits::Float)) {
    // Cannot check the values here
    assert(bits == zero);
    return true;
  }
  assert(bits == zero);
  return false;
}
#endif

const egg::ovum::Variant& egg::ovum::Variant::direct() const {
  return const_cast<Variant*>(this)->direct();
}

egg::ovum::Variant& egg::ovum::Variant::direct() {
  assert(this->validate());
  auto* p = this;
  assert(p != nullptr);
  if (p->hasIndirect()) {
    assert(p->u.p != nullptr);
    p = &p->u.p->getVariant();
    assert(p != nullptr);
    assert(!p->hasFlowControl());
    assert(!(p->hasIndirect()));
    assert(p->validate());
  }
  return *p;
}

void egg::ovum::Variant::indirect(IAllocator& allocator, IBasket& basket) {
  // Make this value indirect (i.e. heap-based)
  assert(this->validate());
  if (!this->hasIndirect()) {
    // Create a soft reference to a soft copy
    auto heap = VariantFactory::createVariantSoft(allocator, basket, std::move(*this));
    assert(this->kind == VariantBits::Void); // as a side-effect of the move
    this->kind = VariantBits::Indirect;
    this->u.p = heap.get();
    assert(heap != nullptr);
  }
  assert(this->validate());
}

egg::ovum::Variant egg::ovum::Variant::address() const {
  // Create a hard pointer to this indirect value
  assert(this->validate(true));
  assert(this->hasIndirect());
  return Variant(VariantBits::Pointer | VariantBits::Hard, *this->u.p);
}

void egg::ovum::Variant::soften(IBasket& basket) {
  assert(this->validate());
  if (this->hasAny(VariantBits::Hard)) {
    ICollectable* hard;
    if (this->hasAny(VariantBits::Object)) {
      // This is a hard pointer to an IObject, make it a soft pointer, if possible
      hard = this->u.o;
    } else if (this->hasAny(VariantBits::Pointer | VariantBits::Indirect)) {
      // This is a hard pointer to an IVariantSoft, make it a soft pointer, if possible
      hard = this->u.p;
    } else {
      // We cannot soften string (there's no need)
      assert(this->isString());
      return;
    }
    assert(hard != nullptr);
    if (hard->softGetBasket() == nullptr) {
      // Need to add it to the basket
      basket.take(*hard);
    }
    assert(hard->softGetBasket() == &basket);
    // Successfully linked in the basket, so release our hard reference
    this->kind = Bits::clear(this->kind, VariantBits::Hard);
    hard->hardRelease();
  }
  assert(this->validate());
}

bool egg::ovum::Variant::equals(const Variant& lhs, const Variant& rhs) {
  assert(lhs.validate());
  assert(rhs.validate());
  auto& da = lhs.direct();
  auto& db = rhs.direct();
  auto ka = Bits::clear(da.kind, VariantBits::Hard);
  auto kb = Bits::clear(db.kind, VariantBits::Hard);
  auto& a = da.u;
  auto& b = db.u;
  if (ka != kb) {
    // Need to worry about expressions like (0 == 0.0)
    if ((ka == VariantBits::Float) && (kb == VariantBits::Int)) {
      return arithmeticEqual(a.f, b.i);
    }
    if ((ka == VariantBits::Int) && (kb == VariantBits::Float)) {
      return arithmeticEqual(b.f, a.i);
    }
    return false;
  }
  ka = Bits::clear(ka, VariantBits::FlowControl);
  EGG_WARNING_SUPPRESS_SWITCH_BEGIN
  switch (ka) {
  case VariantBits::Void:
  case VariantBits::Null:
    return true;
  case VariantBits::Bool:
    return a.b == b.b;
  case VariantBits::Int:
    return a.i == b.i;
  case VariantBits::Float:
    return a.f == b.f;
  case VariantBits::String:
  case VariantBits::Memory:
    return Memory::equals(a.s, b.s); // binary equality
  case VariantBits::Object:
    return a.o == b.o; // identity
  case VariantBits::Pointer:
    return a.p == b.p; // same address
  }
  EGG_WARNING_SUPPRESS_SWITCH_END
  assert(false);
  return false;
}

void egg::ovum::Variant::softVisitLink(const egg::ovum::ICollectable::Visitor& visitor) const {
  assert(this->validate());
  if (!this->hasAny(VariantBits::Hard)) {
    if (this->hasAny(VariantBits::Object)) {
      // Soft reference to an object
      assert(this->u.o != nullptr);
      visitor(*this->u.o);
    } else if (this->hasAny(VariantBits::Pointer | VariantBits::Indirect)) {
      // Soft reference to a variant
      assert(this->u.p != nullptr);
      visitor(*this->u.p);
    }
  }
}

void egg::ovum::Variant::addFlowControl(VariantBits bits) {
  assert(this->validate());
  assert(Bits::mask(bits, VariantBits::FlowControl) == bits);
  assert(!this->hasFlowControl());
  this->kind = this->kind | bits;
  assert(this->hasFlowControl());
  assert(this->validate());
}

bool egg::ovum::Variant::stripFlowControl(VariantBits bits) {
  assert(this->validate());
  assert(Bits::mask(bits, VariantBits::FlowControl) == bits);
  if (this->hasAny(bits)) {
    assert(this->hasFlowControl());
    this->kind = Bits::clear(this->kind, bits);
    assert(!this->hasFlowControl());
    assert(this->validate());
    return true;
  }
  return false;
}

egg::ovum::Type egg::ovum::Variant::getRuntimeType() const {
  assert(this->validate());
  assert(!this->hasIndirect());
  if (this->hasObject()) {
    return this->u.o->getRuntimeType();
  }
  if (this->hasPointer()) {
    return this->u.p->getPointerType();
  }
  auto mask = BasalBits::Void | BasalBits::AnyQ;
  auto basal = Bits::mask(static_cast<BasalBits>(this->kind), mask);
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
  if (this->isString()) {
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

egg::ovum::HardPtr<egg::ovum::IVariantSoft> egg::ovum::VariantFactory::createVariantSoft(IAllocator& allocator, IBasket& basket, Variant&& value) {
  assert(value.validate());
  assert(!value.hasAny(VariantBits::Indirect));
  IVariantSoft* soften = nullptr;
  if (value.is(VariantBits::Object | VariantBits::Hard)) {
    // We need to soften objects
    soften = value.u.p;
    value.kind = VariantBits::Object;
  } else if (value.is(VariantBits::Pointer | VariantBits::Hard)) {
    // We need to soften pointers
    soften = value.u.p;
    value.kind = VariantBits::Pointer;
  }
  HardPtr<IVariantSoft> created{ allocator.create<VariantSoft>(0, allocator, std::move(value)) };
  assert(created != nullptr);
  basket.take(*created);
  if (soften != nullptr) {
    // Release the hard reference that we masked off earlier
    assert(value.is(VariantBits::Void));
    soften->hardRelease();
  }
  assert(created->getVariant().validate(true));
  return created;
}
