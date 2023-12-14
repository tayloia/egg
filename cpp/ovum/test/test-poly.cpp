#include "ovum/test.h"

TEST(TestPoly, CreateSoftValue) {
  egg::test::VM vm;
  auto* soft = vm->createSoftValue();
  ASSERT_NE(nullptr, soft);
  ASSERT_EQ(egg::ovum::ValueFlags::Void, soft->getPrimitiveFlag());
  // 'soft' is automagically cleaned up by GC
}

TEST(TestPoly, SoftValueVoid) {
  egg::test::VM vm;
  egg::ovum::SoftValue soft(*vm.vm);
  ASSERT_EQ(egg::ovum::ValueFlags::Void, soft->getPrimitiveFlag());
  vm->setSoftValue(soft, egg::ovum::HardValue::Void);
  ASSERT_EQ(egg::ovum::ValueFlags::Void, soft->getPrimitiveFlag());
  ASSERT_TRUE(soft->getVoid());
}

TEST(TestPoly, SoftValueNull) {
  egg::test::VM vm;
  egg::ovum::SoftValue soft(*vm);
  vm->setSoftValue(soft, egg::ovum::HardValue::Null);
  ASSERT_EQ(egg::ovum::ValueFlags::Null, soft->getPrimitiveFlag());
  ASSERT_TRUE(soft->getNull());
}

TEST(TestPoly, SoftValueBool) {
  egg::test::VM vm;
  egg::ovum::SoftValue soft(*vm);
  vm->setSoftValue(soft, egg::ovum::HardValue::True);
  ASSERT_EQ(egg::ovum::ValueFlags::Bool, soft->getPrimitiveFlag());
  egg::ovum::Bool bvalue = false;
  ASSERT_TRUE(soft->getBool(bvalue));
  ASSERT_TRUE(bvalue);
}

TEST(TestPoly, SoftValueInt) {
  egg::test::VM vm;
  egg::ovum::SoftValue soft(*vm);
  vm->setSoftValue(soft, vm->createHardValueInt(12345));
  ASSERT_EQ(egg::ovum::ValueFlags::Int, soft->getPrimitiveFlag());
  egg::ovum::Int ivalue = 0;
  ASSERT_TRUE(soft->getInt(ivalue));
  ASSERT_EQ(12345, ivalue);
}

TEST(TestPoly, SoftValueFloat) {
  egg::test::VM vm;
  egg::ovum::SoftValue soft(*vm);
  vm->setSoftValue(soft, vm->createHardValueFloat(1234.5));
  ASSERT_EQ(egg::ovum::ValueFlags::Float, soft->getPrimitiveFlag());
  egg::ovum::Float fvalue = 0.0;
  ASSERT_TRUE(soft->getFloat(fvalue));
  ASSERT_EQ(1234.5, fvalue);
}

TEST(TestPoly, SoftValueString) {
  egg::test::VM vm;
  egg::ovum::SoftValue soft(*vm);
  vm->setSoftValue(soft, vm->createHardValueString(vm->createString("hello")));
  ASSERT_EQ(egg::ovum::ValueFlags::String, soft->getPrimitiveFlag());
  egg::ovum::String svalue;
  ASSERT_TRUE(soft->getString(svalue));
  ASSERT_STRING("hello", svalue);
}

TEST(TestPoly, SoftValueObject) {
  egg::test::VM vm;
  egg::ovum::SoftValue soft(*vm);
  auto builtin = vm->createBuiltinAssert();
  vm->setSoftValue(soft, vm->createHardValueObject(builtin));
  ASSERT_EQ(egg::ovum::ValueFlags::Object, soft->getPrimitiveFlag());
  egg::ovum::HardObject ovalue;
  ASSERT_TRUE(soft->getHardObject(ovalue));
  ASSERT_EQ(builtin.get(), ovalue.get());
}
