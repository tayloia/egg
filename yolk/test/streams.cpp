#include "yolk/test.h"

// For ~/yolk/test/data/utf-8-demo*.txt
// As per https://www.cl.cam.ac.uk/~mgk25/ucs/examples/UTF-8-demo.txt
static const size_t expected_lengths[] = { 0,36,36,0,79,0,0,64,49,0,0,75,0,25,0,57,57,57,57,57,57,57,57,57,0,29,0,38,40,0,4,0,34,0,37,0,46,46,46,46,46,46,46,46,46,46,46,46,46,46,46,46,46,0,21,0,38,0,21,0,19,0,25,26,24,27,0,26,22,27,29,0,53,0,59,53,48,55,52,60,61,61,60,61,59,54,58,61,53,36,0,29,0,9,0,39,0,65,60,61,60,59,62,64,0,8,0,39,0,66,71,69,70,68,69,0,19,0,71,21,0,57,63,64,68,62,58,65,65,60,0,70,70,21,0,10,0,35,0,22,22,18,28,21,17,14,26,19,32,30,31,20,28,28,19,25,21,0,6,0,58,0,69,69,60,0,8,0,23,0,45,46,45,42,52,26,0,39,0,43,45,48,53,42,39,49,52,35,0,68,0,36,0,40,40,40,40,0,31,0,36,0,71,71,79,79,79,79,71,71,79,53,SIZE_MAX };

TEST(TestStreams, FileStreamIn) {
  egg::yolk::FileStream fsi("~/yolk/test/data/utf-8-demo.txt");
  std::stringstream oss;
  oss << fsi.rdbuf();
  ASSERT_EQ(14270u, oss.str().size());
}

TEST(TestStreams, FileStreamInMissing) {
  ASSERT_THROW(egg::yolk::FileStream("~/missing"), egg::yolk::Exception);
}

TEST(TestStreams, ByteStream) {
  egg::yolk::FileStream fs("~/yolk/test/data/utf-8-demo.txt");
  egg::yolk::ByteStream bs(fs, "utf-8-demo.txt");
  ASSERT_EQ("utf-8-demo.txt", bs.getResourceName());
  size_t size = 0;
  for (auto ch = bs.get(); ch >= 0; ch = bs.get()) {
    size++;
  }
  ASSERT_EQ(14270u, size);
  ASSERT_EQ(-1, bs.get());
}

TEST(TestByteStream, FileByteStream) {
  egg::yolk::FileByteStream fbs("~/yolk/test/data/utf-8-demo.txt");
  ASSERT_ENDSWITH(fbs.getResourceName(), "utf-8-demo.txt");
  size_t size = 0;
  for (auto ch = fbs.get(); ch >= 0; ch = fbs.get()) {
    size++;
  }
  ASSERT_EQ(14270u, size);
  ASSERT_EQ(-1, fbs.get());
}

TEST(TestByteStream, FileByteStreamMissing) {
  ASSERT_THROW(egg::yolk::FileByteStream("~/missing"), egg::yolk::Exception);
}

TEST(TestByteStream, StringByteStream) {
  egg::yolk::StringByteStream sbs("Hello World!");
  size_t size = 0;
  for (auto ch = sbs.get(); ch >= 0; ch = sbs.get()) {
    size++;
  }
  ASSERT_EQ(12u, size);
  ASSERT_EQ(-1, sbs.get());
}

TEST(TestStreams, FileCharStream) {
  egg::yolk::FileCharStream fcs("~/yolk/test/data/utf-8-demo.txt");
  ASSERT_ENDSWITH(fcs.getResourceName(), "utf-8-demo.txt");
  size_t size = 0;
  for (auto ch = fcs.get(); ch >= 0; ch = fcs.get()) {
    ASSERT_LE(ch, 0x10FFFF);
    size++;
  }
  ASSERT_EQ(7839u, size);
  ASSERT_EQ(-1, fcs.get());
}

TEST(TestStreams, FileCharStreamWithBOM) {
  egg::yolk::FileCharStream fcs("~/yolk/test/data/utf-8-demo.bom.txt");
  ASSERT_ENDSWITH(fcs.getResourceName(), "utf-8-demo.bom.txt");
  size_t size = 0;
  for (auto ch = fcs.get(); ch >= 0; ch = fcs.get()) {
    ASSERT_LE(ch, 0x10FFFF);
    size++;
  }
  ASSERT_EQ(7839u, size);
  ASSERT_EQ(-1, fcs.get());
}

TEST(TestStreams, StringCharStream) {
  egg::yolk::StringCharStream scs("Hello World!");
  std::u32string text;
  scs.slurp(text);
  ASSERT_EQ(12u, text.size());
}

TEST(TestStreams, StringCharStreamBad) {
  // See http://www.cl.cam.ac.uk/~mgk25/ucs/examples/UTF-8-test.txt
  std::u32string text;
  // Invalid UTF-8 encoding (unexpected continuation)
  ASSERT_THROW(egg::yolk::StringCharStream("\x80").slurp(text), egg::yolk::Exception);
  // Invalid UTF-8 encoding (truncated continuation)
  ASSERT_THROW(egg::yolk::StringCharStream("\xC0").slurp(text), egg::yolk::Exception);
  // Invalid UTF-8 encoding (invalid continuation)
  ASSERT_THROW(egg::yolk::StringCharStream("\xC0\x01").slurp(text), egg::yolk::Exception);
  // Invalid UTF-8 encoding (bad lead byte)
  ASSERT_THROW(egg::yolk::StringCharStream("\xFF").slurp(text), egg::yolk::Exception);
}

TEST(TestStreams, FileTextStream) {
  egg::yolk::FileTextStream fts("~/yolk/test/data/utf-8-demo.txt");
  ASSERT_ENDSWITH(fts.getResourceName(), "utf-8-demo.txt");
  size_t size = 0;
  for (auto ch = fts.get(); ch >= 0; ch = fts.get()) {
    ASSERT_LE(ch, 0x10FFFF);
    size++;
  }
  ASSERT_EQ(7839u, size);
  ASSERT_EQ(-1, fts.get());
}

TEST(TestStreams, StringTextStream) {
  egg::yolk::StringTextStream sts("one\n!two\nthree");
  std::string text;
  // "one"
  ASSERT_EQ(1u, sts.getCurrentLine());
  ASSERT_EQ(1u, sts.getCurrentColumn());
  ASSERT_TRUE(sts.readline(text));
  ASSERT_EQ("one", text);
  // "two"
  ASSERT_EQ(2u, sts.getCurrentLine());
  ASSERT_EQ(1u, sts.getCurrentColumn());
  ASSERT_EQ('!', sts.get());
  ASSERT_EQ(2u, sts.getCurrentLine());
  ASSERT_EQ(2u, sts.getCurrentColumn());
  ASSERT_TRUE(sts.readline(text));
  ASSERT_EQ("two", text);
  // "three"
  ASSERT_EQ(3u, sts.getCurrentLine());
  ASSERT_EQ(1u, sts.getCurrentColumn());
  ASSERT_TRUE(sts.readline(text));
  ASSERT_EQ("three", text);
  // EOF
  ASSERT_EQ(3u, sts.getCurrentLine());
  ASSERT_EQ(6u, sts.getCurrentColumn());
  ASSERT_FALSE(sts.readline(text));
  ASSERT_EQ("", text);
  ASSERT_EQ(-1, sts.get());
}

static size_t lastLine(const std::string& path) {
  egg::yolk::FileTextStream fts(path);
  while (fts.get() >= 0) {
    // Slurp the whole file
  }
  return fts.getCurrentLine();
}

TEST(TestStreams, FileTextStreamLastLine) {
  ASSERT_EQ(213u, lastLine("~/yolk/test/data/utf-8-demo.txt"));
  ASSERT_EQ(213u, lastLine("~/yolk/test/data/utf-8-demo.cr.txt"));
  ASSERT_EQ(213u, lastLine("~/yolk/test/data/utf-8-demo.lf.txt"));
}

TEST(TestStreams, FileTextStreamPeek) {
  egg::yolk::FileTextStream fts("~/yolk/test/data/utf-8-demo.txt");
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
  ASSERT_EQ(1u, fts.getCurrentLine());
  std::u32string text;
  size_t line;
  for (line = 0; fts.readline(text); ++line) {
    ASSERT_EQ(expected_lengths[line], text.size());
  }
  ASSERT_EQ(expected_lengths[line], SIZE_MAX);
}

TEST(TestStreams, FileTextStreamReadLine) {
  readLines("~/yolk/test/data/utf-8-demo.txt");
  readLines("~/yolk/test/data/utf-8-demo.bom.txt");
  readLines("~/yolk/test/data/utf-8-demo.cr.txt");
  readLines("~/yolk/test/data/utf-8-demo.lf.txt");
}

TEST(TestStreams, FileTextStreamSlurp) {
  std::string slurped;
  egg::yolk::FileTextStream("~/yolk/test/data/utf-8-demo.txt").slurp(slurped);
  ASSERT_EQ(14270u, slurped.size());
  slurped.clear();
  egg::yolk::FileTextStream("~/yolk/test/data/utf-8-demo.bom.txt").slurp(slurped);
  ASSERT_EQ(14270u, slurped.size());
  slurped.clear();
  egg::yolk::FileTextStream("~/yolk/test/data/utf-8-demo.cr.txt").slurp(slurped);
  ASSERT_EQ(14058u, slurped.size());
  slurped.clear();
  egg::yolk::FileTextStream("~/yolk/test/data/utf-8-demo.lf.txt").slurp(slurped);
  ASSERT_EQ(14058u, slurped.size());
  slurped.clear();
  egg::yolk::FileTextStream("~/yolk/test/data/utf-8-demo.txt").slurp(slurped, '\n');
  ASSERT_EQ(14058u, slurped.size());
  slurped.clear();
  egg::yolk::FileTextStream("~/yolk/test/data/utf-8-demo.bom.txt").slurp(slurped, '\n');
  ASSERT_EQ(14058u, slurped.size());
  slurped.clear();
  egg::yolk::FileTextStream("~/yolk/test/data/utf-8-demo.cr.txt").slurp(slurped, '\n');
  ASSERT_EQ(14058u, slurped.size());
  slurped.clear();
  egg::yolk::FileTextStream("~/yolk/test/data/utf-8-demo.lf.txt").slurp(slurped, '\n');
  ASSERT_EQ(14058u, slurped.size());
}

TEST(TestStreams, FileTextStreamSlurp32) {
  std::u32string slurped;
  egg::yolk::FileTextStream("~/yolk/test/data/utf-8-demo.txt").slurp(slurped);
  ASSERT_EQ(7839u, slurped.size());
  slurped.clear();
  egg::yolk::FileTextStream("~/yolk/test/data/utf-8-demo.bom.txt").slurp(slurped);
  ASSERT_EQ(7839u, slurped.size());
  slurped.clear();
  egg::yolk::FileTextStream("~/yolk/test/data/utf-8-demo.cr.txt").slurp(slurped);
  ASSERT_EQ(7627u, slurped.size());
  slurped.clear();
  egg::yolk::FileTextStream("~/yolk/test/data/utf-8-demo.lf.txt").slurp(slurped);
  ASSERT_EQ(7627u, slurped.size());
  slurped.clear();
  egg::yolk::FileTextStream("~/yolk/test/data/utf-8-demo.txt").slurp(slurped, '\n');
  ASSERT_EQ(7627u, slurped.size());
  slurped.clear();
  egg::yolk::FileTextStream("~/yolk/test/data/utf-8-demo.bom.txt").slurp(slurped, '\n');
  ASSERT_EQ(7627u, slurped.size());
  slurped.clear();
  egg::yolk::FileTextStream("~/yolk/test/data/utf-8-demo.cr.txt").slurp(slurped, '\n');
  ASSERT_EQ(7627u, slurped.size());
  slurped.clear();
  egg::yolk::FileTextStream("~/yolk/test/data/utf-8-demo.lf.txt").slurp(slurped, '\n');
  ASSERT_EQ(7627u, slurped.size());
}
