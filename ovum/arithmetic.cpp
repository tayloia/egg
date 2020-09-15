#include "ovum/ovum.h"

namespace {
  struct FloatParts {
    constexpr static size_t MANTISSA_CHARS = 32;

    const char* special;
    bool negative;
    int exponent; // radix 10
    char mantissa[MANTISSA_CHARS]; // may not be NUL-terminated

    FloatParts(double value, size_t sigfigs)
    : special(nullptr), negative(std::signbit(value)), exponent(0), mantissa() {
      assert(sigfigs > 0);
      assert(sigfigs <= MANTISSA_CHARS);
      switch (std::fpclassify(value)) {
      case FP_INFINITE:
        // Positive or negative infinity
        this->special = this->negative ? "#-INF" : "#+INF";
        return;
      case FP_NAN:
        this->special = "#NAN";
        return;
      case FP_ZERO:
        this->special = "0.0";
        return;
      }
      this->special = nullptr;
      auto m = FloatParts::getMantissaExponent10(std::abs(value), this->exponent);
      for (size_t i = 0; i < sigfigs; ++i) {
        assert((m >= 0.0) && (m < 1.0));
        double n;
        m = std::modf(m * 10.0, &n);
        this->mantissa[i] = char(n) + '0';
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
      }
      else {
        this->writeMantissa(os, 1, sigfigs);
      }
      int e = this->exponent - 1;
      if (e < 0) {
        os.write("e-", 2);
        e = -e;
      }
      else {
        os.write("e+", 2);
      }
      assert((e >= 0) && (e <= 999));
      os.put(char(e / 100) + '0');
      os.put(char((e / 10) % 10) + '0');
      os.put(char(e % 10) + '0');
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
    if (parts.special != nullptr) {
      os << parts.special;
      return;
    }
    if (parts.negative) {
      os.put('-');
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
      }
      else if (before >= sigfigs) {
        // We've got something like "mmmmm0.0" or "mmmmm.0"
        parts.writeMantissa(os, 0, sigfigs);
        parts.writeZeroes(os, before - sigfigs);
        os.write(".0", 2);
      }
      else {
        // We've got something like "mmm.mm"
        parts.writeMantissa(os, 0, before);
        os.put('.');
        parts.writeMantissa(os, before, sigfigs);
      }
    }
    else {
      // There is nothing before the decimal point
      // We've got something like "0.00mmmmm" or "0.mmmmm"
      auto zeroes = size_t(-parts.exponent);
      auto after = zeroes + sigfigs;
      if (after > max_after) {
        parts.writeScientific(os, sigfigs);
      }
      else {
        os.write("0.", 2);
        parts.writeZeroes(os, zeroes);
        parts.writeMantissa(os, 0, sigfigs);
      }
    }
  }
}

void egg::ovum::MantissaExponent::fromFloat(Float f) {
  switch (std::fpclassify(f)) {
  case FP_NORMAL:
    // Finite
    break;
  case FP_ZERO:
    // Zero
    this->mantissa = 0;
    this->exponent = 0;
    return;
  case FP_INFINITE:
    // Positive or negative infinity
    this->mantissa = 0;
    this->exponent = std::signbit(f) ? ExponentNegativeInfinity : ExponentPositiveInfinity;
    return;
  case FP_NAN:
    // Not-a-Number
    this->mantissa = 0;
    this->exponent = ExponentNaN;
    return;
  }
  constexpr int bitsInMantissa = std::numeric_limits<Float>::digits; // DBL_MANT_DIG
  constexpr double scale = 1ull << bitsInMantissa;
  int e;
  double m = std::frexp(f, &e);
  m = std::floor(scale * m);
  this->mantissa = std::llround(m);
  if (this->mantissa == 0) {
    // Failed to convert
    this->exponent = ExponentNaN;
    return;
  }
  this->exponent = int64_t(e) - bitsInMantissa;
  while ((this->mantissa & 1) == 0) {
    // Reduce the mantissa
    this->mantissa >>= 1;
    this->exponent++;
  }
}

egg::ovum::Float egg::ovum::MantissaExponent::toFloat() const {
  if (this->mantissa == 0) {
    switch (this->exponent) {
    case 0:
      return 0.0;
    case ExponentPositiveInfinity:
      return std::numeric_limits<Float>::infinity();
    case ExponentNegativeInfinity:
      return -std::numeric_limits<Float>::infinity();
    case ExponentNaN:
    default:
      return std::numeric_limits<Float>::quiet_NaN();
    }
  }
  return std::ldexp(this->mantissa, int(this->exponent));
}

void egg::ovum::Arithmetic::write(std::ostream& stream, double value, size_t sigfigs) {
  assert(sigfigs > 0);
  writeFloat(stream, value, sigfigs, sigfigs + 3, sigfigs + 3);
}
