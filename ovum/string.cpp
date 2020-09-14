#include "ovum/ovum.h"
#include "ovum/utf.h"

#include <algorithm>

namespace {
  using namespace egg::ovum;

  const uint8_t emptyByte = 0;
  const UTF8 emptyReader(&emptyByte, &emptyByte, 0);

  UTF8 readerBegin(const String& s) {
    auto* p = s.get();
    if (p == nullptr) {
      return emptyReader;
    }
    return UTF8(p->begin(), p->end(), 0);
  }

  UTF8 readerEnd(const String& s) {
    auto* p = s.get();
    if (p == nullptr) {
      return emptyReader;
    }
    return UTF8(p->begin(), p->end(), p->bytes());
  }

  UTF8 readerIndex(const String& s, size_t index) {
    auto* p = s.get();
    if (p == nullptr) {
      return emptyReader;
    }
    auto length = s.length();
    if (index >= length) {
      // Go to the very end
      return readerEnd(s);
    }
    if (index > (length >> 1)) {
      // We're closer to the end of the string
      auto reader = readerEnd(s);
      reader.skipBackward(length - index);
      return reader;
    }
    auto reader = readerBegin(s);
    reader.skipForward(index);
    return reader;
  }

  bool iterationMatch(UTF8 lhsReader, UTF8 rhsReader, size_t count, bool compensateForBackwards) {
    // OPTIMIZE using std::memcmp
    // Note reader parameters passed-by-value
    assert(count > 0);
    if (compensateForBackwards) {
      // Compensate for the lhs originally reading backwards
      lhsReader.forward();
    }
    char32_t lhsCodePoint;
    char32_t rhsCodePoint;
    while (--count > 0) {
      if (!lhsReader.forward(lhsCodePoint) || !rhsReader.forward(rhsCodePoint) || (lhsCodePoint != rhsCodePoint)) {
        return false;
      }
    }
    return true;
  }

  int64_t indexOfCodePointByIteration(const String& haystack, char32_t needle, size_t fromIndex) {
    // Iterate around the haystack for the needle
    auto haystackReader = readerIndex(haystack, fromIndex);
    int64_t index = int64_t(fromIndex);
    char32_t haystackCodePoint;
    while (haystackReader.forward(haystackCodePoint)) {
      if (haystackCodePoint == needle) {
        return index;
      }
      index++;
    }
    return -1; // Not found
  }

  int64_t indexOfStringByIteration(const String& haystack, const String& needle, size_t fromIndex) {
    // Iterate around the haystack for the needle
    auto needleLength = needle.length();
    assert(needleLength > 0);
    auto needleReader = readerBegin(needle);
    char32_t needleCodePoint;
    if (needleReader.forward(needleCodePoint)) {
      int64_t index = 0;
      auto haystackReader = readerIndex(haystack, fromIndex);
      char32_t haystackCodePoint;
      while (haystackReader.forward(haystackCodePoint)) {
        if ((haystackCodePoint == needleCodePoint) && iterationMatch(haystackReader, needleReader, needleLength, false)) {
          return index;
        }
        index++;
      }
    }
    return -1; // Not found
  }

  int64_t lastIndexOfCodePointByIteration(const String& haystack, char32_t needle, size_t fromIndex) {
    // Iterate around the haystack for the needle from back-to-front
    auto haystackReader = readerIndex(haystack, fromIndex);
    auto index = int64_t(std::min(fromIndex, haystack.length()));
    auto haystackCodePoint = needle;
    while (haystackReader.backward(haystackCodePoint)) {
      index--;
      if (haystackCodePoint == needle) {
        return index;
      }
    }
    return -1; // Not found
  }

  int64_t lastIndexOfStringByIteration(const String& haystack, const String& needle, size_t fromIndex) {
    // Iterate around the haystack for the needle from back-to-front
    auto needleLength = needle.length();
    assert(needleLength > 0);
    auto needleReader = readerBegin(needle);
    char32_t needleCodePoint;
    if (needleReader.forward(needleCodePoint)) {
      auto index = int64_t(std::min(fromIndex, haystack.length()));
      auto haystackReader = readerIndex(haystack, fromIndex);
      char32_t haystackCodePoint;
      while (haystackReader.backward(haystackCodePoint)) {
        index--;
        if ((haystackCodePoint == needleCodePoint) && iterationMatch(haystackReader, needleReader, needleLength, true)) {
          return index;
        }
      }
    }
    return -1; // Not found
  }

  void splitPositive(std::vector<String>& dst, const String& src, const String& separator, size_t limit) {
    // Unlike the original parameter, 'limit' is the maximum number of SPLITS to perform
    // OPTIMIZE
    assert(dst.size() == 0);
    size_t begin = 0;
    assert(limit > 0);
    if (separator.empty()) {
      // Split into codepoints
      do {
        auto cp = src.codePointAt(begin);
        if (cp < 0) {
          return; // Don't add a trailing empty string
        }
        dst.push_back(String::fromCodePoint(char32_t(cp)));
      } while (++begin < limit);
    } else {
      // Split by string
      assert(separator.length() > 0);
      auto index = src.indexOfString(separator, 0);
      while (index >= 0) {
        dst.emplace_back(src.substring(begin, size_t(index)));
        begin = size_t(index) + separator.length();
        if (--limit == 0) {
          break;
        }
        index = src.indexOfString(separator, begin);
      }
    }
    dst.emplace_back(src.substring(begin, SIZE_MAX));
  }

  void splitNegative(std::vector<String>& dst, const String& src, const String& separator, size_t limit) {
    // Unlike the original parameter, 'limit' is the maximum number of SPLITS to perform
    // OPTIMIZE
    assert(dst.size() == 0);
    size_t end = src.length();
    assert(limit > 0);
    auto length = separator.length();
    if (length == 0) {
      // Split into codepoints
      do {
        auto cp = src.codePointAt(--end);
        if (cp < 0) {
          std::reverse(dst.begin(), dst.end());
          return; // Don't add a trailing empty string
        }
        dst.push_back(String::fromCodePoint(char32_t(cp)));
      } while (--limit > 0);
    } else {
      // Split by string
      auto index = src.lastIndexOfString(separator, SIZE_MAX);
      while (index >= 0) {
        dst.emplace_back(src.substring(size_t(index) + length, end));
        end = size_t(index);
        if ((end < length) || (--limit == 0)) {
          break;
        }
        index = src.lastIndexOfString(separator, end - length);
      }
    }
    dst.emplace_back(src.substring(0, end));
    std::reverse(dst.begin(), dst.end());
  }

  void splitString(std::vector<String>& result, const String& haystack, const String& needle, int64_t limit) {
    assert(limit != 0);
    if (limit > 0) {
      // Split from the beginning
      if (uint64_t(limit) < uint64_t(SIZE_MAX)) {
        splitPositive(result, haystack, needle, size_t(limit - 1));
      } else {
        splitPositive(result, haystack, needle, SIZE_MAX);
      }
    } else {
      // Split from the end
      if (uint64_t(-limit) < uint64_t(SIZE_MAX)) {
        splitNegative(result, haystack, needle, size_t(-1 - limit));
      } else {
        splitNegative(result, haystack, needle, SIZE_MAX);
      }
    }
  }

  class StringFallbackAllocator final : public IAllocator {
    StringFallbackAllocator(const StringFallbackAllocator&) = delete;
    StringFallbackAllocator& operator=(const StringFallbackAllocator&) = delete;
  private:
    Atomic<int64_t> atomic;
  public:
    StringFallbackAllocator() : atomic(0) {
    }
    ~StringFallbackAllocator() {
      // Make sure all our strings have been destroyed when the process exits
      // VVIBBLE assert(this->atomic.get() == 0);
    }
    virtual void* allocate(size_t bytes, size_t alignment) override {
      this->atomic.increment();
      return AllocatorDefaultPolicy::memalloc(bytes, alignment);
    }
    virtual void deallocate(void* allocated, size_t alignment) override {
      assert(allocated != nullptr);
      this->atomic.decrement();
      AllocatorDefaultPolicy::memfree(allocated, alignment);
    }
    virtual bool statistics(Statistics&) const override {
      return false;
    }
  };

  const IMemory* createContiguous(IAllocator* allocator, const void* buffer, size_t bytes, size_t codepoints = SIZE_MAX) {
    // TODO detect malformed/overlong/etc
    if ((buffer == nullptr) || (bytes == 0)) {
      return nullptr;
    }
    auto* utf8 = static_cast<const uint8_t*>(buffer);
    if (codepoints == SIZE_MAX) {
      codepoints = UTF8::measure(utf8, utf8 + bytes);
    }
    if (codepoints > bytes) {
      throw std::invalid_argument("String: Invalid UTF-8 input data");
    }
    if (allocator == nullptr) {
      static StringFallbackAllocator fallback;
      allocator = &fallback;
    }
    auto* memory = allocator->create<MemoryContiguous>(bytes, *allocator, bytes, IMemory::Tag{ codepoints });
    assert(memory != nullptr);
    std::memcpy(memory->base(), utf8, bytes);
    return memory;
  }

  const IMemory* createContiguous(IAllocator* allocator, const char* utf8) {
    return (utf8 == nullptr) ? nullptr : createContiguous(allocator, utf8, std::strlen(utf8));
  }
}

egg::ovum::String::String(const char* utf8)
  : Memory(createContiguous(nullptr, utf8)) {
  // We've got to create this string without an allocator, so use a fallback
}

egg::ovum::String::String(const std::string& utf8, size_t codepoints)
  : Memory(createContiguous(nullptr, utf8.data(), utf8.size(), codepoints)) {
  // We've got to create this string without an allocator, so use a fallback
}

bool egg::ovum::String::validate() const {
  auto* memory = this->get();
  if (memory == nullptr) {
    return true;
  }
  auto codepoints = size_t(memory->tag().u);
  auto bytes = memory->bytes();
  return (codepoints >= ((bytes + 3) / 4)) && (codepoints <= bytes);
}

size_t egg::ovum::String::length() const {
  auto* memory = this->get();
  if (memory == nullptr) {
    return 0;
  }
  auto codepoints = size_t(memory->tag().u);
  assert(codepoints >= ((memory->bytes() + 3) / 4));
  assert(codepoints <= memory->bytes());
  return codepoints;
}

int32_t egg::ovum::String::codePointAt(size_t index) const {
  auto reader = readerIndex(*this, index);
  char32_t codepoint = 0;
  return reader.forward(codepoint) ? int32_t(codepoint) : -1;
}

bool egg::ovum::String::equals(const char* utf8) const {
  // Ordinal comparison (note: cannot handle NUL characters in UTF8 strings)
  auto* memory = this->get();
  if (memory == nullptr) {
    return (utf8 == nullptr) || (*utf8 == '\0');
  }
  auto* p = memory->begin();
  auto* q = memory->end();
  if (utf8 != nullptr) {
    while (*utf8 != '\0') {
      if ((p >= q) || (*p++ != uint8_t(*utf8++))) {
        return false;
      }
    }
  }
  return (p == q);
}

bool egg::ovum::String::equals(const String& other) const {
  // Ordinal comparison
  return Memory::equals(this->get(), other.get());
}

int64_t egg::ovum::String::hash() const {
  // See https://docs.oracle.com/javase/6/docs/api/java/lang/String.html#hashCode()
  int64_t i = 0;
  auto reader = readerBegin(*this);
  char32_t codepoint = 0;
  while (reader.forward(codepoint)) {
    i = i * 31 + int64_t(uint32_t(codepoint));
  }
  return i;
}

int64_t egg::ovum::String::compareTo(const String& other) const {
  // Ordinal comparison via UTF8
  auto* lhs = this->get();
  if (lhs == nullptr) {
    return other.empty() ? 0 : -1;
  }
  auto* rhs = other.get();
  if (rhs == nullptr) {
    return this->empty() ? 0 : 1;
  }
  auto lsize = lhs->bytes();
  auto rsize = rhs->bytes();
  int cmp;
  if (lsize < rsize) {
    cmp = std::memcmp(lhs->begin(), rhs->begin(), lsize);
    if (cmp == 0) {
      return -1;
    }
  } else if (lsize > rsize) {
    cmp = std::memcmp(lhs->begin(), rhs->begin(), rsize);
    if (cmp == 0) {
      return 1;
    }
  } else {
    cmp = std::memcmp(lhs->begin(), rhs->begin(), rsize);
  }
  return int64_t(cmp > 0) - int64_t(cmp < 0); // ensure {-1,0,+1}
}

bool egg::ovum::String::contains(const String& needle) const {
  return this->indexOfString(needle) >= 0;
}

bool egg::ovum::String::startsWith(const String& needle) const {
  if (needle.empty()) {
    return true;
  }
  auto* haystack = this->get();
  if (haystack == nullptr) {
    return false;
  }
  auto bytes = needle->bytes();
  if (haystack->bytes() < bytes) {
    return false;
  }
  return std::memcmp(haystack->begin(), needle->begin(), bytes) == 0;
}

bool egg::ovum::String::endsWith(const String& needle) const {
  if (needle.empty()) {
    return true;
  }
  auto* haystack = this->get();
  if (haystack == nullptr) {
    return false;
  }
  auto bytes = needle->bytes();
  if (haystack->bytes() < bytes) {
    return false;
  }
  return std::memcmp(haystack->end() - bytes, needle->begin(), bytes) == 0;
}

int64_t egg::ovum::String::indexOfCodePoint(char32_t codepoint, size_t fromIndex) const {
  return indexOfCodePointByIteration(*this, codepoint, fromIndex);
}

int64_t egg::ovum::String::indexOfString(const String& needle, size_t fromIndex) const {
  switch (needle.length()) {
  case 0:
    return (fromIndex <= this->length()) ? int64_t(fromIndex) : -1;
  case 1:
    return indexOfCodePointByIteration(*this, char32_t(needle.codePointAt(0)), fromIndex);
  }
  return indexOfStringByIteration(*this, needle, fromIndex);
}

int64_t egg::ovum::String::lastIndexOfCodePoint(char32_t codepoint, size_t fromIndex) const {
  return lastIndexOfCodePointByIteration(*this, codepoint, fromIndex);
}

int64_t egg::ovum::String::lastIndexOfString(const String& needle, size_t fromIndex) const {
  switch (needle.length()) {
  case 0:
    return int64_t(std::min(fromIndex, this->length()));
  case 1:
    return lastIndexOfCodePointByIteration(*this, char32_t(needle.codePointAt(0)), fromIndex);
  }
  return lastIndexOfStringByIteration(*this, needle, fromIndex);
}

egg::ovum::String egg::ovum::String::replace(const String& needle, const String& replacement, int64_t occurrences) const {
  // One replacement requires splitting into two parts, and so on
  if (occurrences > 0) {
    if (occurrences < INT64_MAX) {
      occurrences++;
    }
  } else if (occurrences < 0) {
    if (occurrences > INT64_MIN) {
      occurrences--;
    }
  } else {
    // No replacements required
    return *this;
  }
  if (needle == replacement) {
    // No effect on the string
    return *this;
  }
  std::vector<String> part;
  if (!this->empty()) {
    splitString(part, *this, needle, occurrences);
    auto parts = part.size();
    assert(parts > 0);
    if (parts == 1) {
      // No replacement occurred
      assert(part[0].length() == this->length());
      return *this;
    }
  }
  return replacement.join(part);
}

egg::ovum::String egg::ovum::String::substring(size_t begin, size_t end) const {
  end = std::min(end, this->length());
  if (begin >= end) {
    // Empty string
    return String();
  }
  auto codepoints = size_t(end - begin);
  if (codepoints >= this->length()) {
    // The whole string
    return *this;
  }
  auto p = readerIndex(*this, begin);
  auto q = p;
  if (!q.skipForward(codepoints)) {
    // Malformed string
    throw std::runtime_error("Malformed UTF-8 string");
  }
  auto bytes = size_t(q.get() - p.get());
  return String(createContiguous(nullptr, p.get(), bytes, codepoints));
}

egg::ovum::String egg::ovum::String::repeat(size_t count) const {
  if (this->length() == 0) {
    return *this;
  }
  switch (count) {
  case 0:
    return String();
  case 1:
    return *this;
  }
  StringBuilder sb;
  do {
    sb.add(*this);
  } while (--count > 0);
  return sb.str();
}

bool egg::ovum::String::empty() const {
  return this->length() == 0;
}

bool egg::ovum::String::lessThan(const String& other) const {
  // Ordinal comparison via UTF8
  auto* lhs = this->get();
  if (lhs == nullptr) {
    return !other.empty();
  }
  auto* rhs = other.get();
  if (rhs == nullptr) {
    return false;
  }
  auto lsize = lhs->bytes();
  auto rsize = rhs->bytes();
  if (lsize < rsize) {
    return std::memcmp(lhs->begin(), rhs->begin(), lsize) <= 0;
  }
  return std::memcmp(lhs->begin(), rhs->begin(), rsize) < 0;
}

String egg::ovum::String::slice(int64_t begin, int64_t end) const {
  auto n = int64_t(this->length());
  auto a = (begin < 0) ? std::max(n + begin, int64_t(0)) : begin;
  auto b = (end < 0) ? std::max(n + end, int64_t(0)) : end;
  assert(a >= 0);
  assert(b >= 0);
  return this->substring(size_t(a), size_t(b));
}

std::vector<egg::ovum::String> egg::ovum::String::split(const String& separator, int64_t limit) const {
  // See https://docs.oracle.com/javase/8/docs/api/java/lang/String.html#split-java.lang.String-int-
  // However, if limit == 0 we return an empty vector
  std::vector<String> result;
  if (limit != 0) {
    splitString(result, *this, separator, limit);
  }
  return result;
}

egg::ovum::String egg::ovum::String::join(const std::vector<String>& parts) const {
  auto n = parts.size();
  switch (n) {
  case 0:
    return String();
  case 1:
    return parts[0];
  }
  StringBuilder sb;
  sb.add(parts[0]);
  auto between = this->toUTF8();
  for (size_t i = 1; i < n; ++i) {
    sb.add(between, parts[i]);
  }
  return sb.str();
}

egg::ovum::String egg::ovum::String::padLeft(size_t target) const {
  // OPTIMIZE
  return this->padLeft(target, " ");
}

egg::ovum::String egg::ovum::String::padLeft(size_t target, const String& padding) const {
  // OPTIMIZE
  auto length = this->length();
  auto n = padding.length();
  if ((n == 0) || (target <= length)) {
    return *this;
  }
  auto extra = target - length;
  assert(extra > 0);
  std::string dst;
  auto utf8 = padding.toUTF8();
  auto partial = extra % n;
  if (partial > 0) {
    dst.assign(egg::ovum::UTF8::offsetOfCodePoint(utf8, n - partial), utf8.cend());
  }
  for (auto whole = extra / n; whole > 0; --whole) {
    dst.append(utf8);
  }
  dst.append(this->toUTF8());
  return String(dst, target);
}

egg::ovum::String egg::ovum::String::padRight(size_t target, const String& padding) const {
  // OPTIMIZE
  auto length = this->length();
  auto n = padding.length();
  if ((n == 0) || (target <= length)) {
    return *this;
  }
  auto extra = target - length;
  assert(extra > 0);
  auto dst = this->toUTF8();
  auto utf8 = padding.toUTF8();
  for (auto whole = extra / n; whole > 0; --whole) {
    dst.append(utf8);
  }
  auto partial = extra % n;
  if (partial > 0) {
    dst.append(utf8.cbegin(), egg::ovum::UTF8::offsetOfCodePoint(utf8, partial));
  }
  return String(dst, target);
}

egg::ovum::String egg::ovum::String::padRight(size_t target) const {
  // OPTIMIZE
  return this->padRight(target, " ");
}

std::string egg::ovum::String::toUTF8() const {
  auto* memory = this->get();
  if (memory == nullptr) {
    return std::string();
  }
  return std::string(memory->begin(), memory->end());
}

egg::ovum::String egg::ovum::String::fromCodePoint(char32_t codepoint) {
  // OPTIMIZE
  assert(codepoint <= 0x10FFFF);
  auto utf8 = egg::ovum::UTF32::toUTF8(codepoint);
  return String(createContiguous(nullptr, utf8.data(), utf8.size(), 1));
}

egg::ovum::String egg::ovum::String::fromUTF8(const std::string& utf8, size_t codepoints) {
  return String(createContiguous(nullptr, utf8.data(), utf8.size(), codepoints));
}

egg::ovum::String egg::ovum::String::fromUTF32(const std::u32string& utf32) {
  auto utf8 = egg::ovum::UTF32::toUTF8(utf32);
  return String(createContiguous(nullptr, utf8.data(), utf8.size(), utf32.size()));
}

egg::ovum::String egg::ovum::StringBuilder::str() const {
  // OPTIMIZE
  return String(this->ss.str());
}

egg::ovum::String egg::ovum::StringFactory::fromCodePoint(IAllocator& allocator, char32_t codepoint) {
  // OPTIMIZE
  assert(codepoint <= 0x10FFFF);
  auto utf8 = egg::ovum::UTF32::toUTF8(codepoint);
  return String(createContiguous(&allocator, utf8.data(), utf8.size(), 1));
}

egg::ovum::String egg::ovum::StringFactory::fromUTF8(IAllocator& allocator, const uint8_t* begin, const uint8_t* end, size_t codepoints) {
  assert(begin != nullptr);
  assert(end >= begin);
  auto bytes = size_t(end - begin);
  return String(createContiguous(&allocator, begin, bytes, codepoints));
}

egg::ovum::String egg::ovum::StringFactory::fromASCIIZ(IAllocator& allocator, const char* asciiz) {
  assert(asciiz != nullptr);
  auto bytes = std::strlen(asciiz);
  return String(createContiguous(&allocator, asciiz, bytes, bytes));
}

const egg::ovum::IMemory* egg::ovum::Variant::acquireFallbackString(const char* utf8, size_t bytes) {
  // We've got to create this string without an allocator, so use a fallback
  return HardPtr<IMemory>::hardAcquire(createContiguous(nullptr, utf8, bytes));
}

std::ostream& operator<<(std::ostream& os, const egg::ovum::String& text) {
  // OPTIMIZE
  return os << text.toUTF8();
}
