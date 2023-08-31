#include "yolk/yolk.h"
#include "yolk/lexers.h"
#include "yolk/eon-tokenizer.h"

namespace {
  using namespace egg::yolk;

  class EonTokenizer : public IEonTokenizer {
    EonTokenizer(const EonTokenizer&) = delete;
    EonTokenizer& operator=(const EonTokenizer&) = delete;
  private:
    egg::ovum::IAllocator& allocator;
    std::shared_ptr<ILexer> lexer;
    LexerItem upcoming;
  public:
    EonTokenizer(egg::ovum::IAllocator& allocator, const std::shared_ptr<ILexer>& lexer)
      : allocator(allocator), lexer(lexer) {
      this->upcoming.line = 0;
    }
    virtual EonTokenizerKind next(EonTokenizerItem& item) override {
      if (this->upcoming.line == 0) {
        // This is the first time through
        this->lexer->next(this->upcoming);
      }
      item.line = this->upcoming.line;
      item.column = this->upcoming.column;
      item.value = egg::ovum::Value::Void;
      item.contiguous = true;
      bool skip;
      egg::ovum::Int i64;
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
          i64 = egg::ovum::Int(this->upcoming.value.i);
          if (i64 < 0) {
            this->unexpected("Invalid integer constant in JSON");
          }
          item.value = egg::ovum::ValueFactory::createInt(this->allocator, i64);
          item.kind = EonTokenizerKind::Integer;
          break;
        case LexerKind::Float:
          // This is a float without a preceding '-'
          item.value = egg::ovum::ValueFactory::createFloat(this->allocator, this->upcoming.value.f);
          item.kind = EonTokenizerKind::Float;
          break;
        case LexerKind::String:
          item.value = egg::ovum::ValueFactory::createString(this->allocator, egg::ovum::String::fromUTF32(this->upcoming.value.s));
          item.kind = EonTokenizerKind::String;
          break;
        case LexerKind::Operator:
          // Fortunately all "operators" in JSON are single characters
          switch (this->upcoming.verbatim.front()) {
          case '{':
            item.kind = EonTokenizerKind::ObjectStart;
            break;
          case '}':
            item.kind = EonTokenizerKind::ObjectEnd;
            break;
          case '[':
            item.kind = EonTokenizerKind::ArrayStart;
            break;
          case ']':
            item.kind = EonTokenizerKind::ArrayEnd;
            break;
          case ':':
            item.kind = EonTokenizerKind::Colon;
            break;
          case ',':
            item.kind = EonTokenizerKind::Comma;
            break;
          case '-':
            if (this->upcoming.verbatim.size() == 1) {
              auto kind = lexer->next(this->upcoming);
              if (kind == LexerKind::Float) {
                item.kind = EonTokenizerKind::Float;
                item.value = egg::ovum::ValueFactory::createFloat(this->allocator, -this->upcoming.value.f);
              } else if (kind == LexerKind::Integer) {
                i64 = -egg::ovum::Int(this->upcoming.value.i);
                if (i64 > 0) {
                  this->unexpected("Invalid negative integer constant");
                }
                item.kind = EonTokenizerKind::Integer;
                item.value = egg::ovum::ValueFactory::createInt(this->allocator, i64);
              } else {
                this->unexpected("Expected number to follow minus sign");
              }
              this->lexer->next(this->upcoming);
              return item.kind;
            }
            EGG_FALLTHROUGH
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
          if (this->upcoming.verbatim == "null") {
            item.kind = EonTokenizerKind::Null;
            item.value = egg::ovum::Value::Null;
          } else if (this->upcoming.verbatim == "false") {
            item.kind = EonTokenizerKind::Boolean;
            item.value = egg::ovum::Value::False;
          } else if (this->upcoming.verbatim == "true") {
            item.kind = EonTokenizerKind::Boolean;
            item.value = egg::ovum::Value::True;
          } else {
            item.kind = EonTokenizerKind::Identifier;
            item.value = egg::ovum::ValueFactory::createString(this->allocator, egg::ovum::String(this->upcoming.verbatim));
          }
          break;
        case LexerKind::EndOfFile:
          item.kind = EonTokenizerKind::EndOfFile;
          return EonTokenizerKind::EndOfFile;
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
      throw SyntaxException(message, this->lexer->getResourceName(), this->upcoming);
    }
    void unexpected(const std::string& message, const std::string& token) {
      throw SyntaxException(message + ": " + token, this->lexer->getResourceName(), this->upcoming, token);
    }
  };
}

std::shared_ptr<egg::yolk::IEonTokenizer> egg::yolk::EonTokenizerFactory::createFromLexer(egg::ovum::IAllocator& allocator, const std::shared_ptr<ILexer>& lexer) {
  return std::make_shared<EonTokenizer>(allocator, lexer);
}
