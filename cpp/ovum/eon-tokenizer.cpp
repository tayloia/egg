#include "ovum/ovum.h"
#include "ovum/lexer.h"
#include "ovum/eon-tokenizer.h"
#include "ovum/utf.h"

namespace {
  using namespace egg::ovum;

  class EonTokenizer : public IEonTokenizer {
    EonTokenizer(const EonTokenizer&) = delete;
    EonTokenizer& operator=(const EonTokenizer&) = delete;
  private:
    IAllocator& allocator;
    std::shared_ptr<ILexer> lexer;
    LexerItem upcoming;
  public:
    EonTokenizer(IAllocator& allocator, const std::shared_ptr<ILexer>& lexer)
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
      item.value = HardValue::Void;
      item.contiguous = true;
      bool skip;
      Int i64;
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
          i64 = Int(this->upcoming.value.i);
          if (i64 < 0) {
            this->unexpected("Invalid integer constant in JSON");
          }
          item.value = ValueFactory::createInt(this->allocator, i64);
          item.kind = EonTokenizerKind::Integer;
          break;
        case LexerKind::Float:
          // This is a float without a preceding '-'
          item.value = ValueFactory::createFloat(this->allocator, this->upcoming.value.f);
          item.kind = EonTokenizerKind::Float;
          break;
        case LexerKind::String:
          item.value = ValueFactory::createStringUTF32(this->allocator, this->upcoming.value.s);
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
                item.value = ValueFactory::createFloat(this->allocator, -this->upcoming.value.f);
              } else if (kind == LexerKind::Integer) {
                i64 = -Int(this->upcoming.value.i);
                if (i64 > 0) {
                  this->unexpected("Invalid negative integer constant");
                }
                item.kind = EonTokenizerKind::Integer;
                item.value = ValueFactory::createInt(this->allocator, i64);
              } else {
                this->unexpected("Expected number to follow minus sign");
              }
              this->lexer->next(this->upcoming);
              return item.kind;
            }
            EGG_WARNING_SUPPRESS_FALLTHROUGH
          default:
            this->unexpected("Unexpected character", UTF32::toReadable(this->upcoming.verbatim.front()));
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
            item.value = HardValue::Null;
          } else if (this->upcoming.verbatim == "false") {
            item.kind = EonTokenizerKind::Boolean;
            item.value = HardValue::False;
          } else if (this->upcoming.verbatim == "true") {
            item.kind = EonTokenizerKind::Boolean;
            item.value = HardValue::True;
          } else {
            item.kind = EonTokenizerKind::Identifier;
            item.value = ValueFactory::createStringUTF8(this->allocator, this->upcoming.verbatim.data(), this->upcoming.verbatim.size());
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

std::shared_ptr<egg::ovum::IEonTokenizer> egg::ovum::EonTokenizerFactory::createFromLexer(IAllocator& allocator, const std::shared_ptr<ILexer>& lexer) {
  return std::make_shared<EonTokenizer>(allocator, lexer);
}
