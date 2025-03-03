#include "ovum/ovum.h"
#include "ovum/exception.h"
#include "ovum/lexer.h"
#include "ovum/egg-tokenizer.h"
#include "ovum/utf.h"

#define EGG_TOKENIZER_KEYWORD_DEFINE(key, text) \
  { egg::ovum::EggTokenizerKeyword::key, sizeof(text)-1, text },
#define EGG_TOKENIZER_OPERATOR_DEFINE(key, text) \
  { egg::ovum::EggTokenizerOperator::key, sizeof(text)-1, text },

namespace {
  const struct KeywordEntry {
    egg::ovum::EggTokenizerKeyword key;
    size_t length;
    char text[16];
  }
  keywords[] = {
    EGG_TOKENIZER_KEYWORDS(EGG_TOKENIZER_KEYWORD_DEFINE)
  };
  const struct OperatorEntry {
    egg::ovum::EggTokenizerOperator key;
    size_t length;
    char text[16];
  }
  operators[] = {
    EGG_TOKENIZER_OPERATORS(EGG_TOKENIZER_OPERATOR_DEFINE)
  };
}

std::string egg::ovum::EggTokenizerValue::getKeywordString(EggTokenizerKeyword value) {
  size_t index = size_t(value);
  assert(index < EGG_NELEMS(keywords));
  return keywords[index].text;
}

std::string egg::ovum::EggTokenizerValue::getOperatorString(EggTokenizerOperator value) {
  size_t index = size_t(value);
  assert(index < EGG_NELEMS(operators));
  return operators[index].text;
}

bool egg::ovum::EggTokenizerValue::tryParseKeyword(const std::string& text, EggTokenizerKeyword& value) {
  // OPTIMIZE
  for (size_t i = 0; i < EGG_NELEMS(keywords); ++i) {
    if (text == keywords[i].text) {
      value = keywords[i].key;
      return true;
    }
  }
  return false;
}

bool egg::ovum::EggTokenizerValue::tryParseOperator(const std::string& text, EggTokenizerOperator& value, size_t& length) {
  // OPTIMIZE
  auto* data = text.data();
  auto size = text.size();
  for (size_t i = EGG_NELEMS(operators); i > 0; --i) {
    auto& candidate = operators[i - 1];
    if ((candidate.length <= size) && (std::strncmp(candidate.text, data, candidate.length) == 0)) {
      value = candidate.key;
      length = candidate.length;
      return true;
    }
  }
  return false;
}

std::string egg::ovum::EggTokenizerItem::toString() const {
  switch (this->kind) {
  case EggTokenizerKind::Keyword:
    return "keyword '" + EggTokenizerValue::getKeywordString(this->value.k) + '\'';
  case EggTokenizerKind::Operator:
    return "operator '" + EggTokenizerValue::getOperatorString(this->value.o) + '\'';
  case EggTokenizerKind::String:
    return '"' + this->value.s.toUTF8() + '"';
  case EggTokenizerKind::Integer:
  case EggTokenizerKind::Float:
  case EggTokenizerKind::Identifier:
  case EggTokenizerKind::Attribute:
    return '\'' + this->value.s.toUTF8() + '\'';
  case EggTokenizerKind::EndOfFile:
    return "<end-of-file>";
  }
  return "<unknown>";
}

namespace {
  using namespace egg::ovum;

  class EggTokenizer : public IEggTokenizer {
    EggTokenizer(EggTokenizer&) = delete;
    EggTokenizer& operator=(EggTokenizer&) = delete;
  private:
    IAllocator& allocator;
    std::shared_ptr<ILexer> lexer;
    LexerItem upcoming;
  public:
    EggTokenizer(IAllocator& allocator, const std::shared_ptr<ILexer>& lexer)
      : allocator(allocator),
        lexer(lexer) {
      this->upcoming.line = 0;
    }
    virtual EggTokenizerKind next(EggTokenizerItem& item) override {
      if (this->upcoming.line == 0) {
        // This is the first time through
        this->lexer->next(this->upcoming);
      }
      item.value.s = String();
      item.contiguous = true;
      bool skip;
      do {
        skip = false;
        item.line = this->upcoming.line;
        item.column = this->upcoming.column;
        item.width = this->upcoming.verbatim.size();
        switch (this->upcoming.kind) {
        case LexerKind::Whitespace:
        case LexerKind::Comment:
          // Skip whitespace and comments
          item.contiguous = false;
          skip = true;
          break;
        case LexerKind::Integer:
          // This is an unsigned integer excluding any preceding sign
          item.value.i = int64_t(this->upcoming.value.i);
          if (item.value.i < 0) {
            this->unexpected("Invalid integer constant");
          }
          item.value.s = String::fromUTF8(this->allocator, this->upcoming.verbatim.data(), this->upcoming.verbatim.size());
          item.kind = EggTokenizerKind::Integer;
          break;
        case LexerKind::Float:
          // This is a float excluding any preceding sign
          item.value.f = this->upcoming.value.f;
          item.value.s = String::fromUTF8(this->allocator, this->upcoming.verbatim.data(), this->upcoming.verbatim.size());
          item.kind = EggTokenizerKind::Float;
          break;
        case LexerKind::String:
          item.value.s = String::fromUTF32(this->allocator, this->upcoming.value.s.data(), this->upcoming.value.s.size());
          item.kind = EggTokenizerKind::String;
          break;
        case LexerKind::Operator:
          if (this->upcoming.verbatim.front() == '@') {
            return this->nextAttribute(item);
          }
          return this->nextOperator(item);
        case LexerKind::Identifier:
          item.value.s = String::fromUTF8(this->allocator, this->upcoming.verbatim.data(), this->upcoming.verbatim.size());
          if (EggTokenizerValue::tryParseKeyword(this->upcoming.verbatim, item.value.k)) {
            item.kind = EggTokenizerKind::Keyword;
          } else {
            item.kind = EggTokenizerKind::Identifier;
          }
          break;
        case LexerKind::EndOfFile:
          item.kind = EggTokenizerKind::EndOfFile;
          assert(item.width == 0);
          return EggTokenizerKind::EndOfFile;
        default:
          this->unexpected("Internal tokenizer error", this->upcoming.verbatim);
          break;
        }
        this->lexer->next(this->upcoming);
      } while (skip);
      return item.kind;
    }
    virtual String resource() const override {
      auto utf8 = this->lexer->getResourceName();
      return String::fromUTF8(this->allocator, utf8.data(), utf8.size());
    }
  private:
    EggTokenizerKind nextOperator(EggTokenizerItem& item) {
      // Look for the longest operator that matches the beginning of the upcoming text
      assert(this->upcoming.kind == LexerKind::Operator);
      size_t length = 0;
      if (!EggTokenizerValue::tryParseOperator(this->upcoming.verbatim, item.value.o, length)) {
        this->unexpected("Unexpected character", UTF32::toReadable(this->upcoming.verbatim.front()));
      }
      assert(length > 0);
      this->eatOperator(length);
      item.kind = EggTokenizerKind::Operator;
      return EggTokenizerKind::Operator;
    }
    EggTokenizerKind nextAttribute(EggTokenizerItem& item) {
      assert(this->upcoming.kind == LexerKind::Operator);
      assert(this->upcoming.verbatim.front() == '@');
      for (auto ch : this->upcoming.verbatim) {
        if (ch != '@') {
          this->unexpected("Expected attribute name to follow '@'", UTF32::toReadable(ch));
        }
      }
      StringBuilder sb;
      sb.add(this->upcoming.verbatim);
      if (this->lexer->next(this->upcoming) != LexerKind::Identifier) {
        this->unexpected("Expected attribute name to follow '@'");
      }
      sb.add(this->upcoming.verbatim);
      while ((this->lexer->next(this->upcoming) == LexerKind::Operator) && (this->upcoming.verbatim == ".")) {
        if (this->lexer->next(this->upcoming) != LexerKind::Identifier) {
          this->unexpected("Expected attribute name component to follow '.' in attribute name");
        }
        sb.add('.', this->upcoming.verbatim);
      }
      item.value.s = sb.build(this->allocator);
      item.kind = EggTokenizerKind::Attribute;
      return EggTokenizerKind::Attribute;
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
      throw SyntaxException(message, this->lexer->getResourceName(), this->upcoming);
    }
    void unexpected(const std::string& message, const std::string& token) {
      throw SyntaxException(message + ": " + token, this->lexer->getResourceName(), this->upcoming, token);
    }
  };
}

std::shared_ptr<egg::ovum::IEggTokenizer> egg::ovum::EggTokenizerFactory::createFromLexer(IAllocator& allocator, const std::shared_ptr<ILexer>& lexer) {
  return std::make_shared<EggTokenizer>(allocator, lexer);
}
