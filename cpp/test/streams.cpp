#include "yolk.h"

TEST(TestStreams, FileStreamIn) {
  egg::yolk::FileStream fsi("~/cpp/test/data/utf-8-demo.txt");
  std::stringstream ss;
  ss << fsi.rdbuf();
  ASSERT_EQ(14270, ss.str().size());
}

TEST(TestStreams, FileStreamInMissing) {
  ASSERT_THROW(egg::yolk::FileStream("~/missing"), egg::yolk::Exception);
}

TEST(TestStreams, ByteStream) {
  egg::yolk::FileStream fs("~/cpp/test/data/utf-8-demo.txt");
  egg::yolk::ByteStream bs(fs, "utf-8-demo.txt");
  ASSERT_EQ("utf-8-demo.txt", bs.filename());
  size_t size = 0;
  for (auto ch = bs.get(); ch >= 0; ch = bs.get()) {
    size++;
  }
  ASSERT_EQ(14270, size);
  ASSERT_EQ(-1, bs.get());
}

TEST(TestByteStream, FileByteStream) {
  egg::yolk::FileByteStream fbs("~/cpp/test/data/utf-8-demo.txt");
  ASSERT_ENDSWITH(fbs.filename(), "utf-8-demo.txt");
  size_t size = 0;
  for (auto ch = fbs.get(); ch >= 0; ch = fbs.get()) {
    size++;
  }
  ASSERT_EQ(14270, size);
  ASSERT_EQ(-1, fbs.get());
}

TEST(TestByteStream, FileByteStreamMissing) {
  ASSERT_THROW(egg::yolk::FileByteStream("~/missing"), egg::yolk::Exception);
}

TEST(TestStreams, FileCharStream) {
  egg::yolk::FileCharStream fcs("~/cpp/test/data/utf-8-demo.txt");
  ASSERT_ENDSWITH(fcs.filename(), "utf-8-demo.txt");
  size_t size = 0;
  for (auto ch = fcs.get(); ch >= 0; ch = fcs.get()) {
    ASSERT_LE(ch, 0x10FFFF);
    size++;
  }
  ASSERT_EQ(7839, size);
  ASSERT_EQ(-1, fcs.get());
}
