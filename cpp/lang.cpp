#include "yolk.h"

namespace {
  using namespace egg::lang;

  Value canAlwaysAssignSimple(IExecution& execution, Discriminator lhs, Discriminator rhs) {
    assert(lhs != Discriminator::None);
    if (rhs != Discriminator::None) {
      // The source is a simple type
      auto intersection = Bits::mask(lhs, rhs);
      if (intersection == lhs) {
        // All possible source values can be accommodated in the destination
        return Value::True;
      }
      if (intersection != Discriminator::None) {
        // Only some of the source values can be accommodated in the destination
        return Value::False;
      }
      if (Bits::hasAnySet(lhs, Discriminator::Float) && Bits::hasAnySet(rhs, Discriminator::Int)) {
        // We allow type promotion int->float unless there's an overflow
        return Value::False;
      }
    }
    return execution.raiseFormat("Cannot assign a value of type '", Value::getTagString(rhs), "' to a target of type '", Value::getTagString(lhs), "'");
  }

  Value promoteAssignmentSimple(IExecution& execution, Discriminator lhs, const Value& rhs) {
    if (rhs.has(lhs)) {
      // It's an exact type match
      return rhs;
    }
    if (Bits::hasAnySet(lhs, Discriminator::Float) && rhs.is(Discriminator::Int)) {
      // We allow type promotion int->float
      return Value(double(rhs.getInt())); // TODO overflows?
    }
    return execution.raiseFormat("Cannot promote a value of type '", rhs.getRuntimeType().toString(), "' to a target of type '", Value::getTagString(lhs), "'");
  }

  Value castString(const IParameters& parameters) {
    assert(parameters.getNamedCount() == 0);
    auto n = parameters.getPositionalCount();
    switch (n) {
    case 0:
      return Value::EmptyString;
    case 1:
      return Value{ parameters.getPositional(0).toString() };
    }
    StringBuilder sb;
    for (size_t i = 0; i < n; ++i) {
      sb.add(parameters.getPositional(i).toString());
    }
    return Value{ sb.str() };
  }

  Value castSimple(IExecution& execution, Discriminator tag, const IParameters& parameters) {
    // OPTIMIZE
    if (parameters.getNamedCount() != 0) {
      return execution.raiseFormat("Named parameters in type-casts are not supported");
    }
    if (tag == Discriminator::String) {
      return castString(parameters);
    }
    if (parameters.getPositionalCount() != 1) {
      return execution.raiseFormat("Type-cast expected a single parameter: '", Value::getTagString(tag), "()'");

    }
    auto rhs = parameters.getPositional(0);
    if (rhs.is(tag)) {
      // It's an exact type match
      return rhs;
    }
    if (Bits::hasAnySet(tag, Discriminator::Float) && rhs.is(Discriminator::Int)) {
      // We allow type promotion int->float
      return Value(double(rhs.getInt())); // TODO overflows?
    }
    return execution.raiseFormat("Cannot cast a value of type '", rhs.getRuntimeType().toString(), "' to type '", Value::getTagString(tag), "'");
  }

  Value dotSimple(IExecution& execution, const Value& instance, const String& property) {
    // OPTIMIZE
    if (instance.is(Discriminator::String)) {
      return instance.getString().builtin(execution, property);
    }
    return execution.raiseFormat("Properties are not yet supported for '", instance.getRuntimeType().toString(), "'");
  }

  Value bracketsString(IExecution& execution, const String& instance, const Value& index) {
    // string operator[](int index)
    if (!index.is(Discriminator::Int)) {
      return execution.raiseFormat("String indexing '[]' only supports indices of type 'int', not '", index.getRuntimeType().toString(), "'");
    }
    auto i = index.getInt();
    auto c = instance.codePointAt(size_t(i));
    if (c < 0) {
      // Invalid index
      if ((i < 0) || (size_t(i) >= instance.length())) {
        return execution.raiseFormat("String index ", i, " is out of range for a string of length ", instance.length());
      }
      return execution.raiseFormat("Cannot index a malformed string");
    }
    return Value{ String::fromCodePoint(char32_t(c)) };
  }

  void formatSourceLocation(StringBuilder& sb, const LocationSource& location) {
    sb.add(location.file);
    if (location.column > 0) {
      sb.add("(").add(location.line).add(",").add(location.column).add(")");
    } else if (location.line > 0) {
      sb.add("(").add(location.line).add(")");
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
    explicit StringBufferCodePoint(char32_t codepoint)
      : HardReferenceCounted(0), codepoint(codepoint) {
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
      static String space{ *new StringBufferCodePoint(U' ') };
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
    StringBufferUTF8(const std::string& utf8, size_t length)
      : HardReferenceCounted(0), utf8(utf8), codepoints(length) {
      // Creation is via factories below
      assert(!this->utf8.empty());
      assert(this->codepoints > 0);
    }
  public:
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
      return new StringBufferUTF8(sub, length);
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
      return new StringBufferUTF8(utf8, length);
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
          begin = index + separator.length();
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
      sb.add(between).add(part[i]);
    }
    return StringBufferUTF8::create(sb.toUTF8());
  }

  class TypeReference : public egg::gc::HardReferenceCounted<IType> {
    EGG_NO_COPY(TypeReference);
  private:
    ITypeRef referenced;
  public:
    explicit TypeReference(const IType& referenced)
      : HardReferenceCounted(0), referenced(&referenced) {
    }
    virtual String toString() const override {
      return String::concat(this->referenced->toString(), "*");
    }
    virtual ITypeRef referencedType() const override {
      return this->referenced;
    }
    virtual Value canAlwaysAssignFrom(IExecution& execution, const IType&) const override {
      return execution.raiseFormat("TODO: Cannot yet assign to reference value"); // TODO
    }
    virtual Value promoteAssignment(IExecution& execution, const Value&) const override {
      return execution.raiseFormat("TODO: Cannot yet assign to reference value"); // TODO
    }
    virtual const ISignature* callable(IExecution&) const override {
      return nullptr;
    }
  };

  class TypeNull : public egg::gc::NotReferenceCounted<IType>{
  private:
    String name;
  public:
    TypeNull()
      : name(String::fromUTF8("null")) {
    }
    virtual String toString() const override {
      return this->name;
    }
    virtual Discriminator getSimpleTypes() const override {
      return Discriminator::Null;
    }
    virtual ITypeRef coallescedType(const IType& rhs) const override {
      // We're always null, so the type is just the type of the rhs
      return ITypeRef(&rhs);
    }
    virtual ITypeRef unionWith(const IType& other) const override {
      auto simple = other.getSimpleTypes();
      if (Bits::hasAnySet(simple, Discriminator::Null)) {
        // The other type supports Null anyway
        return ITypeRef(&other);
      }
      return Type::makeUnion(*this, other);
    }
    virtual Value canAlwaysAssignFrom(IExecution& execution, const IType&) const override {
      return execution.raiseFormat("Cannot assign to 'null' value");
    }
    virtual Value promoteAssignment(IExecution& execution, const Value&) const override {
      return execution.raiseFormat("Cannot assign to 'null' value");
    }
    virtual const ISignature* callable(IExecution&) const override {
      return nullptr;
    }
  };
  const TypeNull typeNull{};

  template<Discriminator TAG>
  class TypeNative : public egg::gc::NotReferenceCounted<IType> {
  private:
    String name;
  public:
    TypeNative()
      : name(String::fromUTF8(Value::getTagString(TAG))) {
      assert(!Bits::hasAnySet(TAG, Discriminator::Null));
    }
    virtual String toString() const override {
      return this->name;
    }
    virtual Discriminator getSimpleTypes() const override {
      return TAG;
    }
    virtual ITypeRef unionWith(const IType& other) const override {
      if (other.getSimpleTypes() == TAG) {
        // It's the identical native type
        return ITypeRef(this);
      }
      return Type::makeUnion(*this, other);
    }
    virtual Value canAlwaysAssignFrom(IExecution& execution, const IType& rhs) const override {
      return canAlwaysAssignSimple(execution, TAG, rhs.getSimpleTypes());
    }
    virtual Value promoteAssignment(IExecution& execution, const Value& rhs) const override {
      return promoteAssignmentSimple(execution, TAG, rhs);
    }
    virtual const ISignature* callable(IExecution&) const override {
      return nullptr;
    }
    virtual Value cast(IExecution& execution, const IParameters& parameters) const override {
      return castSimple(execution, TAG, parameters);
    }
  };
  const TypeNative<Discriminator::Void> typeVoid{};
  const TypeNative<Discriminator::Bool> typeBool{};
  const TypeNative<Discriminator::Int> typeInt{};
  const TypeNative<Discriminator::Float> typeFloat{};
  const TypeNative<Discriminator::Arithmetic> typeArithmetic{};

  class TypeString : public TypeNative<Discriminator::String> {
  public:
    virtual Value dotGet(IExecution& execution, const Value& instance, const String& property) const override {
      return instance.getString().builtin(execution, property);
    }
    virtual Value bracketsGet(IExecution& execution, const Value& instance, const Value& index) const override {
      return bracketsString(execution, instance.getString(), index);
    }
  };
  const TypeString typeString{};

  class TypeSimple : public egg::gc::HardReferenceCounted<IType> {
    EGG_NO_COPY(TypeSimple);
  private:
    Discriminator tag;
  public:
    explicit TypeSimple(Discriminator tag)
      : HardReferenceCounted(0), tag(tag) {
    }
    virtual String toString() const override {
      return String::fromUTF8(Value::getTagString(this->tag));
    }
    virtual Discriminator getSimpleTypes() const override {
      return this->tag;
    }
    virtual ITypeRef coallescedType(const IType& rhs) const override {
      auto denulled = Bits::clear(this->tag, Discriminator::Null);
      if (this->tag != denulled) {
        // We need to clear the bit
        return Type::makeSimple(denulled)->unionWith(rhs);
      }
      return this->unionWith(rhs);
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
    virtual Value canAlwaysAssignFrom(IExecution& execution, const IType& rhs) const override {
      return canAlwaysAssignSimple(execution, this->tag, rhs.getSimpleTypes());
    }
    virtual Value promoteAssignment(IExecution& execution, const Value& rhs) const override {
      return promoteAssignmentSimple(execution, this->tag, rhs);
    }
    virtual const ISignature* callable(IExecution&) const override {
      return nullptr;
    }
    virtual Value dotGet(IExecution& execution, const Value& instance, const String& property) const override {
      return dotSimple(execution, instance, property);
    }
  };

  class TypeUnion : public egg::gc::HardReferenceCounted<IType> {
    EGG_NO_COPY(TypeUnion);
  private:
    ITypeRef a;
    ITypeRef b;
  public:
    TypeUnion(const IType& a, const IType& b)
      : HardReferenceCounted(0), a(&a), b(&b) {
    }
    virtual String toString() const override {
      return String::concat(this->a->toString(), "|", this->b->toString());
    }
    virtual Value canAlwaysAssignFrom(IExecution& execution, const IType&) const override {
      return execution.raiseFormat("TODO: Cannot yet assign to union value"); // TODO
    }
    virtual Value promoteAssignment(IExecution& execution, const Value&) const override {
      return execution.raiseFormat("TODO: Cannot yet assign to union value"); // TODO
    }
    virtual const ISignature* callable(IExecution&) const override {
      EGG_THROW("TODO: Cannot yet call to union value"); // TODO
    }
  };

  class ExceptionType : public egg::gc::NotReferenceCounted<IType> {
  public:
    virtual String toString() const override {
      return String::fromUTF8("exception");
    }
    virtual Value canAlwaysAssignFrom(IExecution& execution, const IType&) const override {
      return execution.raiseFormat("Cannot re-assign exceptions");
    }
    virtual Value promoteAssignment(IExecution& execution, const Value&) const override {
      return execution.raiseFormat("Cannot re-assign exceptions");
    }
    virtual const ISignature* callable(IExecution&) const override {
      return nullptr;
    }
  };

  class Exception : public egg::gc::HardReferenceCounted<IObject> {
    EGG_NO_COPY(Exception);
  private:
    static const ExceptionType type;
    LocationRuntime location;
    String message;
  public:
    Exception(const LocationRuntime& location, const String& message)
      : HardReferenceCounted(0), location(location), message(message) {
    }
    virtual bool dispose() override {
      return false;
    }
    virtual Value toString() const override {
      auto where = this->location.toSourceString();
      if (where.length() > 0) {
        return Value(String::concat(where, ": ", this->message));
      }
      return Value(this->message);
    }
    virtual Value getRuntimeType() const override {
      return Value(Exception::type);
    }
    virtual Value call(IExecution& execution, const IParameters&) override {
      return execution.raiseFormat("Exceptions cannot be called");
    }
  };
  const ExceptionType Exception::type{};
}

// Native types
const egg::lang::Type egg::lang::Type::Void{ typeVoid };
const egg::lang::Type egg::lang::Type::Null{ typeNull };
const egg::lang::Type egg::lang::Type::Bool{ typeBool };
const egg::lang::Type egg::lang::Type::Int{ typeInt };
const egg::lang::Type egg::lang::Type::Float{ typeFloat };
const egg::lang::Type egg::lang::Type::String{ typeString };
const egg::lang::Type egg::lang::Type::Arithmetic{ typeArithmetic };
const egg::lang::Type egg::lang::Type::Any{ *Type::make<TypeSimple>(egg::lang::Discriminator::Any) };

// Constants
const egg::lang::IString& egg::lang::String::emptyBuffer = stringEmpty;
const egg::lang::String egg::lang::String::Empty{ stringEmpty };
const egg::lang::String egg::lang::String::Space{ StringBufferCodePoint::makeSpace() };

egg::lang::String egg::lang::String::fromCodePoint(char32_t codepoint) {
  return String(*new StringBufferCodePoint(codepoint));
}

egg::lang::String egg::lang::String::fromUTF8(const std::string& utf8) {
  return String(*StringBufferUTF8::create(utf8));
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
  if (this->has(Discriminator::Type)) {
    this->t = other.t->acquireHard();
  } else if (this->has(Discriminator::Object)) {
    this->o = other.o->acquireHard();
  } else if (this->has(Discriminator::String)) {
    this->s = other.s->acquireHard();
  } else if (this->has(Discriminator::Float)) {
    this->f = other.f;
  } else if (this->has(Discriminator::Int)) {
    this->i = other.i;
  } else if (this->has(Discriminator::Bool)) {
    this->b = other.b;
  }
}

void egg::lang::Value::moveInternals(Value& other) {
  this->tag = other.tag;
  if (this->has(Discriminator::Type)) {
    this->t = other.t;
  } else if (this->has(Discriminator::Object)) {
    this->o = other.o;
  } else if (this->has(Discriminator::String)) {
    this->s = other.s;
  } else if (this->has(Discriminator::Float)) {
    this->f = other.f;
  } else if (this->has(Discriminator::Int)) {
    this->i = other.i;
  } else if (this->has(Discriminator::Bool)) {
    this->b = other.b;
  }
  other.tag = Discriminator::None;
}

void egg::lang::Value::destroyInternals() {
  if (this->has(Discriminator::Type)) {
    this->t->releaseHard();
  } else if (this->has(Discriminator::Object)) {
    this->o->releaseHard();
  } else if (this->has(Discriminator::String)) {
    this->s->releaseHard();
  }
}

egg::lang::Value::Value(const Value& value) {
  this->copyInternals(value);
}

egg::lang::Value::Value(Value&& value) {
  this->moveInternals(value);
}

egg::lang::Value& egg::lang::Value::operator=(const Value& value) {
  if (this != &value) {
    this->destroyInternals();
    this->copyInternals(value);
  }
  return *this;
}

egg::lang::Value& egg::lang::Value::operator=(Value&& value) {
  if (this != &value) {
    this->destroyInternals();
    this->moveInternals(value);
  }
  return *this;
}

egg::lang::Value::~Value() {
  this->destroyInternals();
}

bool egg::lang::Value::equal(const Value& lhs, const Value& rhs) {
  if (lhs.tag != rhs.tag) {
    return false;
  }
  if (lhs.tag == Discriminator::Bool) {
    return lhs.b == rhs.b;
  }
  if (lhs.tag == Discriminator::Int) {
    return lhs.i == rhs.i;
  }
  if (lhs.tag == Discriminator::Float) {
    return lhs.f == rhs.f;
  }
  if (lhs.tag == Discriminator::String) {
    return lhs.s->equal(*rhs.s);
  }
  if (lhs.tag == Discriminator::Type) {
    return lhs.t == rhs.t;
  }
  return lhs.o == rhs.o;
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
      sb.add(" ");
    }
    sb.add("[").add(this->function).add("]");
  }
  return sb.str();
}

egg::lang::Value egg::lang::Value::raise(const LocationRuntime& location, const String& message) {
  auto exception = Value::make<Exception>(location, message);
  exception.addFlowControl(Discriminator::Exception);
  return exception;
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
    { int(Discriminator::Type), "type" },
    { int(Discriminator::Object), "object" },
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
    return egg::yolk::String::fromEnum(Bits::clear(tag, Discriminator::Null), table) + "?";
  }
  return egg::yolk::String::fromEnum(tag, table);
}

const egg::lang::IType& egg::lang::Value::getRuntimeType() const {
  if (this->tag == Discriminator::Type) {
    // TODO Is the runtime type of a type just the type itself?
    return *this->t;
  }
  if (this->tag == Discriminator::Object) {
    // Ask the object for its type
    auto runtime = this->o->getRuntimeType();
    if (runtime.is(Discriminator::Type)) {
      return *runtime.t;
    }
  }
  auto* native = Type::getNative(this->tag);
  if (native != nullptr) {
    return *native;
  }
  EGG_THROW("Internal type error: Unknown runtime type");
}

egg::lang::String egg::lang::Value::toString() const {
  // OPTIMIZE
  if (this->tag == Discriminator::Object) {
    auto str = this->o->toString();
    if (str.tag == Discriminator::String) {
      return str.getString();
    }
    return String::fromUTF8("[invalid]");
  }
  return String::fromUTF8(this->toUTF8());
}

std::string egg::lang::Value::toUTF8() const {
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
  if (this->tag == Discriminator::Type) {
    return "[type]"; // TODO
  }
  if (this->tag == Discriminator::Object) {
    auto str = this->o->toString();
    if (str.tag == Discriminator::String) {
      return str.getString().toUTF8();
    }
    return "[invalid]";
  }
  return "[" + Value::getTagString(this->tag) + "]";
}

egg::lang::ITypeRef egg::lang::IType::referencedType() const {
  // The default implementation is to return a new type 'Type*'
  return egg::lang::ITypeRef::make<TypeReference>(*this);
}

egg::lang::ITypeRef egg::lang::IType::dereferencedType() const {
  // The default implementation is to return 'Void' indicating that this is NOT dereferencable
  return Type::Void;
}

egg::lang::ITypeRef egg::lang::IType::coallescedType(const IType& rhs) const {
  // The default implementation is to create the union
  return this->unionWith(rhs);
}

egg::lang::Value egg::lang::IType::decantParameters(egg::lang::IExecution& execution, const egg::lang::IParameters&, Setter) const {
  // The default implementation is to return an error (only function-like types decant parameters)
  return execution.raiseFormat("Internal type error: Cannot decant parameters for type '", this->toString(), "'");
}

egg::lang::Value egg::lang::IType::cast(IExecution& execution, const IParameters&) const {
  // The default implementation is to return an error (only native types are castable)
  return execution.raiseFormat("Internal type error: Cannot cast to type '", this->toString(), "'");
}

egg::lang::Value egg::lang::IType::dotGet(IExecution& execution, const Value&, const String& property) const {
  // The default implementation is to return an error (only complex types support dot-properties)
  return execution.raiseFormat("Values of type '", this->toString(), "' do not support properties such as '.", property, "'");
}

egg::lang::Value egg::lang::IType::bracketsGet(IExecution& execution, const Value&, const Value&) const {
  // The default implementation is to return an error (only complex types support index-lookup)
  return execution.raiseFormat("Values of type '", this->toString(), "' do not support the indexing '[]'");
}

egg::lang::String egg::lang::ISignature::toString() const {
  // TODO better formatting of named/variadic etc.
  StringBuilder sb;
  sb.add(this->getReturnType().toString(), " ", this->getFunctionName(), "(");
  auto n = this->getParameterCount();
  for (size_t i = 0; i < n; ++i) {
    if (i > 0) {
      sb.add(", ");
    }
    auto& parameter = this->getParameter(i);
    assert(parameter.getPosition() != SIZE_MAX);
    if (parameter.isVariadic()) {
      sb.add("...");
    } else {
      sb.add(parameter.getType().toString());
      auto pname = parameter.getName();
      if (!pname.empty()) {
        sb.add(" ", pname);
      }
      if (!parameter.isRequired()) {
        sb.add(" = null");
      }
    }
  }
  sb.add(")");
  return sb.str();
}

bool egg::lang::ISignature::validateCall(IExecution& execution, const IParameters& runtime, Value& problem) const {
  // The default implementation just calls validateCallDefault()
  return this->validateCallDefault(execution, runtime, problem);
}

bool egg::lang::ISignature::validateCallDefault(IExecution& execution, const IParameters& parameters, Value& problem) const {
  // TODO type checking, etc
  if (parameters.getNamedCount() > 0) {
    problem = execution.raiseFormat(this->toString(), ": Named parameters are not yet supported"); // TODO
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
      problem = execution.raiseFormat(this->toString(), ": At least one parameter was expected");
    } else {
      problem = execution.raiseFormat(this->toString(), ": At least ", minPositional, " parameters were expected, not ", actual);
    }
    return false;
  }
  if ((maxPositional > 0) && this->getParameter(maxPositional - 1).isVariadic()) {
    // TODO Variadic
  } else if (actual > maxPositional) {
    // Not variadic
    if (maxPositional == 1) {
      problem = execution.raiseFormat(this->toString(), ": Only one parameter was expected, not ", actual);
    } else {
      problem = execution.raiseFormat(this->toString(), ": No more than ", maxPositional, " parameters were expected, not ", actual);
    }
    return false;
  }
  return true;
}

egg::lang::Discriminator egg::lang::IType::getSimpleTypes() const {
  // The default implementation is to say we don't support any simple types
  return egg::lang::Discriminator::None;
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
  return Type::make<TypeSimple>(simple);
}

egg::lang::ITypeRef egg::lang::Type::makeUnion(const egg::lang::IType& a, const egg::lang::IType& b) {
  // If they are both simple types, just union the tags
  auto sa = a.getSimpleTypes();
  auto sb = b.getSimpleTypes();
  if ((sa != Discriminator::None) && (sb != Discriminator::None)) {
    return Type::makeSimple(sa | sb);
  }
  return Type::make<TypeUnion>(a, b);
}
