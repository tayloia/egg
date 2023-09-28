namespace egg::ovum {
  class Arithmetic {
  public:
    static const size_t DEFAULT_SIGFIGS = 12;
    enum class Compare {
      LT,
      LE,
      EQ,
      NE,
      GE,
      GT
    };
    enum class Shift {
      ShiftL,
      ShiftR,
      ShiftU,
      RotateL,
      RotateR
    };

    static void print(std::ostream& stream, double value, size_t sigfigs = DEFAULT_SIGFIGS);

    inline static bool equal(double a, int64_t b) {
      return std::isfinite(a) && (int64_t(a) == b) && (a == double(b));
    }

    inline static bool equal(double a, double b, bool ieee) {
      if (std::isfinite(a)) {
        return a == b;
      }
      if (std::isnan(a)) {
        return !ieee && std::isnan(b);
      }
      return std::isinf(b) && (std::signbit(a) == std::signbit(b));
    }

    template<typename T>
    inline static int order(T a, T b) {
      if (a < b) {
        return -1;
      }
      if (b < a) {
        return +1;
      }
      return 0;
    }

    inline static int order(double a, double b) {
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
    inline static bool compare(Compare op, T a, T b) {
      switch (op) {
      case Compare::LT:
        return a < b;
      case Compare::LE:
        return a <= b;
      case Compare::EQ:
        return a == b;
      case Compare::NE:
        return a != b;
      case Compare::GE:
        return a >= b;
      case Compare::GT:
        return a > b;
      default:
        assert(false);
      }
      return false;
    }

    inline static bool compare(Compare op, double a, double b, bool ieee) {
      // See https://en.wikipedia.org/wiki/NaN#Comparison_with_NaN
      if (std::isnan(a)) {
        if (ieee) {
          return op == Compare::NE;
        }
        if (std::isnan(b)) {
          return (op == Compare::LE) || (op == Compare::EQ) || (op == Compare::GE);
        }
        return (op == Compare::LT) || (op == Compare::LE) || (op == Compare::NE);
      }
      if (std::isnan(b)) {
        if (ieee) {
          return op == Compare::NE;
        }
        return (op == Compare::GT) || (op == Compare::GE) || (op == Compare::NE);
      }
      return Arithmetic::compare(op, a, b);
    }

    inline static int64_t shift(Shift op, int64_t a, int64_t b) {
      switch (op) {
      case Shift::ShiftL:
        if ((b >= 0) && (b <= 63)) {
          return Arithmetic::shiftL(a, size_t(b));
        }
        if ((b <= -1) && (b >= -63)) {
          return Arithmetic::shiftR(a, size_t(-b));
        }
        break;
      case Shift::ShiftR:
        if ((b >= 0) && (b <= 63)) {
          return Arithmetic::shiftR(a, size_t(b));
        }
        if ((b <= -1) && (b >= -63)) {
          return Arithmetic::shiftL(a, size_t(-b));
        }
        break;
      case Shift::ShiftU:
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

    inline static int64_t rotateL(int64_t a, size_t b) {
      assert(b < 64);
      auto u = uint64_t(a);
      return int64_t((u << b) | (u >> (64 - b)));
    }

    inline static int64_t shiftL(int64_t a, size_t b) {
      assert(b < 64);
      return a << b;
    }

    inline static int64_t shiftR(int64_t a, size_t b) {
      // See https://stackoverflow.com/a/59595766
      assert(b < 64);
      auto sign = -int64_t(uint64_t(a) >> 63);
      return ((a ^ sign) >> b) ^ sign;
    }

    inline static int64_t shiftU(int64_t a, size_t b) {
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
