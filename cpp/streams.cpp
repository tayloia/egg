#include "yolk.h"

namespace {
  // See https://en.wikipedia.org/wiki/UTF-8
  int readContinuation(egg::yolk::ByteStream& stream, int value, size_t count) {
    do {
      auto b = stream.get();
      if (b < 0) {
        EGG_THROW("Invalid UTF-8 encoding (truncated continuation): " + stream.filename());
      }
      b ^= 0x80;
      if (b > 0x3F) {
        EGG_THROW("Invalid UTF-8 encoding (invalid continuation): " + stream.filename());
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
      EGG_THROW("Invalid UTF-8 encoding (unexpected continuation): " + stream.filename());
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
    EGG_THROW("Invalid UTF-8 encoding (bad lead byte): " + stream.filename());
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

int egg::yolk::TextStream::get() {
  if (this->line == 0) {
    // This is our first read
    this->line = 1;
    this->column = 1;
    this->next = this->chars.get();
  }
  int current = this->next;
  if (current < 0) {
    // Already at EOF
    return -1;
  }
  this->next = this->chars.get();
  switch (current) {
  case '\r':
    if (this->next != '\n') {
      // Don't delay the line advance until next time
      this->line++;
      this->column = 1;
    }
    return '\r';
  case '\n':
    // Newline
    this->line++;
    this->column = 1;
    return '\n';
  }
  return current;
}

void egg::yolk::TextStream::initialize() {
  if (this->line == 0) {
    // This is our first access
    this->line = 1;
    this->column = 1;
    this->next = this->chars.get();
  }
}
