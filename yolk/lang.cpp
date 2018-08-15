#include "yolk/yolk.h"

namespace {
  using namespace egg::lang;

  egg::ovum::IAllocator& defaultAllocator() {
    static egg::ovum::AllocatorDefault allocator; // WIBBLE
    return allocator;
  }

  bool arithmeticEqual(double a, int64_t b) {
    // TODO
    return a == b;
  }

  std::pair<std::string, int> tagToStringPriority(Discriminator tag) {
    // Adjust the priority of the result if the string has '|' (union) in it
    // This is so that parentheses can be added appropriately to handle "(void|int)*" versus "void|int*"
    auto result = std::make_pair(Value::getTagString(tag), 0);
    if (result.first.find('|') != std::string::npos) {
      result.second--;
    }
    return result;
  }

  IType::AssignmentSuccess canBeAssignedFromSimple(Discriminator lhs, const IType& rtype) {
    assert(lhs != Discriminator::Inferred);
    auto rhs = rtype.getSimpleTypes();
    assert(rhs != Discriminator::Inferred);
    if (rhs == Discriminator::None) {
      // The source is not simple
      return IType::AssignmentSuccess::Never;
    }
    if (Bits::hasAllSet(lhs, rhs)) {
      // The assignment will always work (unless it includes 'void')
      if (Bits::hasAnySet(rhs, Discriminator::Void)) {
        return IType::AssignmentSuccess::Sometimes;
      }
      return IType::AssignmentSuccess::Always;
    }
    if (Bits::hasAnySet(lhs, rhs)) {
      // There's a possibility that the assignment might work
      return IType::AssignmentSuccess::Sometimes;
    }
    if (Bits::hasAnySet(lhs, Discriminator::Float) && Bits::hasAnySet(rhs, Discriminator::Int)) {
      // We allow type promotion int->float
      return IType::AssignmentSuccess::Sometimes;
    }
    return IType::AssignmentSuccess::Never;
  }

  Value promoteAssignmentSimple(IExecution& execution, Discriminator lhs, const Value& rhs) {
    assert(lhs != Discriminator::Inferred);
    assert(!rhs.has(Discriminator::Indirect));
    if (rhs.has(lhs)) {
      // It's an exact type match (narrowing)
      return rhs;
    }
    if (Bits::hasAnySet(lhs, Discriminator::Float) && rhs.is(Discriminator::Int)) {
      // We allow type promotion int->float
      return Value(double(rhs.getInt())); // TODO overflows?
    }
    return execution.raiseFormat("Cannot assign a value of type '", rhs.getRuntimeType()->toString(), "' to a target of type '", Value::getTagString(lhs), "'");
  }

  void formatSourceLocation(StringBuilder& sb, const LocationSource& location) {
    sb.add(location.file);
    if (location.column > 0) {
      sb.add('(', location.line, ',', location.column, ')');
    } else if (location.line > 0) {
      sb.add('(', location.line, ')');
    }
  }

  // Forward declarations
  const IString* repeatString(const IString& src, size_t length, size_t count);
  const IString* replaceString(const IString& haystack, const IString& needle, const IString& replacement, int64_t occurrences);

  class StringEmpty : public egg::gc::NotReferenceCounted<IString> {
  public:
    virtual size_t length() const override {
      return 0;
    }
    virtual bool empty() const override {
      return true;
    }
    virtual bool equal(const IString& other) const override {
      return other.empty();
    }
    virtual bool less(const IString& other) const override {
      return !other.empty();
    }
    virtual int64_t compare(const IString& other) const override {
      return other.empty() ? 0 : -1;
    }
    virtual bool startsWith(const IString& needle) const override {
      return needle.empty();
    }
    virtual bool endsWith(const IString& needle) const override {
      return needle.empty();
    }
    virtual int64_t hashCode() const override {
      return 0;
    }
    virtual int32_t codePointAt(size_t) const override {
      return -1;
    }
    virtual int64_t indexOfCodePoint(char32_t, size_t) const override {
      return -1;
    }
    virtual int64_t indexOfString(const IString& needle, size_t fromIndex) const override {
      return ((fromIndex == 0) && needle.empty()) ? 0 : -1;
    }
    virtual int64_t lastIndexOfCodePoint(char32_t, size_t) const override {
      return -1;
    }
    virtual int64_t lastIndexOfString(const IString& needle, size_t fromIndex) const override {
      return ((fromIndex > 0) && needle.empty()) ? 0 : -1;
    }
    virtual const IString* substring(size_t, size_t) const override {
      return this;
    }
    virtual const IString* repeat(size_t) const override {
      return this;
    }
    virtual const IString* replace(const IString&, const IString&, int64_t) const override {
      return this;
    }
    virtual std::string toUTF8() const override {
      return std::string();
    }
    virtual bool iterateFirst(StringIteration&) const override {
      // There's nothing to iterate
      return false;
    }
    virtual bool iterateNext(StringIteration&) const override {
      // There's nothing to iterate
      return false;
    }
    virtual bool iteratePrevious(StringIteration&) const override {
      // There's nothing to iterate
      return false;
    }
    virtual bool iterateLast(StringIteration&) const override {
      // There's nothing to iterate
      return false;
    }
  };
  const StringEmpty stringEmpty{};

  class StringBufferCodePoint : public egg::gc::HardReferenceCounted<IString> {
    EGG_NO_COPY(StringBufferCodePoint);
  private:
    char32_t codepoint;
  public:
    StringBufferCodePoint(egg::ovum::IAllocator& allocator, char32_t codepoint)
      : HardReferenceCounted(allocator, 0), codepoint(codepoint) {
    }
    virtual size_t length() const override {
      return 1;
    }
    virtual bool empty() const override {
      return false;
    }
    virtual bool equal(const IString& other) const override {
      if (other.length() != 1) {
        return false;
      }
      auto cp = other.codePointAt(0);
      assert(cp >= 0);
      return this->codepoint == char32_t(cp);
    }
    virtual bool less(const IString& other) const override {
      auto length = other.length();
      int32_t threshold;
      switch (length) {
      case 0:
        // The other string is empty
        return false;
      case 1:
        // Just compare the codepoints
        threshold = 0;
        break;
      default:
        // In case of a tie, the longer string is greater
        threshold = 1;
        break;
      }
      auto cp = other.codePointAt(0);
      assert(cp >= 0);
      auto diff = int32_t(this->codepoint) - cp;
      return diff < threshold;
    }
    virtual int64_t compare(const IString& other) const override {
      if (other.empty()) {
        return +1;
      }
      auto cp = other.codePointAt(0);
      assert(cp >= 0);
      auto diff = int32_t(this->codepoint) - cp;
      if (diff < 0) {
        return -1;
      }
      if (diff > 0) {
        return +1;
      }
      return (other.length() > 1) ? -1 : 0;
    }
    virtual bool startsWith(const IString& needle) const override {
      switch (needle.length()) {
      case 0:
        return true;
      case 1:
        auto cp = needle.codePointAt(0);
        assert(cp >= 0);
        return int32_t(this->codepoint) == cp;
      }
      return false;
    }
    virtual bool endsWith(const IString& needle) const override {
      switch (needle.length()) {
      case 0:
        return true;
      case 1:
        auto cp = needle.codePointAt(0);
        assert(cp >= 0);
        return int32_t(this->codepoint) == cp;
      }
      return false;
    }
    virtual int64_t hashCode() const override {
      return int64_t(this->codepoint);
    }
    virtual int32_t codePointAt(size_t index) const override {
      return (index == 0) ? int32_t(this->codepoint) : -1;
    }
    virtual int64_t indexOfCodePoint(char32_t needle, size_t fromIndex) const override {
      return ((fromIndex == 0) && (this->codepoint == needle)) ? 0 : -1;
    }
    virtual int64_t indexOfString(const IString& needle, size_t fromIndex) const override {
      if (fromIndex == 0) {
        switch (needle.length()) {
        case 0:
          return 0;
        case 1:
          return (int32_t(this->codepoint) == needle.codePointAt(0)) ? 0 : -1;
        }
      }
      return -1;
    }
    virtual int64_t lastIndexOfCodePoint(char32_t needle, size_t fromIndex) const override {
      return ((fromIndex > 0) && (this->codepoint == needle)) ? 0 : -1;
    }
    virtual int64_t lastIndexOfString(const IString& needle, size_t fromIndex) const override {
      if (fromIndex > 0) {
        switch (needle.length()) {
        case 0:
          return 0;
        case 1:
          return (int32_t(this->codepoint) == needle.codePointAt(0)) ? 0 : -1;
        }
      }
      return -1;
    }
    virtual const IString* substring(size_t begin, size_t end) const override {
      if ((begin == 0) && (end > 0)) {
        return this;
      }
      return &stringEmpty;
    }
    virtual const IString* repeat(size_t count) const override {
      return repeatString(*this, 1, count);
    }
    virtual const IString* replace(const IString& needle, const IString& replacement, int64_t occurrences) const override {
      if ((occurrences > 0) && (needle.length() == 1) && (int32_t(this->codepoint) == needle.codePointAt(0))) {
        // The replacement occurs
        return &replacement;
      }
      return this;
    }
    virtual std::string toUTF8() const override {
      return egg::utf::to_utf8(this->codepoint);
    }
    virtual bool iterateFirst(StringIteration& iteration) const override {
      // There's only one element to iterate
      iteration.codepoint = this->codepoint;
      return true;
    }
    virtual bool iterateNext(StringIteration&) const override {
      // There's only one element to iterate
      return false;
    }
    virtual bool iteratePrevious(StringIteration&) const override {
      // There's only one element to iterate
      return false;
    }
    virtual bool iterateLast(StringIteration& iteration) const override {
      // There's only one element to iterate
      iteration.codepoint = this->codepoint;
      return true;
    }
    static String makeSpace() {
      static String space(*defaultAllocator().make<StringBufferCodePoint>(U' '));
      return space;
    }
  };

  inline bool iterationOffset(StringIteration& iteration, const IString& src, size_t offset) {
    // OPTIMIZE for offsets *closer* to the end
    auto length = src.length();
    if (offset >= length) {
      return false;
    }
    if (src.iterateFirst(iteration)) {
      do {
        if (offset-- == 0) {
          return true;
        }
      } while (src.iterateNext(iteration));
    }
    return false;
  }

  inline bool iterationEqualStarted(StringIteration& lhsIteration, const IString& lhs, const IString& rhs, size_t count) {
    // Check for equality using iteration
    assert(count > 0);
    StringIteration rhsIteration;
    if (rhs.iterateFirst(rhsIteration)) {
      while (lhsIteration.codepoint == rhsIteration.codepoint) {
        if (--count == 0) {
          return true;
        }
        if (!lhs.iterateNext(lhsIteration) || !rhs.iterateNext(rhsIteration)) {
          return false;
        }
      }
    }
    return false; // Malformed or not equal
  }

  inline bool iterationEqual(const IString& lhs, const IString& rhs, size_t count) {
    // Check for equality using iteration
    assert(count > 0);
    StringIteration lhsIteration;
    if (lhs.iterateFirst(lhsIteration)) {
      return iterationEqualStarted(lhsIteration, lhs, rhs, count);
    }
    return false; // Malformed or not equal
  }

  inline int iterationCompareStarted(StringIteration& lhsIteration, const IString& lhs, const IString& rhs, size_t count) {
    // Less/equal/greater comparison using iteration
    assert(count > 0);
    StringIteration rhsIteration;
    if (rhs.iterateFirst(rhsIteration)) {
      while (lhsIteration.codepoint == rhsIteration.codepoint) {
        if (--count == 0) {
          return 0;
        }
        if (!lhs.iterateNext(lhsIteration) || !rhs.iterateNext(rhsIteration)) {
          return 2; // Malformed
        }
      }
      return (lhsIteration.codepoint < rhsIteration.codepoint) ? -1 : 1;
    }
    return 2; // Malformed
  }

  inline int iterationCompare(const IString& lhs, const IString& rhs, size_t count) {
    // Less/equal/greater comparison using iteration
    assert(count > 0);
    StringIteration lhsIteration;
    if (lhs.iterateFirst(lhsIteration)) {
      return iterationCompareStarted(lhsIteration, lhs, rhs, count);
    }
    return 2; // Malformed
  }

  inline bool iterationMatch(const IString& lhs, StringIteration lhsIteration, const IString& rhs, StringIteration rhsIteration, size_t count) {
    // Note iteration parameters passed-by-value
    assert(lhsIteration.codepoint == rhsIteration.codepoint);
    assert(count > 0);
    while (--count > 0) {
      if (!lhs.iterateNext(lhsIteration) || !rhs.iterateNext(rhsIteration) || (lhsIteration.codepoint != rhsIteration.codepoint)) {
        return false;
      }
    }
    return true;
  }

  inline int64_t indexOfCodePointByIteration(const IString& haystack, char32_t needle, size_t fromIndex) {
    // Iterate around the haystack for the needle
    assert(!haystack.empty());
    StringIteration haystackIteration;
    if (iterationOffset(haystackIteration, haystack, fromIndex)) {
      int64_t index = int64_t(fromIndex);
      do {
        if (haystackIteration.codepoint == needle) {
          return index;
        }
        index++;
      } while (haystack.iterateNext(haystackIteration));
    }
    return -1; // Not found
  }

  inline int64_t indexOfStringByIteration(const IString& haystack, const IString& needle, size_t fromIndex) {
    // Iterate around the haystack for the needle
    assert(!haystack.empty());
    assert(!needle.empty());
    StringIteration haystackIteration;
    StringIteration needleIteration;
    if (iterationOffset(haystackIteration, haystack, fromIndex) && needle.iterateFirst(needleIteration)) {
      auto needleLength = needle.length();
      int64_t index = 0;
      do {
        if ((haystackIteration.codepoint == needleIteration.codepoint) && iterationMatch(haystack, haystackIteration, needle, needleIteration, needleLength)) {
          return index;
        }
        index++;
      } while (haystack.iterateNext(haystackIteration));
    }
    return -1; // Not found
  }

  inline int64_t lastIndexOfCodePointByIteration(const IString& haystack, char32_t needle, size_t fromIndex) {
    // Iterate around the haystack for the needle from back-to-front
    assert(!haystack.empty());
    auto haystackLength = haystack.length();
    fromIndex = std::min(fromIndex, haystackLength - 1);
    StringIteration haystackIteration;
    if (iterationOffset(haystackIteration, haystack, fromIndex)) {
      auto index = int64_t(fromIndex);
      do {
        if (haystackIteration.codepoint == needle) {
          return index;
        }
        index--;
      } while (haystack.iteratePrevious(haystackIteration));
    }
    return -1; // Not found
  }

  inline int64_t lastIndexOfStringByIteration(const IString& haystack, const IString& needle, size_t fromIndex) {
    // Iterate around the haystack for the needle
    assert(!haystack.empty());
    assert(!needle.empty());
    auto haystackLength = haystack.length();
    auto needleLength = needle.length();
    if (needleLength > haystackLength) {
      // The needle is too long
      return -1;
    }
    fromIndex = std::min(fromIndex, haystackLength - needleLength);
    StringIteration haystackIteration;
    StringIteration needleIteration;
    if (iterationOffset(haystackIteration, haystack, fromIndex) && needle.iterateFirst(needleIteration)) {
      auto index = int64_t(fromIndex);
      do {
        if ((haystackIteration.codepoint == needleIteration.codepoint) && iterationMatch(haystack, haystackIteration, needle, needleIteration, needleLength)) {
          return index;
        }
        index--;
      } while (haystack.iteratePrevious(haystackIteration));
    }
    return -1; // Not found
  }

  class StringBufferUTF8 : public egg::gc::HardReferenceCounted<IString> {
    EGG_NO_COPY(StringBufferUTF8);
  private:
    std::string utf8;
    size_t codepoints;
  public:
    StringBufferUTF8(egg::ovum::IAllocator& allocator, const std::string& utf8, size_t length)
      : HardReferenceCounted(allocator, 0), utf8(utf8), codepoints(length) {
      // Creation is via factories below
      assert(!this->utf8.empty());
      assert(this->codepoints > 0);
    }
    virtual size_t length() const override {
      return this->codepoints;
    }
    virtual bool empty() const override {
      return this->codepoints == 0;
    }
    virtual bool equal(const IString& rhs) const override {
      return (rhs.length() == this->codepoints) && iterationEqual(*this, rhs, this->codepoints);
    }
    virtual bool less(const IString& rhs) const override {
      auto rhsLength = rhs.length();
      if (rhsLength == 0) {
        return false;
      }
      if (rhsLength <= this->codepoints) {
        return iterationCompare(*this, rhs, rhsLength) < 0;
      }
      return iterationCompare(*this, rhs, this->codepoints) <= 0;
    }
    virtual int64_t compare(const IString& rhs) const override {
      auto rhsLength = rhs.length();
      if (rhsLength > this->codepoints) {
        int retval = iterationCompare(*this, rhs, this->codepoints);
        return (retval == 0) ? -1 : retval;
      }
      if (rhsLength < this->codepoints) {
        if (rhsLength == 0) {
          return +1;
        }
        int retval = iterationCompare(*this, rhs, rhsLength);
        return (retval == 0) ? +1 : retval;
      }
      return iterationCompare(*this, rhs, rhsLength);
    }
    virtual bool startsWith(const IString& needle) const override {
      auto needleLength = needle.length();
      if (needleLength == 0) {
        return true;
      }
      if (needleLength > this->codepoints) {
        return false;
      }
      return iterationEqual(*this, needle, needleLength);
    }
    virtual bool endsWith(const IString& needle) const override {
      auto needleLength = needle.length();
      if (needleLength == 0) {
        return true;
      }
      if (needleLength > this->codepoints) {
        return false;
      }
      StringIteration iteration;
      if (iterationOffset(iteration, *this, this->codepoints - needleLength)) {
        return iterationEqualStarted(iteration, *this, needle, needleLength);
      }
      return false; // Malformed
    }
    virtual int64_t hashCode() const override {
      // See https://docs.oracle.com/javase/6/docs/api/java/lang/String.html#hashCode()
      int64_t hash = 0;
      egg::utf::utf8_reader reader(this->utf8, egg::utf::utf8_reader::First);
      char32_t codepoint = 0;
      while (reader.forward(codepoint)) {
        hash = hash * 31 + int64_t(uint32_t(codepoint));
      }
      return hash;
    }
    virtual int32_t codePointAt(size_t index) const override {
      egg::utf::utf8_reader reader(this->utf8, egg::utf::utf8_reader::First);
      if (!reader.skipForward(index)) {
        return -1;
      }
      char32_t codepoint = 0;
      return reader.forward(codepoint) ? int32_t(codepoint) : -1;
    }
    virtual int64_t indexOfCodePoint(char32_t needle, size_t fromIndex) const override {
      return indexOfCodePointByIteration(*this, needle, fromIndex);
    }
    virtual int64_t indexOfString(const IString& needle, size_t fromIndex) const override {
      switch (needle.length()) {
      case 0:
        return (fromIndex < this->codepoints) ? int64_t(fromIndex) : -1;
      case 1:
        return indexOfCodePointByIteration(*this, char32_t(needle.codePointAt(0)), fromIndex);
      }
      return indexOfStringByIteration(*this, needle, fromIndex);
    }
    virtual int64_t lastIndexOfCodePoint(char32_t needle, size_t fromIndex) const override {
      return lastIndexOfCodePointByIteration(*this, needle, fromIndex);
    }
    virtual int64_t lastIndexOfString(const IString& needle, size_t fromIndex) const override {
      switch (needle.length()) {
      case 0:
        return 0;
      case 1:
        return lastIndexOfCodePointByIteration(*this, char32_t(needle.codePointAt(0)), fromIndex);
      }
      return lastIndexOfStringByIteration(*this, needle, fromIndex);
    }
    virtual const IString* substring(size_t begin, size_t end) const override {
      if ((begin >= this->codepoints) || (end <= begin)) {
        return &stringEmpty;
      }
      if ((begin == 0) && (end >= this->codepoints)) {
        return this;
      }
      egg::utf::utf8_reader reader(this->utf8, egg::utf::utf8_reader::First);
      if (!reader.skipForward(begin)) {
        return &stringEmpty; // Malformed
      }
      size_t p = reader.getIterationInternal();
      size_t q;
      size_t length;
      if (end < this->codepoints) {
        // It's a proper substring
        length = end - begin;
        if (!reader.skipForward(length)) {
          return &stringEmpty; // Malformed
        }
        q = reader.getIterationInternal();
      } else {
        // It's the end of the string
        length = this->codepoints - begin;
        q = this->utf8.length();
      }
      assert(length > 0);
      std::string sub(this->utf8.data() + p, q - p);
      return defaultAllocator().create<StringBufferUTF8>(0, defaultAllocator(), sub, length);
    }
    virtual const IString* repeat(size_t count) const override {
      return repeatString(*this, this->codepoints, count);
    }
    virtual const IString* replace(const IString& needle, const IString& replacement, int64_t occurrences) const override {
      return replaceString(*this, needle, replacement, occurrences);
    }
    virtual std::string toUTF8() const override {
      return this->utf8;
    }
    virtual bool iterateFirst(StringIteration& iteration) const override {
      // There should be at least one element to iterate
      egg::utf::utf8_reader reader(this->utf8, egg::utf::utf8_reader::First);
      if (reader.forward(iteration.codepoint)) {
        iteration.internal = reader.getIterationInternal();
        return true;
      }
      return false;
    }
    virtual bool iterateNext(StringIteration& iteration) const override {
      // Fetch the next element
      egg::utf::utf8_reader reader(this->utf8, iteration.internal);
      if (reader.forward(iteration.codepoint)) {
        iteration.internal = reader.getIterationInternal();
        return true;
      }
      return false;
    }
    virtual bool iteratePrevious(StringIteration& iteration) const override {
      // Fetch the previous element
      egg::utf::utf8_reader reader(this->utf8, iteration.internal);
      if (reader.backward(iteration.codepoint)) {
        iteration.internal = reader.getIterationInternal();
        return true;
      }
      return false;
    }
    virtual bool iterateLast(StringIteration& iteration) const override {
      // There should be at least one element to iterate
      egg::utf::utf8_reader reader(this->utf8, egg::utf::utf8_reader::Last);
      if (reader.backward(iteration.codepoint)) {
        iteration.internal = reader.getIterationInternal();
        return true;
      }
      return false;
    }
    // Factories
    static const IString* create(const std::string& utf8) {
      egg::utf::utf8_reader reader(utf8, egg::utf::utf8_reader::First);
      auto length = reader.validate(); // TODO malformed?
      return StringBufferUTF8::create(utf8, length);
    }
    static const IString* create(const std::string& utf8, size_t length) {
      if (length == 0) {
        return &stringEmpty;
      }
      return defaultAllocator().create<StringBufferUTF8>(0, defaultAllocator(), utf8, length);
    }
  };

  const IString* repeatString(const IString& src, size_t length, size_t count) {
    assert(length > 0);
    switch (count) {
    case 0:
      return &stringEmpty;
    case 1:
      return &src;
    }
    auto utf8 = src.toUTF8();
    std::string dst;
    dst.reserve(utf8.size() * count);
    for (size_t i = 0; i < count; ++i) {
      dst.append(utf8);
    }
    return StringBufferUTF8::create(dst, length * count);
  }

  void splitPositive(std::vector<String>& dst, const IString& src, const IString& separator, size_t limit) {
    // Unlike the original parameter, 'limit' is the maximum number of SPLITS to perform
    // OPTIMIZE
    assert(dst.size() == 0);
    size_t begin = 0;
    if (limit > 0) {
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
          dst.emplace_back(*src.substring(begin, size_t(index)));
          begin = size_t(index) + separator.length();
          if (--limit == 0) {
            break;
          }
          index = src.indexOfString(separator, begin);
        }
      }
    }
    dst.emplace_back(*src.substring(begin, SIZE_MAX));
  }

  void splitNegative(std::vector<String>& dst, const IString& src, const IString& separator, size_t limit) {
    // Unlike the original parameter, 'limit' is the maximum number of SPLITS to perform
    // OPTIMIZE
    assert(dst.size() == 0);
    size_t end = src.length();
    if (limit > 0) {
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
          dst.emplace_back(*src.substring(size_t(index) + length, end));
          end = size_t(index);
          if ((end < length) || (--limit == 0)) {
            break;
          }
          index = src.lastIndexOfString(separator, end - length);
        }
      }
    }
    dst.emplace_back(*src.substring(0, end));
    std::reverse(dst.begin(), dst.end());
  }

  void splitString(std::vector<egg::lang::String>& result, const IString& haystack, const IString& needle, int64_t limit) {
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

  const IString* replaceString(const IString& haystack, const IString& needle, const IString& replacement, int64_t occurrences) {
    // One replacement requires splitting into two parts, and so on
    std::vector<egg::lang::String> part;
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
      return &haystack;
    }
    splitString(part, haystack, needle, occurrences);
    auto parts = part.size();
    assert(parts > 0);
    if (parts == 1) {
      // No replacement occurred
      assert(part[0].length() == haystack.length());
      return &haystack;
    }
    StringBuilder sb;
    sb.add(part[0]);
    auto between = replacement.toUTF8();
    for (size_t i = 1; i < parts; ++i) {
      sb.add(between, part[i]);
    }
    return StringBufferUTF8::create(sb.toUTF8());
  }

  class TypePointer : public egg::gc::HardReferenceCounted<IType> {
    EGG_NO_COPY(TypePointer);
  private:
    ITypeRef referenced;
  public:
    TypePointer(egg::ovum::IAllocator& allocator, const IType& referenced)
      : HardReferenceCounted(allocator, 0), referenced(&referenced) {
    }
    virtual std::pair<std::string, int> toStringPrecedence() const override {
      auto s = this->referenced->toString(0);
      return std::make_pair(s.toUTF8() + "*", 0);
    }
    virtual egg::lang::Discriminator getSimpleTypes() const override {
      return egg::lang::Discriminator::None;
    }
    virtual ITypeRef pointeeType() const override {
      return this->referenced;
    }
    virtual AssignmentSuccess canBeAssignedFrom(const IType& rhs) const {
      return this->referenced->canBeAssignedFrom(*rhs.pointeeType());
    }
  };

  class TypeNull : public egg::gc::NotReferenceCounted<IType>{
  public:
    virtual std::pair<std::string, int> toStringPrecedence() const override {
      static std::string name{ "null" };
      return std::make_pair(name, 0);
    }
    virtual Discriminator getSimpleTypes() const override {
      return Discriminator::Null;
    }
    virtual ITypeRef unionWith(const IType& other) const override {
      auto simple = other.getSimpleTypes();
      if (Bits::hasAnySet(simple, Discriminator::Null)) {
        // The other type supports Null anyway
        return ITypeRef(&other);
      }
      return Type::makeUnion(*this, other);
    }
    virtual AssignmentSuccess canBeAssignedFrom(const IType&) const {
      return AssignmentSuccess::Never;
    }
    virtual Value promoteAssignment(IExecution& execution, const Value&) const override {
      return execution.raiseFormat("Cannot assign to 'null' value");
    }
  };
  const TypeNull typeNull{};

  template<Discriminator TAG>
  class TypeNative : public egg::gc::NotReferenceCounted<IType> {
  public:
    TypeNative() {
      assert(!Bits::hasAnySet(TAG, Discriminator::Null));
    }
    virtual std::pair<std::string, int> toStringPrecedence() const override {
      return tagToStringPriority(TAG);
    }
    virtual Discriminator getSimpleTypes() const override {
      return TAG;
    }
    virtual ITypeRef denulledType() const override {
      auto denulled = Bits::clear(TAG, Discriminator::Null);
      if (denulled == TAG) {
        // It's the identical native type
        return ITypeRef(this);
      }
      return ITypeRef(Type::getNative(denulled));
    }
    virtual ITypeRef unionWith(const IType& other) const override {
      if (other.getSimpleTypes() == TAG) {
        // It's the identical native type
        return ITypeRef(this);
      }
      return Type::makeUnion(*this, other);
    }
    virtual AssignmentSuccess canBeAssignedFrom(const IType& rhs) const {
      return canBeAssignedFromSimple(TAG, rhs);
    }
    virtual Value promoteAssignment(IExecution& execution, const Value& rhs) const override {
      return promoteAssignmentSimple(execution, TAG, rhs);
    }
  };
  const TypeNative<Discriminator::Void> typeVoid{};
  const TypeNative<Discriminator::Bool> typeBool{};
  const TypeNative<Discriminator::Int> typeInt{};
  const TypeNative<Discriminator::Float> typeFloat{};
  const TypeNative<Discriminator::Arithmetic> typeArithmetic{};
  const TypeNative<Discriminator::Type> typeType{};

  class TypeString : public TypeNative<Discriminator::String> {
  public:
    // TODO callable and indexable
    virtual bool iterable(ITypeRef& type) const override {
      // When strings are iterated, they iterate through strings (of codepoints)
      type = Type::String;
      return true;
    }
  };
  const TypeString typeString{};

  // An 'omni' function looks like this: 'any?(...any?[])'
  class OmniFunctionSignature : public IFunctionSignature {
  private:
    class Parameter : public IFunctionSignatureParameter {
    public:
      virtual String getName() const override {
        return String::Empty;
      }
      virtual ITypeRef getType() const override {
        return Type::AnyQ;
      }
      virtual size_t getPosition() const override {
        return 0;
      }
      virtual Flags getFlags() const override {
        return Flags::Variadic;
      }
    };
    Parameter parameter;
  public:
    virtual String getFunctionName() const override {
      return String::Empty;
    }
    virtual ITypeRef getReturnType() const override {
      return Type::AnyQ;
    }
    virtual size_t getParameterCount() const override {
      return 1;
    }
    virtual const IFunctionSignatureParameter& getParameter(size_t) const override {
      return this->parameter;
    }
  };
  const OmniFunctionSignature omniFunctionSignature{};

  class TypeSimple : public egg::gc::HardReferenceCounted<IType> {
    EGG_NO_COPY(TypeSimple);
  private:
    Discriminator tag;
  public:
    TypeSimple(egg::ovum::IAllocator& allocator, Discriminator tag)
      : HardReferenceCounted(allocator, 0), tag(tag) {
    }
    virtual std::pair<std::string, int> toStringPrecedence() const override {
      return tagToStringPriority(this->tag);
    }
    virtual Discriminator getSimpleTypes() const override {
      return this->tag;
    }
    virtual ITypeRef denulledType() const override {
      auto denulled = Bits::clear(this->tag, Discriminator::Null);
      if (this->tag != denulled) {
        // We need to clear the bit
        return Type::makeSimple(denulled);
      }
      return ITypeRef(this);
    }
    virtual ITypeRef unionWith(const IType& other) const override {
      auto simple = other.getSimpleTypes();
      if (simple == Discriminator::None) {
        // The other type is not simple
        return Type::makeUnion(*this, other);
      }
      auto both = Bits::set(this->tag, simple);
      if (both != this->tag) {
        // There's a new simple type that we don't support, so create a new type
        return Type::makeSimple(both);
      }
      return ITypeRef(this);
    }
    virtual AssignmentSuccess canBeAssignedFrom(const IType& rhs) const {
      return canBeAssignedFromSimple(this->tag, rhs);
    }
    virtual Value promoteAssignment(IExecution& execution, const Value& rhs) const override {
      return promoteAssignmentSimple(execution, this->tag, rhs);
    }
    virtual const IFunctionSignature* callable() const override {
      if (Bits::hasAnySet(this->tag, Discriminator::Object)) {
        return &omniFunctionSignature;
      }
      return nullptr;
    }
    virtual bool iterable(ITypeRef& type) const override {
      if (Bits::hasAnySet(this->tag, Discriminator::Object)) {
        type = Type::Any;
        return true;
      }
      if (Bits::hasAnySet(this->tag, Discriminator::String)) {
        type = Type::String;
        return true;
      }
      return false;
    }
  };

  class TypeUnion : public egg::gc::HardReferenceCounted<IType> {
    EGG_NO_COPY(TypeUnion);
  private:
    ITypeRef a;
    ITypeRef b;
  public:
    TypeUnion(egg::ovum::IAllocator& allocator, const IType& a, const IType& b)
      : HardReferenceCounted(allocator, 0), a(&a), b(&b) {
    }
    virtual std::pair<std::string, int> toStringPrecedence() const override {
      auto sa = this->a->toStringPrecedence().first;
      auto sb = this->b->toStringPrecedence().first;
      return std::make_pair(sa + "|" + sb, -1);
    }
    virtual egg::lang::Discriminator getSimpleTypes() const override {
      return this->a->getSimpleTypes() | this->b->getSimpleTypes();
    }
    virtual AssignmentSuccess canBeAssignedFrom(const IType&) const {
      return AssignmentSuccess::Never; // TODO
    }
    virtual const IFunctionSignature* callable() const override {
      EGG_THROW("TODO: Cannot yet call to union value"); // TODO
    }
  };

  class ValueOnHeap : public egg::gc::HardReferenceCounted<egg::lang::ValueReferenceCounted> {
    ValueOnHeap(const ValueOnHeap&) = delete;
    ValueOnHeap& operator=(const ValueOnHeap&) = delete;
  public:
    ValueOnHeap(egg::ovum::IAllocator& allocator, Value&& value) noexcept
      : egg::gc::HardReferenceCounted<egg::lang::ValueReferenceCounted>(allocator, 0, std::move(value)) {
    }
  };
}

// Native types
const egg::lang::Type egg::lang::Type::Void{ typeVoid };
const egg::lang::Type egg::lang::Type::Null{ typeNull };
const egg::lang::Type egg::lang::Type::Bool{ typeBool };
const egg::lang::Type egg::lang::Type::Int{ typeInt };
const egg::lang::Type egg::lang::Type::Float{ typeFloat };
const egg::lang::Type egg::lang::Type::String{ typeString };
const egg::lang::Type egg::lang::Type::Arithmetic{ typeArithmetic };
const egg::lang::Type egg::lang::Type::Type_{ typeType };
const egg::lang::Type egg::lang::Type::Any{ *defaultAllocator().make<TypeSimple>(egg::lang::Discriminator::Any) };
const egg::lang::Type egg::lang::Type::AnyQ{ *defaultAllocator().make<TypeSimple>(egg::lang::Discriminator::Any | egg::lang::Discriminator::Null) };

// Constants
const egg::lang::IString& egg::lang::String::emptyBuffer = stringEmpty;
const egg::lang::String egg::lang::String::Empty{ stringEmpty };
const egg::lang::String egg::lang::String::Space{ StringBufferCodePoint::makeSpace() };

egg::lang::String egg::lang::String::fromCodePoint(char32_t codepoint) {
  return String(*defaultAllocator().make<StringBufferCodePoint>(codepoint));
}

egg::lang::String egg::lang::String::fromUTF8(const std::string& utf8) {
  return String(*StringBufferUTF8::create(utf8));
}

egg::lang::String egg::lang::String::fromUTF32(const std::u32string& utf32) {
  // OPTIMIZE
  return String(*StringBufferUTF8::create(egg::utf::to_utf8(utf32)));
}

std::vector<egg::lang::String> egg::lang::String::split(const String& separator, int64_t limit) const {
  // See https://docs.oracle.com/javase/8/docs/api/java/lang/String.html#split-java.lang.String-int-
  // However, if limit == 0 we return an empty vector
  std::vector<egg::lang::String> result;
  if (limit != 0) {
    splitString(result, *this->get(), *separator, limit);
  }
  return result;
}

egg::lang::String egg::lang::String::padLeft(size_t target, const String& padding) const {
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
    dst.assign(egg::utf::utf8_offset(utf8, n - partial), utf8.cend());
  }
  for (auto whole = extra / n; whole > 0; --whole) {
    dst.append(utf8);
  }
  dst.append(this->toUTF8());
  return String(*StringBufferUTF8::create(dst, target));
}

egg::lang::String egg::lang::String::padRight(size_t target, const String& padding) const {
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
    dst.append(utf8.cbegin(), egg::utf::utf8_offset(utf8, partial));
  }
  return String(*StringBufferUTF8::create(dst, target));
}

egg::lang::String egg::lang::StringBuilder::str() const {
  return String::fromUTF8(this->ss.str());
}

std::ostream& operator<<(std::ostream& os, const egg::lang::String& text) {
  return os << text.toUTF8();
}

// Trivial constant values
const egg::lang::Value egg::lang::Value::Void{ Discriminator::Void };
const egg::lang::Value egg::lang::Value::Null{ Discriminator::Null };
const egg::lang::Value egg::lang::Value::False{ false };
const egg::lang::Value egg::lang::Value::True{ true };
const egg::lang::Value egg::lang::Value::EmptyString{ String::Empty };
const egg::lang::Value egg::lang::Value::Break{ Discriminator::Break };
const egg::lang::Value egg::lang::Value::Continue{ Discriminator::Continue };
const egg::lang::Value egg::lang::Value::Rethrow{ Discriminator::Exception | Discriminator::Void };
const egg::lang::Value egg::lang::Value::ReturnVoid{ Discriminator::Return | Discriminator::Void };

void egg::lang::Value::copyInternals(const Value& other) {
  assert(this != &other);
  this->tag = other.tag;
  if (this->has(Discriminator::Object)) {
    // We can only copy the reference as a hard pointer
    this->tag = Bits::clear(other.tag, Discriminator::Pointer);
    this->o = other.getObjectPointer()->hardAcquire();
  } else if (this->has(Discriminator::Indirect | Discriminator::Pointer)) {
    this->v = other.v->hardAcquire();
  } else if (this->has(Discriminator::String)) {
    this->s = String::hardAcquire(other.s);
  } else if (this->has(Discriminator::Float)) {
    this->f = other.f;
  } else if (this->has(Discriminator::Int)) {
    this->i = other.i;
  } else if (this->has(Discriminator::Bool)) {
    this->b = other.b;
  } else if (this->has(Discriminator::Type)) {
    this->t = other.t->hardAcquire();
  }
}

void egg::lang::Value::moveInternals(Value& other) {
  this->tag = other.tag;
  if (this->has(Discriminator::Object)) {
    auto* ptr = other.getObjectPointer();
    if (this->has(Discriminator::Pointer)) {
      // Convert the soft pointer to a hard one
      this->tag = Bits::clear(this->tag, Discriminator::Pointer);
      this->o = ptr->hardAcquire();
      other.r->destroy();
    } else {
      // Not need to increment/decrement the reference count
      this->o = ptr;
    }
  } else if (this->has(Discriminator::Indirect | Discriminator::Pointer)) {
    this->v = other.v;
  } else if (this->has(Discriminator::String)) {
    this->s = other.s;
  } else if (this->has(Discriminator::Float)) {
    this->f = other.f;
  } else if (this->has(Discriminator::Int)) {
    this->i = other.i;
  } else if (this->has(Discriminator::Bool)) {
    this->b = other.b;
  } else if (this->has(Discriminator::Type)) {
    this->t = other.t;
  }
  other.tag = Discriminator::None;
}

void egg::lang::Value::destroyInternals() {
  if (this->has(Discriminator::Object)) {
    if (this->has(Discriminator::Pointer)) {
      this->r->destroy();
    } else {
      this->o->hardRelease();
    }
  } else if (this->has(Discriminator::Indirect | Discriminator::Pointer)) {
    this->v->hardRelease();
  } else if (this->has(Discriminator::String)) {
    this->s->hardRelease();
  } else if (this->has(Discriminator::Type)) {
    this->t->hardRelease();
  }
}

egg::lang::Value::Value(const ValueReferenceCounted& vrc)
  : tag(Discriminator::Pointer) {
  this->v = vrc.hardAcquire();
}

egg::lang::Value::Value(const Value& value) {
  this->copyInternals(value);
}

egg::lang::Value::Value(Value&& value) noexcept {
  this->moveInternals(value);
}

egg::lang::Value& egg::lang::Value::operator=(const Value& value) {
  if (this != &value) {
    this->destroyInternals();
    this->copyInternals(value);
  }
  return *this;
}

egg::lang::Value& egg::lang::Value::operator=(Value&& value) noexcept {
  if (this != &value) {
    this->destroyInternals();
    this->moveInternals(value);
  }
  return *this;
}

egg::lang::Value::~Value() {
  this->destroyInternals();
}

const egg::lang::Value& egg::lang::Value::direct() const {
  auto* p = this;
  while (p->has(Discriminator::Indirect)) {
    p = p->v;
    assert(p != nullptr);
    assert(!p->has(egg::lang::Discriminator::FlowControl));
  }
  return *p;
}

egg::lang::Value& egg::lang::Value::direct() {
  auto* p = this;
  while (p->has(Discriminator::Indirect)) {
    p = p->v;
    assert(p != nullptr);
    assert(!p->has(egg::lang::Discriminator::FlowControl));
  }
  return *p;
}

egg::lang::ValueReferenceCounted& egg::lang::Value::indirect(egg::ovum::IAllocator& allocator) {
  // Make this value indirect (i.e. heap-based)
  if (!this->has(Discriminator::Indirect)) {
    auto* heap = allocator.create<ValueOnHeap>(0, allocator, std::move(*this)); // WIBBLE
    assert(this->tag == Discriminator::None); // as a side-effect of the move
    this->tag = Discriminator::Indirect;
    this->v = heap->hardAcquire();
  }
  return *this->v;
}

egg::lang::Value& egg::lang::Value::soft(egg::ovum::IAllocator& allocator, egg::gc::Collectable& container) {
  if (this->has(Discriminator::Object) && !this->has(Discriminator::Pointer)) {
    // This is a hard pointer to an IObject, make it a soft pointer
    auto* before = this->o;
    assert(before != nullptr);
    auto* after = allocator.create<egg::gc::SoftRef<IObject>>(0, container, before);
    this->tag = Bits::set(this->tag, Discriminator::Pointer);
    this->r = after;
    before->hardRelease();
  }
  return *this;
}

bool egg::lang::Value::equal(const Value& lhs, const Value& rhs) {
  auto& a = lhs.direct();
  auto& b = rhs.direct();
  if (a.tag != b.tag) {
    // Need to worry about expressions like (0 == 0.0)
    if ((a.tag == Discriminator::Float) && (b.tag == Discriminator::Int)) {
      return arithmeticEqual(a.f, b.i);
    }
    if ((a.tag == Discriminator::Int) && (b.tag == Discriminator::Float)) {
      return arithmeticEqual(b.f, a.i);
    }
    return false;
  }
  if (a.tag == Discriminator::Bool) {
    return a.b == b.b;
  }
  if (a.tag == Discriminator::Int) {
    return a.i == b.i;
  }
  if (a.tag == Discriminator::Float) {
    return a.f == b.f;
  }
  if (a.tag == Discriminator::String) {
    return a.s->equal(*b.s);
  }
  if (a.tag == Discriminator::Type) {
    return a.t == b.t;
  }
  if (a.tag == Discriminator::Pointer) {
    return a.v == b.v;
  }
  if (a.tag == Discriminator::Object) {
    return a.o == b.o;
  }
  assert(a.tag == (Discriminator::Object | Discriminator::Pointer));
  return a.r->get() == b.r->get();
}

std::string egg::lang::Value::getTagString() const {
  if (this->tag == Discriminator::Indirect) {
    return this->v->getTagString();
  }
  if (this->tag == Discriminator::Pointer) {
    return this->v->getTagString() + "*";
  }
  return Value::getTagString(this->tag);
}

egg::lang::String egg::lang::LocationSource::toSourceString() const {
  StringBuilder sb;
  formatSourceLocation(sb, *this);
  return sb.str();
}

egg::lang::String egg::lang::LocationRuntime::toRuntimeString() const {
  StringBuilder sb;
  formatSourceLocation(sb, *this);
  if (!this->function.empty()) {
    if (sb.empty()) {
      sb.add(' ');
    }
    sb.add('<', this->function, '>');
  }
  return sb.str();
}

void egg::lang::Value::addFlowControl(Discriminator bits) {
  assert(Bits::mask(bits, Discriminator::FlowControl) == bits);
  assert(!this->has(Discriminator::FlowControl));
  this->tag = this->tag | bits;
  assert(this->has(Discriminator::FlowControl));
}

bool egg::lang::Value::stripFlowControl(Discriminator bits) {
  assert(Bits::mask(bits, Discriminator::FlowControl) == bits);
  if (Bits::hasAnySet(this->tag, bits)) {
    assert(this->has(egg::lang::Discriminator::FlowControl));
    this->tag = Bits::clear(this->tag, bits);
    assert(!this->has(egg::lang::Discriminator::FlowControl));
    return true;
  }
  return false;
}

std::string egg::lang::Value::getTagString(Discriminator tag) {
  static const egg::yolk::String::StringFromEnum table[] = {
    { int(Discriminator::Any), "any" },
    { int(Discriminator::Void), "void" },
    { int(Discriminator::Bool), "bool" },
    { int(Discriminator::Int), "int" },
    { int(Discriminator::Float), "float" },
    { int(Discriminator::String), "string" },
    { int(Discriminator::Object), "object" },
    { int(Discriminator::Indirect), "indirect" },
    { int(Discriminator::Pointer), "pointer" },
    { int(Discriminator::Break), "break" },
    { int(Discriminator::Continue), "continue" },
    { int(Discriminator::Return), "return" },
    { int(Discriminator::Yield), "yield" },
    { int(Discriminator::Exception), "exception" }
  };
  if (tag == Discriminator::Inferred) {
    return "var";
  }
  if (tag == Discriminator::Null) {
    return "null";
  }
  if (Bits::hasAnySet(tag, Discriminator::Null)) {
    return Value::getTagString(Bits::clear(tag, Discriminator::Null)) + "?";
  }
  return egg::yolk::String::fromEnum(tag, table);
}

egg::lang::ITypeRef egg::lang::Value::getRuntimeType() const {
  assert(this->tag != Discriminator::Indirect);
  if (this->has(Discriminator::Object)) {
    return this->getObjectPointer()->getRuntimeType();
  }
  if (this->has(Discriminator::Pointer)) {
    return this->v->getRuntimeType()->pointerType();
  }
  if (this->has(Discriminator::Type)) {
    // TODO Is a type's type itself?
    return ITypeRef(this->t);
  }
  auto* native = Type::getNative(this->tag);
  if (native != nullptr) {
    return ITypeRef(native);
  }
  EGG_THROW("Internal type error: Unknown runtime type");
}

egg::lang::String egg::lang::Value::toString() const {
  // OPTIMIZE
  if (this->has(Discriminator::Object)) {
    auto str = this->getObject()->toString();
    if (str.tag == Discriminator::String) {
      return str.getString();
    }
    return String::fromUTF8("<invalid>");
  }
  if (this->has(Discriminator::Type)) {
    return this->t->toString();
  }
  return String::fromUTF8(this->toUTF8());
}

std::string egg::lang::Value::toUTF8() const {
  // OPTIMIZE
  if (this->tag == Discriminator::Null) {
    return "null";
  }
  if (this->tag == Discriminator::Bool) {
    return this->b ? "true" : "false";
  }
  if (this->tag == Discriminator::Int) {
    return egg::yolk::String::fromSigned(this->i);
  }
  if (this->tag == Discriminator::Float) {
    return egg::yolk::String::fromFloat(this->f);
  }
  if (this->tag == Discriminator::String) {
    return this->s->toUTF8();
  }
  if (this->has(Discriminator::Object)) {
    auto str = this->getObject()->toString();
    if (str.tag == Discriminator::String) {
      return str.getString().toUTF8();
    }
    return "<invalid>";
  }
  if (this->tag == Discriminator::Type) {
    return this->t->toString().toUTF8();
  }
  return "<" + Value::getTagString(this->tag) + ">";
}

egg::lang::String egg::lang::IType::toString(int priority) const {
  auto pair = this->toStringPrecedence();
  if (pair.second < priority) {
    return egg::lang::String::concat("(", pair.first, ")");
  }
  return String::fromUTF8(pair.first);
}

egg::lang::ITypeRef egg::lang::IType::pointerType() const {
  // The default implementation is to return a new type 'Type*'
  return defaultAllocator().make<TypePointer>(*this);
}

egg::lang::ITypeRef egg::lang::IType::pointeeType() const {
  // The default implementation is to return 'Void' indicating that this is NOT dereferencable
  return Type::Void;
}

egg::lang::ITypeRef egg::lang::IType::denulledType() const {
  // The default implementation is to return 'Void'
  return Type::Void;
}

egg::lang::String egg::lang::IFunctionSignature::toString(Parts parts) const {
  StringBuilder sb;
  this->buildStringDefault(sb, parts);
  return sb.str();
}

bool egg::lang::IFunctionSignature::validateCall(IExecution& execution, const IParameters& runtime, Value& problem) const {
  // The default implementation just calls validateCallDefault()
  return this->validateCallDefault(execution, runtime, problem);
}

bool egg::lang::IFunctionSignature::validateCallDefault(IExecution& execution, const IParameters& parameters, Value& problem) const {
  // TODO type checking, etc
  if (parameters.getNamedCount() > 0) {
    problem = execution.raiseFormat(this->toString(Parts::All), ": Named parameters are not yet supported"); // TODO
    return false;
  }
  auto maxPositional = this->getParameterCount();
  auto minPositional = maxPositional;
  while ((minPositional > 0) && !this->getParameter(minPositional - 1).isRequired()) {
    minPositional--;
  }
  auto actual = parameters.getPositionalCount();
  if (actual < minPositional) {
    if (minPositional == 1) {
      problem = execution.raiseFormat(this->toString(Parts::All), ": At least 1 parameter was expected");
    } else {
      problem = execution.raiseFormat(this->toString(Parts::All), ": At least ", minPositional, " parameters were expected, not ", actual);
    }
    return false;
  }
  if ((maxPositional > 0) && this->getParameter(maxPositional - 1).isVariadic()) {
    // TODO Variadic
  } else if (actual > maxPositional) {
    // Not variadic
    if (maxPositional == 1) {
      problem = execution.raiseFormat(this->toString(Parts::All), ": Only 1 parameter was expected, not ", actual);
    } else {
      problem = execution.raiseFormat(this->toString(Parts::All), ": No more than ", maxPositional, " parameters were expected, not ", actual);
    }
    return false;
  }
  return true;
}

egg::lang::String egg::lang::IIndexSignature::toString() const {
  return String::concat(this->getResultType()->toString(), "[", this->getIndexType()->toString(), "]");
}

egg::lang::Value egg::lang::IType::promoteAssignment(IExecution& execution, const Value& rhs) const {
  // The default implementation calls IType::canBeAssignedFrom() but does not actually promote
  auto& direct = rhs.direct();
  auto rtype = direct.getRuntimeType();
  if (this->canBeAssignedFrom(*rtype) == AssignmentSuccess::Never) {
    return execution.raiseFormat("Cannot assign a value of type '", rtype->toString(), "' to a target of type '", this->toString(), "'");
  }
  return direct;
}

const egg::lang::IFunctionSignature* egg::lang::IType::callable() const {
  // The default implementation is to say we don't support calling with '()'
  return nullptr;
}

const egg::lang::IIndexSignature* egg::lang::IType::indexable() const {
  // The default implementation is to say we don't support indexing with '[]'
  return nullptr;
}

bool egg::lang::IType::dotable(const String*, ITypeRef&, String& reason) const {
  // The default implementation is to say we don't support properties with '.'
  reason = egg::lang::String::concat("Values of type '", this->toString(), "' do not support the '.' operator for property access");
  return false;
}

bool egg::lang::IType::iterable(ITypeRef&) const {
  // The default implementation is to say we don't support iteration
  return false;
}

egg::lang::Discriminator egg::lang::IType::getSimpleTypes() const {
  // The default implementation is to say we are an object
  return egg::lang::Discriminator::Object;
}

egg::lang::ITypeRef egg::lang::IType::unionWith(const IType& other) const {
  // The default implementation is to simply make a new union (native and simple types can be more clever)
  return Type::makeUnion(*this, other);
}

const egg::lang::IType* egg::lang::Type::getNative(egg::lang::Discriminator tag) {
  // OPTIMIZE
  if (tag == egg::lang::Discriminator::Void) {
    return &typeVoid;
  }
  if (tag == egg::lang::Discriminator::Null) {
    return &typeNull;
  }
  if (tag == egg::lang::Discriminator::Bool) {
    return &typeBool;
  }
  if (tag == egg::lang::Discriminator::Int) {
    return &typeInt;
  }
  if (tag == egg::lang::Discriminator::Float) {
    return &typeFloat;
  }
  if (tag == egg::lang::Discriminator::String) {
    return &typeString;
  }
  if (tag == egg::lang::Discriminator::Arithmetic) {
    return &typeArithmetic;
  }
  return nullptr;
}

egg::lang::ITypeRef egg::lang::Type::makeSimple(Discriminator simple) {
  // Try to use non-reference-counted globals
  auto* native = Type::getNative(simple);
  if (native != nullptr) {
    return ITypeRef(native);
  }
  return defaultAllocator().make<TypeSimple>(simple);
}

egg::lang::ITypeRef egg::lang::Type::makeUnion(const egg::lang::IType& a, const egg::lang::IType& b) {
  // If they are both simple types, just union the tags
  auto sa = a.getSimpleTypes();
  auto sb = b.getSimpleTypes();
  if ((sa != Discriminator::None) && (sb != Discriminator::None)) {
    return Type::makeSimple(sa | sb);
  }
  return defaultAllocator().make<TypeUnion>(a, b);
}
