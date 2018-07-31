#include "yolk.h"

namespace {
  bool isEndOfLine(int ch) {
    return (ch == '\r') || (ch == '\n');
  }
  // See https://en.wikipedia.org/wiki/UTF-8
  int readContinuation(egg::yolk::ByteStream& stream, int value, size_t count) {
    do {
      auto b = stream.get();
      if (b < 0) {
        EGG_THROW("Invalid UTF-8 encoding (truncated continuation): " + stream.getResourceName());
      }
      b ^= 0x80;
      if (b > 0x3F) {
        EGG_THROW("Invalid UTF-8 encoding (invalid continuation): " + stream.getResourceName());
      }
      value = (value << 6) | b;
    } while (--count);
    return value;
  }
  int readCodepoint(egg::yolk::ByteStream& stream) {
    auto b = stream.get();
    if (b < 0x80) {
      // EOF or ASCII codepoint
      return b;
    }
    if (b < 0xC0) {
      EGG_THROW("Invalid UTF-8 encoding (unexpected continuation): " + stream.getResourceName());
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
    EGG_THROW("Invalid UTF-8 encoding (bad lead byte): " + stream.getResourceName());
  }
}

int egg::yolk::CharStream::get() {
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

void egg::yolk::CharStream::slurp(std::u32string& text) {
  for (auto ch = this->get(); ch >= 0; ch = this->get()) {
    text.push_back(char32_t(ch));
  }
}

bool egg::yolk::CharStream::rewind() {
  return this->bytes.rewind();
}

bool egg::yolk::TextStream::ensure(size_t count) {
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

int egg::yolk::TextStream::get() {
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

bool egg::yolk::TextStream::readline(std::string& text) {
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
      egg::utf::utf32_to_utf8(target, char32_t(ch));

    }
  } while (this->line == start);
  return true;
}

bool egg::yolk::TextStream::readline(std::u32string& text) {
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

void egg::yolk::TextStream::slurp(std::string& text, int eol) {
  auto target = std::back_inserter(text);
  if (eol < 0) {
    // Don't perform end-of-line substitution
    int ch = this->get();
    while (ch >= 0) {
      egg::utf::utf32_to_utf8(target, char32_t(ch));
      ch = this->get();
    }
  } else {
    // Perform end-of-line substitution
    auto curr = this->getCurrentLine();
    int ch = this->get();
    while (ch >= 0) {
      if (!isEndOfLine(ch)) {
        egg::utf::utf32_to_utf8(target, char32_t(ch));
      } else if (this->line != curr) {
        egg::utf::utf32_to_utf8(target, char32_t(eol));
        curr = this->line;
      }
      ch = this->get();
    }
  }
}

void egg::yolk::TextStream::slurp(std::u32string& text, int eol) {
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

bool egg::yolk::TextStream::rewind() {
  if (this->chars.rewind()) {
    this->upcoming.clear();
    this->line = 1;
    this->column = 1;
    return true;
  }
  return false;
}