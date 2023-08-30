#include "yolk/yolk.h"
#include "yolk/lexers.h"
#include "ovum/utf.h"

#include <iomanip>

namespace {
  using namespace egg::yolk;

  class Lexer : public ILexer {
    EGG_NO_COPY(Lexer)
  private:
    TextStream& stream;
  public:
    explicit Lexer(TextStream& stream)
      : stream(stream) {
    }
    virtual LexerKind next(LexerItem& item) override {
      item.verbatim.clear();
      item.value.s.clear();
      auto peek = this->stream.peek();
      item.line = this->stream.getCurrentLine();
      item.column = this->stream.getCurrentColumn();
      if (peek < 0) {
        item.kind = LexerKind::EndOfFile;
      } else if (Lexer::isWhitespace(peek)) {
        this->nextWhitespace(item);
      } else if (Lexer::isIdentifierStart(peek)) {
        this->nextIdentifier(item);
      } else if (Lexer::isDigit(peek)) {
        this->nextNumber(item);
      } else if (peek == '/') {
        switch (this->stream.peek(1)) {
        case '/':
          this->nextCommentSingleLine(item);
          break;
        case '*':
          this->nextCommentMultiLine(item);
          break;
        default:
          this->nextOperator(item);
          break;
        }
      } else if (peek == '"') {
        this->nextQuoted(item);
      } else if (peek == '`') {
        this->nextBackquoted(item);
      } else if (Lexer::isOperator(peek)) {
        this->nextOperator(item);
      } else {
        this->unexpected(item, "Unexpected character", peek);
      }
      return item.kind;
    }
    virtual std::string getResourceName() const override {
      return this->stream.getResourceName();
    }

  private:
    static bool isWhitespace(int ch) {
      return std::isspace(ch) != 0;
    }
    static bool isIdentifierStart(int ch) {
      return (std::isalpha(ch) != 0) || (ch == '_');
    }
    static bool isIdentifierContinue(int ch) {
      return (std::isalnum(ch) != 0) || (ch == '_');
    }
    static bool isDigit(int ch) {
      return std::isdigit(ch) != 0;
    }
    static bool isHexadecimal(int ch) {
      return std::isxdigit(ch) != 0;
    }
    static bool isLetter(int ch) {
      return std::isalpha(ch) != 0;
    }
    static bool isOperator(int ch) {
      return std::strchr("!$%&()*+,-./:;<=>?@[]^{|}~", ch) != nullptr;
    }
    int eat(LexerItem& item) {
      auto curr = this->stream.get();
      assert(curr >= 0);
      egg::ovum::UTF32::toUTF8(std::back_inserter(item.verbatim), char32_t(curr));
      return this->stream.peek();
    }
    void nextWhitespace(LexerItem& item) {
      item.kind = LexerKind::Whitespace;
      int ch;
      do {
        ch = this->eat(item);
      } while (Lexer::isWhitespace(ch));
    }
    void nextCommentSingleLine(LexerItem& item) {
      item.kind = LexerKind::Comment;
      auto line = this->stream.getCurrentLine();
      int ch;
      do {
        ch = this->eat(item);
      } while ((ch >= 0) && (this->stream.getCurrentLine() == line));
    }
    void nextCommentMultiLine(LexerItem& item) {
      item.kind = LexerKind::Comment;
      this->eat(item); // swallow the initial '/'
      int ch0 = this->eat(item); // swallow the initial '*'
      int ch1 = this->eat(item);
      while ((ch0 != '*') || (ch1 != '/')) {
        if (ch1 < 0) {
          this->unexpected(item, "Unexpected end of file found in comment");
        }
        ch0 = ch1;
        ch1 = this->eat(item);
      }
      this->eat(item); // swallow the trailing '/'
    }
    void nextOperator(LexerItem& item) {
      // We mustn't consume extra slashes as this breaks the comment detection
      item.kind = LexerKind::Operator;
      int ch;
      do {
        ch = this->eat(item);
      } while ((ch != '/') && Lexer::isOperator(ch));
    }
    void nextIdentifier(LexerItem& item) {
      item.kind = LexerKind::Identifier;
      int ch;
      do {
        ch = this->eat(item);
      } while (Lexer::isIdentifierContinue(ch));
    }
    void nextNumber(LexerItem& item) {
      // See http://json.org/ but with the addition of hexadecimals
      if (this->stream.peek() == '0') {
        auto peek = this->stream.peek(1);
        if ((peek == 'x') || (peek == 'X')) {
          nextHexadecimal(item);
          return;
        }
        if (Lexer::isDigit(peek)) {
          this->unexpected(item, "Invalid integer constant (extraneous leading '0')");
        }
      }
      auto ch = this->eat(item);
      while (Lexer::isDigit(ch)) {
        ch = this->eat(item);
      }
      switch (ch) {
      case '.':
        nextFloatFraction(item);
        return;
      case 'e':
      case 'E':
        nextFloatExponent(item);
        return;
      }
      if (Lexer::isLetter(ch)) {
        this->unexpected(item, "Unexpected letter in integer constant", ch);
      }
      item.kind = LexerKind::Integer;
      if (!String::tryParseUnsigned(item.value.i, item.verbatim)) {
        this->unexpected(item, "Invalid integer constant");
      }
    }
    void nextHexadecimal(LexerItem& item) {
      assert(this->stream.peek() == '0');
      assert(this->stream.peek(1) == 'x');
      this->eat(item); // swallow '0'
      auto ch = this->eat(item); // swallow 'x'
      while (Lexer::isHexadecimal(ch)) {
        ch = this->eat(item);
      }
      if (Lexer::isLetter(ch)) {
        this->unexpected(item, "Unexpected letter in hexadecimal constant", ch);
      }
      if (item.verbatim.size() <= 2) {
        this->unexpected(item, "Truncated hexadecimal constant");
      }
      if (item.verbatim.size() > 18) {
        this->unexpected(item, "Hexadecimal constant too long");
      }
      item.kind = LexerKind::Integer;
      if (!String::tryParseUnsigned(item.value.i, item.verbatim, 16)) {
        this->unexpected(item, "Invalid hexadecimal integer constant"); // NOCOVERAGE
      }
    }
    void nextFloatFraction(LexerItem& item) {
      assert(this->stream.peek() == '.');
      item.kind = LexerKind::Float;
      auto ch = this->eat(item);
      if (!Lexer::isDigit(ch)) {
        this->unexpected(item, "Expected digit to follow decimal point in floating-point constant", ch);
      }
      do {
        ch = this->eat(item);
      } while (Lexer::isDigit(ch));
      if ((ch == 'e') || (ch == 'E')) {
        nextFloatExponent(item);
      } else if (Lexer::isLetter(ch)) {
        this->unexpected(item, "Unexpected letter in floating-point constant", ch);
      } else if (!String::tryParseFloat(item.value.f, item.verbatim)) {
        this->unexpected(item, "Invalid floating-point constant"); // NOCOVERAGE
      }
    }
    void nextFloatExponent(LexerItem& item) {
      assert((this->stream.peek() == 'e') || (this->stream.peek() == 'E'));
      item.kind = LexerKind::Float;
      auto ch = this->eat(item);
      if ((ch == '+') || (ch == '-')) {
        ch = this->eat(item);
      }
      if (!Lexer::isDigit(ch)) {
        this->unexpected(item, "Expected digit in exponent of floating-point constant", ch);
      }
      do {
        ch = this->eat(item);
      } while (Lexer::isDigit(ch));
      if (Lexer::isLetter(ch)) {
        this->unexpected(item, "Unexpected letter in exponent of floating-point constant", ch);
      } else if (!String::tryParseFloat(item.value.f, item.verbatim)) {
        this->unexpected(item, "Invalid floating-point constant");
      }
    }
    void nextQuoted(LexerItem& item) {
      assert((this->stream.peek() == '"'));
      item.kind = LexerKind::String;
      auto ch = this->eat(item);
      while (ch >= 0) {
        if (ch == '\\') {
          ch = this->eat(item);
          switch (ch) {
          case '"':
          case '\\':
          case '/':
            break;
          case '0':
            ch = '\0';
            break;
          case 'b':
            ch = '\b';
            break;
          case 'f':
            ch = '\f';
            break;
          case 'n':
            ch = '\n';
            break;
          case 'r':
            ch = '\r';
            break;
          case 't':
            ch = '\t';
            break;
          case 'u':
            ch = nextQuotedUnicode16(item);
            break;
          case 'U':
            ch = nextQuotedUnicode32(item);
            break;
          default:
            this->unexpected(item, "Invalid escaped character in quoted string", ch);
            break;
          }
        } else if (ch == '"') {
          if (this->stream.getCurrentLine() != item.line) {
            // There's an EOL in the middle of the string
            this->unexpected(item, "Unexpected end of line found in quoted string");
          }
          this->eat(item);
          return;
        }
        item.value.s.push_back(char32_t(ch));
        ch = this->eat(item);
      }
      this->unexpected(item, "Unexpected end of file found in quoted string");
    }
    int nextQuotedUnicode16(LexerItem& item) {
      assert((this->stream.peek() == 'u'));
      char hex[5];
      for (size_t i = 0; i < 4; ++i) {
        auto ch = this->eat(item);
        if (!Lexer::isHexadecimal(ch)) {
          this->unexpected(item, "Expected hexadecimal digit in '\\u' escape sequence in quoted string", ch);
        }
        hex[i] = char(ch);
      }
      hex[4] = '\0';
      auto value = std::strtol(hex, nullptr, 16);
      assert((value >= 0x0000) && (value <= 0xFFFF));
      return int(value);
    }
    int nextQuotedUnicode32(LexerItem& item) {
      assert((this->stream.peek() == 'U'));
      size_t length;
      char hex[9];
      for (length = 0; length < 8; ++length) {
        auto ch = this->eat(item);
        if (!Lexer::isHexadecimal(ch)) {
          if ((ch == ';') && (length > 0)) {
            break;
          }
          this->unexpected(item, "Expected hexadecimal digit in '\\U' escape sequence in quoted string", ch);
        }
        hex[length] = char(ch);
      }
      assert((length > 0) && (length < sizeof(hex)));
      hex[length] = '\0';
      auto value = std::strtol(hex, nullptr, 16);
      if ((value < 0x0000) || (value > 0x10FFFF)) {
        this->unexpected(item, "Invalid Unicode code point value in '\\U' escape sequence in quoted string", int(value));
      }
      return int(value);
    }
    void nextBackquoted(LexerItem& item) {
      assert((this->stream.peek() == '`'));
      item.kind = LexerKind::String;
      auto ch = this->eat(item);
      while (ch >= 0) {
        if (ch == '`') {
          ch = this->eat(item);
          if (ch != '`') {
            return;
          }
        }
        item.value.s.push_back(char32_t(ch));
        ch = this->eat(item);
      }
      this->unexpected(item, "Unexpected end of file found in backquoted string");
    }
    void unexpected(const LexerItem& item, const std::string& message) {
      throw egg::yolk::SyntaxException(message, this->stream.getResourceName(), item);
    } // NOCOVERAGE
    void unexpected(const LexerItem& item, const std::string& message, int ch) {
      auto token = String::unicodeToString(ch);
      throw egg::yolk::SyntaxException(message + ": " + token, this->stream.getResourceName(), item, token);
    } // NOCOVERAGE
  };

  class FileLexer : public Lexer {
    EGG_NO_COPY(FileLexer)
  private:
    FileTextStream stream;
  public:
    FileLexer(const std::string& path, bool swallowBOM)
      : Lexer(stream), stream(path, swallowBOM) {
    }
  };

  class StringLexer : public Lexer {
    EGG_NO_COPY(StringLexer)
  private:
    StringTextStream stream;
  public:
    explicit StringLexer(const std::string& text, const std::string& resource)
      : Lexer(stream), stream(text, resource) {
    }
  };
}

std::shared_ptr<egg::yolk::ILexer> egg::yolk::LexerFactory::createFromPath(const std::string& path, bool swallowBOM) {
  return std::make_shared<FileLexer>(path, swallowBOM);
}

std::shared_ptr<egg::yolk::ILexer> egg::yolk::LexerFactory::createFromString(const std::string& text, const std::string& resource) {
  return std::make_shared<StringLexer>(text, resource);
}

std::shared_ptr<egg::yolk::ILexer> egg::yolk::LexerFactory::createFromTextStream(egg::yolk::TextStream& stream) {
  return std::make_shared<Lexer>(stream);
}
