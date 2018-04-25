#include "yolk.h"

namespace {
  egg::lang::Value canAlwaysAssignSimple(egg::lang::IExecution& execution, egg::lang::Discriminator lhs, egg::lang::Discriminator rhs) {
    assert(lhs != egg::lang::Discriminator::None);
    if (rhs != egg::lang::Discriminator::None) {
      // The source is a simple type
      auto intersection = egg::lang::Bits::mask(lhs, rhs);
      if (intersection == lhs) {
        // All possible source values can be accommodated in the destination
        return egg::lang::Value::True;
      }
      if (intersection != egg::lang::Discriminator::None) {
        // Only some of the source values can be accommodated in the destination
        return egg::lang::Value::False;
      }
      if (egg::lang::Bits::hasAnySet(lhs, egg::lang::Discriminator::Float) && egg::lang::Bits::hasAnySet(rhs, egg::lang::Discriminator::Int)) {
        // We allow type promotion int->float unless there's an overflow
        return egg::lang::Value::False;
      }
    }
    return execution.raiseFormat("Cannot assign a value of type '", egg::lang::Value::getTagString(rhs), "' to a target of type '", egg::lang::Value::getTagString(lhs), "'");
  }

  egg::lang::Value promoteAssignmentSimple(egg::lang::IExecution& execution, egg::lang::Discriminator lhs, const egg::lang::Value& rhs) {
    if (rhs.has(lhs)) {
      // It's an exact type match
      return rhs;
    }
    if (egg::lang::Bits::hasAnySet(lhs, egg::lang::Discriminator::Float) && rhs.is(egg::lang::Discriminator::Int)) {
      // We allow type promotion int->float
      return egg::lang::Value(double(rhs.getInt())); // TODO overflows?
    }
    return execution.raiseFormat("Cannot promote a value of type '", rhs.getRuntimeType().toString(), "' to a target of type '", egg::lang::Value::getTagString(lhs), "'");
  }

  egg::lang::Value castString(const egg::lang::IParameters& parameters) {
    assert(parameters.getNamedCount() == 0);
    auto n = parameters.getPositionalCount();
    switch (n) {
    case 0:
      return egg::lang::Value::EmptyString;
    case 1:
      return egg::lang::Value{ parameters.getPositional(0).toString() };
    }
    egg::lang::StringBuilder sb;
    for (size_t i = 0; i < n; ++i) {
      sb.add(parameters.getPositional(i).toString());
    }
    return egg::lang::Value{ sb.str() };
  }

  egg::lang::Value castSimple(egg::lang::IExecution& execution, egg::lang::Discriminator tag, const egg::lang::IParameters& parameters) {
    // OPTIMIZE
    if (parameters.getNamedCount() != 0) {
      return execution.raiseFormat("Named parameters in type-casts are not supported");
    }
    if (tag == egg::lang::Discriminator::String) {
      return castString(parameters);
    }
    if (parameters.getPositionalCount() != 1) {
      return execution.raiseFormat("Type-cast expected a single parameter: '", egg::lang::Value::getTagString(tag), "()'");

    }
    auto rhs = parameters.getPositional(0);
    if (rhs.is(tag)) {
      // It's an exact type match
      return rhs;
    }
    if (egg::lang::Bits::hasAnySet(tag, egg::lang::Discriminator::Float) && rhs.is(egg::lang::Discriminator::Int)) {
      // We allow type promotion int->float
      return egg::lang::Value(double(rhs.getInt())); // TODO overflows?
    }
    return execution.raiseFormat("Cannot cast a value of type '", rhs.getRuntimeType().toString(), "' to type '", egg::lang::Value::getTagString(tag), "'");
  }

  egg::lang::Value dotString(egg::lang::IExecution& execution, const egg::lang::String& instance, const egg::lang::String& property) {
// From https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/String/prototype
// Properties:
//  .length
//  .prototype
// Methods:
//  .charAt()
//  .charCodeAt()
//  .codePointAt()
//  .concat()
//  .includes()
//  .endsWith()
//  .indexOf()
//  .lastIndexOf()
//  .localeCompare()
//  .match()
//  .normalize()
//  .padEnd()
//  .padStart()
//  .quote()
//  .repeat()
//  .replace()
//  .search()
//  .slice()
//  .split()
//  .startsWith()
//  .substr()
//  .substring()
//  .toLocaleLowerCase()
//  .toLocaleUpperCase()
//  .toLowerCase()
//  .toString()
//  .toUpperCase()
//  .trim()
//  .trimLeft() ???
//  .trimRight() ???
//  .valueOf()
// Iterable:
//  true

// From https://docs.oracle.com/javase/8/docs/api/?java/lang/String.html
//  .charAt(int index)
//  .codePointAt(int index)
//  .codePointBefore(int index)
//  .codePointCount(int beginIndex, int endIndex)
//  .compareTo(String anotherString)
//  .compareToIgnoreCase(String str)
//  .concat(String str)
//  .contains(CharSequence s)
//  .contentEquals(CharSequence cs)
//  .contentEquals(StringBuffer sb)
//  .endsWith(String suffix)
//  .equals(Object anObject)
//  .equalsIgnoreCase(String anotherString)
//  .getBytes()
//  .getBytes(Charset charset)
//  .getBytes(String charsetName)
//  .getChars(int srcBegin, int srcEnd, char[] dst, int dstBegin)
//  .hashCode()
//  .indexOf(int ch)
//  .indexOf(int ch, int fromIndex)
//  .indexOf(String str)
//  .indexOf(String str, int fromIndex)
//  .intern()
//  .isEmpty()
//  .lastIndexOf(int ch)
//  .lastIndexOf(int ch, int fromIndex)
//  .lastIndexOf(String str)
//  .lastIndexOf(String str, int fromIndex)
//  .length()
//  .matches(String regex)
//  .offsetByCodePoints(int index, int codePointOffset)
//  .regionMatches(boolean ignoreCase, int toffset, String other, int ooffset, int len)
//  .regionMatches(int toffset, String other, int ooffset, int len)
//  .replace(char oldChar, char newChar)
//  .replace(CharSequence target, CharSequence replacement)
//  .replaceAll(String regex, String replacement)
//  .replaceFirst(String regex, String replacement)
//  .split(String regex)
//  .split(String regex, int limit)
//  .startsWith(String prefix)
//  .startsWith(String prefix, int toffset)
//  .subSequence(int beginIndex, int endIndex)
//  .substring(int beginIndex)
//  .substring(int beginIndex, int endIndex)
//  .toCharArray()
//  .toLowerCase()
//  .toLowerCase(Locale locale)
//  .toString()
//  .toUpperCase()
//  .toUpperCase(Locale locale)
//  .trim()

// From http://www.cplusplus.com/reference/string/string/
// Iterators:
//  .begin()
//  .end()
//  .rbegin()
//  .rend()
//  .cbegin()
//  .cend()
//  .crbegin()
//  .crend()
// Capacity:
//  .size()
//  .length()
//  .max_size()
//  .resize()
//  .capacity()
//  .reserve()
//  .clear()
//  .empty()
//  .shrink_to_fit()
// Element access:
//  operator[]
//  .at()
//  .back()
//  .front()
// Modifiers:
//  operator=
//  operator+=
//  .append()
//  .push_back()
//  .assign()
//  .insert()
//  .erase()
//  .replace()
//  .swap()
//  .pop_back()
// String operations:
//  .c_str()
//  .data()
//  .get_allocator()
//  .copy()
//  .find()
//  .rfind()
//  .find_first_of()
//  .find_last_of()
//  .find_first_not_of()
//  .find_last_not_of()
//  .substr()
//  .compare()
// Member constants:
//  .npos
// Non-member functions:
//  operator+
//  relational operators
//  .swap()
//  operator>>
//  operator<<
//  .getline()

// From https://msdn.microsoft.com/en-us/library/system.string(v=vs.110).aspx
// Constructors
//  String(Char*)
//  String(Char*, Int32, Int32)
//  String(Char, Int32)
//  String(Char[])
//  String(Char[], Int32, Int32)
//  String(SByte*)
//  String(SByte*, Int32, Int32)
//  String(SByte*, Int32, Int32, Encoding)
// Properties
//  .Chars[Int32]
//  .Length
// Methods
//  .Clone()
//  static Compare(String, Int32, String, Int32, Int32)
//  static Compare(String, Int32, String, Int32, Int32, Boolean)
//  static Compare(String, Int32, String, Int32, Int32, Boolean, CultureInfo)
//  static Compare(String, Int32, String, Int32, Int32, CultureInfo, CompareOptions)
//  static Compare(String, Int32, String, Int32, Int32, StringComparison)
//  static Compare(String, String)
//  static Compare(String, String, Boolean)
//  static Compare(String, String, Boolean, CultureInfo)
//  static Compare(String, String, CultureInfo, CompareOptions)
//  static Compare(String, String, StringComparison)
//  static CompareOrdinal(String, Int32, String, Int32, Int32)
//  static CompareOrdinal(String, String)
//  .CompareTo(Object)
//  .CompareTo(String)
//  static Concat(IEnumerable<String>)
//  static Concat(Object)
//  static Concat(Object, Object)
//  static Concat(Object, Object, Object)
//  static Concat(Object, Object, Object, Object)
//  static Concat(Object[])
//  static Concat(String, String)
//  static Concat(String, String, String)
//  static Concat(String, String, String, String)
//  static Concat(String[])
//  static Concat<T>(IEnumerable<T>)
//  .Contains(String)
//  static Copy(String)
//  .CopyTo(Int32, Char[], Int32, Int32)
//  .EndsWith(String)
//  .EndsWith(String, Boolean, CultureInfo)
//  .EndsWith(String, StringComparison)
//  .Equals(Object)
//  .Equals(String)
//  static Equals(String, String)
//  static Equals(String, String, StringComparison)
//  .Equals(String, StringComparison)
//  static Format(IFormatProvider, String, Object)
//  static Format(IFormatProvider, String, Object, Object)
//  static Format(IFormatProvider, String, Object, Object, Object)
//  static Format(IFormatProvider, String, Object[])
//  static Format(String, Object)
//  static Format(String, Object, Object)
//  static Format(String, Object, Object, Object)
//  static Format(String, Object[])
//  .GetEnumerator()
//  .GetHashCode()
//  .GetType()
//  .GetTypeCode()
//  .IndexOf(Char)
//  .IndexOf(Char, Int32)
//  .IndexOf(Char, Int32, Int32)
//  .IndexOf(String)
//  .IndexOf(String, Int32)
//  .IndexOf(String, Int32, Int32)
//  .IndexOf(String, Int32, Int32, StringComparison)
//  .IndexOf(String, Int32, StringComparison)
//  .IndexOf(String, StringComparison)
//  .IndexOfAny(Char[])
//  .IndexOfAny(Char[], Int32)
//  .IndexOfAny(Char[], Int32, Int32)
//  .Insert(Int32, String)
//  static Intern(String)
//  static IsInterned(String)
//  .IsNormalized()
//  .IsNormalized(NormalizationForm)
//  static IsNullOrEmpty(String)
//  static IsNullOrWhiteSpace(String)
//  static Join(String, IEnumerable<String>)
//  static Join(String, Object[])
//  static Join(String, String[])
//  static Join(String, String[], Int32, Int32)
//  static Join<T>(String, IEnumerable<T>)
//  .LastIndexOf(Char)
//  .LastIndexOf(Char, Int32)
//  .LastIndexOf(Char, Int32, Int32)
//  .LastIndexOf(String)
//  .LastIndexOf(String, Int32)
//  .LastIndexOf(String, Int32, Int32)
//  .LastIndexOf(String, Int32, Int32, StringComparison)
//  .LastIndexOf(String, Int32, StringComparison)
//  .LastIndexOf(String, StringComparison)
//  .LastIndexOfAny(Char[])
//  .LastIndexOfAny(Char[], Int32)
//  .LastIndexOfAny(Char[], Int32, Int32)
//  .Normalize()
//  .Normalize(NormalizationForm)
//  .PadLeft(Int32)
//  .PadLeft(Int32, Char)
//  .PadRight(Int32)
//  .PadRight(Int32, Char)
//  .Remove(Int32)
//  .Remove(Int32, Int32)
//  .Replace(Char, Char)
//  .Replace(String, String)
//  .Split(Char[])
//  .Split(Char[], Int32)
//  .Split(Char[], Int32, StringSplitOptions)
//  .Split(Char[], StringSplitOptions)
//  .Split(String[], Int32, StringSplitOptions)
//  .Split(String[], StringSplitOptions)
//  .StartsWith(String)
//  .StartsWith(String, Boolean, CultureInfo)
//  .StartsWith(String, StringComparison)
//  .Substring(Int32)
//  .Substring(Int32, Int32)
//  .ToCharArray()
//  .ToCharArray(Int32, Int32)
//  .ToLower()
//  .ToLower(CultureInfo)
//  .ToLowerInvariant()
//  .ToString()
//  .ToString(IFormatProvider)
//  .ToUpper()
//  .ToUpper(CultureInfo)
//  .ToUpperInvariant()
//  .Trim()
//  .Trim(Char[])
//  .TrimEnd(Char[])
//  .TrimStart(Char[])
// Fields
//  static Empty
// Operators
//  static Equality(String, String)
//  static Inequality(String, String)

    auto utf8 = property.toUTF8();
    if (utf8 == "length") {
      return egg::lang::Value{ int64_t(instance.length()) };
    }
    return execution.raiseFormat("Unknown properties for type 'string': '", property, "'");
  }

  egg::lang::Value dotSimple(egg::lang::IExecution& execution, const egg::lang::Value& instance, const egg::lang::String& property) {
    // OPTIMIZE
    if (instance.is(egg::lang::Discriminator::String)) {
      return dotString(execution, instance.getString(), property);
    }
    return execution.raiseFormat("Properties are not yet supported for '", instance.getRuntimeType().toString(), "'");
  }

  void formatSourceLocation(egg::lang::StringBuilder& sb, const egg::lang::LocationSource& location) {
    sb.add(location.file);
    if (location.column > 0) {
      sb.add("(").add(location.line).add(",").add(location.column).add(")");
    } else if (location.line > 0) {
      sb.add("(").add(location.line).add(")");
    }
  }

  class StringBufferUTF8 : public egg::gc::HardReferenceCounted<egg::lang::IString> {
    EGG_NO_COPY(StringBufferUTF8);
  private:
    std::string utf8;
  public:
    explicit StringBufferUTF8(const std::string& other)
      : utf8(other) {
    }
    virtual size_t length() const override {
      return this->utf8.size(); // TODO UTF8 encoding
    }
    virtual bool empty() const override {
      return this->utf8.empty();
    }
    virtual bool equal(const IString& other) const override {
      auto rhs = other.toUTF8();
      return this->utf8 == rhs;
    }
    virtual bool less(const IString& other) const override {
      auto rhs = other.toUTF8();
      return this->utf8 < rhs;
    }
    virtual std::string toUTF8() const override {
      return this->utf8;
    }
  };

  class StringEmpty : public egg::gc::NotReferenceCounted<egg::lang::IString> {
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
    virtual std::string toUTF8() const override {
      return std::string();
    }
  };
  const StringEmpty stringEmpty{};

  class TypeReference : public egg::gc::HardReferenceCounted<egg::lang::IType> {
    EGG_NO_COPY(TypeReference);
  private:
    egg::lang::ITypeRef referenced;
  public:
    explicit TypeReference(const egg::lang::IType& referenced)
      : referenced(&referenced) {
    }
    virtual egg::lang::String toString() const override {
      return egg::lang::String::concat(this->referenced->toString(), "*");
    }
    virtual egg::lang::ITypeRef referencedType() const override {
      return this->referenced;
    }
    virtual egg::lang::Value canAlwaysAssignFrom(egg::lang::IExecution& execution, const egg::lang::IType&) const override {
      return execution.raiseFormat("TODO: Cannot yet assign to reference value"); // TODO
    }
    virtual egg::lang::Value promoteAssignment(egg::lang::IExecution& execution, const egg::lang::Value&) const override {
      return execution.raiseFormat("TODO: Cannot yet assign to reference value"); // TODO
    }
  };

  class TypeNull : public egg::gc::NotReferenceCounted<egg::lang::IType>{
  private:
    egg::lang::String name;
  public:
    TypeNull()
      : name(egg::lang::String::fromUTF8("null")) {
    }
    virtual egg::lang::String toString() const override {
      return this->name;
    }
    virtual egg::lang::Discriminator getSimpleTypes() const override {
      return egg::lang::Discriminator::Null;
    }
    virtual egg::lang::ITypeRef coallescedType(const IType& rhs) const override {
      // We're always null, so the type is just the type of the rhs
      return egg::lang::ITypeRef(&rhs);
    }
    virtual egg::lang::ITypeRef unionWith(const IType& other) const override {
      auto simple = other.getSimpleTypes();
      if (egg::lang::Bits::hasAnySet(simple, egg::lang::Discriminator::Null)) {
        // The other type supports Null anyway
        return egg::lang::ITypeRef(&other);
      }
      return egg::lang::Type::makeUnion(*this, other);
    }
    virtual egg::lang::Value canAlwaysAssignFrom(egg::lang::IExecution& execution, const egg::lang::IType&) const override {
      return execution.raiseFormat("Cannot assign to 'null' value");
    }
    virtual egg::lang::Value promoteAssignment(egg::lang::IExecution& execution, const egg::lang::Value&) const override {
      return execution.raiseFormat("Cannot assign to 'null' value");
    }
  };
  const TypeNull typeNull{};

  template<egg::lang::Discriminator TAG>
  class TypeNative : public egg::gc::NotReferenceCounted<egg::lang::IType> {
  private:
    egg::lang::String name;
  public:
    TypeNative()
      : name(egg::lang::String::fromUTF8(egg::lang::Value::getTagString(TAG))) {
      assert(!egg::lang::Bits::hasAnySet(TAG, egg::lang::Discriminator::Null));
    }
    virtual egg::lang::String toString() const override {
      return this->name;
    }
    virtual egg::lang::Discriminator getSimpleTypes() const override {
      return TAG;
    }
    virtual egg::lang::ITypeRef unionWith(const IType& other) const override {
      if (other.getSimpleTypes() == TAG) {
        // It's the identical native type
        return egg::lang::ITypeRef(this);
      }
      return egg::lang::Type::makeUnion(*this, other);
    }
    virtual egg::lang::Value canAlwaysAssignFrom(egg::lang::IExecution& execution, const egg::lang::IType& rhs) const override {
      return canAlwaysAssignSimple(execution, TAG, rhs.getSimpleTypes());
    }
    virtual egg::lang::Value promoteAssignment(egg::lang::IExecution& execution, const egg::lang::Value& rhs) const override {
      return promoteAssignmentSimple(execution, TAG, rhs);
    }
    virtual egg::lang::Value cast(egg::lang::IExecution& execution, const egg::lang::IParameters& parameters) const override {
      return castSimple(execution, TAG, parameters);
    }
    virtual egg::lang::Value dotGet(egg::lang::IExecution& execution, const egg::lang::Value& instance, const egg::lang::String& property) const override {
      return dotSimple(execution, instance, property);
    }
  };
  const TypeNative<egg::lang::Discriminator::Void> typeVoid{};
  const TypeNative<egg::lang::Discriminator::Bool> typeBool{};
  const TypeNative<egg::lang::Discriminator::Int> typeInt{};
  const TypeNative<egg::lang::Discriminator::Float> typeFloat{};
  const TypeNative<egg::lang::Discriminator::String> typeString{};
  const TypeNative<egg::lang::Discriminator::Arithmetic> typeArithmetic{};
  const TypeNative<egg::lang::Discriminator::Any> typeAny{};

  class TypeSimple : public egg::gc::HardReferenceCounted<egg::lang::IType> {
    EGG_NO_COPY(TypeSimple);
  private:
    egg::lang::Discriminator tag;
  public:
    explicit TypeSimple(egg::lang::Discriminator tag) : tag(tag) {
    }
    virtual egg::lang::Discriminator getSimpleTypes() const override {
      return this->tag;
    }
    virtual egg::lang::ITypeRef coallescedType(const IType& rhs) const override {
      auto denulled = egg::lang::Bits::clear(this->tag, egg::lang::Discriminator::Null);
      if (this->tag != denulled) {
        // We need to clear the bit
        return egg::lang::Type::makeSimple(denulled)->unionWith(rhs);
      }
      return this->unionWith(rhs);
    }
    virtual egg::lang::ITypeRef unionWith(const IType& other) const override {
      auto simple = other.getSimpleTypes();
      if (simple == egg::lang::Discriminator::None) {
        // The other type is not simple
        return egg::lang::Type::makeUnion(*this, other);
      }
      auto both = egg::lang::Bits::set(this->tag, simple);
      if (both != this->tag) {
        // There's a new simple type that we don't support, so create a new type
        return egg::lang::Type::makeSimple(both);
      }
      return egg::lang::ITypeRef(this);
    }
    virtual egg::lang::Value canAlwaysAssignFrom(egg::lang::IExecution& execution, const egg::lang::IType& rhs) const override {
      return canAlwaysAssignSimple(execution, this->tag, rhs.getSimpleTypes());
    }
    virtual egg::lang::Value promoteAssignment(egg::lang::IExecution& execution, const egg::lang::Value& rhs) const override {
      return promoteAssignmentSimple(execution, this->tag, rhs);
    }
    virtual egg::lang::String toString() const override {
      return egg::lang::String::fromUTF8(egg::lang::Value::getTagString(this->tag));
    }
  };

  class TypeUnion : public egg::gc::HardReferenceCounted<egg::lang::IType> {
    EGG_NO_COPY(TypeUnion);
  private:
    egg::lang::ITypeRef a;
    egg::lang::ITypeRef b;
  public:
    TypeUnion(const egg::lang::IType& a, const egg::lang::IType& b)
      : a(&a), b(&b) {
    }
    virtual egg::lang::String toString() const override {
      return egg::lang::String::concat(this->a->toString(), "|", this->b->toString());
    }
    virtual egg::lang::Value canAlwaysAssignFrom(egg::lang::IExecution& execution, const egg::lang::IType&) const override {
      return execution.raiseFormat("TODO: Cannot yet assign to union value"); // TODO
    }
    virtual egg::lang::Value promoteAssignment(egg::lang::IExecution& execution, const egg::lang::Value&) const override {
      return execution.raiseFormat("TODO: Cannot yet assign to union value"); // TODO
    }
  };

  class ExceptionType : public egg::gc::NotReferenceCounted<egg::lang::IType> {
  public:
    virtual egg::lang::String toString() const override {
      return egg::lang::String::fromUTF8("exception");
    }
    virtual egg::lang::Value canAlwaysAssignFrom(egg::lang::IExecution& execution, const egg::lang::IType&) const override {
      return execution.raiseFormat("Cannot re-assign exceptions");
    }
    virtual egg::lang::Value promoteAssignment(egg::lang::IExecution& execution, const egg::lang::Value&) const override {
      return execution.raiseFormat("Cannot re-assign exceptions");
    }
  };

  class Exception : public egg::gc::HardReferenceCounted<egg::lang::IObject> {
    EGG_NO_COPY(Exception);
  private:
    static const ExceptionType type;
    egg::lang::LocationRuntime location;
    egg::lang::String message;
  public:
    explicit Exception(const egg::lang::LocationRuntime& location, const egg::lang::String& message)
      : location(location), message(message) {
    }
    virtual bool dispose() override {
      return false;
    }
    virtual egg::lang::Value toString() const override {
      auto where = this->location.toSourceString();
      if (where.length() > 0) {
        return egg::lang::Value(egg::lang::String::concat(where, ": ", this->message));
      }
      return egg::lang::Value(this->message);
    }
    virtual egg::lang::Value getRuntimeType() const override {
      return egg::lang::Value(Exception::type);
    }
    virtual egg::lang::Value call(egg::lang::IExecution& execution, const egg::lang::IParameters&) override {
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
const egg::lang::Type egg::lang::Type::Any{ typeAny };

// Empty constants
const egg::lang::IString& egg::lang::String::emptyBuffer = stringEmpty;
const egg::lang::String egg::lang::String::Empty{ stringEmpty };

egg::lang::String egg::lang::String::fromUTF8(const std::string& utf8) {
  return String(*new StringBufferUTF8(utf8));
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

egg::lang::Value::Value(const Value& value) {
  this->copyInternals(value);
}

egg::lang::Value::Value(Value&& value) {
  this->moveInternals(value);
}

egg::lang::Value& egg::lang::Value::operator=(const Value& value) {
  if (this != &value) {
    this->copyInternals(value);
  }
  return *this;
}

egg::lang::Value& egg::lang::Value::operator=(Value&& value) {
  if (this != &value) {
    this->moveInternals(value);
  }
  return *this;
}

egg::lang::Value::~Value() {
  if (this->has(Discriminator::Type)) {
    this->t->releaseHard();
  } else if (this->has(Discriminator::Object)) {
    this->o->releaseHard();
  } else if (this->has(Discriminator::String)) {
    this->s->releaseHard();
  }
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
  if (tag == egg::lang::Discriminator::Any) {
    return &typeAny;
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
