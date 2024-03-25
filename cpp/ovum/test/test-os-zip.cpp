#include "ovum/test.h"
#include "ovum/file.h"
#include "ovum/os-zip.h"

using namespace egg::ovum::os::zip;

TEST(TestOS_Zip, CreateFactory) {
  std::shared_ptr<IZipFactory> factory = createFactory();
  ASSERT_NE(nullptr, factory);
}

TEST(TestOS_Zip, GetFactoryVersion) {
  // TODO keep up-to-date
  std::shared_ptr<IZipFactory> factory = createFactory();
  ASSERT_NE(nullptr, factory);
  ASSERT_EQ("4.0.5", factory->getVersion());
}

TEST(TestOS_Zip, OpenFile) {
  std::shared_ptr<IZipFactory> factory = createFactory();
  ASSERT_NE(nullptr, factory);
  auto zip = factory->openFile(egg::ovum::File::resolvePath("~/cpp/data/egg.zip"));
  ASSERT_NE(nullptr, zip);
}

TEST(TestOS_Zip, GetComment) {
  std::shared_ptr<IZipFactory> factory = createFactory();
  ASSERT_NE(nullptr, factory);
  auto zip = factory->openFile(egg::ovum::File::resolvePath("~/cpp/data/egg.zip"));
  ASSERT_NE(nullptr, zip);
  ASSERT_EQ("Twas brillig, and the slithy toves", zip->getComment());
}
