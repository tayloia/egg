namespace egg::ovum {
  class Operation {
  public:
    union UnaryValue {
      Bool b;
      Int i;
      Float f;
      enum class ExtractResult {
        Match,
        Mismatch
      };
      ExtractResult extractBool(const HardValue& arg) {
        if (arg->getBool(this->b)) {
          return ExtractResult::Match;
        }
        return ExtractResult::Mismatch;
      }
      ExtractResult extractInt(const HardValue& arg) {
        if (arg->getInt(this->i)) {
          return ExtractResult::Match;
        }
        return ExtractResult::Mismatch;
      }
      enum class ArithmeticResult {
        Int,
        Float,
        Mismatch
      };
      ArithmeticResult extractArithmetic(const HardValue& arg) {
        if (arg->getInt(this->i)) {
          return ArithmeticResult::Int;
        }
        if (arg->getFloat(this->f)) {
          return ArithmeticResult::Float;
        }
        return ArithmeticResult::Mismatch;
      }
    };

    union BinaryValues {
      Bool b[2];
      Int i[2];
      Float f[2];
      enum class ExtractResult {
        Match,
        BadLeft,
        BadRight
      };
      ExtractResult extractBools(const HardValue& lhs, const HardValue& rhs) {
        if (!lhs->getBool(this->b[0])) {
          return ExtractResult::BadLeft;
        }
        if (!rhs->getBool(this->b[1])) {
          return ExtractResult::BadRight;
        }
        return ExtractResult::Match;
      }
      ExtractResult extractInts(const HardValue& lhs, const HardValue& rhs) {
        if (!lhs->getInt(this->i[0])) {
          return ExtractResult::BadLeft;
        }
        if (!rhs->getInt(this->i[1])) {
          return ExtractResult::BadRight;
        }
        return ExtractResult::Match;
      }
      enum class BitwiseResult {
        Bools,
        Ints,
        BadLeft,
        BadRight,
        Mismatch
      };
      BitwiseResult extractBitwise(const HardValue& lhs, const HardValue& rhs) {
        if (lhs->getBool(this->b[0])) {
          if (rhs->getBool(this->b[1])) {
            return BitwiseResult::Bools;
          }
          Int t;
          if (rhs->getInt(t)) {
            return BitwiseResult::Mismatch;
          }
          return BitwiseResult::BadRight;
        }
        if (lhs->getInt(this->i[0])) {
          if (rhs->getInt(this->i[1])) {
            return BitwiseResult::Ints;
          }
          Bool t;
          if (rhs->getBool(t)) {
            return BitwiseResult::Mismatch;
          }
          return BitwiseResult::BadRight;
        }
        return BitwiseResult::BadLeft;
      }
      enum class PromotionResult {
        Ints,
        Floats,
        BadLeft,
        BadRight
      };
      PromotionResult promote(const HardValue& lhs, const HardValue& rhs) {
        Int t;
        if (lhs->getFloat(this->f[0])) {
          // Need to promote rhs to float
          if (rhs->getFloat(this->f[1])) {
            // Both values are already floats
            return PromotionResult::Floats;
          }
          if (rhs->getInt(t)) {
            // Left is float, right is int
            this->f[1] = Float(t);
            return PromotionResult::Floats;
          }
          return PromotionResult::BadRight;
        }
        if (lhs->getInt(t)) {
          // May need to promote lhs to float
          if (rhs->getFloat(this->f[1])) {
            // Left is int, right is float
            this->f[0] = Float(t);
            return PromotionResult::Floats;
          }
          if (rhs->getInt(this->i[1])) {
            // Both values are already ints
            this->i[0] = t;
            return PromotionResult::Ints;
          }
          return PromotionResult::BadRight;
        }
        return PromotionResult::BadLeft;
      }
      bool compareInts(Arithmetic::Compare compare) const {
        return Arithmetic::compare(compare, this->i[0], this->i[1]);
      }
      bool compareFloats(Arithmetic::Compare compare, bool ieee) const {
        return Arithmetic::compare(compare, this->f[0], this->f[1], ieee);
      }
      int64_t shiftInts(Arithmetic::Shift shift) const {
        return Arithmetic::shift(shift, this->i[0], this->i[1]);
      }
    };

    static bool areEqual(const HardValue& lhs, const HardValue& rhs, bool promote, bool ieee) {
      EGG_WARNING_SUPPRESS_SWITCH_BEGIN
      switch (lhs->getFlags()) {
      case ValueFlags::Null:
        return rhs->getNull();
      case ValueFlags::Bool:
      {
        Bool blhs, brhs;
        return lhs->getBool(blhs) && rhs->getBool(brhs) && (blhs == brhs);
      }
      case ValueFlags::Int:
      {
        Int ilhs;
        if (lhs->getInt(ilhs)) {
          Int irhs;
          if (rhs->getInt(irhs)) {
            return ilhs == irhs;
          }
          if (promote) {
            Float frhs;
            if (rhs->getFloat(frhs)) {
              // Equality after promotion (exact)
              return Arithmetic::equal(frhs, ilhs);
            }
          }
        }
        break;
      }
      case ValueFlags::Float:
      {
        Float flhs;
        if (lhs->getFloat(flhs)) {
          Float frhs;
          if (rhs->getFloat(frhs)) {
            // Equality without promotion
            return Arithmetic::equal(flhs, frhs, ieee);
          }
          if (promote) {
            Int irhs;
            if (rhs->getInt(irhs)) {
              // Equality after promotion (exact)
              return Arithmetic::equal(flhs, irhs);
            }
          }
        }
        break;
      }
      case ValueFlags::String:
      {
        String slhs;
        if (lhs->getString(slhs)) {
          String srhs;
          if (rhs->getString(srhs)) {
            return slhs.equals(srhs);
          }
        }
        break;
      }
      case ValueFlags::Object:
      {
        HardObject olhs;
        if (lhs->getHardObject(olhs)) {
          HardObject orhs;
          if (rhs->getHardObject(orhs)) {
            return olhs.equals(orhs);
          }
        }
        break;
      }
      default:
        assert(false);
      }
      EGG_WARNING_SUPPRESS_SWITCH_END
      return false;
    }
  };
}
