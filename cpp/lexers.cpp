#include "yolk.h"

namespace {
  using namespace egg::yolk;

  class Lexer : public ILexer {
    EGG_NO_COPY(Lexer);
  private:
    TextStream & stream;
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
        this->unexpected(item, "Unexpected code point");
      }
      return item.kind;
    }
  private:
    static bool isWhitespace(int ch) {
      return std::isspace(ch);
    }
    static bool isIdentifierStart(int ch) {
      return std::isalpha(ch) || (ch == '_');
    }
    static bool isIdentifierContinue(int ch) {
      return std::isalnum(ch) || (ch == '_');
    }
    static bool isDigit(int ch) {
      return std::isdigit(ch);
    }
    static bool isOperator(int ch) {
      return std::strchr("!$%&()*+,-./:;<=>?[]^{|}~", ch) != nullptr;
    }
    int eat(LexerItem& item) {
      auto curr = this->stream.get();
      assert(curr >= 0);
      String::push_utf8(item.verbatim, char(curr));
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
      item.kind = LexerKind::Operator;
      int ch;
      do {
        ch = this->eat(item);
      } while (Lexer::isOperator(ch));
    }
    void nextIdentifier(LexerItem& item) {
      item.kind = LexerKind::Identifier;
      int ch;
      do {
        ch = this->eat(item);
      } while (Lexer::isIdentifierContinue(ch));
    }
    void nextNumber(LexerItem& item) {
      // TODO floats and hex constants
      item.kind = LexerKind::Integer;
      int ch;
      do {
        ch = this->eat(item);
      } while (Lexer::isDigit(ch));
      item.value.i = std::stoi(item.verbatim);
    }
    void nextQuoted(LexerItem& item) {
      item.kind = LexerKind::String;
      auto ch = this->eat(item);
      while (ch >= 0) {
        item.value.s.push_back(char32_t(ch));
        ch = this->eat(item);
        if (ch == '"') {
          this->eat(item);
          return;
        }
        if (this->stream.getCurrentLine() != item.line) {
          // There's an EOL in the middle of the string
          this->unexpected(item, "Unexpected end of line found in quoted string");
        }
      }
      this->unexpected(item, "Unexpected end of file found in quoted string");
    }
    void nextBackquoted(LexerItem& item) {
      item.kind = LexerKind::String;
      auto ch = this->eat(item);
      while (ch >= 0) {
        item.value.s.push_back(char32_t(ch));
        ch = this->eat(item);
        if (ch == '`') {
          this->eat(item);
          return;
        }
      }
      this->unexpected(item, "Unexpected end of file found in backquoted string");
    }
    void unexpected(const LexerItem& item, std::string message) {
      throw egg::yolk::Exception(message, this->stream.getResourceName(), item.line, item.column);
    }
  };

  class FileLexer : public Lexer {
    EGG_NO_COPY(FileLexer);
  private:
    FileTextStream stream;
  public:
    FileLexer(const std::string& path, bool swallowBOM)
      : Lexer(stream), stream(path, swallowBOM) {
    }
    virtual ~FileLexer() {
    }
  };

  class StringLexer : public Lexer {
    EGG_NO_COPY(StringLexer);
  private:
    StringTextStream stream;
  public:
    explicit StringLexer(const std::string& text)
      : Lexer(stream), stream(text) {
    }
    virtual ~StringLexer() {
    }
  };
}

std::shared_ptr<egg::yolk::ILexer> egg::yolk::LexerFactory::createFromPath(const std::string& path, bool swallowBOM) {
  return std::make_shared<FileLexer>(path, swallowBOM);
}

std::shared_ptr<egg::yolk::ILexer> egg::yolk::LexerFactory::createFromString(const std::string& text) {
  return std::make_shared<StringLexer>(text);
}

std::shared_ptr<egg::yolk::ILexer> egg::yolk::LexerFactory::createFromTextStream(egg::yolk::TextStream& stream) {
  return std::make_shared<Lexer>(stream);
}
