namespace egg::ovum {
  class Operation {
  public:
    enum class BinaryResult {
      Ints,
      Floats,
      BadLeft,
      BadRight
    };
    union BinaryValues {
      Int i[2];
      Float f[2];
      BinaryResult promote(const HardValue& lhs, const HardValue& rhs) {
        Int t;
        if (lhs->getFloat(this->f[0])) {
          // Need to promote rhs to float
          if (rhs->getFloat(this->f[1])) {
            // Both values are already floats
            return BinaryResult::Floats;
          }
          if (rhs->getInt(t)) {
            // Left is float, right is int
            this->f[1] = Float(t);
            return BinaryResult::Floats;
          }
          return BinaryResult::BadRight;
        }
        if (lhs->getInt(t)) {
          // May need to promote lhs to float
          if (rhs->getFloat(this->f[1])) {
            // Left is int, right is float
            this->f[0] = Float(t);
            return BinaryResult::Floats;
          }
          if (rhs->getInt(this->i[1])) {
            // Both values are already ints
            this->i[0] = t;
            return BinaryResult::Ints;
          }
          return BinaryResult::BadRight;
        }
        return BinaryResult::BadLeft;
      }
      bool compareInts(Arithmetic::Compare compare) const {
        return Arithmetic::compare(compare, this->i[0], this->i[1]);
      }
      bool compareFloats(Arithmetic::Compare compare, bool ieee) const {
        return Arithmetic::compare(compare, this->f[0], this->f[1], ieee);
      }
    };
  };
}
