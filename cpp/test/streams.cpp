#include "test.h"

// For ~/cpp/test/data/utf-8-demo*.txt
// As per https://www.cl.cam.ac.uk/~mgk25/ucs/examples/UTF-8-demo.txt
static const size_t expected_lengths[] = { 0,36,36,0,79,0,0,64,49,0,0,75,0,25,0,57,57,57,57,57,57,57,57,57,0,29,0,38,40,0,4,0,34,0,37,0,46,46,46,46,46,46,46,46,46,46,46,46,46,46,46,46,46,0,21,0,38,0,21,0,19,0,25,26,24,27,0,26,22,27,29,0,53,0,59,53,48,55,52,60,61,61,60,61,59,54,58,61,53,36,0,29,0,9,0,39,0,65,60,61,60,59,62,64,0,8,0,39,0,66,71,69,70,68,69,0,19,0,71,21,0,57,63,64,68,62,58,65,65,60,0,70,70,21,0,10,0,35,0,22,22,18,28,21,17,14,26,19,32,30,31,20,28,28,19,25,21,0,6,0,58,0,69,69,60,0,8,0,23,0,45,46,45,42,52,26,0,39,0,43,45,48,53,42,39,49,52,35,0,68,0,36,0,40,40,40,40,0,31,0,36,0,71,71,79,79,79,79,71,71,79,53,SIZE_MAX };

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
  ASSERT_EQ("utf-8-demo.txt", bs.getFilename());
  size_t size = 0;
  for (auto ch = bs.get(); ch >= 0; ch = bs.get()) {
    size++;
  }
  ASSERT_EQ(14270, size);
  ASSERT_EQ(-1, bs.get());
}

TEST(TestByteStream, FileByteStream) {
  egg::yolk::FileByteStream fbs("~/cpp/test/data/utf-8-demo.txt");
  ASSERT_ENDSWITH(fbs.getFilename(), "utf-8-demo.txt");
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
  ASSERT_ENDSWITH(fcs.getFilename(), "utf-8-demo.txt");
  size_t size = 0;
  for (auto ch = fcs.get(); ch >= 0; ch = fcs.get()) {
    ASSERT_LE(ch, 0x10FFFF);
    size++;
  }
  ASSERT_EQ(7839, size);
  ASSERT_EQ(-1, fcs.get());
}

TEST(TestStreams, FileCharStreamWithBOM) {
  egg::yolk::FileCharStream fcs("~/cpp/test/data/utf-8-demo.bom.txt");
  ASSERT_ENDSWITH(fcs.getFilename(), "utf-8-demo.bom.txt");
  size_t size = 0;
  for (auto ch = fcs.get(); ch >= 0; ch = fcs.get()) {
    ASSERT_LE(ch, 0x10FFFF);
    size++;
  }
  ASSERT_EQ(7839, size);
  ASSERT_EQ(-1, fcs.get());
}

TEST(TestStreams, FileTextStream) {
  egg::yolk::FileTextStream fts("~/cpp/test/data/utf-8-demo.txt");
  ASSERT_ENDSWITH(fts.getFilename(), "utf-8-demo.txt");
  size_t size = 0;
  for (auto ch = fts.get(); ch >= 0; ch = fts.get()) {
    ASSERT_LE(ch, 0x10FFFF);
    size++;
  }
  ASSERT_EQ(7839, size);
  ASSERT_EQ(-1, fts.get());
}

static size_t lastLine(const std::string& path) {
  egg::yolk::FileTextStream fts(path);
  while (fts.get() >= 0) {
    // Slurp the whole file
  }
  return fts.getCurrentLine();
}

TEST(TestStreams, FileTextStreamLastLine) {
  ASSERT_EQ(213, lastLine("~/cpp/test/data/utf-8-demo.txt"));
  ASSERT_EQ(213, lastLine("~/cpp/test/data/utf-8-demo.cr.txt"));
  ASSERT_EQ(213, lastLine("~/cpp/test/data/utf-8-demo.lf.txt"));
}

TEST(TestStreams, FileTextStreamPeek) {
  egg::yolk::FileTextStream fts("~/cpp/test/data/utf-8-demo.txt");
  ASSERT_EQ('\r', fts.peek());
  ASSERT_EQ('\n', fts.peek(1));
  ASSERT_EQ('U', fts.peek(2));
  ASSERT_EQ('T', fts.peek(3));
  ASSERT_EQ('F', fts.peek(4));
  ASSERT_EQ('-', fts.peek(5));
  ASSERT_EQ('8', fts.peek(6));
}

static void readLines(const std::string& path) {
  egg::yolk::FileTextStream fts(path);
  ASSERT_EQ(1, fts.getCurrentLine());
  std::vector<int> text;
  size_t line;
  for (line = 0; fts.readline(text); ++line) {
    ASSERT_EQ(expected_lengths[line], text.size());
  }
  ASSERT_EQ(expected_lengths[line], SIZE_MAX);
}

TEST(TestStreams, FileTextStreamReadLine) {
  readLines("~/cpp/test/data/utf-8-demo.txt");
  readLines("~/cpp/test/data/utf-8-demo.bom.txt");
  readLines("~/cpp/test/data/utf-8-demo.cr.txt");
  readLines("~/cpp/test/data/utf-8-demo.lf.txt");
}

TEST(TestStreams, FileTextStreamSlurp) {
  std::string slurped;
  egg::yolk::FileTextStream("~/cpp/test/data/utf-8-demo.txt").slurp(slurped);
  ASSERT_EQ(14270, slurped.size());
  slurped.clear();
  egg::yolk::FileTextStream("~/cpp/test/data/utf-8-demo.bom.txt").slurp(slurped);
  ASSERT_EQ(14270, slurped.size());
  slurped.clear();
  egg::yolk::FileTextStream("~/cpp/test/data/utf-8-demo.cr.txt").slurp(slurped);
  ASSERT_EQ(14058, slurped.size());
  slurped.clear();
  egg::yolk::FileTextStream("~/cpp/test/data/utf-8-demo.lf.txt").slurp(slurped);
  ASSERT_EQ(14058, slurped.size());
  slurped.clear();
  egg::yolk::FileTextStream("~/cpp/test/data/utf-8-demo.txt").slurp(slurped, '\n');
  ASSERT_EQ(14058, slurped.size());
  slurped.clear();
  egg::yolk::FileTextStream("~/cpp/test/data/utf-8-demo.bom.txt").slurp(slurped, '\n');
  ASSERT_EQ(14058, slurped.size());
  slurped.clear();
  egg::yolk::FileTextStream("~/cpp/test/data/utf-8-demo.cr.txt").slurp(slurped, '\n');
  ASSERT_EQ(14058, slurped.size());
  slurped.clear();
  egg::yolk::FileTextStream("~/cpp/test/data/utf-8-demo.lf.txt").slurp(slurped, '\n');
  ASSERT_EQ(14058, slurped.size());
}

TEST(TestStreams, FileTextStreamSlurp32) {
  std::u32string slurped;
  egg::yolk::FileTextStream("~/cpp/test/data/utf-8-demo.txt").slurp(slurped);
  ASSERT_EQ(7839, slurped.size());
  slurped.clear();
  egg::yolk::FileTextStream("~/cpp/test/data/utf-8-demo.bom.txt").slurp(slurped);
  ASSERT_EQ(7839, slurped.size());
  slurped.clear();
  egg::yolk::FileTextStream("~/cpp/test/data/utf-8-demo.cr.txt").slurp(slurped);
  ASSERT_EQ(7627, slurped.size());
  slurped.clear();
  egg::yolk::FileTextStream("~/cpp/test/data/utf-8-demo.lf.txt").slurp(slurped);
  ASSERT_EQ(7627, slurped.size());
  slurped.clear();
  egg::yolk::FileTextStream("~/cpp/test/data/utf-8-demo.txt").slurp(slurped, '\n');
  ASSERT_EQ(7627, slurped.size());
  slurped.clear();
  egg::yolk::FileTextStream("~/cpp/test/data/utf-8-demo.bom.txt").slurp(slurped, '\n');
  ASSERT_EQ(7627, slurped.size());
  slurped.clear();
  egg::yolk::FileTextStream("~/cpp/test/data/utf-8-demo.cr.txt").slurp(slurped, '\n');
  ASSERT_EQ(7627, slurped.size());
  slurped.clear();
  egg::yolk::FileTextStream("~/cpp/test/data/utf-8-demo.lf.txt").slurp(slurped, '\n');
  ASSERT_EQ(7627, slurped.size());
}
