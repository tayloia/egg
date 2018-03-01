#include "yolk.h"

namespace {
  using namespace egg::yolk;

  class Lexer : public ILexer {
    EGG_NO_COPY(Lexer);
  private:
    TextStream& stream;
  public:
    explicit Lexer(TextStream& stream)
      : stream(stream) {
    }
    virtual LexerKind next(LexerItem& item) override {
      item.verbatim.clear();
      item.value.valid = true;
      item.value.s.clear();
      auto peek = this->stream.peek();
      item.line = this->stream.getCurrentLine();
      item.column = this->stream.getCurrentColumn();
      if (peek < 0) {
        item.value.valid = true;
        item.kind = LexerKind::EndOfFile;
      } else if (Lexer::isWhitespace(peek)) {
        item.value.valid = this->nextWhitespace(item);
      } else if (Lexer::isIdentifierStart(peek)) {
        item.value.valid = this->nextIdentifier(item);
      } else if (Lexer::isDigit(peek)) {
        item.value.valid = this->nextNumber(item);
      } else if (peek == '/') {
        switch (this->stream.peek(1)) {
        case '/':
          item.value.valid = this->nextCommentSingleLine(item);
          break;
        case '*':
          item.value.valid = this->nextCommentMultiLine(item);
          break;
        default:
          item.value.valid = this->nextOperator(item);
          break;
        }
      } else if (peek == '"') {
        item.value.valid = this->nextQuoted(item);
      } else if (peek == '`') {
        item.value.valid = this->nextBackquoted(item);
      } else if (Lexer::isOperator(peek)) {
        item.value.valid = this->nextOperator(item);
      } else {
        this->unexpected("code point");
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
    bool nextNumber(LexerItem&) {
      this->unexpected("TODO quoted");
      /* TODO nextNumber
      auto ch = this->eat(item).ch;
      if (ch == '0') {
        switch (this->reader.peek(1).ch) {
        case 'x':
        case 'X':
          this->nextNumberHex(item);
          return;
        case '.':
          this->nextNumberFloat(item);
          break;
        }
      }
      */
      return false;
    }
    bool nextQuoted(LexerItem&) {
      this->unexpected("TODO quoted");
      return false;
    }
    bool nextBackquoted(LexerItem&) {
      this->unexpected("TODO backquoted");
      return false;
    }
    void unexpected(std::string message) {
      throw egg::yolk::Exception(message, this->stream.getFilename(), this->stream.getCurrentLine(), this->stream.getCurrentLine());
    }
  };
}

std::shared_ptr<egg::yolk::ILexer> egg::yolk::LexerFactory::createFromTextStream(egg::yolk::TextStream& stream) {
  auto lexer = std::make_shared<Lexer>(stream);
  return lexer;
}
