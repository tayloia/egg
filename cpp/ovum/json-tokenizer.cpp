#include "ovum/ovum.h"
#include "ovum/lexer.h"
#include "ovum/json-tokenizer.h"
#include "ovum/utf.h"

namespace {
  using namespace egg::ovum;

  class JsonTokenizer : public IJsonTokenizer {
  private:
    std::shared_ptr<ILexer> lexer;
    LexerItem upcoming;
  public:
    explicit JsonTokenizer(const std::shared_ptr<ILexer>& lexer)
      : lexer(lexer) {
      this->upcoming.line = 0;
    }
    virtual JsonTokenizerKind next(JsonTokenizerItem& item) override {
      if (this->upcoming.line == 0) {
        // This is the first time through
        this->lexer->next(this->upcoming);
      }
      item.line = this->upcoming.line;
      item.column = this->upcoming.column;
      item.value.s.clear();
      bool skip;
      do {
        skip = false;
        switch (this->upcoming.kind) {
        case LexerKind::Whitespace:
          // Skip whitespace
          skip = true;
          break;
        case LexerKind::Comment:
          // Not allowed
          this->unexpected("Strict JSON does not permit comments");
          break;
        case LexerKind::Integer:
          // This is an unsigned integer without a preceding '-'
          item.value.u = this->upcoming.value.i;
          item.kind = JsonTokenizerKind::Unsigned;
          break;
        case LexerKind::Float:
          // This is a float without a preceding '-'
          item.value.f = this->upcoming.value.f;
          item.kind = JsonTokenizerKind::Float;
          break;
        case LexerKind::String:
          if (this->upcoming.verbatim.front() == '`') {
            this->unexpected("Strict JSON does not permit backquoted strings");
          }
          item.value.s = UTF32::toUTF8(this->upcoming.value.s);
          item.kind = JsonTokenizerKind::String;
          break;
        case LexerKind::Operator:
          // Fortunately all "operators" in JSON are single characters
          switch (this->upcoming.verbatim.front()) {
          case '{':
            item.kind = JsonTokenizerKind::ObjectStart;
            break;
          case '}':
            item.kind = JsonTokenizerKind::ObjectEnd;
            break;
          case '[':
            item.kind = JsonTokenizerKind::ArrayStart;
            break;
          case ']':
            item.kind = JsonTokenizerKind::ArrayEnd;
            break;
          case ':':
            item.kind = JsonTokenizerKind::Colon;
            break;
          case ',':
            item.kind = JsonTokenizerKind::Comma;
            break;
          case '-':
            if (this->upcoming.verbatim.size() == 1) {
              auto kind = lexer->next(this->upcoming);
              if (kind == LexerKind::Float) {
                item.kind = JsonTokenizerKind::Float;
                item.value.f = -this->upcoming.value.f;
              } else if (kind == LexerKind::Integer) {
                item.kind = JsonTokenizerKind::Signed;
                item.value.i = -int64_t(this->upcoming.value.i);
                if (item.value.i > 0) {
                  this->unexpected("Invalid negative integer constant in JSON");
                }
              } else {
                this->unexpected("Expected number to follow minus sign in JSON");
              }
              this->lexer->next(this->upcoming);
              return item.kind;
            }
            EGG_WARNING_SUPPRESS_FALLTHROUGH
          default:
            this->unexpected("Unexpected character in JSON", UTF32::toReadable(this->upcoming.verbatim.front()));
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
          if (this->upcoming.verbatim == "null") {
            item.kind = JsonTokenizerKind::Null;
          } else if (this->upcoming.verbatim == "false") {
            item.kind = JsonTokenizerKind::Boolean;
            item.value.b = false;
          } else if (this->upcoming.verbatim == "true") {
            item.kind = JsonTokenizerKind::Boolean;
            item.value.b = true;
          } else {
            this->unexpected("Unexpected identifier in JSON", this->upcoming.verbatim);
          }
          break;
        case LexerKind::EndOfFile:
          item.kind = JsonTokenizerKind::EndOfFile;
          return JsonTokenizerKind::EndOfFile;
        default:
          this->unexpected("Internal JSON tokenizer error", this->upcoming.verbatim);
          break;
        }
        this->lexer->next(this->upcoming);
      } while (skip);
      return item.kind;
    }
  private:
    void unexpected(const std::string& message) {
      throw SyntaxException(message, this->lexer->getResourceName(), this->upcoming);
    }
    void unexpected(const std::string& message, const std::string& token) {
      throw SyntaxException(message + ": " + token, this->lexer->getResourceName(), this->upcoming, token);
    }
  };
}

std::shared_ptr<egg::ovum::IJsonTokenizer> egg::ovum::JsonTokenizerFactory::createFromLexer(const std::shared_ptr<ILexer>& lexer) {
  return std::make_shared<JsonTokenizer>(lexer);
}
