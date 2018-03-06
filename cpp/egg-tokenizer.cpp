#include "yolk.h"

#include <codecvt>

const egg::yolk::EggTokenizerState egg::yolk::EggTokenizerState::ExpectNone = { false, false };
const egg::yolk::EggTokenizerState egg::yolk::EggTokenizerState::ExpectIncrement = { true, false };
const egg::yolk::EggTokenizerState egg::yolk::EggTokenizerState::ExpectNumberLiteral = { false, true };

#define EGG_TOKENIZER_KEYWORD_DEFINE(key, text) \
  { egg::yolk::EggTokenizerKeyword::key, sizeof(text)-1, text },
#define EGG_TOKENIZER_OPERATOR_DEFINE(key, text) \
  { egg::yolk::EggTokenizerOperator::key, sizeof(text)-1, text },

namespace {
  const struct KeywordEntry {
    egg::yolk::EggTokenizerKeyword key;
    size_t length;
    char text[16];
  }
  keywords[] = {
    EGG_TOKENIZER_KEYWORDS(EGG_TOKENIZER_KEYWORD_DEFINE)
  };
  const struct OperatorEntry {
    egg::yolk::EggTokenizerOperator key;
    size_t length;
    char text[16];
  }
  operators[] = {
    EGG_TOKENIZER_OPERATORS(EGG_TOKENIZER_OPERATOR_DEFINE)
  };
}

namespace {
  using namespace egg::yolk;

  // See https://stackoverflow.com/a/31302660 and https://stackoverflow.com/a/35103224
  std::wstring_convert<std::codecvt_utf8<int32_t>, int32_t> convert;

  class EggTokenizer : public IEggTokenizer {
  private:
    std::shared_ptr<ILexer> lexer;
    LexerItem upcoming;
  public:
    explicit EggTokenizer(const std::shared_ptr<ILexer>& lexer)
      : lexer(lexer) {
      this->upcoming.line = 0;
    }
    virtual ~EggTokenizer() {
    }
    virtual EggTokenizerKind next(EggTokenizerItem& item, const EggTokenizerState& state) override {
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
            this->unexpected("Invalid integer constant");
          }
          item.kind = EggTokenizerKind::Integer;
          break;
        case LexerKind::Float:
          // This is a float without a preceding '-'
          item.value.f = this->upcoming.value.f;
          item.kind = EggTokenizerKind::Float;
          break;
        case LexerKind::String:
          item.value.s = convert.to_bytes(reinterpret_cast<const int32_t*>(this->upcoming.value.s.data()));
          item.kind = EggTokenizerKind::String;
          break;
        case LexerKind::Operator:
          switch (this->upcoming.verbatim.front()) {
          case '+':
          case '-':
            return nextSign(item, state);
          }
          return nextOperator(item);
        case LexerKind::Identifier:
          item.value.s = this->upcoming.verbatim;
          item.kind = EggTokenizerKind::Identifier;
          break;
        case LexerKind::EndOfFile:
          item.kind = EggTokenizerKind::EndOfFile;
          return EggTokenizerKind::EndOfFile;
        default:
          this->unexpected("Internal tokenizer error: " + this->upcoming.verbatim);
          break;
        }
        this->lexer->next(this->upcoming);
      } while (skip);
      return item.kind;
    }
  private:
    EggTokenizerKind nextOperator(EggTokenizerItem& item) {
      // Look for the longest operator that matches the beginning of the upcoming text
      assert(this->upcoming.kind == LexerKind::Operator);
      auto incoming = this->upcoming.verbatim.data();
      auto length = this->upcoming.verbatim.size();
      // OPTIMIZE
      for (size_t i = EGG_NELEMS(operators); i > 0; --i) {
        auto& candidate = operators[i - 1];
        if ((candidate.length <= length) && (std::strncmp(candidate.text, incoming, candidate.length) == 0)) {
          // Found a match
          this->eatOperator(candidate.length);
          item.kind = EggTokenizerKind::Operator;
          item.value.o = candidate.key;
          return EggTokenizerKind::Operator;
        }
      }
      this->unexpected("Unexpected character: " + String::unicodeToString(this->upcoming.verbatim.front()));
      return EggTokenizerKind::Operator;
    }
    EggTokenizerKind nextSign(EggTokenizerItem& item, const EggTokenizerState& state) {
      assert(this->upcoming.kind == LexerKind::Operator);
      auto ch0 = this->upcoming.verbatim.front();
      assert((ch0 == '+') || (ch0 == '-'));
      auto sign = (ch0 == '+') ? EggTokenizerOperator::Plus : EggTokenizerOperator::Minus;
      if (this->upcoming.verbatim.size() == 1) {
        // There's no contiguously-following operator character
        auto kind = lexer->next(this->upcoming);
        if (state.expectNumberLiteral) {
          // Could be a sign attached to a number literal
          if (kind == LexerKind::Integer) {
            // Something like "-" "12.345"
            return this->nextSignInteger(item, sign);
          }
          if (kind == LexerKind::Float) {
            // Something like "-" "12345"
            return this->nextSignFloat(item, sign);
          }
        }
        // Treat the sign as an operator (unary or binary)
        item.kind = EggTokenizerKind::Operator;
        item.value.o = sign;
        return item.kind;
      }
      assert(this->upcoming.verbatim.size() > 1);
      auto ch1 = this->upcoming.verbatim[1];
      if (ch0 == ch1) {
        // Could be "++" or "--"
        if (!state.expectIncrement) {
          // But we aren't expecting increment/decrement so assume it's a single character
          this->eatOperator(1);
          item.kind = EggTokenizerKind::Operator;
          item.value.o = sign;
          return item.kind;
        }
      }
      return this->nextOperator(item);
    }
    EggTokenizerKind nextSignInteger(EggTokenizerItem& item, EggTokenizerOperator sign) {
      assert(this->upcoming.kind == LexerKind::Integer);
      assert((sign == EggTokenizerOperator::Plus) || (sign == EggTokenizerOperator::Minus));
      item.kind = EggTokenizerKind::Integer;
      if (sign == EggTokenizerOperator::Plus) {
        item.value.i = int64_t(this->upcoming.value.i);
      } else {
        item.value.i = -int64_t(this->upcoming.value.i);
        if (item.value.i > 0) {
          this->unexpected("Invalid negative integer constant");
        }
      }
      this->lexer->next(this->upcoming);
      return EggTokenizerKind::Integer;
    }
    EggTokenizerKind nextSignFloat(EggTokenizerItem& item, EggTokenizerOperator sign) {
      assert(this->upcoming.kind == LexerKind::Float);
      assert((sign == EggTokenizerOperator::Plus) || (sign == EggTokenizerOperator::Minus));
      item.kind = EggTokenizerKind::Float;
      if (sign == EggTokenizerOperator::Plus) {
        item.value.f = this->upcoming.value.f;
      } else {
        item.value.f = -this->upcoming.value.f;
      }
      this->lexer->next(this->upcoming);
      return EggTokenizerKind::Float;
    }
    void eatOperator(size_t characters) {
      assert(this->upcoming.kind == LexerKind::Operator);
      assert(this->upcoming.verbatim.size() >= characters);
      if (this->upcoming.verbatim.size() <= characters) {
        // Consume the whole operator
        this->lexer->next(this->upcoming);
      } else {
        // Just consume the first few characters
        this->upcoming.verbatim.erase(0, characters);
        this->upcoming.column += characters;
      }
    }
    void unexpected(const std::string& message) {
      throw Exception(message, this->lexer->resource(), this->upcoming.line, this->upcoming.column);
    }
  };
}

std::shared_ptr<egg::yolk::IEggTokenizer> egg::yolk::EggTokenizerFactory::createFromLexer(const std::shared_ptr<ILexer>& lexer) {
  return std::make_shared<EggTokenizer>(lexer);
}
