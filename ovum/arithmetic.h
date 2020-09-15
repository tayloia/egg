namespace egg::ovum {
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

  inline bool arithmeticEquals(double a, int64_t b) {
    return std::isfinite(a) && (int64_t(a) == b) && (a == double(b));
  }

  inline bool arithmeticEquals(double a, double b, bool ieee) {
    if (std::isfinite(a)) {
      return a == b;
    }
    if (std::isnan(a)) {
      return !ieee && std::isnan(b);
    }
    return std::isinf(b) && (std::signbit(a) == std::signbit(b));
  }
}
