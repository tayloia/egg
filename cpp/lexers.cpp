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
      auto valid = true;
      item.verbatim.clear();
      item.value.s.clear();
      auto peek = this->stream.peek();
      item.line = this->stream.getCurrentLine();
      item.column = this->stream.getCurrentColumn();
      if (peek < 0) {
        valid = true;
        item.kind = LexerKind::EndOfFile;
      } else if (Lexer::isWhitespace(peek)) {
        valid = this->nextWhitespace(item);
      } else if (Lexer::isIdentifierStart(peek)) {
        valid = this->nextIdentifier(item);
      } else if (Lexer::isDigit(peek)) {
        valid = this->nextNumber(item);
      } else if (peek == '/') {
        switch (this->stream.peek(1)) {
        case '/':
          valid = this->nextCommentSingleLine(item);
          break;
        case '*':
          valid = this->nextCommentMultiLine(item);
          break;
        default:
          valid = this->nextOperator(item);
          break;
        }
      } else if (peek == '"') {
        valid = this->nextQuoted(item);
      } else if (peek == '`') {
        valid = this->nextBackquoted(item);
      } else if (Lexer::isOperator(peek)) {
        valid = this->nextOperator(item);
      } else {
        this->unexpected("Unexpected code point");
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
      item.verbatim.push_back(char(curr));
      return this->stream.peek();
    }
    bool nextWhitespace(LexerItem& item) {
      item.kind = LexerKind::Whitespace;
      int ch;
      do {
        ch = this->eat(item);
      } while (Lexer::isWhitespace(ch));
      return true;
    }
    bool nextCommentSingleLine(LexerItem& item) {
      item.kind = LexerKind::Comment;
      auto line = this->stream.getCurrentLine();
      int ch;
      do {
        ch = this->eat(item);
      } while ((ch >= 0) && (this->stream.getCurrentLine() == line));
      return true;
    }
    bool nextCommentMultiLine(LexerItem& item) {
      item.kind = LexerKind::Comment;
      this->eat(item);
      this->eat(item);
      int ch0 = this->eat(item);
      int ch1 = this->eat(item);
      while ((ch0 != '*') || (ch1 != '/')) {
        if (ch1 < 0) {
          return false;
        }
      }
      return true;
    }
    bool nextOperator(LexerItem& item) {
      item.kind = LexerKind::Operator;
      int ch;
      do {
        ch = this->eat(item);
      } while (Lexer::isOperator(ch));
      return true;
    }
    bool nextIdentifier(LexerItem& item) {
      item.kind = LexerKind::Identifier;
      int ch;
      do {
        ch = this->eat(item);
      } while (Lexer::isIdentifierContinue(ch));
      return true;
    }
    bool nextNumber(LexerItem& item) {
      // TODO floats and hex constants
      item.kind = LexerKind::Integer;
      int ch;
      do {
        ch = this->eat(item);
      } while (Lexer::isDigit(ch));
      item.value.i = std::stoi(item.verbatim);
      return true;
    }
    bool nextQuoted(LexerItem& item) {
      item.kind = LexerKind::String;
      auto ch = this->eat(item);
      do {
        item.value.s.push_back(char(ch)); // TODO unicode
        ch = this->eat(item);
        if (ch == '"') {
          this->eat(item);
          return true;
        }
        if (this->stream.getCurrentLine() != item.line) {
          // There's an EOL in the middle of the string
          break;
        }
      } while (ch >= 0);
      return false;
    }
    bool nextBackquoted(LexerItem& item) {
      item.kind = LexerKind::String;
      auto ch = this->eat(item);
      do {
        item.value.s.push_back(char(ch)); // TODO unicode
        ch = this->eat(item);
        if (ch == '`') {
          this->eat(item);
          return true;
        }
      } while (ch >= 0);
      return false;
    }
    void unexpected(std::string message) {
      throw egg::yolk::Exception(message, this->stream.getFilename(), this->stream.getCurrentLine(), this->stream.getCurrentLine());
    }
  };

  class FileLexer : public Lexer {
    EGG_NO_COPY(FileLexer);
  private:
    FileTextStream stream;
  public:
    explicit FileLexer(const std::string& path, bool swallowBOM)
      : Lexer(stream), stream(path, swallowBOM) {
    }
    virtual ~FileLexer() {
    }
  };
}

std::shared_ptr<egg::yolk::ILexer> egg::yolk::LexerFactory::createFromPath(const std::string& path, bool swallowBOM) {
  return std::make_shared<FileLexer>(path, swallowBOM);
}

std::shared_ptr<egg::yolk::ILexer> egg::yolk::LexerFactory::createFromTextStream(egg::yolk::TextStream& stream) {
  return std::make_shared<Lexer>(stream);
}
