#include "yolk/yolk.h"
#include "yolk/lexers.h"
#include "yolk/egg-tokenizer.h"
#include "yolk/egg-syntax.h"
#include "yolk/egg-parser.h"

#include <map>
#include <set>

namespace {
  using namespace egg::yolk;

  const char* assignmentExpectation(const std::string& op) {
    static std::map<std::string, const char*> table = {
#define EGG_PROGRAM_ASSIGN_EXPECTATION(op, text) { text, "Expected expression after assignment '" text "' operator" },
      EGG_PROGRAM_ASSIGN_OPERATORS(EGG_PROGRAM_ASSIGN_EXPECTATION)
    };
    auto found = table.find(op);
    if (found != table.end()) {
      return found->second;
    }
    return nullptr;
  }

  egg::ovum::ValueFlags keywordToFlags(const EggTokenizerItem& item) {
    // Accept only type-like keywords: void, null, bool, int, float, string, object and any
    // OPTIMIZE
    if (item.kind == EggTokenizerKind::Keyword) {
      EGG_WARNING_SUPPRESS_SWITCH_BEGIN
      switch (item.value.k) {
      case EggTokenizerKeyword::Void:
        return egg::ovum::ValueFlags::Void;
      case EggTokenizerKeyword::Null:
        return egg::ovum::ValueFlags::Null;
      case EggTokenizerKeyword::Bool:
        return egg::ovum::ValueFlags::Bool;
      case EggTokenizerKeyword::Int:
        return egg::ovum::ValueFlags::Int;
      case EggTokenizerKeyword::Float:
        return egg::ovum::ValueFlags::Float;
      case EggTokenizerKeyword::String:
        return egg::ovum::ValueFlags::String;
      case EggTokenizerKeyword::Object:
        return egg::ovum::ValueFlags::Object;
      case EggTokenizerKeyword::Any:
        return egg::ovum::ValueFlags::Any;
      }
      EGG_WARNING_SUPPRESS_SWITCH_END
    }
    return egg::ovum::ValueFlags::None;
  }

  class ParserDump final {
    EGG_NO_COPY(ParserDump)
  private:
    std::ostream& os;
  public:
    ParserDump(std::ostream& os, const std::string& text)
      :os(os) {
      this->os << '(' << text;
    }
    ~ParserDump() {
      this->os << ')';
    }
    void add() {
    }
    void add(const std::string& text) {
      this->os << ' ' << '\'' << text << '\'';
    }
    void add(const egg::ovum::String& text) {
      this->os << ' ' << '\'' << text.toUTF8() << '\'';
    }
    void add(EggTokenizerOperator op) {
      this->os << ' ' << '\'' << EggTokenizerValue::getOperatorString(op) << '\'';
    }
    void add(const std::unique_ptr<IEggSyntaxNode>& child) {
      this->os << ' ';
      if (child == nullptr) {
        this->os << '(' << ')';
      } else {
        child->dump(this->os);
      }
    }
    void add(const std::vector<std::unique_ptr<IEggSyntaxNode>>& children) {
      for (auto& child : children) {
        this->add(child);
      }
    }
    template<size_t N>
    void add(const std::unique_ptr<IEggSyntaxNode> (&children)[N]) {
      for (auto& child : children) {
        this->add(child);
      }
    }
    template<typename T, typename... ARGS>
    void add(const T& value, ARGS&&... args) {
      this->add(value);
      this->add(std::forward<ARGS>(args)...);
    }
    template<typename... ARGS>
    static void dump(std::ostream& os, const std::string& text, ARGS&&... args) {
      ParserDump(os, text).add(std::forward<ARGS>(args)...);
    }
  };
}

void egg::yolk::EggSyntaxNode_Module::dump(std::ostream& os) const {
  ParserDump::dump(os, "module", this->child);
}

void egg::yolk::EggSyntaxNode_Block::dump(std::ostream& os) const {
  ParserDump::dump(os, "block", this->child);
}

void egg::yolk::EggSyntaxNode_Type::dump(std::ostream& os) const {
  auto tname = (this->type == nullptr) ? "var" : this->type.toString();
  ParserDump::dump(os, "type", tname);
}

void egg::yolk::EggSyntaxNode_Declare::dump(std::ostream& os) const {
  ParserDump::dump(os, "declare", this->name, this->child);
}

void egg::yolk::EggSyntaxNode_Guard::dump(std::ostream& os) const {
  ParserDump::dump(os, "guard", this->name, this->child);
}

void egg::yolk::EggSyntaxNode_Assignment::dump(std::ostream& os) const {
  ParserDump::dump(os, "assign", this->op, this->child);
}

void egg::yolk::EggSyntaxNode_Mutate::dump(std::ostream& os) const {
  ParserDump::dump(os, "mutate", this->op, this->child);
}

void egg::yolk::EggSyntaxNode_Break::dump(std::ostream& os) const {
  ParserDump::dump(os, "break");
}

void egg::yolk::EggSyntaxNode_Case::dump(std::ostream& os) const {
  ParserDump::dump(os, "case", this->child);
}

void egg::yolk::EggSyntaxNode_Catch::dump(std::ostream& os) const {
  ParserDump::dump(os, "catch", this->name, this->child);
}

void egg::yolk::EggSyntaxNode_Continue::dump(std::ostream& os) const {
  ParserDump::dump(os, "continue");
}

void egg::yolk::EggSyntaxNode_Default::dump(std::ostream& os) const {
  ParserDump::dump(os, "default");
}

void egg::yolk::EggSyntaxNode_Do::dump(std::ostream& os) const {
  ParserDump::dump(os, "do", this->child);
}

void egg::yolk::EggSyntaxNode_If::dump(std::ostream& os) const {
  ParserDump::dump(os, "if", this->child);
}

void egg::yolk::EggSyntaxNode_Finally::dump(std::ostream& os) const {
  ParserDump::dump(os, "finally", this->child);
}

void egg::yolk::EggSyntaxNode_For::dump(std::ostream& os) const {
  ParserDump::dump(os, "for", this->child);
}

void egg::yolk::EggSyntaxNode_Foreach::dump(std::ostream& os) const {
  ParserDump::dump(os, "foreach", this->child);
}

void egg::yolk::EggSyntaxNode_FunctionDefinition::dump(std::ostream& os) const {
  ParserDump::dump(os, "function", this->name, this->child);
}

void egg::yolk::EggSyntaxNode_Parameter::dump(std::ostream& os) const {
  ParserDump::dump(os, this->optional ? "parameter?" : "parameter", this->name, this->child);
}

void egg::yolk::EggSyntaxNode_Return::dump(std::ostream& os) const {
  ParserDump::dump(os, "return", this->child);
}

void egg::yolk::EggSyntaxNode_Switch::dump(std::ostream& os) const {
  ParserDump::dump(os, "switch", this->child);
}

void egg::yolk::EggSyntaxNode_Throw::dump(std::ostream& os) const {
  ParserDump::dump(os, "throw", this->child);
}

void egg::yolk::EggSyntaxNode_Try::dump(std::ostream& os) const {
  ParserDump::dump(os, "try", this->child);
}

void egg::yolk::EggSyntaxNode_While::dump(std::ostream& os) const {
  ParserDump::dump(os, "while", this->child);
}

void egg::yolk::EggSyntaxNode_Yield::dump(std::ostream& os) const {
  ParserDump::dump(os, "yield", this->child);
}

void egg::yolk::EggSyntaxNode_UnaryOperator::dump(std::ostream& os) const {
  ParserDump::dump(os, "unary", this->op, this->child);
}

void egg::yolk::EggSyntaxNode_BinaryOperator::dump(std::ostream& os) const {
  ParserDump::dump(os, "binary", this->op, this->child);
}

void egg::yolk::EggSyntaxNode_TernaryOperator::dump(std::ostream& os) const {
  ParserDump::dump(os, "ternary", this->child);
}

void egg::yolk::EggSyntaxNode_Array::dump(std::ostream& os) const {
  ParserDump::dump(os, "array", this->child);
}

void egg::yolk::EggSyntaxNode_Object::dump(std::ostream& os) const {
  ParserDump::dump(os, "object", this->child);
}

void egg::yolk::EggSyntaxNode_Call::dump(std::ostream& os) const {
  ParserDump::dump(os, "call", this->child);
}

void egg::yolk::EggSyntaxNode_Named::dump(std::ostream& os) const {
  ParserDump::dump(os, "named", this->name, this->child);
}

void egg::yolk::EggSyntaxNode_Identifier::dump(std::ostream& os) const {
  ParserDump::dump(os, "identifier", this->name);
}

void egg::yolk::EggSyntaxNode_Literal::dump(std::ostream& os) const {
  switch (this->kind) {
  case EggTokenizerKind::Integer:
    ParserDump::dump(os, "literal int " + this->value.s.toUTF8());
    break;
  case EggTokenizerKind::Float:
    ParserDump::dump(os, "literal float " + this->value.s.toUTF8());
    break;
  case EggTokenizerKind::String:
    ParserDump::dump(os, "literal string", this->value.s.toUTF8());
    break;
  case EggTokenizerKind::Keyword:
    if (this->value.k == EggTokenizerKeyword::Null) {
      ParserDump::dump(os, "literal null");
    } else if (this->value.k == EggTokenizerKeyword::False) {
      ParserDump::dump(os, "literal bool false");
    } else if (this->value.k == EggTokenizerKeyword::True) {
      ParserDump::dump(os, "literal bool true");
    } else {
      ParserDump::dump(os, "literal keyword unknown");
    }
    break;
  case EggTokenizerKind::Operator:
  case EggTokenizerKind::Identifier:
  case EggTokenizerKind::Attribute:
  case EggTokenizerKind::EndOfFile:
  default:
    ParserDump::dump(os, "literal unknown");
    break;
  }
}

void egg::yolk::EggSyntaxNode_Dot::dump(std::ostream& os) const {
  ParserDump::dump(os, this->query ? "dot?" : "dot", this->child, this->property);
}

egg::yolk::EggTokenizerKeyword egg::yolk::EggSyntaxNodeBase::keyword() const {
  return EggTokenizerKeyword::Void;
}

egg::yolk::EggTokenizerKeyword egg::yolk::EggSyntaxNode_Break::keyword() const {
  return EggTokenizerKeyword::Break;
}

egg::yolk::EggTokenizerKeyword egg::yolk::EggSyntaxNode_Case::keyword() const {
  return EggTokenizerKeyword::Case;
}

egg::yolk::EggTokenizerKeyword egg::yolk::EggSyntaxNode_Catch::keyword() const {
  return EggTokenizerKeyword::Catch;
}

egg::yolk::EggTokenizerKeyword egg::yolk::EggSyntaxNode_Continue::keyword() const {
  return EggTokenizerKeyword::Continue;
}

egg::yolk::EggTokenizerKeyword egg::yolk::EggSyntaxNode_Default::keyword() const {
  return EggTokenizerKeyword::Default;
}

egg::yolk::EggTokenizerKeyword egg::yolk::EggSyntaxNode_Do::keyword() const {
  return EggTokenizerKeyword::Do;
}

egg::yolk::EggTokenizerKeyword egg::yolk::EggSyntaxNode_If::keyword() const {
  return EggTokenizerKeyword::If;
}

egg::yolk::EggTokenizerKeyword egg::yolk::EggSyntaxNode_Finally::keyword() const {
  return EggTokenizerKeyword::Finally;
}

egg::yolk::EggTokenizerKeyword egg::yolk::EggSyntaxNode_For::keyword() const {
  return EggTokenizerKeyword::For;
}

egg::yolk::EggTokenizerKeyword egg::yolk::EggSyntaxNode_Foreach::keyword() const {
  return EggTokenizerKeyword::For;
}

egg::yolk::EggTokenizerKeyword egg::yolk::EggSyntaxNode_Return::keyword() const {
  return EggTokenizerKeyword::Return;
}

egg::yolk::EggTokenizerKeyword egg::yolk::EggSyntaxNode_Switch::keyword() const {
  return EggTokenizerKeyword::Switch;
}

egg::yolk::EggTokenizerKeyword egg::yolk::EggSyntaxNode_Throw::keyword() const {
  return EggTokenizerKeyword::Throw;
}

egg::yolk::EggTokenizerKeyword egg::yolk::EggSyntaxNode_Try::keyword() const {
  return EggTokenizerKeyword::Try;
}

egg::yolk::EggTokenizerKeyword egg::yolk::EggSyntaxNode_While::keyword() const {
  return EggTokenizerKeyword::While;
}

egg::yolk::EggTokenizerKeyword egg::yolk::EggSyntaxNode_Yield::keyword() const {
  return EggTokenizerKeyword::Yield;
}

const EggSyntaxNodeLocation& egg::yolk::EggSyntaxNodeBase::location() const {
  return *this;
}

const std::vector<std::unique_ptr<IEggSyntaxNode>>* egg::yolk::EggSyntaxNodeBase::children() const {
  return nullptr;
}

bool egg::yolk::EggSyntaxNodeBase::negate() {
  return false;
}

egg::ovum::String egg::yolk::EggSyntaxNodeBase::token() const {
  return egg::ovum::String();
}

egg::ovum::String egg::yolk::EggSyntaxNode_Type::token() const {
  return this->type.toString();
}

egg::ovum::String egg::yolk::EggSyntaxNode_Declare::token() const {
  return this->name;
}

egg::ovum::String egg::yolk::EggSyntaxNode_Guard::token() const {
  return this->name;
}

egg::ovum::String egg::yolk::EggSyntaxNode_Assignment::token() const {
  return EggTokenizerValue::getOperatorString(this->op);
}

egg::ovum::String egg::yolk::EggSyntaxNode_Mutate::token() const {
  return EggTokenizerValue::getOperatorString(this->op);
}

egg::ovum::String egg::yolk::EggSyntaxNode_Catch::token() const {
  return this->name;
}

egg::ovum::String egg::yolk::EggSyntaxNode_UnaryOperator::token() const {
  return EggTokenizerValue::getOperatorString(this->op);
}

egg::ovum::String egg::yolk::EggSyntaxNode_BinaryOperator::token() const {
  return EggTokenizerValue::getOperatorString(this->op);
}

egg::ovum::String egg::yolk::EggSyntaxNode_TernaryOperator::token() const {
  return "?:";
}

egg::ovum::String egg::yolk::EggSyntaxNode_FunctionDefinition::token() const {
  return this->name;
}

egg::ovum::String egg::yolk::EggSyntaxNode_Parameter::token() const {
  return this->name;
}

egg::ovum::String egg::yolk::EggSyntaxNode_Named::token() const {
  return this->name;
}

egg::ovum::String egg::yolk::EggSyntaxNode_Identifier::token() const {
  return this->name;
}

egg::ovum::String egg::yolk::EggSyntaxNode_Literal::token() const {
  return this->value.s;
}

egg::ovum::String egg::yolk::EggSyntaxNode_Dot::token() const {
  return this->property;
}

bool egg::yolk::EggSyntaxNode_Literal::negate() {
  // Try to negate (times-minus-one) as literal value
  if (this->kind == EggTokenizerKind::Integer) {
    auto negative = -this->value.i;
    if (negative <= 0) {
      this->value.i = negative;
      this->value.s = egg::ovum::StringBuilder::concat("-", this->value.s.toUTF8());
      return true;
    }
  } else if (this->kind == EggTokenizerKind::Float) {
    this->value.f = -this->value.f;
    this->value.s = egg::ovum::StringBuilder::concat("-", this->value.s);
    return true;
  }
  return false;
}

#define EGG_TOKENIZER_OPERATOR_EXPECTATION(key, text) \
  "Expected expression after infix '" text "' operator",

#define PARSE_BINARY_LTR(parent, child, condition) \
  std::unique_ptr<IEggSyntaxNode> parent(const char* expected) { \
    EggSyntaxParserBacktrackMark mark(this->backtrack); \
    auto expr = this->child(expected); \
    for (auto* token = &mark.peek(0); (expr != nullptr) && (condition); token = &mark.peek(0)) { \
      mark.advance(1); \
      auto lhs = std::move(expr); \
      auto rhs = this->child(getInfixOperatorExpectation(token->value.o)); \
      EggSyntaxNodeLocation location(*token); \
      expr = std::make_unique<EggSyntaxNode_BinaryOperator>(location, token->value.o, std::move(lhs), std::move(rhs)); \
    } \
    mark.accept(0); \
    return expr; \
  }
#define PARSE_BINARY1_LTR(parent, child, op) \
  PARSE_BINARY_LTR(parent, child, token->isOperator(op))
#define PARSE_BINARY2_LTR(parent, child, op1, op2) \
  PARSE_BINARY_LTR(parent, child, token->isOperator(op1) || token->isOperator(op2))
#define PARSE_BINARY3_LTR(parent, child, op1, op2, op3) \
  PARSE_BINARY_LTR(parent, child, token->isOperator(op1) || token->isOperator(op2) || token->isOperator(op3))
#define PARSE_BINARY4_LTR(parent, child, op1, op2, op3, op4) \
  PARSE_BINARY_LTR(parent, child, token->isOperator(op1) || token->isOperator(op2) || token->isOperator(op3) || token->isOperator(op4))

namespace {
  using namespace egg::yolk;

  const char* getInfixOperatorExpectation(EggTokenizerOperator value) {
    static const char* const table[] = {
      EGG_TOKENIZER_OPERATORS(EGG_TOKENIZER_OPERATOR_EXPECTATION)
    };
    auto i = size_t(value);
    assert(i < EGG_NELEMS(table));
    return table[i];
  }

  class EggSyntaxParserLookahead {
  private:
    IEggTokenizer* tokenizer;
    std::deque<EggTokenizerItem> upcoming;
  public:
    explicit EggSyntaxParserLookahead(IEggTokenizer& tokenizer)
      : tokenizer(&tokenizer) {
    }
    const EggTokenizerItem& peek(size_t index) {
      if (!this->ensure(index + 1)) {
        assert(this->upcoming.back().kind == EggTokenizerKind::EndOfFile);
        return this->upcoming.back();
      }
      // Microsoft's std::deque indexer is borked
      return *(this->upcoming.begin() + std::ptrdiff_t(index));
    }
    void pop(size_t count = 1) {
      assert(count > 0);
      if (this->ensure(count + 1)) {
        assert(this->upcoming.size() > count);
        for (size_t i = 0; i < count; ++i) {
          // Microsoft's std::deque erase is borked
          this->upcoming.pop_front();
        }
      } else {
        while (this->upcoming.size() > 1) {
          // Microsoft's std::deque erase is borked
          this->upcoming.pop_front();
        }
      }
    }
    egg::ovum::String resource() const {
      return this->tokenizer->resource();
    }
  private:
    bool ensure(size_t count) {
      if (this->upcoming.empty()) {
        // This is the very first token
        this->push();
      }
      assert(!this->upcoming.empty());
      while (this->upcoming.size() < count) {
        if (this->upcoming.back().kind == EggTokenizerKind::EndOfFile) {
          return false;
        }
        this->push();
      }
      return true;
    }
    void push() {
      // In C++ 14 std::vector::emplace_back did not return a value!
      this->upcoming.emplace_back();
      this->tokenizer->next(this->upcoming.back());
    }
  };

  class EggSyntaxParserBacktrack {
    friend class EggSyntaxParserBacktrackMark;
  private:
    EggSyntaxParserLookahead lookahead;
    size_t cursor;
  public:
    explicit EggSyntaxParserBacktrack(IEggTokenizer& tokenizer)
      : lookahead(tokenizer), cursor(0) {
    }
    const EggTokenizerItem& peek(size_t index) {
      return this->lookahead.peek(this->cursor + index);
    }
    size_t advance(size_t count) {
      // Called by EggSyntaxParserBacktrackMark advance
      this->cursor += count;
      return this->cursor;
    }
    void commit() {
      if (this->cursor > 0) {
        this->lookahead.pop(this->cursor);
        this->cursor = 0;
      }
    }
    egg::ovum::String resource() const {
      return this->lookahead.resource();
    }
  private:
    size_t mark() {
      // Called by EggSyntaxParserBacktrackMark ctor
      return this->cursor;
    }
    void abandon(size_t previous) {
      // Called by EggSyntaxParserBacktrackMark dtor
      this->cursor = previous;
    }
  };

  class EggSyntaxParserBacktrackMark {
  private:
    EggSyntaxParserBacktrack* backtrack;
    size_t previous;
  public:
    explicit EggSyntaxParserBacktrackMark(EggSyntaxParserBacktrack& backtrack)
      : backtrack(&backtrack), previous(backtrack.mark()) {
    }
    ~EggSyntaxParserBacktrackMark() {
      this->backtrack->abandon(this->previous);
    }
    const EggTokenizerItem& peek(size_t index) {
      return this->backtrack->peek(index);
    }
    void advance(size_t count) {
      this->backtrack->advance(count);
    }
    void accept(size_t count) {
      this->previous = this->backtrack->advance(count);
    }
  };

  class EggSyntaxParserContext {
  private:
    egg::ovum::TypeFactory* factory;
    EggSyntaxParserBacktrack backtrack;
  public:
    EggSyntaxParserContext(egg::ovum::TypeFactory& factory, IEggTokenizer& tokenizer)
      : factory(&factory), backtrack(tokenizer) {
    }
    EGG_NORETURN void unexpected(const std::string& message) {
      auto& item = this->backtrack.peek(0);
      throw SyntaxException(message, this->backtrack.resource().toUTF8(), item);
    }
    EGG_NORETURN void unexpected(const std::string& expected, const EggTokenizerItem& item) {
      auto token = item.toString();
      throw SyntaxException(expected + ", not " + token, this->backtrack.resource().toUTF8(), item, token);
    }
    void parseEndOfFile(const char* expected);
    std::unique_ptr<IEggSyntaxNode> parseCompoundStatement();
    std::unique_ptr<IEggSyntaxNode> parseCondition(const char* expected);
    std::unique_ptr<IEggSyntaxNode> parseExpression(const char* expected);
    std::unique_ptr<IEggSyntaxNode> parseExpressionTernary(const char* expected);
    PARSE_BINARY1_LTR(parseExpressionNullCoalescing, parseExpressionLogicalOr, EggTokenizerOperator::QueryQuery)
    PARSE_BINARY1_LTR(parseExpressionLogicalOr, parseExpressionLogicalAnd, EggTokenizerOperator::BarBar)
    PARSE_BINARY1_LTR(parseExpressionLogicalAnd, parseExpressionInclusiveOr, EggTokenizerOperator::AmpersandAmpersand)
    PARSE_BINARY1_LTR(parseExpressionInclusiveOr, parseExpressionExclusiveOr, EggTokenizerOperator::Bar)
    PARSE_BINARY1_LTR(parseExpressionExclusiveOr, parseExpressionAnd, EggTokenizerOperator::Caret)
    PARSE_BINARY1_LTR(parseExpressionAnd, parseExpressionEquality, EggTokenizerOperator::Ampersand)
    PARSE_BINARY2_LTR(parseExpressionEquality, parseExpressionRelational, EggTokenizerOperator::EqualEqual, EggTokenizerOperator::BangEqual)
    PARSE_BINARY4_LTR(parseExpressionRelational, parseExpressionShift, EggTokenizerOperator::Less, EggTokenizerOperator::LessEqual, EggTokenizerOperator::Greater, EggTokenizerOperator::GreaterEqual)
    PARSE_BINARY3_LTR(parseExpressionShift, parseExpressionAdditive, EggTokenizerOperator::ShiftLeft, EggTokenizerOperator::ShiftRight, EggTokenizerOperator::ShiftRightUnsigned)
    PARSE_BINARY3_LTR(parseExpressionMultiplicative, parseExpressionUnary, EggTokenizerOperator::Star, EggTokenizerOperator::Slash, EggTokenizerOperator::Percent)
    std::unique_ptr<IEggSyntaxNode> parseExpressionAdditive(const char* expected);
    std::unique_ptr<IEggSyntaxNode> parseExpressionNegative(const EggSyntaxNodeLocation& location);
    std::unique_ptr<IEggSyntaxNode> parseExpressionUnary(const char* expected);
    std::unique_ptr<IEggSyntaxNode> parseExpressionPostfix(const char* expected);
    std::unique_ptr<IEggSyntaxNode> parseExpressionPostfixGreedy(std::unique_ptr<IEggSyntaxNode>&& expr);
    std::unique_ptr<IEggSyntaxNode> parseExpressionPostfixFunctionCall(std::unique_ptr<IEggSyntaxNode>&& callee);
    std::unique_ptr<IEggSyntaxNode> parseExpressionPrimary(const char* expected);
    std::unique_ptr<IEggSyntaxNode> parseExpressionDeclaration();
    std::unique_ptr<IEggSyntaxNode> parseExpressionParenthesis();
    std::unique_ptr<IEggSyntaxNode> parseExpressionArray(const EggSyntaxNodeLocation& location);
    std::unique_ptr<IEggSyntaxNode> parseExpressionObject(const EggSyntaxNodeLocation& location);
    std::unique_ptr<IEggSyntaxNode> parseModule();
    std::unique_ptr<IEggSyntaxNode> parseStatement();
    std::unique_ptr<IEggSyntaxNode> parseStatementSimple(const char* expected, EggTokenizerOperator terminal);
    std::unique_ptr<IEggSyntaxNode> parseStatementAssignment(std::unique_ptr<IEggSyntaxNode>&& lhs, EggTokenizerOperator terminal);
    std::unique_ptr<IEggSyntaxNode> parseStatementBreak();
    std::unique_ptr<IEggSyntaxNode> parseStatementCase();
    std::unique_ptr<IEggSyntaxNode> parseStatementContinue();
    std::unique_ptr<IEggSyntaxNode> parseStatementDecrementIncrement(EggTokenizerOperator op, const std::string& what, const char* expected, EggTokenizerOperator terminal);
    std::unique_ptr<IEggSyntaxNode> parseStatementDefault();
    std::unique_ptr<IEggSyntaxNode> parseStatementDo();
    std::unique_ptr<IEggSyntaxNode> parseStatementExpression(std::unique_ptr<IEggSyntaxNode>&& expr, EggTokenizerOperator terminal);
    std::unique_ptr<IEggSyntaxNode> parseStatementFor();
    std::unique_ptr<IEggSyntaxNode> parseStatementForeach();
    std::unique_ptr<IEggSyntaxNode> parseStatementFunction(std::unique_ptr<IEggSyntaxNode>&& type, bool generator);
    std::unique_ptr<IEggSyntaxNode> parseStatementIf();
    std::unique_ptr<IEggSyntaxNode> parseStatementReturn();
    std::unique_ptr<IEggSyntaxNode> parseStatementSwitch();
    std::unique_ptr<IEggSyntaxNode> parseStatementThrow();
    std::unique_ptr<IEggSyntaxNode> parseStatementTry();
    std::unique_ptr<IEggSyntaxNode> parseStatementType(std::unique_ptr<IEggSyntaxNode>&& type, bool simple);
    std::unique_ptr<IEggSyntaxNode> parseStatementWhile();
    std::unique_ptr<IEggSyntaxNode> parseStatementYield();
    std::unique_ptr<IEggSyntaxNode> parseType(const char* expected);
    bool parseTypeExpression(egg::ovum::Type& type);
    bool parseTypePostfixExpression(egg::ovum::Type& type);
    egg::ovum::Type parseTypePostfixFunction(const egg::ovum::Type& rettype);
    bool parseTypePrimaryExpression(egg::ovum::Type& type);
  };

  class EggSyntaxParserBase : public IEggSyntaxParser {
  protected:
    egg::ovum::TypeFactory* factory;
  public:
    explicit EggSyntaxParserBase(egg::ovum::TypeFactory& factory) : factory(&factory) {}
  };

  class EggSyntaxParserModule final : public EggSyntaxParserBase {
  public:
    explicit EggSyntaxParserModule(egg::ovum::TypeFactory& factory) : EggSyntaxParserBase(factory) {}
    virtual std::shared_ptr<IEggSyntaxNode> parse(IEggTokenizer& tokenizer) override {
      EggSyntaxParserContext context(*this->factory, tokenizer);
      return context.parseModule();
    }
  };

  class EggSyntaxParserStatement final : public EggSyntaxParserBase {
  public:
    explicit EggSyntaxParserStatement(egg::ovum::TypeFactory& factory) : EggSyntaxParserBase(factory) {}
    virtual std::shared_ptr<IEggSyntaxNode> parse(IEggTokenizer& tokenizer) override {
      EggSyntaxParserContext context(*this->factory, tokenizer);
      auto result = context.parseStatement();
      assert(result != nullptr);
      context.parseEndOfFile("Expected end of input after statement");
      return result;
    }
  };

  class EggSyntaxParserExpression final : public EggSyntaxParserBase {
  public:
    explicit EggSyntaxParserExpression(egg::ovum::TypeFactory& factory) : EggSyntaxParserBase(factory) {}
    virtual std::shared_ptr<IEggSyntaxNode> parse(IEggTokenizer& tokenizer) override {
      EggSyntaxParserContext context(*this->factory, tokenizer);
      auto result = context.parseExpression("Expression expected");
      assert(result != nullptr);
      context.parseEndOfFile("Expected end of input after expression");
      return result;
    }
  };
}

void EggSyntaxParserContext::parseEndOfFile(const char* expected) {
  auto& p0 = this->backtrack.peek(0);
  if (p0.kind != EggTokenizerKind::EndOfFile) {
    this->unexpected(expected, p0);
  }
}

std::unique_ptr<IEggSyntaxNode> EggSyntaxParserContext::parseModule() {
  /*
      module ::= statement+
  */
  EggSyntaxNodeLocation location(this->backtrack.peek(0));
  auto module = std::make_unique<EggSyntaxNode_Module>(location);
  while (this->backtrack.peek(0).kind != EggTokenizerKind::EndOfFile) {
    module->addChild(std::move(this->parseStatement()));
    this->backtrack.commit();
  }
  module->setLocationEnd(this->backtrack.peek(0), 0);
  return module;
}

std::unique_ptr<IEggSyntaxNode> EggSyntaxParserContext::parseStatement() {
  /*
      statement ::= simple-statement ';'
                  | compound-statement
                  | function-definition
                  | flow-statement

      simple-statement ::= type-definition
                         | variable-definition
                         | assignment-statement
                         | void-function-call
  */
  auto& p0 = this->backtrack.peek(0);
  switch (p0.kind) {
  case EggTokenizerKind::Integer:
  case EggTokenizerKind::Float:
    this->unexpected("Unexpected number at start of statement");
  case EggTokenizerKind::String:
    this->unexpected("Unexpected string at start of statement");
  case EggTokenizerKind::Keyword:
    switch (p0.value.k) {
    case EggTokenizerKeyword::Any:
    case EggTokenizerKeyword::Bool:
    case EggTokenizerKeyword::Float:
    case EggTokenizerKeyword::Int:
    case EggTokenizerKeyword::Object:
    case EggTokenizerKeyword::String:
    case EggTokenizerKeyword::Type:
    case EggTokenizerKeyword::Var:
    case EggTokenizerKeyword::Void:
      break;
    case EggTokenizerKeyword::Break:
      return this->parseStatementBreak();
    case EggTokenizerKeyword::Case:
      return this->parseStatementCase();
    case EggTokenizerKeyword::Catch:
      this->unexpected("Unexpected 'catch' clause without matching 'try'");
    case EggTokenizerKeyword::Continue:
      return this->parseStatementContinue();
    case EggTokenizerKeyword::Default:
      return this->parseStatementDefault();
    case EggTokenizerKeyword::Do:
      return this->parseStatementDo();
    case EggTokenizerKeyword::Else:
      this->unexpected("Unexpected 'else' clause without matching 'if'");
    case EggTokenizerKeyword::False:
      this->unexpected("Unexpected 'false' at start of statement");
    case EggTokenizerKeyword::Finally:
      this->unexpected("Unexpected 'finally' clause without matching 'try'");
    case EggTokenizerKeyword::For:
      return this->parseStatementFor();
    case EggTokenizerKeyword::If:
      return this->parseStatementIf();
    case EggTokenizerKeyword::Null:
      this->unexpected("Unexpected 'null' at start of statement");
    case EggTokenizerKeyword::Return:
      return this->parseStatementReturn();
    case EggTokenizerKeyword::Switch:
      return this->parseStatementSwitch();
    case EggTokenizerKeyword::Throw:
      return this->parseStatementThrow();
    case EggTokenizerKeyword::True:
      this->unexpected("Unexpected 'true' at start of statement");
    case EggTokenizerKeyword::Try:
      return this->parseStatementTry();
    case EggTokenizerKeyword::While:
      return this->parseStatementWhile();
    case EggTokenizerKeyword::Yield:
      return this->parseStatementYield();
    default:
      this->unexpected("Internal egg parser error, expected statement", p0);
    }
    break;
  case EggTokenizerKind::Operator:
    // Handle special cases for prefix decrement/increment and compound statements
    if (p0.value.o == EggTokenizerOperator::MinusMinus) {
      return this->parseStatementDecrementIncrement(EggTokenizerOperator::MinusMinus, "decrement", "Expected expression after decrement '--' operator", EggTokenizerOperator::Semicolon);
    } else if (p0.value.o == EggTokenizerOperator::PlusPlus) {
      return this->parseStatementDecrementIncrement(EggTokenizerOperator::PlusPlus, "increment", "Expected expression after increment '++' operator", EggTokenizerOperator::Semicolon);
    } else if (p0.value.o == EggTokenizerOperator::CurlyLeft) {
      return this->parseCompoundStatement();
    } else if (p0.value.o == EggTokenizerOperator::CurlyRight) {
      this->unexpected("Unexpected '}' (no matching '{' seen before)");
    } else if (p0.value.o == EggTokenizerOperator::Semicolon) {
      this->unexpected("Unexpected ';' (empty statements are not permitted)");
    }
    break;
  case EggTokenizerKind::Identifier:
    break;
  case EggTokenizerKind::Attribute:
    this->unexpected("Unimplemented attribute"); // TODO
  case EggTokenizerKind::EndOfFile:
    this->unexpected("Expected statement", p0);
  default:
    this->unexpected("Internal egg parser error (statement kind)");
  }
  auto expression = this->parseExpression(nullptr);
  if (expression != nullptr) {
    return this->parseStatementExpression(std::move(expression), EggTokenizerOperator::Semicolon);
  }
  auto type = this->parseType(nullptr);
  if (type == nullptr) {
      this->unexpected("Unexpected " + p0.toString()); // TODO
  }
  return this->parseStatementType(std::move(type), false);
}

std::unique_ptr<IEggSyntaxNode> EggSyntaxParserContext::parseStatementSimple(const char* expected, EggTokenizerOperator terminal) {
  /*
      simple-statement ::= type-definition
                         | variable-definition
                         | assignment-statement
                         | void-function-call
  */
  assert(expected != nullptr);
  auto& p0 = this->backtrack.peek(0);
  if (p0.isOperator(EggTokenizerOperator::MinusMinus)) {
    return this->parseStatementDecrementIncrement(EggTokenizerOperator::MinusMinus, "decrement", "Expected expression after decrement '--' operator", terminal);
  }
  if (p0.isOperator(EggTokenizerOperator::PlusPlus)) {
    return this->parseStatementDecrementIncrement(EggTokenizerOperator::PlusPlus, "increment", "Expected expression after increment '++' operator", terminal);
  }
  auto expression = this->parseExpression(nullptr);
  if (expression != nullptr) {
    return this->parseStatementExpression(std::move(expression), terminal);
  }
  if (terminal != EggTokenizerOperator::Semicolon) {
    // Only look for type statements if we end with a semicolon
    this->unexpected(expected, p0);
  }
  auto type = this->parseType(nullptr);
  if (type == nullptr) {
    this->unexpected(expected, p0);
  }
  return this->parseStatementType(std::move(type), true);
}

std::unique_ptr<IEggSyntaxNode> EggSyntaxParserContext::parseCompoundStatement() {
  /*
      compound-statement ::= '{' statement* '}'
  */
  // cppcheck-suppress assertWithSideEffect
  assert(this->backtrack.peek(0).isOperator(EggTokenizerOperator::CurlyLeft));
  EggSyntaxNodeLocation location(this->backtrack.peek(0), 0);
  this->backtrack.advance(1); // skip '{'
  auto block = std::make_unique<EggSyntaxNode_Block>(location);
  while (!this->backtrack.peek(0).isOperator(EggTokenizerOperator::CurlyRight)) {
    block->addChild(std::move(this->parseStatement()));
    this->backtrack.commit();
  }
  block->setLocationEnd(this->backtrack.peek(0), 1);
  this->backtrack.advance(1); // skip '}'
  this->backtrack.commit();
  return block;
}

std::unique_ptr<IEggSyntaxNode> EggSyntaxParserContext::parseExpression(const char* expected) {
  /*
      expression ::= conditional-expression
  */
  return this->parseExpressionTernary(expected);
}

std::unique_ptr<IEggSyntaxNode> EggSyntaxParserContext::parseExpressionTernary(const char* expected) {
  /*
      conditional-expression ::= null-coalescing-expression
                               | null-coalescing-expression '?' expression ':' conditional-expression
  */
  EggSyntaxParserBacktrackMark mark(this->backtrack);
  auto expr = this->parseExpressionNullCoalescing(expected);
  if (expr) {
    if (mark.peek(0).isOperator(EggTokenizerOperator::Query)) {
      // Expect <expression> ? <expression> : <conditional-expression>
      EggSyntaxNodeLocation location(mark.peek(0), 0);
      mark.advance(1);
      auto exprTrue = this->parseExpression("Expected expression after '?' of ternary '?:' operator");
      if (!mark.peek(0).isOperator(EggTokenizerOperator::Colon)) {
        this->unexpected("Expected ':' as part of ternary '?:' operator", mark.peek(0));
      }
      location.setLocationEnd(mark.peek(0), 1);
      mark.advance(1);
      auto exprFalse = this->parseExpression("Expected expression after ':' of ternary '?:' operator");
      mark.accept(0);
      return std::make_unique<EggSyntaxNode_TernaryOperator>(location, std::move(expr), std::move(exprTrue), std::move(exprFalse));
    }
    mark.accept(0);
  }
  return expr;
}

std::unique_ptr<IEggSyntaxNode> EggSyntaxParserContext::parseExpressionAdditive(const char* expected) {
  EggSyntaxParserBacktrackMark mark(this->backtrack);
  auto expr = this->parseExpressionMultiplicative(expected);
  for (auto* token = &mark.peek(0); (expr != nullptr); token = &mark.peek(0)) {
    EggSyntaxNodeLocation location(*token, 1);
    if (token->isOperator(EggTokenizerOperator::Plus)) {
      expected = "Expected expression after infix '+' operator";
    } else if (token->isOperator(EggTokenizerOperator::Minus)) {
      expected = "Expected expression after infix '-' operator";
    } else if (token->isOperator(EggTokenizerOperator::PlusPlus)) {
      // We don't handle the special case of 'a++b' or 'a++1' because we have no unary plus operator
      this->unexpected("Unexpected '+' after infix '+' operator");
    } else if (token->isOperator(EggTokenizerOperator::MinusMinus)) {
      // Handle the special case of 'a--b' or 'a--1'
      mark.advance(1);
      auto lhs = std::move(expr);
      auto rhs = this->parseExpressionNegative(location);
      expr = std::make_unique<EggSyntaxNode_BinaryOperator>(location, EggTokenizerOperator::Minus, std::move(lhs), std::move(rhs));
      continue;
    } else {
      break;
    }
    mark.advance(1);
    auto lhs = std::move(expr);
    auto rhs = this->parseExpressionMultiplicative(expected);
    expr = std::make_unique<EggSyntaxNode_BinaryOperator>(location, token->value.o, std::move(lhs), std::move(rhs));
  }
  mark.accept(0);
  return expr;
}

std::unique_ptr<IEggSyntaxNode> EggSyntaxParserContext::parseExpressionNegative(const EggSyntaxNodeLocation& location) {
  auto& p0 = this->backtrack.peek(0);
  auto expr = this->parseExpressionUnary("Expected expression after prefix '-' operator");
  if (p0.contiguous && expr->negate()) {
    // Successfully negated the literal
    return expr;
  }
  return std::make_unique<EggSyntaxNode_UnaryOperator>(location, EggTokenizerOperator::Minus, std::move(expr));
}

std::unique_ptr<IEggSyntaxNode> EggSyntaxParserContext::parseExpressionUnary(const char* expected) {
  /*
      unary-expression ::= postfix-expression
                         | unary-operator unary-expression

      unary-operator ::= '&'
                       | '*'
                       | '-'
                       | '~'
                       | '!'
*/
  auto& p0 = this->backtrack.peek(0);
  if (p0.isOperator(EggTokenizerOperator::Ampersand)) {
    expected = "Expected expression after prefix '&' operator";
  } else if (p0.isOperator(EggTokenizerOperator::Star)) {
    expected = "Expected expression after prefix '*' operator";
  } else if (p0.isOperator(EggTokenizerOperator::Minus)) {
    this->backtrack.advance(1);
    EggSyntaxNodeLocation location(p0, 1);
    return this->parseExpressionNegative(location);
  } else if (p0.isOperator(EggTokenizerOperator::MinusMinus)) {
    this->backtrack.advance(1);
    EggSyntaxNodeLocation location(p0, 1);
    auto negative = this->parseExpressionNegative(location);
    return std::make_unique<EggSyntaxNode_UnaryOperator>(location, EggTokenizerOperator::Minus, std::move(negative));
  } else if (p0.isOperator(EggTokenizerOperator::Tilde)) {
    expected = "Expected expression after prefix '~' operator";
  } else if (p0.isOperator(EggTokenizerOperator::Bang)) {
    expected = "Expected expression after prefix '!' operator";
  } else {
    return this->parseExpressionPostfix(expected);
  }
  EggSyntaxParserBacktrackMark mark(this->backtrack);
  EggSyntaxNodeLocation location(p0, 1);
  mark.advance(1);
  auto expr = this->parseExpressionUnary(expected);
  mark.accept(0);
  return std::make_unique<EggSyntaxNode_UnaryOperator>(location, p0.value.o, std::move(expr));
}

std::unique_ptr<IEggSyntaxNode> EggSyntaxParserContext::parseExpressionPostfix(const char* expected) {
  auto expr = this->parseExpressionPrimary(expected);
  return (expr == nullptr) ? nullptr : this->parseExpressionPostfixGreedy(std::move(expr));
}

std::unique_ptr<IEggSyntaxNode> EggSyntaxParserContext::parseExpressionPostfixGreedy(std::unique_ptr<IEggSyntaxNode>&& expr) {
  /*
      postfix-expression ::= primary-expression
                           | postfix-expression '[' expression ']'
                           | postfix-expression '(' parameter-list? ')'
                           | postfix-expression '.' identifier
                           | postfix-expression '?.' identifier
  */
  for (;;) {
    auto& p0 = this->backtrack.peek(0);
    if (p0.isOperator(EggTokenizerOperator::BracketLeft)) {
      // Expect <expression> '[' <expression> ']' 
      EggSyntaxNodeLocation location(p0, 0);
      EggSyntaxParserBacktrackMark mark(this->backtrack);
      mark.advance(1);
      auto index = this->parseExpression("Expected expression inside indexing '[]' operators");
      auto& p1 = mark.peek(0);
      if (!p1.isOperator(EggTokenizerOperator::BracketRight)) {
        this->unexpected("Expected ']' after indexing expression following '['", p1);
      }
      location.setLocationEnd(p1, 1);
      mark.accept(1);
      expr = std::make_unique<EggSyntaxNode_BinaryOperator>(location, EggTokenizerOperator::BracketLeft, std::move(expr), std::move(index));
    } else if (p0.isOperator(EggTokenizerOperator::ParenthesisLeft)) {
      // Expect <expression> '(' <parameter-list>? ')'
      expr = this->parseExpressionPostfixFunctionCall(std::move(expr));
    } else if (p0.isOperator(EggTokenizerOperator::Dot)) {
      // Expect <expression> '.' <identifer>
      EggSyntaxNodeLocation location(p0, 1);
      EggSyntaxParserBacktrackMark mark(this->backtrack);
      auto& p1 = mark.peek(1);
      if (p1.kind != EggTokenizerKind::Identifier) {
        this->unexpected("Expected property name to follow '.' operator", p1);
      }
      expr = std::make_unique<EggSyntaxNode_Dot>(location, std::move(expr), p1.value.s, false);
      mark.accept(2);
    } else if (p0.isOperator(EggTokenizerOperator::Query)) {
      // Expect <expression> '?.' <identifer>
      EggSyntaxParserBacktrackMark mark(this->backtrack);
      auto& p1 = mark.peek(1);
      // We use contiguous sequential operators to disambiguate "a?...x:y" from "a?.b"
      if (!p1.isOperator(EggTokenizerOperator::Dot) || !p1.contiguous) {
        break;
      }
      EggSyntaxNodeLocation location(p0, 2);
      auto& p2 = mark.peek(2);
      if (p2.kind != EggTokenizerKind::Identifier) {
        this->unexpected("Expected property name to follow '?.' operator", p2);
      }
      expr = std::make_unique<EggSyntaxNode_Dot>(location, std::move(expr), p2.value.s, true);
      mark.accept(3);
    } else {
      // No postfix operator, return just the expression
      break;
    }
  }
  return std::move(expr);
}

std::unique_ptr<IEggSyntaxNode> EggSyntaxParserContext::parseExpressionPostfixFunctionCall(std::unique_ptr<IEggSyntaxNode>&& callee) {
  /*
      parameter-list ::= positional-parameter-list
                       | positional-parameter-list ',' named-parameter-list
                       | named-parameter-list

      positional-parameter-list ::= positional-parameter
                                  | positional-parameter-list ',' positional-parameter

      positional-parameter ::= expression
                             | '...' expression

      named-parameter-list ::= named-parameter
                             | named-parameter-list ',' named-parameter

      named-parameter ::= variable-identifier ':' expression
  */
  EggSyntaxParserBacktrackMark mark(this->backtrack);
  // cppcheck-suppress assertWithSideEffect
  assert(mark.peek(0).isOperator(EggTokenizerOperator::ParenthesisLeft));
  EggSyntaxNodeLocation location(mark.peek(0), 0);
  auto call = std::make_unique<EggSyntaxNode_Call>(location, std::move(callee));
  if (mark.peek(1).isOperator(EggTokenizerOperator::ParenthesisRight)) {
    // This is an empty parameter list: '(' ')'
    mark.accept(1);
  } else {
    // Don't worry about the order of positional and named parameters at this stage
    const EggTokenizerItem* p0;
    do {
      mark.advance(1);
      p0 = &mark.peek(0);
      if ((p0->kind == EggTokenizerKind::Identifier) && mark.peek(1).isOperator(EggTokenizerOperator::Colon)) {
        // Expect <identifier> ':' <expression>
        EggSyntaxNodeLocation plocation(*p0);
        plocation.setLocationEnd(mark.peek(1), 1);
        mark.advance(2);
        auto expr = this->parseExpression("Expected expression for named function call parameter value");
        auto named = std::make_unique<EggSyntaxNode_Named>(plocation, p0->value.s, std::move(expr));
        call->addChild(std::move(named));
      } else {
        // Expect <expression>
        auto expr = this->parseExpression("Expected expression for function call parameter value");
        call->addChild(std::move(expr));
      }
      p0 = &mark.peek(0);
    } while (p0->isOperator(EggTokenizerOperator::Comma));
    if (!p0->isOperator(EggTokenizerOperator::ParenthesisRight)) {
      this->unexpected("Expected ')' at end of function call parameter list", *p0);
    }
    mark.accept(0);
  }
  // cppcheck-suppress assertWithSideEffect
  assert(mark.peek(0).isOperator(EggTokenizerOperator::ParenthesisRight));
  call->setLocationEnd(mark.peek(0), 1);
  mark.accept(1); // skip ')'
  return call;
}

std::unique_ptr<IEggSyntaxNode> EggSyntaxParserContext::parseExpressionPrimary(const char* expected) {
  /*
      primary-expression ::= identifier
                           | constant-literal
                           | object-value
                           | array-value
                           | lambda-value
                           | '(' expression ')'
                           | cast-specifier '(' expression ')'
  */
  EggSyntaxParserBacktrackMark mark(this->backtrack);
  auto& p0 = mark.peek(0);
  EggSyntaxNodeLocation location(p0);
  switch (p0.kind) {
  case EggTokenizerKind::Integer:
  case EggTokenizerKind::Float:
  case EggTokenizerKind::String:
    mark.accept(1);
    return std::make_unique<EggSyntaxNode_Literal>(location, p0.kind, p0.value);
  case EggTokenizerKind::Identifier:
    mark.accept(1);
    return std::make_unique<EggSyntaxNode_Identifier>(location, p0.value.s);
  case EggTokenizerKind::Keyword:
    EGG_WARNING_SUPPRESS_SWITCH_BEGIN
    switch (p0.value.k) {
    case EggTokenizerKeyword::Null:
    case EggTokenizerKeyword::False:
    case EggTokenizerKeyword::True:
      // Literal constant
      mark.accept(1);
      return std::make_unique<EggSyntaxNode_Literal>(location, p0.kind, p0.value);
    case EggTokenizerKeyword::Bool:
    case EggTokenizerKeyword::Int:
    case EggTokenizerKeyword::Float:
    case EggTokenizerKeyword::String:
    case EggTokenizerKeyword::Object:
    case EggTokenizerKeyword::Type:
      // It could be a constructor like 'string(...)' or a property like 'float.epsilon'
      auto& p1 = mark.peek(1);
      if (p1.isOperator(EggTokenizerOperator::ParenthesisLeft) || p1.isOperator(EggTokenizerOperator::Dot)) {
        mark.accept(1);
        return std::make_unique<EggSyntaxNode_Identifier>(location, p0.value.s);
      }
      break;
    }
    EGG_WARNING_SUPPRESS_SWITCH_END
    break;
  case EggTokenizerKind::Operator:
    if (p0.value.o == EggTokenizerOperator::ParenthesisLeft) {
      auto inside = this->parseExpressionParenthesis();
      mark.accept(0);
      return inside;
    }
    if (p0.value.o == EggTokenizerOperator::BracketLeft) {
      auto array = this->parseExpressionArray(location);
      mark.accept(0);
      return array;
    }
    if (p0.value.o == EggTokenizerOperator::CurlyLeft) {
      auto object = this->parseExpressionObject(location);
      mark.accept(0);
      return object;
    }
    break;
  case EggTokenizerKind::Attribute:
  case EggTokenizerKind::EndOfFile:
  default:
    break;
  }
  if (expected) {
    this->unexpected(expected, p0);
  }
  return nullptr;
}

std::unique_ptr<IEggSyntaxNode> EggSyntaxParserContext::parseCondition(const char* expected) {
  /*
      condition ::= expression
  */
  assert(expected != nullptr);
  auto expr = this->parseExpression(expected);
  assert(expr != nullptr);
  return expr;
}

std::unique_ptr<IEggSyntaxNode> EggSyntaxParserContext::parseExpressionDeclaration() {
  /*
      expression-declaration ::= keyword '(' variable-definition-type variable-identifier '=' expression ')'
                               | keyword '(' expression ')'
  */
  EggSyntaxParserBacktrackMark mark(this->backtrack);
  auto& pk = mark.peek(0);
  assert(pk.kind == EggTokenizerKind::Keyword);
  auto keyword = EggTokenizerValue::getKeywordString(pk.value.k);
  if (!mark.peek(1).isOperator(EggTokenizerOperator::ParenthesisLeft)) {
    this->unexpected("Expected '(' after '" + keyword + "' keyword", mark.peek(1));
  }
  mark.advance(2);
  auto expr = this->parseExpression(nullptr);
  if (expr == nullptr) {
    // Expect <keyword> '(' <type> <identifier> '=' <expression> ')' <compound-statement>
    auto type = this->parseType(nullptr);
    if (type == nullptr) {
      this->unexpected("Expected expression or type after '(' in '" + keyword + "' statement", mark.peek(0));
    }
    auto& p0 = mark.peek(0);
    if (p0.kind != EggTokenizerKind::Identifier) {
      this->unexpected("Expected variable identifier after type in '" + keyword + "' statement", p0);
    }
    auto& p1 = mark.peek(1);
    if (!p1.isOperator(EggTokenizerOperator::Equal)) {
      this->unexpected("Expected '=' after variable identifier in '" + keyword + "' statement", p1);
    }
    EggSyntaxNodeLocation location(p0);
    mark.advance(2);
    auto rhs = this->parseExpression(nullptr);
    if (rhs == nullptr) {
      this->unexpected("Expected expression after '=' in '" + keyword + "' statement", mark.peek(0));
    }
    expr = std::make_unique<EggSyntaxNode_Guard>(location, p0.value.s, std::move(type), std::move(rhs));
  }
  if (!mark.peek(0).isOperator(EggTokenizerOperator::ParenthesisRight)) {
    this->unexpected("Expected ')' after expression in '" + keyword + "' statement", mark.peek(0));
  }
  mark.accept(1);
  return expr;
}

std::unique_ptr<IEggSyntaxNode> EggSyntaxParserContext::parseExpressionParenthesis() {
  /*
      parenthesis-value ::= '(' expression ')'
  */
  EggSyntaxParserBacktrackMark mark(this->backtrack);
  // cppcheck-suppress assertWithSideEffect
  assert(mark.peek(0).isOperator(EggTokenizerOperator::ParenthesisLeft));
  mark.advance(1);
  auto expr = this->parseExpression(nullptr);
  if (expr != nullptr) {
    if (!mark.peek(0).isOperator(EggTokenizerOperator::ParenthesisRight)) {
      this->unexpected("Expected ')' at end of parenthesized expression", mark.peek(0));
    }
    mark.accept(1);
  }
  return expr;
}

std::unique_ptr<IEggSyntaxNode> EggSyntaxParserContext::parseExpressionArray(const EggSyntaxNodeLocation& location) {
  /*
      array-value ::= '[' array-value-list? ']'

      array-value-list ::= expression
                         | array-value-list ',' expression
  */
  EggSyntaxParserBacktrackMark mark(this->backtrack);
  // cppcheck-suppress assertWithSideEffect
  assert(mark.peek(0).isOperator(EggTokenizerOperator::BracketLeft));
  auto array = std::make_unique<EggSyntaxNode_Array>(location);
  if (mark.peek(1).isOperator(EggTokenizerOperator::BracketRight)) {
    // This is an empty array: '[' ']'
    mark.accept(2);
  } else {
    const EggTokenizerItem* p;
    do {
      mark.advance(1);
      auto expr = this->parseExpression("Expected expression for array value");
      array->addChild(std::move(expr));
      p = &mark.peek(0);
    } while (p->isOperator(EggTokenizerOperator::Comma));
    if (!p->isOperator(EggTokenizerOperator::BracketRight)) {
      this->unexpected("Expected ']' at end of array expression", *p);
    }
    mark.accept(1);
  }
  return array;
}

std::unique_ptr<IEggSyntaxNode> EggSyntaxParserContext::parseExpressionObject(const EggSyntaxNodeLocation& location) {
  /*
      object-value ::= '{' object-value-list? '}'

      object-value-list ::= name ':' expression
                          | object-value-list ',' name ':' expression
  */
  EggSyntaxParserBacktrackMark mark(this->backtrack);
  // cppcheck-suppress assertWithSideEffect
  assert(mark.peek(0).isOperator(EggTokenizerOperator::CurlyLeft));
  auto object = std::make_unique<EggSyntaxNode_Object>(location);
  if (mark.peek(1).isOperator(EggTokenizerOperator::CurlyRight)) {
    // This is an empty object: '{' '}'
    mark.accept(2);
  } else {
    std::set<egg::ovum::String> seen;
    const EggTokenizerItem* p;
    do {
      // Expect <identifier> ':' <expression>
      p = &mark.peek(1);
      if (p->kind != EggTokenizerKind::Identifier) {
        this->unexpected("Expected property name in object expression", *p);
      }
      auto name = p->value.s;
      if (!seen.insert(name).second) {
        mark.advance(1); // Point to the property name so the constructed error message is accurate
        this->unexpected("Duplicate property name in object expression: '" + name.toUTF8() + "'");
      }
      p = &mark.peek(2);
      if (!p->isOperator(EggTokenizerOperator::Colon)) {
        this->unexpected("Expected ':' after property name in object expression", *p);
      }
      mark.advance(3);
      auto expr = this->parseExpression("Expected expression after ':' in object expression");
      auto named = std::make_unique<EggSyntaxNode_Named>(location, name, std::move(expr));
      object->addChild(std::move(named));
      p = &mark.peek(0);
    } while (p->isOperator(EggTokenizerOperator::Comma));
    if (!p->isOperator(EggTokenizerOperator::CurlyRight)) {
      this->unexpected("Expected '}' at end of object expression", *p);
    }
    mark.accept(1);
  }
  return object;
}

std::unique_ptr<IEggSyntaxNode> EggSyntaxParserContext::parseStatementAssignment(std::unique_ptr<IEggSyntaxNode>&& lhs, EggTokenizerOperator terminal) {
  /*
      assignment-target ::= variable-identifier
                          | '*'+ expression
                          | expression '[' expression ']'
                          | expression '.' identifier

      assignment-operator ::= '='
                            | '*='
                            | '/='
                            | '%='
                            | '+='
                            | '-='
                            | '<<='
                            | '>>='
                            | '>>>='
                            | '??='
                            | '&='
                            | '&&='
                            | '^='
                            | '|='
                            | '||='
  */
  EggSyntaxParserBacktrackMark mark(this->backtrack);
  auto& p0 = mark.peek(0);
  const char* expected = nullptr;
  if (p0.kind == EggTokenizerKind::Operator) {
    auto key = EggTokenizerValue::getOperatorString(p0.value.o);
    expected = assignmentExpectation(key);
  }
  if (expected == nullptr) {
    this->unexpected("Expected assignment operator after expression", p0);
  }
  EggSyntaxNodeLocation location(p0);
  mark.advance(1);
  auto rhs = this->parseExpression(expected);
  auto& px = mark.peek(0);
  if (!px.isOperator(terminal)) {
    this->unexpected("Expected '" + EggTokenizerValue::getOperatorString(terminal) + "' after assignment statement", px);
  }
  mark.accept(1);
  return std::make_unique<EggSyntaxNode_Assignment>(location, p0.value.o, std::move(lhs), std::move(rhs));
}

std::unique_ptr<IEggSyntaxNode> EggSyntaxParserContext::parseStatementBreak() {
  /*
      break-statement ::= 'break' ';'
  */
  auto& p0 = this->backtrack.peek(0);
  assert(p0.isKeyword(EggTokenizerKeyword::Break));
  auto& p1 = this->backtrack.peek(1);
  if (!p1.isOperator(EggTokenizerOperator::Semicolon)) {
    this->unexpected("Expected ';' after 'break' keyword", p1);
  }
  EggSyntaxNodeLocation location(p0);
  this->backtrack.advance(2);
  return std::make_unique<EggSyntaxNode_Break>(location);
}

std::unique_ptr<IEggSyntaxNode> EggSyntaxParserContext::parseStatementCase() {
  /*
      case-statement ::= 'case' <expression> ':'
  */
  EggSyntaxParserBacktrackMark mark(this->backtrack);
  auto& p0 = mark.peek(0);
  assert(p0.isKeyword(EggTokenizerKeyword::Case));
  mark.advance(1);
  auto expr = this->parseExpression("Expected expression after 'case' keyword");
  auto& px = mark.peek(0);
  if (!px.isOperator(EggTokenizerOperator::Colon)) {
    this->unexpected("Expected colon after 'case' expression", px);
  }
  EggSyntaxNodeLocation location(p0);
  mark.accept(1);
  return std::make_unique<EggSyntaxNode_Case>(location, std::move(expr));
}

std::unique_ptr<IEggSyntaxNode> EggSyntaxParserContext::parseStatementContinue() {
  /*
      continue-statement ::= 'continue' ';'
  */
  auto& p0 = this->backtrack.peek(0);
  assert(p0.isKeyword(EggTokenizerKeyword::Continue));
  auto& p1 = this->backtrack.peek(1);
  if (!p1.isOperator(EggTokenizerOperator::Semicolon)) {
    this->unexpected("Expected ';' after 'continue' keyword", p1);
  }
  EggSyntaxNodeLocation location(p0);
  this->backtrack.advance(2);
  return std::make_unique<EggSyntaxNode_Continue>(location);
}

std::unique_ptr<IEggSyntaxNode> EggSyntaxParserContext::parseStatementDecrementIncrement(EggTokenizerOperator op, const std::string& what, const char* expected, EggTokenizerOperator terminal) {
  /*
      assignment-statement ::= assignment-list assignment-operator expression-list
                             | '++' assignment-target
                             | '--' assignment-target
  */
  EggSyntaxParserBacktrackMark mark(this->backtrack);
  auto& p0 = mark.peek(0);
  assert(p0.isOperator(op));
  mark.advance(1);
  auto expr = this->parseExpression(expected);
  auto& px = mark.peek(0);
  if (!px.isOperator(terminal)) {
    this->unexpected("Expected '" + EggTokenizerValue::getOperatorString(terminal) + "' after " + what + " statement", px);
  }
  EggSyntaxNodeLocation location(p0);
  mark.accept(1);
  return std::make_unique<EggSyntaxNode_Mutate>(location, op, std::move(expr));
}

std::unique_ptr<IEggSyntaxNode> EggSyntaxParserContext::parseStatementDefault() {
  /*
      default-statement ::= 'default' ':'
  */
  auto& p0 = this->backtrack.peek(0);
  assert(p0.isKeyword(EggTokenizerKeyword::Default));
  auto& p1 = this->backtrack.peek(1);
  if (!p1.isOperator(EggTokenizerOperator::Colon)) {
    this->unexpected("Expected colon after 'default' keyword", p1);
  }
  EggSyntaxNodeLocation location(p0);
  this->backtrack.advance(2);
  return std::make_unique<EggSyntaxNode_Default>(location);
}

std::unique_ptr<IEggSyntaxNode> EggSyntaxParserContext::parseStatementDo() {
  /*
      do-statement ::= 'do' <compound-statement> 'while' '(' <expression> ')' ';'
  */
  EggSyntaxParserBacktrackMark mark(this->backtrack);
  auto& p0 = mark.peek(0);
  assert(p0.isKeyword(EggTokenizerKeyword::Do));
  EggSyntaxNodeLocation location(p0);
  mark.advance(1);
  if (!mark.peek(0).isOperator(EggTokenizerOperator::CurlyLeft)) {
    this->unexpected("Expected '{' after 'do' keyword", mark.peek(0));
  }
  auto block = this->parseCompoundStatement();
  if (!mark.peek(0).isKeyword(EggTokenizerKeyword::While)) {
    this->unexpected("Expected 'while' after '}' in 'do' statement", mark.peek(0));
  }
  if (!mark.peek(1).isOperator(EggTokenizerOperator::ParenthesisLeft)) {
    this->unexpected("Expected '(' after 'while' keyword in 'do' statement", mark.peek(1));
  }
  mark.advance(2);
  auto expr = this->parseCondition("Expected condition expression after 'while (' in 'do' statement");
  if (!mark.peek(0).isOperator(EggTokenizerOperator::ParenthesisRight)) {
    this->unexpected("Expected ')' after 'do' condition expression", mark.peek(0));
  }
  if (!mark.peek(1).isOperator(EggTokenizerOperator::Semicolon)) {
    this->unexpected("Expected ';' after ')' at end of 'do' statement", mark.peek(1));
  }
  mark.accept(2);
  return std::make_unique<EggSyntaxNode_Do>(location, std::move(expr), std::move(block));
}

std::unique_ptr<IEggSyntaxNode> EggSyntaxParserContext::parseStatementExpression(std::unique_ptr<IEggSyntaxNode>&& expr, EggTokenizerOperator terminal) {
  // Expect <lhs> <assignment-operator> <rhs> ';'
  //     or <function-call> ';'
  if (!this->backtrack.peek(0).isOperator(terminal)) {
    return this->parseStatementAssignment(std::move(expr), terminal);
  }
  // Assume function call expression
  this->backtrack.advance(1);
  return std::move(expr);
}

std::unique_ptr<IEggSyntaxNode> EggSyntaxParserContext::parseStatementFor() {
  /*
      for-statement ::= 'for' '(' simple-statement? ';' condition? ';' simple-statement? ')' compound-statement
                      | 'for' '(' variable-definition-type? variable-identifier ':' expression ')' compound-statement
  */
  EggSyntaxParserBacktrackMark mark(this->backtrack);
  auto& p0 = mark.peek(0);
  assert(p0.isKeyword(EggTokenizerKeyword::For));
  if (!mark.peek(1).isOperator(EggTokenizerOperator::ParenthesisLeft)) {
    this->unexpected("Expected '(' after 'for' keyword", mark.peek(1));
  }
  auto foreach = this->parseStatementForeach();
  if (foreach != nullptr) {
    mark.accept(0);
    return foreach;
  }
  EggSyntaxNodeLocation forLocation(p0);
  mark.advance(2);
  std::unique_ptr<IEggSyntaxNode> pre, cond, post;
  if (mark.peek(0).isOperator(EggTokenizerOperator::Semicolon)) {
    mark.advance(1); // skip ';'
  } else {
    pre = this->parseStatementSimple("Expected simple statement after '(' in 'for' statement", EggTokenizerOperator::Semicolon);
  }
  if (mark.peek(0).isOperator(EggTokenizerOperator::Semicolon)) {
    mark.advance(1); // skip ';'
  } else {
    cond = this->parseCondition("Expected condition expression as second clause in 'for' statement");
    if (!mark.peek(0).isOperator(EggTokenizerOperator::Semicolon)) {
      this->unexpected("Expected ';' after condition expression of 'for' statement", mark.peek(0));
    }
    mark.advance(1); // skip ';'
  }
  if (mark.peek(0).isOperator(EggTokenizerOperator::ParenthesisRight)) {
    mark.advance(1); // skip ')'
  } else {
    post = this->parseStatementSimple("Expected simple statement as third clause in 'for' statement", EggTokenizerOperator::ParenthesisRight);
  }
  if (!mark.peek(0).isOperator(EggTokenizerOperator::CurlyLeft)) {
    this->unexpected("Expected '{' after ')' in 'for' statement", mark.peek(0));
  }
  auto block = this->parseCompoundStatement();
  mark.accept(0);
  return std::make_unique<EggSyntaxNode_For>(forLocation, std::move(pre), std::move(cond), std::move(post), std::move(block));
}

std::unique_ptr<IEggSyntaxNode> EggSyntaxParserContext::parseStatementForeach() {
  /*
      foreach-statement ::= 'for' '(' variable-definition-type? variable-identifier ':' expression ')' compound-statement
  */
  EggSyntaxParserBacktrackMark mark(this->backtrack);
  // cppcheck-suppress assertWithSideEffect
  assert(mark.peek(0).isKeyword(EggTokenizerKeyword::For));
  // cppcheck-suppress assertWithSideEffect
  assert(mark.peek(1).isOperator(EggTokenizerOperator::ParenthesisLeft));
  mark.advance(2);
  std::unique_ptr<IEggSyntaxNode> target;
  auto type = this->parseType(nullptr);
  if (type != nullptr) {
    // Expect <type> <identifier> ':' <expression>
    auto& p0 = mark.peek(0);
    if (p0.kind != EggTokenizerKind::Identifier) {
      return nullptr;
    }
    target = std::make_unique<EggSyntaxNode_Declare>(EggSyntaxNodeLocation(p0), p0.value.s, std::move(type));
    mark.advance(1);
  } else {
    // Expect <expression> ':' <expression>
    target = this->parseExpression(nullptr);
    if (target == nullptr) {
      return nullptr;
    }
  }
  // Expect ':' <expression> ')' <compound-statement>
  if (!mark.peek(0).isOperator(EggTokenizerOperator::Colon)) {
    return nullptr;
  }
  EggSyntaxNodeLocation location(mark.peek(0), 1);
  mark.advance(1);
  auto expr = this->parseExpression("Expected expression after ':' in 'for' statement");
  if (!mark.peek(0).isOperator(EggTokenizerOperator::ParenthesisRight)) {
    this->unexpected("Expected ')' after expression in 'for' statement", mark.peek(0));
  }
  mark.advance(1);
  if (!mark.peek(0).isOperator(EggTokenizerOperator::CurlyLeft)) {
    this->unexpected("Expected '{' after ')' in 'for' statement", mark.peek(0));
  }
  auto block = this->parseCompoundStatement();
  mark.accept(0);
  return std::make_unique<EggSyntaxNode_Foreach>(location, std::move(target), std::move(expr), std::move(block));
}

std::unique_ptr<IEggSyntaxNode> EggSyntaxParserContext::parseStatementFunction(std::unique_ptr<IEggSyntaxNode>&& type, bool generator) {
  // Already consumed <type>
  EggSyntaxParserBacktrackMark mark(this->backtrack);
  auto& p0 = mark.peek(0);
  assert(p0.kind == EggTokenizerKind::Identifier);
  // cppcheck-suppress assertWithSideEffect
  assert(mark.peek(1).isOperator(EggTokenizerOperator::ParenthesisLeft));
  auto result = std::make_unique<EggSyntaxNode_FunctionDefinition>(EggSyntaxNodeLocation(p0), p0.value.s, std::move(type), generator);
  mark.advance(2);
  while (!mark.peek(0).isOperator(EggTokenizerOperator::ParenthesisRight)) {
    auto ptype = this->parseType("Expected parameter type in function definition");
    auto& p1 = mark.peek(0);
    if (p1.kind != EggTokenizerKind::Identifier) {
      this->unexpected("Expected identifier after parameter type in function definition", p1);
    }
    EggSyntaxNodeLocation location(p1);
    mark.advance(1);
    auto optional = mark.peek(0).isOperator(EggTokenizerOperator::Equal);
    if (optional) {
      auto& p2 = mark.peek(1);
      if (!p2.isKeyword(EggTokenizerKeyword::Null)) {
        this->unexpected("Expected 'null' as default value for parameter '" + p1.value.s.toUTF8() + "'", p2);
      }
      mark.advance(2);
    }
    auto parameter = std::make_unique<EggSyntaxNode_Parameter>(location, p1.value.s, std::move(ptype), optional);
    result->addChild(std::move(parameter));
    auto& p3 = mark.peek(0);
    if (p3.isOperator(EggTokenizerOperator::Comma)) {
      mark.advance(1);
    } else if (!p3.isOperator(EggTokenizerOperator::ParenthesisRight)) {
      this->unexpected("Expected ',' or ')' after parameter in function definition", p3);
    }
  }
  mark.advance(1); // Skip ')'
  auto block = this->parseCompoundStatement();
  result->addChild(std::move(block));
  mark.accept(0);
  return result;
}

std::unique_ptr<IEggSyntaxNode> EggSyntaxParserContext::parseStatementIf() {
  /*
      if-statement ::= 'if' '(' <condition-expression> ')' <compound-statement> <else-clause>?

      else-clause ::= 'else' <compound-statement>
                    | 'else' <if-statement>
  */
  EggSyntaxParserBacktrackMark mark(this->backtrack);
  auto& p0 = mark.peek(0);
  assert(p0.isKeyword(EggTokenizerKeyword::If));
  auto expr = this->parseExpressionDeclaration();
  if (!mark.peek(0).isOperator(EggTokenizerOperator::CurlyLeft)) {
    this->unexpected("Expected '{' after ')' in 'if' statement", mark.peek(0));
  }
  EggSyntaxNodeLocation location(p0);
  auto block = this->parseCompoundStatement();
  auto result = std::make_unique<EggSyntaxNode_If>(location, std::move(expr), std::move(block));
  if (mark.peek(0).isKeyword(EggTokenizerKeyword::Else)) {
    auto& p1 = mark.peek(1);
    mark.advance(1);
    if (p1.isOperator(EggTokenizerOperator::CurlyLeft)) {
      result->addChild(this->parseCompoundStatement());
    } else if (p1.isKeyword(EggTokenizerKeyword::If)) {
      result->addChild(this->parseStatementIf());
    } else {
      this->unexpected("Expected '{' after 'else' in 'if' statement", p1);
    }
  }
  mark.accept(0);
  return result;
}

std::unique_ptr<IEggSyntaxNode> EggSyntaxParserContext::parseStatementReturn() {
  /*
      return-statement ::= 'return' expression? ';'
  */
  EggSyntaxParserBacktrackMark mark(this->backtrack);
  auto& p0 = mark.peek(0);
  assert(p0.isKeyword(EggTokenizerKeyword::Return));
  auto results = std::make_unique<EggSyntaxNode_Return>(EggSyntaxNodeLocation(p0));
  mark.advance(1);
  auto expr = this->parseExpression(nullptr);
  if (expr != nullptr) {
    results->addChild(std::move(expr));
  }
  auto& px = mark.peek(0);
  if (!px.isOperator(EggTokenizerOperator::Semicolon)) {
    this->unexpected("Expected ';' at end of 'return' statement", px);
  }
  mark.accept(1);
  return std::move(results);
}

std::unique_ptr<IEggSyntaxNode> EggSyntaxParserContext::parseStatementSwitch() {
  /*
      switch-statement ::= 'switch' '(' <condition-expression> ')' <compound-statement>
  */
  EggSyntaxParserBacktrackMark mark(this->backtrack);
  auto& p0 = mark.peek(0);
  assert(p0.isKeyword(EggTokenizerKeyword::Switch));
  auto expr = this->parseExpressionDeclaration();
  if (!mark.peek(0).isOperator(EggTokenizerOperator::CurlyLeft)) {
    this->unexpected("Expected '{' after ')' in 'switch' statement", mark.peek(0));
  }
  EggSyntaxNodeLocation location(p0);
  auto block = this->parseCompoundStatement();
  mark.accept(0);
  return std::make_unique<EggSyntaxNode_Switch>(location, std::move(expr), std::move(block));
}

std::unique_ptr<IEggSyntaxNode> EggSyntaxParserContext::parseStatementThrow() {
  /*
      throw-statement ::= 'throw' expression? ';'
  */
  EggSyntaxParserBacktrackMark mark(this->backtrack);
  auto& p0 = mark.peek(0);
  assert(p0.isKeyword(EggTokenizerKeyword::Throw));
  EggSyntaxNodeLocation location(p0);
  mark.advance(1);
  auto expr = this->parseExpression(nullptr);
  auto result = std::make_unique<EggSyntaxNode_Throw>(location);
  if (expr != nullptr) {
    result->addChild(std::move(expr));
    if (!mark.peek(0).isOperator(EggTokenizerOperator::Semicolon)) {
      this->unexpected("Expected ';' at end of 'throw' statement", mark.peek(0));
    }
  } else if (!mark.peek(0).isOperator(EggTokenizerOperator::Semicolon)) {
    this->unexpected("Expected expression or ';' after 'throw' keyword", mark.peek(0));
  }
  mark.accept(1);
  return std::move(result);
}

std::unique_ptr<IEggSyntaxNode> EggSyntaxParserContext::parseStatementTry() {
  /*
      try-statement ::= 'try' '(' <condition-expression> ')' <compound-statement> <catch-clause>* <finally-clause>?

      catch-clause ::= 'catch' '(' <type> <identifier> ')' <compound-statement>

      finally-clause ::= 'finally' <compound-statement>
  */
  EggSyntaxParserBacktrackMark mark(this->backtrack);
  auto& p0 = mark.peek(0);
  assert(p0.isKeyword(EggTokenizerKeyword::Try));
  EggSyntaxNodeLocation location0(p0);
  mark.advance(1);
  if (!mark.peek(0).isOperator(EggTokenizerOperator::CurlyLeft)) {
    this->unexpected("Expected '{' after 'try' keyword", mark.peek(0));
  }
  auto block = this->parseCompoundStatement();
  auto result = std::make_unique<EggSyntaxNode_Try>(location0, std::move(block));
  size_t catches = 0;
  while (mark.peek(0).isKeyword(EggTokenizerKeyword::Catch)) {
    // Expect 'catch' '(' <type> <identifier> ')' <compound-statement>
    if (!mark.peek(1).isOperator(EggTokenizerOperator::ParenthesisLeft)) {
      this->unexpected("Expected '(' after 'catch' keyword in 'try' statement", mark.peek(1));
    }
    EggSyntaxNodeLocation location(mark.peek(0));
    mark.advance(2);
    auto type = this->parseType("Expected exception type after '(' in 'catch' clause of 'try' statement");
    auto& px = mark.peek(0);
    if (px.kind != EggTokenizerKind::Identifier) {
      this->unexpected("Expected identifier after exception type in 'catch' clause of 'try' statement", px);
    }
    auto name = px.value.s;
    if (!mark.peek(1).isOperator(EggTokenizerOperator::ParenthesisRight)) {
      this->unexpected("Expected ')' after identifier in 'catch' clause of 'try' statement", mark.peek(1));
    }
    if (!mark.peek(2).isOperator(EggTokenizerOperator::CurlyLeft)) {
      this->unexpected("Expected '{' after 'catch' clause of 'try' statement", mark.peek(2));
    }
    mark.advance(2);
    result->addChild(std::make_unique<EggSyntaxNode_Catch>(location, name, std::move(type), this->parseCompoundStatement()));
    catches++;
  }
  if (mark.peek(0).isKeyword(EggTokenizerKeyword::Finally)) {
    // Expect 'finally' <compound-statement>
    if (!mark.peek(1).isOperator(EggTokenizerOperator::CurlyLeft)) {
      this->unexpected("Expected '{' after 'finally' keyword of 'try' statement", mark.peek(1));
    }
    EggSyntaxNodeLocation location(mark.peek(0));
    mark.advance(1);
    result->addChild(std::make_unique<EggSyntaxNode_Finally>(location, this->parseCompoundStatement()));
    if (mark.peek(0).isKeyword(EggTokenizerKeyword::Catch)) {
      this->unexpected("Unexpected 'catch' clause after 'finally' clause in 'try' statement");
    }
    if (mark.peek(0).isKeyword(EggTokenizerKeyword::Finally)) {
      this->unexpected("Unexpected second 'finally' clause in 'try' statement");
    }
  } else if (catches == 0) {
    this->unexpected("Expected at least one 'catch' or 'finally' clause in 'try' statement", mark.peek(0));
  }
  mark.accept(0);
  return result;
}

std::unique_ptr<IEggSyntaxNode> EggSyntaxParserContext::parseStatementType(std::unique_ptr<IEggSyntaxNode>&& type, bool simple) {
  EggSyntaxParserBacktrackMark mark(this->backtrack);
  // Already consumed <type>
  auto& p0 = mark.peek(0);
  EggSyntaxNodeLocation location(p0);
  if (p0.kind == EggTokenizerKind::Identifier) {
    auto& p1 = mark.peek(1);
    if (p1.isOperator(EggTokenizerOperator::Semicolon)) {
      // Found <type> <identifier> ';'
      mark.accept(2);
      return std::make_unique<EggSyntaxNode_Declare>(location, p0.value.s, std::move(type));
    }
    if (p1.isOperator(EggTokenizerOperator::Equal)) {
      // Expect <type> <identifier> = <expression> ';'
      mark.advance(2);
      auto expr = this->parseExpression("Expected expression after assignment '=' operator");
      if (!mark.peek(0).isOperator(EggTokenizerOperator::Semicolon)) {
        this->unexpected("Expected ';' at end of initialization statement");
      }
      mark.accept(1);
      return std::make_unique<EggSyntaxNode_Declare>(location, p0.value.s, std::move(type), std::move(expr));
    }
    if (p1.isOperator(EggTokenizerOperator::ParenthesisLeft)) {
      // Expect <type> <identifier> '(' ... ')' '{' ... '}' with no trailing terminal
      if (simple) {
        this->unexpected("Expected simple statement, but got what looks like a function definition");
      }
      auto result = this->parseStatementFunction(std::move(type), false);
      mark.accept(0);
      return result;
    }
    this->unexpected("Malformed variable declaration or initialization");
  }
  if (p0.isOperator(EggTokenizerOperator::Ellipsis)) {
    // Expect <type> '...' <generator-name> '('
    if ((mark.peek(1).kind == EggTokenizerKind::Identifier) && mark.peek(2).isOperator(EggTokenizerOperator::ParenthesisLeft)) {
      if (simple) {
        this->unexpected("Expected simple statement, but got what looks like a generator definition");
      }
      mark.advance(1);
      auto result = this->parseStatementFunction(std::move(type), true);
      mark.accept(0);
      return result;
    }
  }
  this->unexpected("Expected variable identifier after type", p0);
}

std::unique_ptr<IEggSyntaxNode> EggSyntaxParserContext::parseStatementWhile() {
  /*
      while-statement ::= 'while' '(' <condition-expression> ')' <compound-statement>
  */
  EggSyntaxParserBacktrackMark mark(this->backtrack);
  auto& p0 = mark.peek(0);
  assert(p0.isKeyword(EggTokenizerKeyword::While));
  EggSyntaxNodeLocation location(p0);
  auto expr = this->parseExpressionDeclaration();
  if (!mark.peek(0).isOperator(EggTokenizerOperator::CurlyLeft)) {
    this->unexpected("Expected '{' after ')' in 'while' statement", mark.peek(0));
  }
  auto block = this->parseCompoundStatement();
  mark.accept(0);
  return std::make_unique<EggSyntaxNode_While>(location, std::move(expr), std::move(block));
}

std::unique_ptr<IEggSyntaxNode> EggSyntaxParserContext::parseStatementYield() {
  /*
      yield-statement ::= 'yield' expression ';'
                        | 'yield' '...' expression ';'
  */
  EggSyntaxParserBacktrackMark mark(this->backtrack);
  auto& p0 = mark.peek(0);
  assert(p0.isKeyword(EggTokenizerKeyword::Yield));
  EggSyntaxNodeLocation location(p0);
  std::unique_ptr<IEggSyntaxNode> expr;
  auto& p1 = mark.peek(1);
  if (p1.isOperator(EggTokenizerOperator::Ellipsis)) {
    mark.advance(2);
    auto ellipsis = this->parseExpression("Expected expression after '...' in 'yield' statement");
    expr = std::make_unique<EggSyntaxNode_UnaryOperator>(location, EggTokenizerOperator::Ellipsis, std::move(ellipsis));
  } else {
    mark.advance(1);
    expr = this->parseExpression("Expected expression in 'yield' statement");
  }
  auto& px = mark.peek(0);
  if (!px.isOperator(EggTokenizerOperator::Semicolon)) {
    this->unexpected("Expected ';' at end of 'yield' statement", px);
  }
  mark.accept(1);
  return std::make_unique<EggSyntaxNode_Yield>(location, std::move(expr));
}

std::unique_ptr<IEggSyntaxNode> EggSyntaxParserContext::parseType(const char* expected) {
  EggSyntaxParserBacktrackMark mark(this->backtrack);
  auto& p0 = mark.peek(0);
  EggSyntaxNodeLocation location(p0);
  if (expected == nullptr) {
    // Allow 'var'
    if (p0.isKeyword(EggTokenizerKeyword::Var)) {
      // Don't allow 'var?'
      mark.accept(1);
      return std::make_unique<EggSyntaxNode_Type>(location, nullptr);
    }
  }
  egg::ovum::Type type{ egg::ovum::Type::Void };
  if (this->parseTypeExpression(type)) {
    mark.accept(0);
    return std::make_unique<EggSyntaxNode_Type>(location, type.get());
  }
  if (expected != nullptr) {
    this->unexpected(expected, p0);
  }
  return nullptr;
}

bool EggSyntaxParserContext::parseTypeExpression(egg::ovum::Type& type) {
  /*
      type-expression ::= type-union-expression

      type-union-expression ::= type-nullable-expression
                              | type-union-expression '|' type-nullable-expression
  */
  if (this->parseTypePostfixExpression(type)) {
    EggSyntaxParserBacktrackMark mark(this->backtrack);
    egg::ovum::Type other{ egg::ovum::Type::Void };
    while (mark.peek(0).isOperator(EggTokenizerOperator::Bar)) {
      mark.advance(1);
      if (!this->parseTypePostfixExpression(other)) {
        this->unexpected("Expected type to follow '|' in type expression", mark.peek(0));
      }
      type = this->factory->createUnion(type, other);
    }
    mark.accept(0);
    return true;
  }
  return false;
}

bool EggSyntaxParserContext::parseTypePostfixExpression(egg::ovum::Type& type) {
  /*
      type-nullable-expression ::= type-postfix-expression
                                 | type-postfix-expression '?'

      type-postfix-expression ::= type-primary-expression
                                | type-nullable-expression '*'
                                | type-nullable-expression '[' type-expression? ']'
                                | type-nullable-expression '(' function-parameter-list? ')'
  */
  // TODO arrays, maps, etc.
  if (this->parseTypePrimaryExpression(type)) {
    auto nullabled = false;
    EggSyntaxParserBacktrackMark mark(this->backtrack);
    for (;;) {
      auto& p0 = mark.peek(0);
      if (p0.isOperator(EggTokenizerOperator::Query)) {
        // Union 'type' with 'null'
        if (nullabled) {
          this->unexpected("Redundant repetition of '?' in type expression");
        }
        mark.advance(1);
        type = this->factory->addNull(type);
        nullabled = true;
        continue;
      }
      nullabled = false;
      if (p0.isOperator(EggTokenizerOperator::Star)) {
        // Pointer reference to 'type'
        mark.advance(1);
        auto modifiability = egg::ovum::Modifiability::Read | egg::ovum::Modifiability::Write | egg::ovum::Modifiability::Mutate; // TODO
        type = this->factory->createPointer(type, modifiability);
        continue;
      }
      if (p0.isOperator(EggTokenizerOperator::ParenthesisLeft)) {
        // A function reference like 'type(int a, ...)'
        type = this->parseTypePostfixFunction(type);
        continue;
      }
      break;
    }
    mark.accept(0);
    return true;
  }
  return false;
}

egg::ovum::Type EggSyntaxParserContext::parseTypePostfixFunction(const egg::ovum::Type& rettype) {
  /*
      function-parameter-list ::= function-parameter
                                | function-parameter-list ',' function-parameter

      function-parameter ::= attribute* type-expression variable-identifier? ( '=' 'null' )?
                           | attribute* '...' type-expression '[' ']' variable-identifier
  */
  EggSyntaxParserBacktrackMark mark(this->backtrack);
  // cppcheck-suppress assertWithSideEffect
  assert(mark.peek(0).isOperator(EggTokenizerOperator::ParenthesisLeft));
  mark.advance(1);
  auto builder = this->factory->createFunctionBuilder(rettype, egg::ovum::String(), "Function");
  for (size_t index = 0; !mark.peek(0).isOperator(EggTokenizerOperator::ParenthesisRight); ++index) {
    egg::ovum::Type ptype{ egg::ovum::Type::Void };
    if (!this->parseTypeExpression(ptype)) {
      this->unexpected("Expected parameter type in function type declaration", mark.peek(0));
    }
    egg::ovum::String pname;
    auto& p1 = mark.peek(0);
    if (p1.kind == EggTokenizerKind::Identifier) {
      // Skip the optional parameter name
      pname = p1.value.s;
      mark.advance(1);
    }
    auto flags = egg::ovum::IFunctionSignatureParameter::Flags::Required;
    if (mark.peek(0).isOperator(EggTokenizerOperator::Equal)) {
      auto& p2 = mark.peek(1);
      if (!p2.isKeyword(EggTokenizerKeyword::Null)) {
        if (pname.empty()) {
          this->unexpected("Expected 'null' as default value for parameter index " + std::to_string(index), p2);
        } else {
          this->unexpected("Expected 'null' as default value for parameter '" + pname.toUTF8() + "'", p2);
        }
      }
      mark.advance(2);
      flags = egg::ovum::IFunctionSignatureParameter::Flags::None;
    }
    builder->addPositionalParameter(ptype, pname, flags);
    auto& p3 = mark.peek(0);
    if (p3.isOperator(EggTokenizerOperator::Comma)) {
      mark.advance(1);
    } else if (!p3.isOperator(EggTokenizerOperator::ParenthesisRight)) {
      this->unexpected("Expected ',' or ')' after parameter in function type declaration", p3);
    }
  }
  mark.accept(1); // Skip ')'
  return builder->build();
}

bool EggSyntaxParserContext::parseTypePrimaryExpression(egg::ovum::Type& type) {
  /*
      type-primary-expression ::= 'any'
                                | 'void'
                                | 'null'
                                | 'bool'
                                | 'int'
                                | 'float'
                                | 'string'
                                | 'object'
                                | 'type' object-value
                                | type-identifier
                                | '(' type-expression ')'

      type-identifier ::= identifier
                        | identifier '<' type-list '>'
  */
  // TODO generics
  // TODO type { ... }
  // TODO type-identifier
  // TODO '(' type - expression ')'
  EggSyntaxParserBacktrackMark mark(this->backtrack);
  auto& p0 = mark.peek(0);
  if (p0.isOperator(EggTokenizerOperator::ParenthesisLeft)) {
    mark.advance(1);
    if (this->parseTypeExpression(type)) {
      auto& px = mark.peek(0);
      if (px.isOperator(EggTokenizerOperator::ParenthesisRight)) {
        mark.accept(1);
        return true;
      }
    }
    return false;
  }
  auto flags = keywordToFlags(p0);
  if (flags != egg::ovum::ValueFlags::None) {
    mark.accept(1);
    type = this->factory->createSimple(flags);
    return true;
  }
  return false;
}

std::shared_ptr<egg::yolk::IEggSyntaxParser> egg::yolk::EggParserFactory::createModuleSyntaxParser(egg::ovum::TypeFactory& factory) {
  return std::make_shared<EggSyntaxParserModule>(factory);
}

std::shared_ptr<egg::yolk::IEggSyntaxParser> egg::yolk::EggParserFactory::createStatementSyntaxParser(egg::ovum::TypeFactory& factory) {
  return std::make_shared<EggSyntaxParserStatement>(factory);
}

std::shared_ptr<egg::yolk::IEggSyntaxParser> egg::yolk::EggParserFactory::createExpressionSyntaxParser(egg::ovum::TypeFactory& factory) {
  return std::make_shared<EggSyntaxParserExpression>(factory);
}
