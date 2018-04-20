#include "yolk.h"
#include "lexers.h"
#include "egged-tokenizer.h"

namespace {
  using namespace egg::yolk;

  class EggedTokenizer : public IEggedTokenizer {
  private:
    std::shared_ptr<ILexer> lexer;
    LexerItem upcoming;
  public:
    explicit EggedTokenizer(const std::shared_ptr<ILexer>& lexer)
      : lexer(lexer) {
      this->upcoming.line = 0;
    }
    virtual EggedTokenizerKind next(EggedTokenizerItem& item) override {
      if (this->upcoming.line == 0) {
        // This is the first time through
        this->lexer->next(this->upcoming);
      }
      item.line = this->upcoming.line;
      item.column = this->upcoming.column;
      item.value.s.clear();
      item.contiguous = true;
      bool skip;
      do {
        skip = false;
        switch (this->upcoming.kind) {
        case LexerKind::Whitespace:
        case LexerKind::Comment:
          // Skip whitespace and comments
          item.contiguous = false;
          skip = true;
          break;
        case LexerKind::Integer:
          // This is an unsigned integer without a preceding '-'
          item.value.i = int64_t(this->upcoming.value.i);
          if (item.value.i < 0) {
            this->unexpected("Invalid integer constant in JSON");
          }
          item.kind = EggedTokenizerKind::Integer;
          break;
        case LexerKind::Float:
          // This is a float without a preceding '-'
          item.value.f = this->upcoming.value.f;
          item.kind = EggedTokenizerKind::Float;
          break;
        case LexerKind::String:
          item.value.s = egg::utf::to_bytes(this->upcoming.value.s.c_str());
          item.kind = EggedTokenizerKind::String;
          break;
        case LexerKind::Operator:
          // Fortunately all "operators" in JSON are single characters
          switch (this->upcoming.verbatim.front()) {
          case '{':
            item.kind = EggedTokenizerKind::ObjectStart;
            break;
          case '}':
            item.kind = EggedTokenizerKind::ObjectEnd;
            break;
          case '[':
            item.kind = EggedTokenizerKind::ArrayStart;
            break;
          case ']':
            item.kind = EggedTokenizerKind::ArrayEnd;
            break;
          case ':':
            item.kind = EggedTokenizerKind::Colon;
            break;
          case ',':
            item.kind = EggedTokenizerKind::Comma;
            break;
          case '-':
            if (this->upcoming.verbatim.size() == 1) {
              auto kind = lexer->next(this->upcoming);
              if (kind == LexerKind::Float) {
                item.kind = EggedTokenizerKind::Float;
                item.value.f = -this->upcoming.value.f;
              } else if (kind == LexerKind::Integer) {
                item.kind = EggedTokenizerKind::Integer;
                item.value.i = -int64_t(this->upcoming.value.i);
                if (item.value.i > 0) {
                  this->unexpected("Invalid negative integer constant");
                }
              } else {
                this->unexpected("Expected number to follow minus sign");
              }
              this->lexer->next(this->upcoming);
              return item.kind;
            }
            /* DROPTHROUGH */
          default:
            this->unexpected("Unexpected character", String::unicodeToString(this->upcoming.verbatim.front()));
            break;
          }
          if (this->upcoming.verbatim.size() > 1) {
            // Just remove the first character of operator string
            this->upcoming.verbatim.erase(0, 1);
            this->upcoming.column++;
            return item.kind;
          }
          break;
        case LexerKind::Identifier:
          item.value.s = this->upcoming.verbatim;
          if (item.value.s == "null") {
            item.kind = EggedTokenizerKind::Null;
          } else if (item.value.s == "false") {
            item.kind = EggedTokenizerKind::Boolean;
            item.value.b = false;
          } else if (item.value.s == "true") {
            item.kind = EggedTokenizerKind::Boolean;
            item.value.b = true;
          } else {
            item.kind = EggedTokenizerKind::Identifier;
          }
          break;
        case LexerKind::EndOfFile:
          item.kind = EggedTokenizerKind::EndOfFile;
          return EggedTokenizerKind::EndOfFile;
        default:
          this->unexpected("Internal tokenizer error", this->upcoming.verbatim);
          break;
        }
        this->lexer->next(this->upcoming);
      } while (skip);
      return item.kind;
    }
  private:
    void unexpected(const std::string& message) {
      throw SyntaxException(message, this->lexer->resource(), this->upcoming);
    }
    void unexpected(const std::string& message, const std::string& token) {
      throw SyntaxException(message + ": " + token, this->lexer->resource(), this->upcoming, token);
    }
  };
}

std::shared_ptr<egg::yolk::IEggedTokenizer> egg::yolk::EggedTokenizerFactory::createFromLexer(const std::shared_ptr<ILexer>& lexer) {
  return std::make_shared<EggedTokenizer>(lexer);
}
