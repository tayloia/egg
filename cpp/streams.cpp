#include "yolk.h"

namespace {
  int readUTF8(egg::yolk::ByteStream& stream, int value, size_t count) {
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
}

int egg::yolk::CharStream::get() {
  auto b = this->bytes.get();
  this->allowBOM = false;
  if (b < 0x80) {
    // EOF or ASCII codepoint
    return b;
  }
  if (b < 0xC0) {
    EGG_THROW("Invalid UTF-8 encoding (unexpected continuation): " + this->filename());
  }
  if (b < 0xE0) {
    return readUTF8(this->bytes, b & 0x0F, 1);
  }
  if (b < 0xF0) {
    return readUTF8(this->bytes, b & 0x07, 2);
  }
  if (b < 0xF8) {
    return readUTF8(this->bytes, b & 0x03, 3);
  }
  EGG_THROW("Invalid UTF-8 encoding (bad lead byte): " + this->filename());
}
