namespace egg::ovum {
  class Arithmetic {
  public:
    enum class Compare {
      LT,
      LE,
      EQ,
      NE,
      GE,
      GT
    };
    static const size_t DEFAULT_SIGFIGS = 12;

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
      if (a < b) {
        return (op == Compare::LT) || (op == Compare::LE) || (op == Compare::NE);
      }
      if (b < a) {
        return (op == Compare::GT) || (op == Compare::GE) || (op == Compare::NE);
      }
      return (op == Compare::LE) || (op == Compare::EQ) || (op == Compare::GE);
    }

    inline static bool compare(Compare op, double a, double b, bool ieee) {
      if (std::isnan(a)) {
        if (std::isnan(b)) {
          if (ieee) {
            return op == Compare::NE;
          }
          return (op == Compare::LE) || (op == Compare::EQ) || (op == Compare::GE);
        }
        if (ieee) {
          return op == Compare::NE;
        }
        return (op == Compare::LT) || (op == Compare::LE) || (op == Compare::NE);
      }
      if (std::isnan(b)) {
        if (ieee) {
          return op == Compare::NE;
        }
        return (op == Compare::GT) || (op == Compare::GE) || (op == Compare::NE);
      }
      if (a < b) {
        return (op == Compare::LT) || (op == Compare::LE) || (op == Compare::NE);
      }
      if (a > b) {
        return (op == Compare::GT) || (op == Compare::GE) || (op == Compare::NE);
      }
      return (op == Compare::LE) || (op == Compare::EQ) || (op == Compare::GE);
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
