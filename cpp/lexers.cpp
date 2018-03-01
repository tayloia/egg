#include "yolk.h"

/*TODO legacy lexer
namespace {
  using namespace egg0;

  struct CodeUnit {
    int ch;
    CharacterStream::Advance advance;
    LexerLocation location;
    LexerLocation nextLocation() const {
      LexerLocation location = this->location;
      switch (this->advance) {
      case CharacterStream::Advance::Column:
        location.column++;
        break;
      case CharacterStream::Advance::Line:
        location.line++;
        location.column = 1;
        break;
      }
      return location;
    }
  };

  class Reader {
  private:
    CharacterStream stream;
    std::deque<CodeUnit> deque;
  public:
    explicit Reader(std::istream& stream, const std::string& filename) : stream(stream, filename) {
      this->deque.push_back(this->first());
    }
    const CodeUnit& peek(size_t index) {
      this->ensure(index + 1);
      return this->deque[index];
    }
    const CodeUnit& advance() {
      this->ensure(2);
      this->deque.pop_front();
      return this->peek(0);
    }
    void unexpected(const std::string& what) {
      const auto& unit = this->peek(0);
      throw std::runtime_error("Unexpected " + what + " in " + this->stream.filename() + " at line " + std::to_string(unit.location.line));
    }
  private:
    void ensure(size_t count) {
      while (this->deque.size() < count) {
        this->deque.push_back(this->next());
      }
    }
    CodeUnit first() {
      CodeUnit unit;
      unit.advance = this->stream.get(unit.ch);
      unit.location.line = 1;
      unit.location.column = 1;
      return unit;
    }
    CodeUnit next() {
      const auto& prev = this->deque.back();
      CodeUnit unit;
      unit.advance = this->stream.get(unit.ch);
      unit.location = prev.nextLocation();
      return unit;
    }
  };

  class Lexer : public ILexer {
  private:
    Reader reader;
    std::string filename;
  public:
    Lexer(std::istream& stream, const std::string& filename) : reader(stream, filename) {
      this->filename = filename;
    }
    virtual LexerKind next(LexerItem& item) override {
      item.verbatim.clear();
      item.value.s.clear();
      const auto& peek = this->reader.peek(0);
      item.start = peek.location;
      if (peek.ch < 0) {
        item.kind = LexerKind::EndOfFile;
      } else if (std::isspace(peek.ch)) {
        this->nextWhitespace(item);
      } else if (std::isalpha(peek.ch) || (peek.ch == '_')) {
        item.value.valid = this->nextIdentifier(item);
      } else if (std::isdigit(peek.ch)) {
        this->nextNumber(item);
      } else if (peek.ch == '/') {
        switch (this->reader.peek(1).ch) {
        case '/':
          item.value.valid = this->nextCommentSingleLine(item);
          break;
        case '*':
          item.value.valid = this->nextCommentMultiLine(item);
          break;
        default:
          this->nextOperator(item);
          break;
        }
      } else if (peek.ch == '"') {
        this->nextQuoted(item);
      } else if (peek.ch == '`') {
        this->nextBackquoted(item);
      } else if (peek.ch < 32) {
        this->reader.unexpected("control code");
      } else if (peek.ch > 126) {
        this->reader.unexpected("code point");
      } else {
        this->nextOperator(item);
      }
      return item.kind;
    }
  private:
    const CodeUnit& eat(LexerItem& item, bool advance = true) {
      const auto& curr = this->reader.peek(0);
      item.finish = advance ? curr.nextLocation() : curr.location;
      item.verbatim.push_back(char(curr.ch));
      return this->reader.advance();
    }
    void nextWhitespace(LexerItem& item) {
      item.kind = LexerKind::Whitespace;
      int ch;
      do {
        ch = this->eat(item).ch;
      } while (std::isspace(ch));
    }
    bool nextCommentSingleLine(LexerItem& item) {
      item.kind = LexerKind::Comment;
      auto line = this->eat(item).location.line;
      int ch;
      do {
        ch = this->eat(item, false).ch;
      } while ((ch >= 0) && (this->reader.peek(0).location.line == line));
      return true;
    }
    bool nextCommentMultiLine(LexerItem& item) {
      item.kind = LexerKind::Comment;
      this->eat(item);
      this->eat(item);
      int ch0 = this->eat(item).ch;
      int ch1 = this->eat(item).ch;
      while ((ch0 != '*') || (ch1 != '/')) {
        if (ch1 < 0) {
          return false;
        }
      }
      return true;
    }
    bool nextIdentifier(LexerItem& item) {
      item.kind = LexerKind::Identifier;
      int ch;
      do {
        ch = this->eat(item).ch;
      } while (std::isalnum(ch) || (ch == '_'));
      return true;
    }
    void nextNumber(LexerItem& item) {
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
    }
    void nextQuoted(LexerItem& item) {
      this->reader.unexpected("TODO quoted");
    }
    void nextBackquoted(LexerItem& item) {
      this->reader.unexpected("TODO backquoted");
    }
    void nextOperator(LexerItem& item) {
      this->reader.unexpected("TODO operator");
    }
  };
}

std::shared_ptr<egg0::ILexer> egg0::LexerFactory::createFromStream(std::istream& stream, const std::string& filename) {
  auto lexer = std::make_shared<Lexer>(stream, filename);
  return lexer;
}
*/