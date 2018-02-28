#include "yolk.h"

TEST(TestStreams, FileStreamIn) {
  egg::yolk::FileStream fsi("~/cpp/test/data/utf-8-demo.txt");
  std::stringstream ss;
  ss << fsi.rdbuf();
  EXPECT_EQ(14270, ss.str().size());
}

TEST(TestStreams, FileStreamInMissing) {
  EXPECT_THROW(egg::yolk::FileStream("~/missing"), std::runtime_error);
}

TEST(TestStreams, ByteStreamIn) {
  egg::yolk::FileStream fsi("~/cpp/test/data/utf-8-demo.txt");
  egg::yolk::ByteStream bsi(fsi, "utf-8-demo.txt");
  EXPECT_EQ("utf-8-demo.txt", bsi.filename());
  size_t size = 0;
  for (auto ch = bsi.get(); ch >= 0; ch = bsi.get()) {
    size++;
  }
  EXPECT_EQ(14270, size);
  EXPECT_EQ(-1, bsi.get());
}

TEST(TestByteStream, FileByteStreamIn) {
  egg::yolk::FileByteStream fbsi("~/cpp/test/data/utf-8-demo.txt");
  size_t size = 0;
  for (auto ch = fbsi.get(); ch >= 0; ch = fbsi.get()) {
    size++;
  }
  EXPECT_EQ(14270, size);
  EXPECT_EQ(-1, fbsi.get());
}

TEST(TestByteStream, FileByteStreamInMissing) {
  EXPECT_THROW(egg::yolk::FileByteStream("~/missing"), std::runtime_error);
}
