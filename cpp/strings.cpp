#include "yolk.h"

#include <cmath>
#include <cstring>

namespace {
  struct FloatParts {
    bool negative;
    const char* special; // null if finite
    int exponent; // radix 10
    char mantissa[32]; // may not be NUL-terminated

    inline FloatParts(double value, size_t sigfigs) {
      assert(sigfigs > 0);
      assert(sigfigs <= EGG_NELEMS(this->mantissa));
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
    inline size_t round(size_t sigfigs) {
      // Returns the number of non-zero fraction digits
      assert(sigfigs > 1);
      assert(sigfigs < EGG_NELEMS(this->mantissa));
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
    inline void writeMantissa(std::ostream& os, size_t begin, size_t end) {
      assert(begin < end);
      assert(end <= EGG_NELEMS(this->mantissa));
      os.write(this->mantissa + begin, std::streamsize(end - begin));
    }
    inline void writeZeroes(std::ostream& os, size_t count) {
      std::fill_n(std::ostreambuf_iterator<char>(os), count, '0');
    }
    inline void writeScientific(std::ostream& os, size_t sigfigs) {
      // Write out in the following format "M.MMMe+EEE"
      assert((sigfigs > 0) && (sigfigs <= EGG_NELEMS(this->mantissa)));
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
    inline static const char* getSpecial(double value) {
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
    inline static double getMantissaExponent10(double v, int& e) {
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
}

std::string egg::yolk::String::fromEnum(int value, const StringFromEnum* tableBegin, const StringFromEnum* tableEnd) {
  for (auto* entry = tableBegin; entry != tableEnd; ++entry) {
    // First scan the entire table for an exact match
    if (entry->value == value) {
      return entry->text;
    }
  }
  std::string result;
  for (auto* entry = tableBegin; entry != tableEnd; ++entry) {
    // Now scan for bit fields
    if ((entry->value != 0) && ((entry->value & value) == entry->value)) {
      if (!result.empty()) {
        result += '|';
      }
      result += entry->text;
      value ^= entry->value;
      if (value == 0) {
        return result;
      }
    }
  }
  // Append the remaining numeric value
  if (!result.empty()) {
    result += '|';
  }
  result += String::fromSigned(value);
  return result;
}

std::string egg::yolk::String::fromSigned(int64_t value) {
  return std::to_string(value);
}

std::string egg::yolk::String::fromUnsigned(uint64_t value) {
  return std::to_string(value);
}

std::string egg::yolk::String::fromFloat(double value, size_t sigfigs) {
  assert(sigfigs > 0);
  std::stringstream ss;
  String::writeFloat(ss, value, sigfigs, sigfigs + 3, sigfigs + 3);
  return ss.str();
}

void egg::yolk::String::writeFloat(std::ostream& os, double value, size_t sigfigs, size_t max_before, size_t max_after) {
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
  assert((sigfigs > 0) && (sigfigs <= EGG_NELEMS(parts.mantissa)));
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
