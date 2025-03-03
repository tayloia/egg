namespace egg::ovum {
  class Arithmetic {
  public:
    static const size_t DEFAULT_SIGFIGS = 12;

    enum class Compare {
      LessThan,
      LessThanOrEqual,
      Equal,
      NotEqual,
      GreaterThanOrEqual,
      GreaterThan
    };

    enum class Shift {
      ShiftLeft,
      ShiftRight,
      ShiftRightUnsigned,
      RotateL,
      RotateR
    };

    static void print(std::ostream& stream, double value, size_t sigfigs = DEFAULT_SIGFIGS);

    static bool equal(double a, int64_t b) {
      return std::isfinite(a) && (int64_t(a) == b) && (a == double(b));
    }

    static bool equal(double a, double b, bool ieee) {
      if (std::isfinite(a)) {
        return a == b;
      }
      if (std::isnan(a)) {
        return !ieee && std::isnan(b);
      }
      return std::isinf(b) && (std::signbit(a) == std::signbit(b));
    }

    template<typename T>
    static int order(T a, T b) {
      if (a < b) {
        return -1;
      }
      if (b < a) {
        return +1;
      }
      return 0;
    }

    static int order(double a, double b) {
      // Treat NaNs as smallest (this is NOT as IEEE states)
      if (std::isnan(a)) {
        return std::isnan(b) ? 0 : -1;
      }
      if (std::isnan(b) || (a > b)) {
        return +1;
      }
      return (a < b) ? -1 : 0;
    }

    template<typename T>
    static bool compare(Compare op, T a, T b) {
      switch (op) {
      case Compare::LessThan:
        return a < b;
      case Compare::LessThanOrEqual:
        return a <= b;
      case Compare::Equal:
        return a == b;
      case Compare::NotEqual:
        return a != b;
      case Compare::GreaterThanOrEqual:
        return a >= b;
      case Compare::GreaterThan:
        return a > b;
      default:
        assert(false);
      }
      return false;
    }

    static bool compare(Compare op, double a, double b, bool ieee) {
      // See https://en.wikipedia.org/wiki/NaN#Comparison_with_NaN
      if (std::isnan(a)) {
        if (ieee) {
          return op == Compare::NotEqual;
        }
        if (std::isnan(b)) {
          return (op == Compare::LessThanOrEqual) || (op == Compare::Equal) || (op == Compare::GreaterThanOrEqual);
        }
        return (op == Compare::LessThan) || (op == Compare::LessThanOrEqual) || (op == Compare::NotEqual);
      }
      if (std::isnan(b)) {
        if (ieee) {
          return op == Compare::NotEqual;
        }
        return (op == Compare::GreaterThan) || (op == Compare::GreaterThanOrEqual) || (op == Compare::NotEqual);
      }
      return Arithmetic::compare(op, a, b);
    }

    static double promote(int64_t i) {
      return double(i);
    }

    static int64_t minimum(int64_t a, int64_t b) {
      return std::min(a, b);
    }

    static int64_t maximum(int64_t a, int64_t b) {
      return std::max(a, b);
    }

    static double minimum(double a, double b, bool ieee) {
      if (std::isnan(a)) {
        return ieee ? a : b;
      }
      if (std::isnan(b)) {
        return ieee ? b : a;
      }
      return (a < b) ? a : b;
    }

    static double maximum(double a, double b, bool ieee) {
      if (std::isnan(a)) {
        return ieee ? a : b;
      }
      if (std::isnan(b)) {
        return ieee ? b : a;
      }
      return (a < b) ? b : a;
    }

    static int64_t shift(Shift op, int64_t a, int64_t b) {
      switch (op) {
      case Shift::ShiftLeft:
        if ((b >= 0) && (b <= 63)) {
          return Arithmetic::shiftL(a, size_t(b));
        }
        if ((b <= -1) && (b >= -63)) {
          return Arithmetic::shiftR(a, size_t(-b));
        }
        break;
      case Shift::ShiftRight:
        if ((b >= 0) && (b <= 63)) {
          return Arithmetic::shiftR(a, size_t(b));
        }
        if ((b <= -1) && (b >= -63)) {
          return Arithmetic::shiftL(a, size_t(-b));
        }
        break;
      case Shift::ShiftRightUnsigned:
        if ((b >= 0) && (b <= 63)) {
          return Arithmetic::shiftU(a, size_t(b));
        }
        if ((b <= -1) && (b >= -63)) {
          return Arithmetic::shiftL(a, size_t(-b));
        }
        break;
      case Shift::RotateL:
        return Arithmetic::rotateL(a, size_t(b & 63));
      case Shift::RotateR:
        return Arithmetic::rotateL(a, size_t(-b & 63));
      default:
        assert(false);
      }
      return 0;
    }

    static int64_t rotateL(int64_t a, size_t b) {
      assert(b < 64);
      auto u = uint64_t(a);
      return int64_t((u << b) | (u >> (64 - b)));
    }

    static int64_t shiftL(int64_t a, size_t b) {
      assert(b < 64);
      return a << b;
    }

    static int64_t shiftR(int64_t a, size_t b) {
      // See https://stackoverflow.com/a/59595766
      assert(b < 64);
      auto sign = -int64_t(uint64_t(a) >> 63);
      return ((a ^ sign) >> b) ^ sign;
    }

    static int64_t shiftU(int64_t a, size_t b) {
      assert(b < 64);
      return int64_t(uint64_t(a) >> b);
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
}
