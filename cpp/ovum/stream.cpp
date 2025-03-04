#include "ovum/ovum.h"
#include "ovum/eggbox.h"
#include "ovum/file.h"
#include "ovum/os-zip.h"
#include "ovum/stream.h"
#include "ovum/utf.h"

namespace {
  using namespace egg::ovum;

  bool isEndOfLine(int ch) {
    return (ch == '\r') || (ch == '\n');
  }

  int readContinuation(ByteStream& stream, int value, size_t count) {
    // See https://en.wikipedia.org/wiki/UTF-8
    do {
      auto b = stream.get();
      if (b < 0) {
        throw egg::ovum::Exception("Invalid UTF-8 encoding (truncated continuation): '{resource}'").with("resource", stream.getResourceName());
      }
      b ^= 0x80;
      if (b > 0x3F) {
        throw egg::ovum::Exception("Invalid UTF-8 encoding (invalid continuation): '{resource}'").with("resource", stream.getResourceName());
      }
      value = (value << 6) | b;
    } while (--count);
    return value;
  }

  int readCodepoint(ByteStream& stream) {
    auto b = stream.get();
    if (b < 0x80) {
      // EOF or ASCII codepoint
      return b;
    }
    if (b < 0xC0) {
      throw egg::ovum::Exception("Invalid UTF-8 encoding (unexpected continuation): '{resource}'").with("resource", stream.getResourceName());
    }
    if (b < 0xE0) {
      // One continuation byte
      return readContinuation(stream, b & 0x1F, 1);
    }
    if (b < 0xF0) {
      // Two continuation bytes
      return readContinuation(stream, b & 0x0F, 2);
    }
    if (b < 0xF8) {
      // Three continuation bytes
      return readContinuation(stream, b & 0x07, 3);
    }
    throw egg::ovum::Exception("Invalid UTF-8 encoding (bad lead byte): '{resource}'").with("resource", stream.getResourceName());
  }
}

EggboxByteStream::EggboxByteStream(const std::string& resource, std::shared_ptr<IEggboxFileEntry>&& entry)
  : ByteStream(entry->getReadStream(), resource),
    entry(std::move(entry)) {
  assert(this->entry != nullptr);
}

EggboxByteStream::EggboxByteStream(IEggbox& eggbox, const std::string& subpath)
  : EggboxByteStream(eggbox.getResourcePath(&subpath), eggbox.getFileEntry(subpath)) {
}

EggboxByteStream::~EggboxByteStream() {
  // Required to be in this source file to have access to the destructor of 'Impl'
}

int egg::ovum::CharStream::get() {
  auto codepoint = readCodepoint(this->bytes);
  if (this->swallowBOM) {
    // See https://en.wikipedia.org/wiki/Byte_order_mark
    this->swallowBOM = false;
    if (codepoint == 0xFEFF) {
      codepoint = readCodepoint(this->bytes);
    }
  }
  return codepoint;
}

void egg::ovum::CharStream::slurp(std::u32string& text) {
  for (auto ch = this->get(); ch >= 0; ch = this->get()) {
    text.push_back(char32_t(ch));
  }
}

bool egg::ovum::CharStream::rewind() {
  return this->bytes.rewind();
}

bool egg::ovum::TextStream::ensure(size_t count) {
  int ch = 0;
  if (this->upcoming.empty()) {
    // This is our first access
    ch = this->chars.get();
    this->upcoming.push_back(ch);
  }
  assert(!this->upcoming.empty());
  while (this->upcoming.size() < count) {
    if (ch < 0) {
      return false;
    }
    ch = this->chars.get();
    this->upcoming.push_back(ch);
  }
  return true;
}

int egg::ovum::TextStream::get() {
  if (!this->ensure(2)) {
    // There's only the EOF marker left
    assert(this->upcoming.size() == 1);
    assert(this->upcoming.front() < 0);
    return -1;
  }
  auto result = this->upcoming.front();
  this->upcoming.pop_front();
  if (isEndOfLine(result)) {
    // Newline
    if ((result == '\r') && (this->upcoming.front() == '\n')) {
      // Delay the line advance until next time
      return '\r';
    }
    this->line++;
    this->column = 1;
  } else if (result >= 0) {
    // Any other character
    this->column++;
  }
  return result;
}

bool egg::ovum::TextStream::readline(std::string& text) {
  text.clear();
  if (this->peek() < 0) {
    // Already at EOF
    return false;
  }
  auto target = std::back_inserter(text);
  auto start = this->line;
  do {
    auto ch = this->get();
    if (ch < 0) {
      break;
    }
    if (!isEndOfLine(ch)) {
      egg::ovum::UTF32::toUTF8(target, char32_t(ch));

    }
  } while (this->line == start);
  return true;
}

bool egg::ovum::TextStream::readline(std::u32string& text) {
  text.clear();
  if (this->peek() < 0) {
    // Already at EOF
    return false;
  }
  auto start = this->line;
  do {
    auto ch = this->get();
    if (ch < 0) {
      break;
    }
    if (!isEndOfLine(ch)) {
      text.push_back(char32_t(ch));
    }
  } while (this->line == start);
  return true;
}

void egg::ovum::TextStream::slurp(std::string& text, int eol) {
  auto target = std::back_inserter(text);
  if (eol < 0) {
    // Don't perform end-of-line substitution
    int ch = this->get();
    while (ch >= 0) {
      egg::ovum::UTF32::toUTF8(target, char32_t(ch));
      ch = this->get();
    }
  } else {
    // Perform end-of-line substitution
    auto curr = this->getCurrentLine();
    int ch = this->get();
    while (ch >= 0) {
      if (!isEndOfLine(ch)) {
        egg::ovum::UTF32::toUTF8(target, char32_t(ch));
      } else if (this->line != curr) {
        egg::ovum::UTF32::toUTF8(target, char32_t(eol));
        curr = this->line;
      }
      ch = this->get();
    }
  }
}

void egg::ovum::TextStream::slurp(std::u32string& text, int eol) {
  if (eol < 0) {
    // Don't perform end-of-line substitution
    int ch = this->get();
    while (ch >= 0) {
      text.push_back(char32_t(ch));
      ch = this->get();
    }
  } else {
    // Perform end-of-line substitution
    auto curr = this->getCurrentLine();
    int ch = this->get();
    while (ch >= 0) {
      if (!isEndOfLine(ch)) {
        text.push_back(char32_t(ch));
      } else if (this->line != curr) {
        text.push_back(char32_t(eol));
        curr = this->line;
      }
      ch = this->get();
    }
  }
}

bool egg::ovum::TextStream::rewind() {
  if (this->chars.rewind()) {
    this->upcoming.clear();
    this->line = 1;
    this->column = 1;
    return true;
  }
  return false;
}
