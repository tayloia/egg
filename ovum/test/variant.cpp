#include "ovum/test.h"

using Bits = egg::ovum::VariantBits;

TEST(TestVariant, Kind) {
  egg::ovum::VariantKind kind{ Bits::Exception | Bits::Object };
  // hasAny
  ASSERT_FALSE(kind.hasAny(Bits::Void));
  ASSERT_TRUE(kind.hasAny(Bits::Exception));
  ASSERT_TRUE(kind.hasAny(Bits::Object));
  ASSERT_TRUE(kind.hasAny(Bits::Exception | Bits::Object));
  ASSERT_TRUE(kind.hasAny(Bits::Void | Bits::Exception | Bits::Object));
  // hasAll
  ASSERT_FALSE(kind.hasAll(Bits::Void));
  ASSERT_TRUE(kind.hasAll(Bits::Exception));
  ASSERT_TRUE(kind.hasAll(Bits::Object));
  ASSERT_TRUE(kind.hasAll(Bits::Exception | Bits::Object));
  ASSERT_FALSE(kind.hasAll(Bits::Void | Bits::Exception | Bits::Object));
  // is
  ASSERT_FALSE(kind.is(Bits::Void));
  ASSERT_FALSE(kind.is(Bits::Exception));
  ASSERT_FALSE(kind.is(Bits::Object));
  ASSERT_TRUE(kind.is(Bits::Exception | Bits::Object));
  ASSERT_FALSE(kind.is(Bits::Void | Bits::Exception | Bits::Object));
}

TEST(TestVariant, Void) {
  egg::ovum::Variant variant;
  ASSERT_TRUE(variant.is(Bits::Void));
}

TEST(TestVariant, Bool) {
  egg::ovum::Variant variant{ false };
  ASSERT_TRUE(variant.is(Bits::Bool));
  ASSERT_EQ(false, variant.getBool());
  variant = true;
  ASSERT_EQ(true, variant.getBool());
}

TEST(TestVariant, Int) {
  egg::ovum::Variant variant{ 0 };
  ASSERT_TRUE(variant.is(Bits::Int));
  ASSERT_EQ(0, variant.getInt());
  variant = 123456789;
  ASSERT_EQ(123456789, variant.getInt());
  variant = -1;
  ASSERT_EQ(-1, variant.getInt());
}

TEST(TestVariant, Float) {
  egg::ovum::Variant variant{ 0.0 };
  ASSERT_TRUE(variant.is(Bits::Float));
  ASSERT_EQ(0.0, variant.getFloat());
  variant = 123456789.0;
  ASSERT_EQ(123456789.0, variant.getFloat());
  variant = -1.0;
  ASSERT_EQ(-1.0, variant.getFloat());
  variant = 0.5;
  ASSERT_EQ(0.5, variant.getFloat());
}
