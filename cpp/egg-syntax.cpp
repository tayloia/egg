#include "yolk.h"
#include "lexers.h"
#include "egg-tokenizer.h"
#include "egg-syntax.h"
#include "egg-parser.h"

namespace {
  using namespace egg::yolk;

  class ParserDump {
  private:
    std::ostream* os;
  public:
    ParserDump(std::ostream& os, const char* text)
      :os(&os) {
      *this->os << '(' << text;
    }
    ~ParserDump() {
      *this->os << ')';
    }
    ParserDump& add(const std::string& text) {
      *this->os << ' ' << '\'' << text << '\'';
      return *this;
    }
    ParserDump& add(EggTokenizerOperator op) {
      *this->os << ' ' << '\'' << EggTokenizerValue::getOperatorString(op) << '\'';
      return *this;
    }
    ParserDump& add(const std::unique_ptr<IEggSyntaxNode>& child) {
      *this->os << ' ';
      child->dump(*this->os);
      return *this;
    }
    ParserDump& add(const std::vector<std::unique_ptr<IEggSyntaxNode>>& children) {
      for (auto& child : children) {
        this->add(child);
      }
      return *this;
    }
    template<size_t N>
    ParserDump& add(const std::unique_ptr<IEggSyntaxNode> (&children)[N]) {
      for (auto& child : children) {
        this->add(child);
      }
      return *this;
    }
  };
}

void egg::yolk::EggSyntaxNode_Empty::dump(std::ostream& os) const {
  ParserDump(os, "");
}

void egg::yolk::EggSyntaxNode_Module::dump(std::ostream& os) const {
  ParserDump(os, "module").add(this->child);
}

void egg::yolk::EggSyntaxNode_Block::dump(std::ostream& os) const {
  ParserDump(os, "block").add(this->child);
}

void egg::yolk::EggSyntaxNode_Type::dump(std::ostream& os) const {
  ParserDump(os, "type").add(EggSyntaxNode_Type::tagToString(this->tag));
}

void egg::yolk::EggSyntaxNode_Declare::dump(std::ostream& os) const {
  ParserDump(os, "declare").add(this->name).add(this->child);
}

void egg::yolk::EggSyntaxNode_Assignment::dump(std::ostream& os) const {
  ParserDump(os, "assign").add(this->op).add(this->child);
}

void egg::yolk::EggSyntaxNode_Mutate::dump(std::ostream& os) const {
  ParserDump(os, "mutate").add(this->op).add(this->child);
}

void egg::yolk::EggSyntaxNode_Break::dump(std::ostream& os) const {
  ParserDump(os, "break");
}

void egg::yolk::EggSyntaxNode_Case::dump(std::ostream& os) const {
  ParserDump(os, "case").add(this->child);
}

void egg::yolk::EggSyntaxNode_Catch::dump(std::ostream& os) const {
  ParserDump(os, "catch").add(this->name).add(this->child);
}

void egg::yolk::EggSyntaxNode_Continue::dump(std::ostream& os) const {
  ParserDump(os, "continue");
}

void egg::yolk::EggSyntaxNode_Default::dump(std::ostream& os) const {
  ParserDump(os, "default");
}

void egg::yolk::EggSyntaxNode_Do::dump(std::ostream& os) const {
  ParserDump(os, "do").add(this->child);
}

void egg::yolk::EggSyntaxNode_If::dump(std::ostream& os) const {
  ParserDump(os, "if").add(this->child);
}

void egg::yolk::EggSyntaxNode_Finally::dump(std::ostream& os) const {
  ParserDump(os, "finally").add(this->child);
}

void egg::yolk::EggSyntaxNode_For::dump(std::ostream& os) const {
  ParserDump(os, "for").add(this->child);
}

void egg::yolk::EggSyntaxNode_Foreach::dump(std::ostream& os) const {
  ParserDump(os, "foreach").add(this->child);
}

void egg::yolk::EggSyntaxNode_Return::dump(std::ostream& os) const {
  ParserDump(os, "return").add(this->child);
}

void egg::yolk::EggSyntaxNode_Switch::dump(std::ostream& os) const {
  ParserDump(os, "switch").add(this->child);
}

void egg::yolk::EggSyntaxNode_Throw::dump(std::ostream& os) const {
  ParserDump(os, "throw").add(this->child);
}

void egg::yolk::EggSyntaxNode_Try::dump(std::ostream& os) const {
  ParserDump(os, "try").add(this->child);
}

void egg::yolk::EggSyntaxNode_Using::dump(std::ostream& os) const {
  ParserDump(os, "using").add(this->child);
}

void egg::yolk::EggSyntaxNode_While::dump(std::ostream& os) const {
  ParserDump(os, "while").add(this->child);
}

void egg::yolk::EggSyntaxNode_Yield::dump(std::ostream& os) const {
  ParserDump(os, "yield").add(this->child);
}

void egg::yolk::EggSyntaxNode_UnaryOperator::dump(std::ostream& os) const {
  ParserDump(os, "unary").add(this->op).add(this->child);
}

void egg::yolk::EggSyntaxNode_BinaryOperator::dump(std::ostream& os) const {
  ParserDump(os, "binary").add(this->op).add(this->child);
}

void egg::yolk::EggSyntaxNode_TernaryOperator::dump(std::ostream& os) const {
  ParserDump(os, "ternary").add(this->child);
}

void egg::yolk::EggSyntaxNode_Call::dump(std::ostream& os) const {
  ParserDump(os, "call").add(this->child);
}

void egg::yolk::EggSyntaxNode_Named::dump(std::ostream& os) const {
  ParserDump(os, "named").add(this->name).add(this->child);
}

void egg::yolk::EggSyntaxNode_Identifier::dump(std::ostream& os) const {
  ParserDump(os, "identifier").add(this->name);
}

void egg::yolk::EggSyntaxNode_Literal::dump(std::ostream& os) const {
  switch (this->kind) {
  case EggTokenizerKind::Integer:
    ParserDump(os, ("literal int " + this->value.s).c_str());
    break;
  case EggTokenizerKind::Float:
    ParserDump(os, ("literal float " + this->value.s).c_str());
    break;
  case EggTokenizerKind::String:
    ParserDump(os, "literal string").add(this->value.s);
    break;
  case EggTokenizerKind::Keyword:
    if (this->value.k == EggTokenizerKeyword::Null) {
      ParserDump(os, "literal null");
    } else if (this->value.k == EggTokenizerKeyword::False) {
      ParserDump(os, "literal bool false");
    } else if (this->value.k == EggTokenizerKeyword::True) {
      ParserDump(os, "literal bool true");
    } else {
      ParserDump(os, "literal keyword unknown");
    }
    break;
  case EggTokenizerKind::Operator:
  case EggTokenizerKind::Identifier:
  case EggTokenizerKind::Attribute:
  case EggTokenizerKind::EndOfFile:
  default:
    ParserDump(os, "literal unknown");
    break;
  }
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

egg::yolk::EggTokenizerKeyword egg::yolk::EggSyntaxNode_Using::keyword() const {
  return EggTokenizerKeyword::Using;
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

std::string egg::yolk::EggSyntaxNodeBase::token() const {
  return std::string();
}

std::string egg::yolk::EggSyntaxNode_Type::token() const {
  return EggSyntaxNode_Type::tagToString(this->tag);
}

std::string egg::yolk::EggSyntaxNode_Declare::token() const {
  return this->name;
}

std::string egg::yolk::EggSyntaxNode_Assignment::token() const {
  return EggTokenizerValue::getOperatorString(this->op);
}

std::string egg::yolk::EggSyntaxNode_Mutate::token() const {
  return EggTokenizerValue::getOperatorString(this->op);
}

std::string egg::yolk::EggSyntaxNode_Catch::token() const {
  return this->name;
}

std::string egg::yolk::EggSyntaxNode_UnaryOperator::token() const {
  return EggTokenizerValue::getOperatorString(this->op);
}

std::string egg::yolk::EggSyntaxNode_BinaryOperator::token() const {
  return EggTokenizerValue::getOperatorString(this->op);
}

std::string egg::yolk::EggSyntaxNode_TernaryOperator::token() const {
  return "?:";
}

std::string egg::yolk::EggSyntaxNode_Named::token() const {
  return this->name;
}

std::string egg::yolk::EggSyntaxNode_Identifier::token() const {
  return this->name;
}

std::string egg::yolk::EggSyntaxNode_Literal::token() const {
  return this->value.s;
}

bool egg::yolk::EggSyntaxNode_Literal::negate() {
  // Try to negate (times-minus-one) as literal value
  if (this->kind == EggTokenizerKind::Integer) {
    auto negative = -this->value.i;
    if (negative <= 0) {
      this->value.i = negative;
      this->value.s = "-" + this->value.s;
      return true;
    }
  } else if (this->kind == EggTokenizerKind::Float) {
    this->value.f = -this->value.f;
    this->value.s = "-" + this->value.s;
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
      return *(this->upcoming.begin() + ptrdiff_t(index));
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
    std::string resource() const {
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
      this->tokenizer->next(this->upcoming.emplace_back());
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
    std::string resource() const {
      return this->lookahead.resource();
    }
  private:
    size_t mark() {
      // Called by EggSyntaxParserBacktrackMark ctor
      return this->cursor;
    }
    void abandon(size_t previous) {
      // Called by EggSyntaxParserBacktrackMark dtor
      assert(previous <= this->cursor);
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
    EggSyntaxParserBacktrack backtrack;
  public:
    explicit EggSyntaxParserContext(IEggTokenizer& tokenizer)
      : backtrack(tokenizer) {
    }
    void unexpected(const std::string& message) {
      auto& item = this->backtrack.peek(0);
      throw SyntaxException(message, this->backtrack.resource(), item);
    }
    void unexpected(const std::string& expected, const EggTokenizerItem& item) {
      auto token = item.to_string();
      throw SyntaxException(expected + ", not " + token, this->backtrack.resource(), item, token);
    }
    void parseExpressionList(std::function<void(std::unique_ptr<IEggSyntaxNode>&& node)> adder, const char* expected0, const char* expected1);
    void parseParameterList(std::function<void(std::unique_ptr<IEggSyntaxNode>&& node)> adder);
    void parseEndOfFile(const char* expected);
    std::unique_ptr<IEggSyntaxNode> parseCompoundStatement();
    std::unique_ptr<IEggSyntaxNode> parseConditionGuarded(const char* expected);
    std::unique_ptr<IEggSyntaxNode> parseConditionUnguarded(const char* expected);
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
    std::unique_ptr<IEggSyntaxNode> parseExpressionPrimary(const char* expected);
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
    std::unique_ptr<IEggSyntaxNode> parseStatementIf();
    std::unique_ptr<IEggSyntaxNode> parseStatementReturn();
    std::unique_ptr<IEggSyntaxNode> parseStatementSwitch();
    std::unique_ptr<IEggSyntaxNode> parseStatementThrow();
    std::unique_ptr<IEggSyntaxNode> parseStatementTry();
    std::unique_ptr<IEggSyntaxNode> parseStatementType(std::unique_ptr<IEggSyntaxNode>&& type, EggTokenizerOperator terminal);
    std::unique_ptr<IEggSyntaxNode> parseStatementUsing();
    std::unique_ptr<IEggSyntaxNode> parseStatementWhile();
    std::unique_ptr<IEggSyntaxNode> parseStatementYield();
    std::unique_ptr<IEggSyntaxNode> parseType(const char* expected);
    std::unique_ptr<IEggSyntaxNode> parseTypeSimple(egg::lang::Discriminator tag);
    std::unique_ptr<IEggSyntaxNode> parseTypeDefinition();
  };

  class EggSyntaxParserModule : public IEggSyntaxParser {
  public:
    virtual std::shared_ptr<IEggSyntaxNode> parse(IEggTokenizer& tokenizer) override {
      EggSyntaxParserContext context(tokenizer);
      return context.parseModule();
    }
  };

  class EggSyntaxParserStatement : public IEggSyntaxParser {
  public:
    virtual std::shared_ptr<IEggSyntaxNode> parse(IEggTokenizer& tokenizer) override {
      EggSyntaxParserContext context(tokenizer);
      auto result = context.parseStatement();
      assert(result != nullptr);
      context.parseEndOfFile("Expected end of input after statement");
      return result;
    }
  };

  class EggSyntaxParserExpression : public IEggSyntaxParser {
  public:
    virtual std::shared_ptr<IEggSyntaxNode> parse(IEggTokenizer& tokenizer) override {
      EggSyntaxParserContext context(tokenizer);
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
    case EggTokenizerKeyword::Function:
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
    case EggTokenizerKeyword::Typedef:
      return this->parseTypeDefinition();
    case EggTokenizerKeyword::Using:
      return this->parseStatementUsing();
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
  if (type != nullptr) {
    return this->parseStatementType(std::move(type), EggTokenizerOperator::Semicolon);
  }
  this->unexpected("Unexpected " + p0.to_string()); // TODO
  return nullptr;
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
  auto type = this->parseType(nullptr);
  if (type == nullptr) {
    this->unexpected(expected, p0);
  }
  return this->parseStatementType(std::move(type), terminal);
}

std::unique_ptr<IEggSyntaxNode> EggSyntaxParserContext::parseCompoundStatement() {
  /*
      compound-statement ::= '{' statement* '}'
  */
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
      auto exprTrue = this->parseExpression("Expected expression after '?' of ternary operator '?:'");
      if (!mark.peek(0).isOperator(EggTokenizerOperator::Colon)) {
        this->unexpected("Expected ':' as part of ternary operator '?:'", mark.peek(0));
      }
      location.setLocationEnd(mark.peek(0), 1);
      mark.advance(1);
      auto exprFalse = this->parseExpression("Expected expression after ':' of ternary operator '?:'");
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
    if (token->isOperator(EggTokenizerOperator::Plus)) {
      expected = "Expected expression after infix '+' operator";
    } else if (token->isOperator(EggTokenizerOperator::Minus)) {
      expected = "Expected expression after infix '-' operator";
    } else if (token->isOperator(EggTokenizerOperator::PlusPlus)) {
      // We don't handle the special case of 'a++b' or 'a++1' because we have no unary plus operator
      this->unexpected("Unexpected '+' after infix '+' operator");
    } else if (token->isOperator(EggTokenizerOperator::MinusMinus)) {
      // Handle the special case of 'a--b' or 'a--1'
      EggSyntaxNodeLocation location(*token, 1);
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
    expr = std::make_unique<EggSyntaxNode_BinaryOperator>(EggSyntaxNodeLocation(*token, 1), token->value.o, std::move(lhs), std::move(rhs));
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
      EggSyntaxNodeLocation location(p0, 0);
      EggSyntaxParserBacktrackMark mark(this->backtrack);
      auto call = std::make_unique<EggSyntaxNode_Call>(location, std::move(expr));
      this->parseParameterList([&call](auto&& node) { call->addChild(std::move(node)); });
      call->setLocationEnd(mark.peek(0), 1);
      mark.accept(1); // skip ')'
      expr = std::move(call);
    } else if (p0.isOperator(EggTokenizerOperator::Dot)) {
      // Expect <expression> '.' <identifer>
      EggSyntaxNodeLocation location(p0, 1);
      EggSyntaxParserBacktrackMark mark(this->backtrack);
      auto& p1 = mark.peek(1);
      if (p1.kind != EggTokenizerKind::Identifier) {
        this->unexpected("Expected field name to follow '.' operator", p1);
      }
      auto field = std::make_unique<EggSyntaxNode_Identifier>(EggSyntaxNodeLocation(p1), p1.value.s);
      mark.accept(2);
      expr = std::make_unique<EggSyntaxNode_BinaryOperator>(location, EggTokenizerOperator::Dot, std::move(expr), std::move(field));
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
        this->unexpected("Expected field name to follow '?.' operator", p2);
      }
      auto field = std::make_unique<EggSyntaxNode_Identifier>(EggSyntaxNodeLocation(p2), p2.value.s);
      mark.accept(3);
      expr = std::make_unique<EggSyntaxNode_BinaryOperator>(location, EggTokenizerOperator::Query, std::move(expr), std::move(field));
    } else {
      // No postfix operator, return just the expression
      break;
    }
  }
  return std::move(expr);
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
  switch (p0.kind) {
  case EggTokenizerKind::Integer:
  case EggTokenizerKind::Float:
  case EggTokenizerKind::String:
    mark.accept(1);
    return std::make_unique<EggSyntaxNode_Literal>(EggSyntaxNodeLocation(p0), p0.kind, p0.value);
  case EggTokenizerKind::Identifier:
    mark.accept(1);
    return std::make_unique<EggSyntaxNode_Identifier>(EggSyntaxNodeLocation(p0), p0.value.s);
  case EggTokenizerKind::Keyword:
    if ((p0.value.k == EggTokenizerKeyword::Null) || (p0.value.k == EggTokenizerKeyword::False) || (p0.value.k == EggTokenizerKeyword::True)) {
      mark.accept(1);
      return std::make_unique<EggSyntaxNode_Literal>(EggSyntaxNodeLocation(p0), p0.kind, p0.value);
    }
    /* DROP THROUGH */
  case EggTokenizerKind::Operator:
  case EggTokenizerKind::Attribute:
  case EggTokenizerKind::EndOfFile:
  default:
    if (expected) {
      this->unexpected(expected, p0);
    }
    return nullptr;
  }
}

std::unique_ptr<IEggSyntaxNode> EggSyntaxParserContext::parseConditionGuarded(const char* expected) {
  /*
      guarded-condition ::= expression -- TODO
  */
  auto expr = this->parseExpression(expected);
  assert(expr != nullptr);
  return expr;
}

std::unique_ptr<IEggSyntaxNode> EggSyntaxParserContext::parseConditionUnguarded(const char* expected) {
  /*
      unguarded-condition ::= expression
  */
  auto expr = this->parseExpression(expected);
  assert(expr != nullptr);
  return expr;
}

void EggSyntaxParserContext::parseExpressionList(std::function<void(std::unique_ptr<IEggSyntaxNode>&& node)> adder, const char* expected0, const char* expected1) {
  /*
      expression-list ::= expression
                        | expression-list ',' expression
  */
  EggSyntaxParserBacktrackMark mark(this->backtrack);
  auto expr = this->parseExpression(expected0);
  if (expr != nullptr) {
    // There's at least one expression
    adder(std::move(expr));
    while (mark.peek(0).isOperator(EggTokenizerOperator::Comma)) {
      mark.advance(1);
      expr = this->parseExpression(expected1);
      assert(expr != nullptr);
      adder(std::move(expr));
    }
    mark.accept(0);
  }
}

void EggSyntaxParserContext::parseParameterList(std::function<void(std::unique_ptr<IEggSyntaxNode>&& node)> adder) {
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
  assert(mark.peek(0).isOperator(EggTokenizerOperator::ParenthesisLeft));
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
        EggSyntaxNodeLocation location(*p0);
        location.setLocationEnd(mark.peek(1), 1);
        mark.advance(2);
        auto expr = this->parseExpression("Expected expression for named function call parameter value");
        auto named = std::make_unique<EggSyntaxNode_Named>(location, p0->value.s, std::move(expr));
        adder(std::move(named));
      } else {
        // Expect <expression>
        auto expr = this->parseExpression("Expected expression for function call parameter value");
        adder(std::move(expr));
      }
      p0 = &mark.peek(0);
    } while (p0->isOperator(EggTokenizerOperator::Comma));
    if (!p0->isOperator(EggTokenizerOperator::ParenthesisRight)) {
      this->unexpected("Expected ')' at end of function call parameter list", *p0);
    }
    mark.accept(0);
  }
  assert(mark.peek(0).isOperator(EggTokenizerOperator::ParenthesisRight));
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
                            | '&='
                            | '^='
                            | '|='
  */
  EggSyntaxParserBacktrackMark mark(this->backtrack);
  auto& p0 = mark.peek(0);
  const char* expected = nullptr;
  if (p0.isOperator(EggTokenizerOperator::Equal)) {
    expected = "Expected expression after assignment '=' operator";
  } else if (p0.isOperator(EggTokenizerOperator::StarEqual)) {
    expected = "Expected expression after assignment '*=' operator";
  } else if (p0.isOperator(EggTokenizerOperator::SlashEqual)) {
    expected = "Expected expression after assignment '/=' operator";
  } else if (p0.isOperator(EggTokenizerOperator::PercentEqual)) {
    expected = "Expected expression after assignment '%=' operator";
  } else if (p0.isOperator(EggTokenizerOperator::PlusEqual)) {
    expected = "Expected expression after assignment '+=' operator";
  } else if (p0.isOperator(EggTokenizerOperator::MinusEqual)) {
    expected = "Expected expression after assignment '-=' operator";
  } else if (p0.isOperator(EggTokenizerOperator::ShiftLeftEqual)) {
    expected = "Expected expression after assignment '<<=' operator";
  } else if (p0.isOperator(EggTokenizerOperator::ShiftRightEqual)) {
    expected = "Expected expression after assignment '>>=' operator";
  } else if (p0.isOperator(EggTokenizerOperator::ShiftRightUnsignedEqual)) {
    expected = "Expected expression after assignment '>>>=' operator";
  } else if (p0.isOperator(EggTokenizerOperator::AmpersandEqual)) {
    expected = "Expected expression after assignment '&=' operator";
  } else if (p0.isOperator(EggTokenizerOperator::CaretEqual)) {
    expected = "Expected expression after assignment '^=' operator";
  } else if (p0.isOperator(EggTokenizerOperator::BarEqual)) {
    expected = "Expected expression after assignment '|=' operator";
  } else {
    this->unexpected("Expected assignment operator after expression", p0);
  }
  mark.advance(1);
  auto rhs = this->parseExpression(expected);
  auto& px = mark.peek(0);
  if (!px.isOperator(terminal)) {
    this->unexpected("Expected '" + EggTokenizerValue::getOperatorString(terminal) + "' after assignment statement", px);
  }
  mark.accept(1);
  return std::make_unique<EggSyntaxNode_Assignment>(EggSyntaxNodeLocation(p0), p0.value.o, std::move(lhs), std::move(rhs));
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
  this->backtrack.advance(2);
  return std::make_unique<EggSyntaxNode_Break>(EggSyntaxNodeLocation(p0));
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
  mark.accept(1);
  return std::make_unique<EggSyntaxNode_Case>(EggSyntaxNodeLocation(p0), std::move(expr));
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
  this->backtrack.advance(2);
  return std::make_unique<EggSyntaxNode_Continue>(EggSyntaxNodeLocation(p0));
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
  mark.accept(1);
  return std::make_unique<EggSyntaxNode_Mutate>(EggSyntaxNodeLocation(p0), op, std::move(expr));
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
  this->backtrack.advance(2);
  return std::make_unique<EggSyntaxNode_Default>(EggSyntaxNodeLocation(p0));
}

std::unique_ptr<IEggSyntaxNode> EggSyntaxParserContext::parseStatementDo() {
  /*
      do-statement ::= 'do' <compound-statement> 'while' '(' <expression> ')' ';'
  */
  EggSyntaxParserBacktrackMark mark(this->backtrack);
  auto& p0 = mark.peek(0);
  assert(p0.isKeyword(EggTokenizerKeyword::Do));
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
  auto expr = this->parseConditionUnguarded("Expected condition expression after 'while (' in 'do' statement");
  if (!mark.peek(0).isOperator(EggTokenizerOperator::ParenthesisRight)) {
    this->unexpected("Expected ')' after 'do' condition expression", mark.peek(0));
  }
  if (!mark.peek(1).isOperator(EggTokenizerOperator::Semicolon)) {
    this->unexpected("Expected ';' after ')' at end of 'do' statement", mark.peek(1));
  }
  mark.accept(2);
  return std::make_unique<EggSyntaxNode_Do>(EggSyntaxNodeLocation(p0), std::move(expr), std::move(block));
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
  mark.advance(2);
  std::unique_ptr<IEggSyntaxNode> pre, cond, post;
  if (mark.peek(0).isOperator(EggTokenizerOperator::Semicolon)) {
    EggSyntaxNodeLocation location(mark.peek(0), 1);
    mark.advance(1); // skip ';'
    pre = std::make_unique<EggSyntaxNode_Empty>(location);
  } else {
    pre = this->parseStatementSimple("Expected simple statement after '(' in 'for' statement", EggTokenizerOperator::Semicolon);
  }
  if (mark.peek(0).isOperator(EggTokenizerOperator::Semicolon)) {
    EggSyntaxNodeLocation location(mark.peek(0), 1);
    mark.advance(1); // skip ';'
    cond = std::make_unique<EggSyntaxNode_Empty>(location);
  } else {
    cond = this->parseConditionUnguarded("Expected condition expression as second clause in 'for' statement");
    if (!mark.peek(0).isOperator(EggTokenizerOperator::Semicolon)) {
      this->unexpected("Expected ';' after condition expression of 'for' statement", mark.peek(0));
    }
    mark.advance(1); // skip ';'
  }
  if (mark.peek(0).isOperator(EggTokenizerOperator::ParenthesisRight)) {
    EggSyntaxNodeLocation location(mark.peek(0), 1);
    mark.advance(1); // skip ')'
    post = std::make_unique<EggSyntaxNode_Empty>(location);
  } else {
    post = this->parseStatementSimple("Expected simple statement as third clause in 'for' statement", EggTokenizerOperator::ParenthesisRight);
  }
  if (!mark.peek(0).isOperator(EggTokenizerOperator::CurlyLeft)) {
    this->unexpected("Expected '{' after ')' in 'for' statement", mark.peek(0));
  }
  auto block = this->parseCompoundStatement();
  mark.accept(0);
  return std::make_unique<EggSyntaxNode_For>(EggSyntaxNodeLocation(p0), std::move(pre), std::move(cond), std::move(post), std::move(block));
}

std::unique_ptr<IEggSyntaxNode> EggSyntaxParserContext::parseStatementForeach() {
  /*
      foreach-statement ::= 'for' '(' variable-definition-type? variable-identifier ':' expression ')' compound-statement
  */
  EggSyntaxParserBacktrackMark mark(this->backtrack);
  assert(mark.peek(0).isKeyword(EggTokenizerKeyword::For));
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

std::unique_ptr<IEggSyntaxNode> EggSyntaxParserContext::parseStatementIf() {
  /*
      if-statement ::= 'if' '(' <condition-expression> ')' <compound-statement> <else-clause>?

      else-clause ::= 'else' <compound-statement>
  */
  EggSyntaxParserBacktrackMark mark(this->backtrack);
  auto& p0 = mark.peek(0);
  assert(p0.isKeyword(EggTokenizerKeyword::If));
  if (!mark.peek(1).isOperator(EggTokenizerOperator::ParenthesisLeft)) {
    this->unexpected("Expected '(' after 'if' keyword", mark.peek(1));
  }
  mark.advance(2);
  auto expr = this->parseConditionGuarded("Expected condition expression after '(' in 'if' statement");
  if (!mark.peek(0).isOperator(EggTokenizerOperator::ParenthesisRight)) {
    this->unexpected("Expected ')' after 'if' condition expression", mark.peek(0));
  }
  mark.advance(1);
  if (!mark.peek(0).isOperator(EggTokenizerOperator::CurlyLeft)) {
    this->unexpected("Expected '{' after ')' in 'if' statement", mark.peek(0));
  }
  auto block = this->parseCompoundStatement();
  auto result = std::make_unique<EggSyntaxNode_If>(EggSyntaxNodeLocation(p0), std::move(expr), std::move(block));
  if (mark.peek(0).isKeyword(EggTokenizerKeyword::Else)) {
    mark.advance(1);
    if (!mark.peek(0).isOperator(EggTokenizerOperator::CurlyLeft)) {
      this->unexpected("Expected '{' after 'else' in 'if' statement", mark.peek(0));
    }
    result->addChild(this->parseCompoundStatement());
  }
  mark.accept(0);
  return result;
}

std::unique_ptr<IEggSyntaxNode> EggSyntaxParserContext::parseStatementReturn() {
  /*
      return-statement ::= 'return' expression-list? ';'
                         | 'return' '...' expression ';'
  */
  EggSyntaxParserBacktrackMark mark(this->backtrack);
  auto& p0 = mark.peek(0);
  assert(p0.isKeyword(EggTokenizerKeyword::Return));
  auto results = std::make_unique<EggSyntaxNode_Return>(EggSyntaxNodeLocation(p0));
  auto& p1 = mark.peek(1);
  if (p1.isOperator(EggTokenizerOperator::Ellipsis)) {
    mark.advance(2);
    auto expr = this->parseExpression("Expected expression after '...' in 'return' statement");
    auto ellipsis = std::make_unique<EggSyntaxNode_UnaryOperator>(EggSyntaxNodeLocation(p1), EggTokenizerOperator::Ellipsis, std::move(expr));
    results->addChild(std::move(ellipsis));
  } else {
    mark.advance(1);
    this->parseExpressionList([&results](auto&& node) { results->addChild(std::move(node)); }, nullptr, "Expected expression after ',' in 'return' statement");
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
      switch-statement ::= 'switch' '(' <expression> ')' <compound-statement>
  */
  EggSyntaxParserBacktrackMark mark(this->backtrack);
  auto& p0 = mark.peek(0);
  assert(p0.isKeyword(EggTokenizerKeyword::Switch));
  if (!mark.peek(1).isOperator(EggTokenizerOperator::ParenthesisLeft)) {
    this->unexpected("Expected '(' after 'switch' keyword", mark.peek(1));
  }
  mark.advance(2);
  auto expr = this->parseConditionGuarded("Expected condition expression after '(' in 'switch' statement");
  if (!mark.peek(0).isOperator(EggTokenizerOperator::ParenthesisRight)) {
    this->unexpected("Expected ')' after 'switch' condition expression", mark.peek(0));
  }
  mark.advance(1);
  if (!mark.peek(0).isOperator(EggTokenizerOperator::CurlyLeft)) {
    this->unexpected("Expected '{' after ')' in 'switch' statement", mark.peek(0));
  }
  auto block = this->parseCompoundStatement();
  mark.accept(0);
  return std::make_unique<EggSyntaxNode_Switch>(EggSyntaxNodeLocation(p0), std::move(expr), std::move(block));
}

std::unique_ptr<IEggSyntaxNode> EggSyntaxParserContext::parseStatementThrow() {
  /*
      throw-statement ::= 'throw' expression? ';'
  */
  EggSyntaxParserBacktrackMark mark(this->backtrack);
  auto& p0 = mark.peek(0);
  assert(p0.isKeyword(EggTokenizerKeyword::Throw));
  mark.advance(1);
  auto expr = this->parseExpression(nullptr);
  auto result = std::make_unique<EggSyntaxNode_Throw>(EggSyntaxNodeLocation(p0));
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
      try-statement ::= 'if' '(' <condition-expression> ')' <compound-statement> <else-clause>?

      catch-clause ::= 'catch' '(' <type> <identifier> ')' <compound-statement>

      finally-clause ::= 'finally' <compound-statement>
  */
  EggSyntaxParserBacktrackMark mark(this->backtrack);
  auto& p0 = mark.peek(0);
  assert(p0.isKeyword(EggTokenizerKeyword::Try));
  mark.advance(1);
  if (!mark.peek(0).isOperator(EggTokenizerOperator::CurlyLeft)) {
    this->unexpected("Expected '{' after 'try' keyword", mark.peek(0));
  }
  auto block = this->parseCompoundStatement();
  auto result = std::make_unique<EggSyntaxNode_Try>(EggSyntaxNodeLocation(p0), std::move(block));
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

std::unique_ptr<IEggSyntaxNode> EggSyntaxParserContext::parseStatementType(std::unique_ptr<IEggSyntaxNode>&& type, EggTokenizerOperator terminal) {
  EggSyntaxParserBacktrackMark mark(this->backtrack);
  // Already consumed <type>
  auto& p0 = mark.peek(0);
  if (p0.kind == EggTokenizerKind::Identifier) {
    auto& p1 = mark.peek(1);
    if (p1.isOperator(terminal)) {
      // Found <type> <identifier> ';'
      mark.accept(2);
      return std::make_unique<EggSyntaxNode_Declare>(EggSyntaxNodeLocation(p0), p0.value.s, std::move(type));
    }
    if (p1.isOperator(EggTokenizerOperator::Equal)) {
      // Expect <type> <identifier> = <expression> ';'
      mark.advance(2);
      auto expr = this->parseExpression("Expected expression after assignment operator '='");
      if (!mark.peek(0).isOperator(terminal)) {
        this->unexpected("Expected '" + EggTokenizerValue::getOperatorString(terminal) + "' at end of initialization statement");
      }
      mark.accept(1);
      return std::make_unique<EggSyntaxNode_Declare>(EggSyntaxNodeLocation(p0), p0.value.s, std::move(type), std::move(expr));
    }
    this->unexpected("Malformed variable declaration or initialization");
  }
  this->unexpected("Expected variable identifier after type", p0);
  return nullptr;
}

std::unique_ptr<IEggSyntaxNode> EggSyntaxParserContext::parseStatementUsing() {
  /*
      using-statement ::= 'using' '(' variable-definition-type variable-identifier '=' expression ')' compound-statement
                        | 'using' '(' expression ')' compound-statement
  */
  EggSyntaxParserBacktrackMark mark(this->backtrack);
  assert(mark.peek(0).isKeyword(EggTokenizerKeyword::Using));
  EggSyntaxNodeLocation location(mark.peek(0));
  if (!mark.peek(1).isOperator(EggTokenizerOperator::ParenthesisLeft)) {
    this->unexpected("Expected '(' after 'using' keyword", mark.peek(1));
  }
  mark.advance(2);
  auto expr = this->parseExpression(nullptr);
  if (expr == nullptr) {
    // Expect 'using' '(' <type> <identifier> '=' <expression> ')' <compound-statement>
    auto type = this->parseType("Expected expression or type after '(' in 'using' statement");
    auto& p0 = mark.peek(0);
    if (p0.kind != EggTokenizerKind::Identifier) {
      this->unexpected("Expected variable identifier after type in 'using' statement", p0);
    }
    auto& p1 = mark.peek(1);
    if (!p1.isOperator(EggTokenizerOperator::Equal)) {
      this->unexpected("Expected '=' after variable identifier in 'using' statement", p1);
    }
    mark.advance(2);
    auto rhs = this->parseExpression("Expected expression after '=' in 'using' statement");
    expr = std::make_unique<EggSyntaxNode_Declare>(location, p0.value.s, std::move(type), std::move(rhs));
  }
  if (!mark.peek(0).isOperator(EggTokenizerOperator::ParenthesisRight)) {
    this->unexpected("Expected ')' after expression in 'using' statement", mark.peek(0));
  }
  mark.advance(1);
  if (!mark.peek(0).isOperator(EggTokenizerOperator::CurlyLeft)) {
    this->unexpected("Expected '{' after ')' in 'using' statement", mark.peek(0));
  }
  auto block = this->parseCompoundStatement();
  mark.accept(0);
  return std::make_unique<EggSyntaxNode_Using>(location, std::move(expr), std::move(block));
}

std::unique_ptr<IEggSyntaxNode> EggSyntaxParserContext::parseStatementWhile() {
  /*
      do-statement ::= 'while' '(' <condition-expression> ')' <compound-statement>
  */
  EggSyntaxParserBacktrackMark mark(this->backtrack);
  auto& p0 = mark.peek(0);
  assert(p0.isKeyword(EggTokenizerKeyword::While));
  if (!mark.peek(1).isOperator(EggTokenizerOperator::ParenthesisLeft)) {
    this->unexpected("Expected '(' after 'while' keyword", mark.peek(1));
  }
  mark.advance(2);
  auto expr = this->parseConditionGuarded("Expected condition expression after '(' in 'while' statement");
  if (!mark.peek(0).isOperator(EggTokenizerOperator::ParenthesisRight)) {
    this->unexpected("Expected ')' after 'while' condition expression", mark.peek(0));
  }
  mark.advance(1);
  if (!mark.peek(0).isOperator(EggTokenizerOperator::CurlyLeft)) {
    this->unexpected("Expected '{' after ')' in 'while' statement", mark.peek(0));
  }
  auto block = this->parseCompoundStatement();
  mark.accept(0);
  return std::make_unique<EggSyntaxNode_While>(EggSyntaxNodeLocation(p0), std::move(expr), std::move(block));
}

std::unique_ptr<IEggSyntaxNode> EggSyntaxParserContext::parseStatementYield() {
  /*
      yield-statement ::= 'yield' expression ';'
                        | 'yield' '...' expression ';'
  */
  EggSyntaxParserBacktrackMark mark(this->backtrack);
  auto& p0 = mark.peek(0);
  assert(p0.isKeyword(EggTokenizerKeyword::Yield));
  std::unique_ptr<IEggSyntaxNode> expr;
  auto& p1 = mark.peek(1);
  if (p1.isOperator(EggTokenizerOperator::Ellipsis)) {
    mark.advance(2);
    auto ellipsis = this->parseExpression("Expected expression after '...' in 'yield' statement");
    expr = std::make_unique<EggSyntaxNode_UnaryOperator>(EggSyntaxNodeLocation(p0), EggTokenizerOperator::Ellipsis, std::move(ellipsis));
  } else {
    mark.advance(1);
    expr = this->parseExpression("Expected expression in 'yield' statement");
  }
  auto& px = mark.peek(0);
  if (!px.isOperator(EggTokenizerOperator::Semicolon)) {
    this->unexpected("Expected ';' at end of 'yield' statement", px);
  }
  mark.accept(1);
  return std::make_unique<EggSyntaxNode_Yield>(EggSyntaxNodeLocation(p0), std::move(expr));
}

std::unique_ptr<IEggSyntaxNode> EggSyntaxParserContext::parseType(const char* expected) {
  auto& p0 = this->backtrack.peek(0);
  if (p0.isKeyword(EggTokenizerKeyword::Var)) {
    return parseTypeSimple(egg::lang::Discriminator::Inferred);
  }
  if (p0.isKeyword(EggTokenizerKeyword::Bool)) {
    return parseTypeSimple(egg::lang::Discriminator::Bool);
  }
  if (p0.isKeyword(EggTokenizerKeyword::Int)) {
    return parseTypeSimple(egg::lang::Discriminator::Int);
  }
  if (p0.isKeyword(EggTokenizerKeyword::Float)) {
    return parseTypeSimple(egg::lang::Discriminator::Float);
  }
  if (p0.isKeyword(EggTokenizerKeyword::String)) {
    return parseTypeSimple(egg::lang::Discriminator::String);
  }
  if (p0.isKeyword(EggTokenizerKeyword::Object)) {
    return parseTypeSimple(egg::lang::Discriminator::Object);
  }
  if (p0.isKeyword(EggTokenizerKeyword::Any)) {
    return parseTypeSimple(egg::lang::Discriminator::Any);
  }
  if (expected != nullptr) {
    this->unexpected(expected, p0);
  }
  return nullptr; // TODO
}

std::unique_ptr<IEggSyntaxNode> EggSyntaxParserContext::parseTypeSimple(egg::lang::Discriminator tag) {
  // Expect <simple-type> '?'?
  EggSyntaxParserBacktrackMark mark(this->backtrack);
  EggSyntaxNodeLocation location(mark.peek(0));
  auto& p1 = mark.peek(1);
  if (p1.isOperator(EggTokenizerOperator::Query) && p1.contiguous) {
    tag = tag | egg::lang::Discriminator::Null;
    location.setLocationEnd(p1, 1);
    mark.accept(2);
  } else {
    mark.accept(1);
  }
  return std::make_unique<EggSyntaxNode_Type>(location, tag);
}

std::unique_ptr<IEggSyntaxNode> EggSyntaxParserContext::parseTypeDefinition() {
  EGG_THROW(__FUNCTION__ " TODO"); // TODO
}

std::string egg::yolk::EggSyntaxNode_Type::tagToString(egg::lang::Discriminator tag) {
  static const egg::yolk::String::StringFromEnum table[] = {
    { int(egg::lang::Discriminator::Any), "any" },
    { int(egg::lang::Discriminator::Void), "void" },
    { int(egg::lang::Discriminator::Bool), "bool" },
    { int(egg::lang::Discriminator::Int), "int" },
    { int(egg::lang::Discriminator::Float), "float" },
    { int(egg::lang::Discriminator::String), "string" },
    { int(egg::lang::Discriminator::Type), "type" },
    { int(egg::lang::Discriminator::Object), "object" },
    { int(egg::lang::Discriminator::FlowControl), "flow" }
  };
  if (tag == egg::lang::Discriminator::Inferred) {
    return "var";
  }
  if (tag == egg::lang::Discriminator::Null) {
    return "null";
  }
  if (egg::lang::Bits::hasAnySet(tag, egg::lang::Discriminator::Null)) {
    return String::fromEnum(egg::lang::Bits::clear(tag, egg::lang::Discriminator::Null), table) + "?";
  }
  return String::fromEnum(tag, table);
}

std::shared_ptr<egg::yolk::IEggSyntaxParser> egg::yolk::EggParserFactory::createModuleSyntaxParser() {
  return std::make_shared<EggSyntaxParserModule>();
}

std::shared_ptr<egg::yolk::IEggSyntaxParser> egg::yolk::EggParserFactory::createStatementSyntaxParser() {
  return std::make_shared<EggSyntaxParserStatement>();
}

std::shared_ptr<egg::yolk::IEggSyntaxParser> egg::yolk::EggParserFactory::createExpressionSyntaxParser() {
  return std::make_shared<EggSyntaxParserExpression>();
}
