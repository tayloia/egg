#include "ovum/test.h"
#include "ovum/file.h"
#include "ovum/os-zip.h"

TEST(TestOS_Zip, CreateFactory) {
  auto factory = egg::ovum::os::zip::createFactory();
  ASSERT_NE(nullptr, factory);
}

TEST(TestOS_Zip, GetFactoryVersion) {
  // TODO keep up-to-date
  auto factory = egg::ovum::os::zip::createFactory();
  ASSERT_NE(nullptr, factory);
  ASSERT_EQ("9.1.15", factory->getVersion());
}

TEST(TestOS_Zip, OpenFile) {
  auto factory = egg::ovum::os::zip::createFactory();
  ASSERT_NE(nullptr, factory);
  auto zip = factory->openFile(egg::ovum::File::resolvePath("~/cpp/data/egg.zip"));
  ASSERT_NE(nullptr, zip);
}

TEST(TestOS_Zip, GetComment) {
  auto factory = egg::ovum::os::zip::createFactory();
  ASSERT_NE(nullptr, factory);
  auto zip = factory->openFile(egg::ovum::File::resolvePath("~/cpp/data/egg.zip"));
  ASSERT_NE(nullptr, zip);
  ASSERT_EQ("Twas brillig, and the slithy toves", zip->getComment());
}
