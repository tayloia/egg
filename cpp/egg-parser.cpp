#include "yolk.h"

#define TODO_LINE(line) #line
#define TODO_LOCATION(file, line) file " at line " TODO_LINE(line)
#define TODO() EGG_THROW(TODO_LOCATION(__FILE__, __LINE__) ": " __FUNCTION__ " TODO")

// TODO stop creating strings that are only used on error
#define PARSE_BINARY_LTR(parent, child, condition) \
  std::unique_ptr<IEggSyntaxNode> parent(const char* expected) { \
    EggParserBacktrackMark mark(this->backtrack); \
    auto expr = this->child(expected); \
    for (auto* token = &mark.peek(0); (expr != nullptr) && (condition); token = &mark.peek(0)) { \
      std::string expectation = "Expected expression after infix '" + EggTokenizerValue::getOperatorString(token->value.o) + "' operator"; \
      mark.advance(1); \
      auto lhs = std::move(expr); \
      auto rhs = this->child(expectation.c_str()); \
      expr = std::make_unique<EggSyntaxNode_BinaryOperator>(token->value.o, std::move(lhs), std::move(rhs)); \
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

  class EggParserLookahead {
  private:
    IEggTokenizer* tokenizer;
    std::deque<EggTokenizerItem> upcoming;
  public:
    explicit EggParserLookahead(IEggTokenizer& tokenizer)
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

  class EggParserBacktrack {
    friend class EggParserBacktrackMark;
  private:
    EggParserLookahead lookahead;
    size_t cursor;
  public:
    explicit EggParserBacktrack(IEggTokenizer& tokenizer)
      : lookahead(tokenizer), cursor(0) {
    }
    const EggTokenizerItem& peek(size_t index) {
      return this->lookahead.peek(this->cursor + index);
    }
    size_t advance(size_t count) {
      // Called by EggParserBacktrackMark advance
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
      // Called by EggParserBacktrackMark ctor
      return this->cursor;
    }
    void abandon(size_t previous) {
      // Called by EggParserBacktrackMark dtor
      assert(previous <= this->cursor);
      this->cursor = previous;
    }
  };

  class EggParserBacktrackMark {
  private:
    EggParserBacktrack* backtrack;
    size_t previous;
  public:
    explicit EggParserBacktrackMark(EggParserBacktrack& backtrack)
      : backtrack(&backtrack), previous(backtrack.mark()) {
    }
    ~EggParserBacktrackMark() {
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

  class EggParserContext {
  private:
    EggParserBacktrack backtrack;
  public:
    explicit EggParserContext(IEggTokenizer& tokenizer)
      : backtrack(tokenizer) {
    }
    void unexpected(const std::string& message) {
      auto& item = this->backtrack.peek(0);
      throw Exception(message, this->backtrack.resource(), item.line, item.column);
    }
    void unexpected(const std::string& expected, const EggTokenizerItem& item) {
      throw Exception(expected + ", not " + item.to_string(), this->backtrack.resource(), item.line, item.column);
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
    std::unique_ptr<IEggSyntaxNode> parseExpressionNegative();
    std::unique_ptr<IEggSyntaxNode> parseExpressionUnary(const char* expected);
    std::unique_ptr<IEggSyntaxNode> parseExpressionPostfix(const char* expected);
    std::unique_ptr<IEggSyntaxNode> parseExpressionPostfixGreedy(std::unique_ptr<IEggSyntaxNode>&& expr);
    std::unique_ptr<IEggSyntaxNode> parseExpressionPrimary(const char* expected);
    std::unique_ptr<IEggSyntaxNode> parseModule();
    std::unique_ptr<IEggSyntaxNode> parseStatement();
    std::unique_ptr<IEggSyntaxNode> parseStatementAssignment(std::unique_ptr<IEggSyntaxNode>&& lhs);
    std::unique_ptr<IEggSyntaxNode> parseStatementBreak();
    std::unique_ptr<IEggSyntaxNode> parseStatementCase();
    std::unique_ptr<IEggSyntaxNode> parseStatementContinue();
    std::unique_ptr<IEggSyntaxNode> parseStatementDecrementIncrement(EggTokenizerOperator op, const std::string& what, const char* expected);
    std::unique_ptr<IEggSyntaxNode> parseStatementDefault();
    std::unique_ptr<IEggSyntaxNode> parseStatementDo();
    std::unique_ptr<IEggSyntaxNode> parseStatementExpression(std::unique_ptr<IEggSyntaxNode>&& expr);
    std::unique_ptr<IEggSyntaxNode> parseStatementFor();
    std::unique_ptr<IEggSyntaxNode> parseStatementIf();
    std::unique_ptr<IEggSyntaxNode> parseStatementReturn();
    std::unique_ptr<IEggSyntaxNode> parseStatementSwitch();
    std::unique_ptr<IEggSyntaxNode> parseStatementTry();
    std::unique_ptr<IEggSyntaxNode> parseStatementType(std::unique_ptr<IEggSyntaxNode>&& type);
    std::unique_ptr<IEggSyntaxNode> parseStatementWhile();
    std::unique_ptr<IEggSyntaxNode> parseStatementYield();
    std::unique_ptr<IEggSyntaxNode> parseType();
    std::unique_ptr<IEggSyntaxNode> parseTypeSimple(EggSyntaxNode_Type::Allowed allowed);
    std::unique_ptr<IEggSyntaxNode> parseTypeDefinition();
  };

  class EggParserModule : public IEggParser {
  public:
    virtual std::shared_ptr<IEggSyntaxNode> parse(IEggTokenizer& tokenizer) override {
      EggParserContext context(tokenizer);
      return context.parseModule();
    }
  };

  class EggParserStatement : public IEggParser {
  public:
    virtual std::shared_ptr<IEggSyntaxNode> parse(IEggTokenizer& tokenizer) override {
      EggParserContext context(tokenizer);
      auto result = context.parseStatement();
      assert(result != nullptr);
      context.parseEndOfFile("Expected end of input after statement");
      return result;
    }
  };

  class EggParserExpression : public IEggParser {
  public:
    virtual std::shared_ptr<IEggSyntaxNode> parse(IEggTokenizer& tokenizer) override {
      EggParserContext context(tokenizer);
      auto result = context.parseExpression("Expression expected");
      assert(result != nullptr);
      context.parseEndOfFile("Expected end of input after expression");
      return result;
    }
  };
}

void EggParserContext::parseEndOfFile(const char* expected) {
  auto& p0 = this->backtrack.peek(0);
  if (p0.kind != EggTokenizerKind::EndOfFile) {
    this->unexpected(expected, p0);
  }
}

std::unique_ptr<IEggSyntaxNode> EggParserContext::parseModule() {
  /*
  module ::= statement+
  */
  auto module = std::make_unique<EggSyntaxNode_Module>();
  while (this->backtrack.peek(0).kind != EggTokenizerKind::EndOfFile) {
    module->addChild(std::move(this->parseStatement()));
    this->backtrack.commit();
  }
  return module;
}

std::unique_ptr<IEggSyntaxNode> EggParserContext::parseStatement() {
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
    case EggTokenizerKeyword::True:
      this->unexpected("Unexpected 'true' at start of statement");
    case EggTokenizerKeyword::Try:
      return this->parseStatementTry();
    case EggTokenizerKeyword::Typedef:
      return this->parseTypeDefinition();
    case EggTokenizerKeyword::While:
      return this->parseStatementWhile();
    case EggTokenizerKeyword::Yield:
      return this->parseStatementYield();
    default:
      this->unexpected("Internal egg parser error, expected statement", p0);
    }
    break;
  case EggTokenizerKind::Operator:
    // Handle spcial cases for prefix decrement/increment and compound statements
    if (p0.value.o == EggTokenizerOperator::MinusMinus) {
      return this->parseStatementDecrementIncrement(EggTokenizerOperator::MinusMinus, "decrement", "Expected expression after decrement '--' operator");
    } else if (p0.value.o == EggTokenizerOperator::PlusPlus) {
      return this->parseStatementDecrementIncrement(EggTokenizerOperator::PlusPlus, "increment", "Expected expression after increment '++' operator");
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
  if (expression) {
    return this->parseStatementExpression(std::move(expression));
  }
  auto type = this->parseType();
  if (type) {
    return this->parseStatementType(std::move(type));
  }
  this->unexpected("Unexpected " + p0.to_string()); // TODO
  return nullptr;
}

std::unique_ptr<IEggSyntaxNode> EggParserContext::parseCompoundStatement() {
  /*
      compound-statement ::= '{' statement* '}'
  */
  assert(this->backtrack.peek(0).isOperator(EggTokenizerOperator::CurlyLeft));
  this->backtrack.advance(1); // skip '{'
  auto block = std::make_unique<EggSyntaxNode_Block>();
  while (!this->backtrack.peek(0).isOperator(EggTokenizerOperator::CurlyRight)) {
    block->addChild(std::move(this->parseStatement()));
    this->backtrack.commit();
  }
  this->backtrack.advance(1); // skip '}'
  this->backtrack.commit();
  return block;
}

std::unique_ptr<IEggSyntaxNode> EggParserContext::parseExpression(const char* expected) {
  /*
      expression ::= conditional-expression
  */
  return this->parseExpressionTernary(expected);
}

std::unique_ptr<IEggSyntaxNode> EggParserContext::parseExpressionTernary(const char* expected) {
  /*
      conditional-expression ::= null-coalescing-expression
                               | null-coalescing-expression '?' expression ':' conditional-expression
  */
  EggParserBacktrackMark mark(this->backtrack);
  auto expr = this->parseExpressionNullCoalescing(expected);
  if (expr) {
    if (mark.peek(0).isOperator(EggTokenizerOperator::Query)) {
      // Expect <expression> ? <expression> : <conditional-expression>
      mark.advance(1);
      auto exprTrue = this->parseExpression("Expected expression after '?' of ternary operator '?:'");
      if (!mark.peek(0).isOperator(EggTokenizerOperator::Colon)) {
        this->unexpected("Expected ':' as part of ternary operator '?:'", mark.peek(0));
      }
      mark.advance(1);
      auto exprFalse = this->parseExpression("Expected expression after ':' of ternary operator '?:'");
      mark.accept(0);
      return std::make_unique<EggSyntaxNode_TernaryOperator>(std::move(expr), std::move(exprTrue), std::move(exprFalse));
    }
    mark.accept(0);
  }
  return expr;
}

std::unique_ptr<IEggSyntaxNode> EggParserContext::parseExpressionAdditive(const char* expected) {
  EggParserBacktrackMark mark(this->backtrack);
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
      mark.advance(1);
      auto lhs = std::move(expr);
      auto rhs = this->parseExpressionNegative();
      expr = std::make_unique<EggSyntaxNode_BinaryOperator>(EggTokenizerOperator::Minus, std::move(lhs), std::move(rhs));
      continue;
    } else {
      break;
    }
    mark.advance(1);
    auto lhs = std::move(expr);
    auto rhs = this->parseExpressionMultiplicative(expected);
    expr = std::make_unique<EggSyntaxNode_BinaryOperator>(token->value.o, std::move(lhs), std::move(rhs));
  }
  mark.accept(0);
  return expr;
}

std::unique_ptr<IEggSyntaxNode> EggParserContext::parseExpressionNegative() {
  // TODO explicitly negative numeric literals
  auto expr = this->parseExpressionUnary("Expected expression after prefix '-' operator");
  return std::make_unique<EggSyntaxNode_UnaryOperator>(EggTokenizerOperator::Minus, std::move(expr));
}

std::unique_ptr<IEggSyntaxNode> EggParserContext::parseExpressionUnary(const char* expected) {
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
    expected = "Expected expression after prefix '-' operator";
  } else if (p0.isOperator(EggTokenizerOperator::MinusMinus)) {
    EggParserBacktrackMark mark(this->backtrack);
    mark.advance(1);
    auto negative = this->parseExpressionNegative();
    mark.accept(0);
    return std::make_unique<EggSyntaxNode_UnaryOperator>(EggTokenizerOperator::Minus, std::move(negative));
  } else if (p0.isOperator(EggTokenizerOperator::Tilde)) {
    expected = "Expected expression after prefix '~' operator";
  } else if (p0.isOperator(EggTokenizerOperator::Bang)) {
    expected = "Expected expression after prefix '!' operator";
  } else {
    return this->parseExpressionPostfix(expected);
  }
  EggParserBacktrackMark mark(this->backtrack);
  mark.advance(1);
  auto expr = this->parseExpressionUnary(expected);
  mark.accept(0);
  return std::make_unique<EggSyntaxNode_UnaryOperator>(p0.value.o, std::move(expr));
}

std::unique_ptr<IEggSyntaxNode> EggParserContext::parseExpressionPostfix(const char* expected) {
  auto expr = this->parseExpressionPrimary(expected);
  return (expr == nullptr) ? nullptr : this->parseExpressionPostfixGreedy(std::move(expr));
}

std::unique_ptr<IEggSyntaxNode> EggParserContext::parseExpressionPostfixGreedy(std::unique_ptr<IEggSyntaxNode>&& expr) {
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
      EggParserBacktrackMark mark(this->backtrack);
      mark.advance(1);
      auto index = this->parseExpression("Expected expression inside indexing '[]' operators");
      auto& p1 = mark.peek(0);
      if (!p1.isOperator(EggTokenizerOperator::BracketRight)) {
        this->unexpected("Expected ']' after indexing expression following '['", p1);
      }
      mark.accept(1);
      expr = std::make_unique<EggSyntaxNode_BinaryOperator>(EggTokenizerOperator::BracketLeft, std::move(expr), std::move(index));
    } else if (p0.isOperator(EggTokenizerOperator::ParenthesisLeft)) {
      // Expect <expression> '(' <parameter-list>? ')'
      auto call = std::make_unique<EggSyntaxNode_Call>(std::move(expr));
      this->parseParameterList([&call](auto&& node) { call->addChild(std::move(node)); });
      expr = std::move(call);
    } else if (p0.isOperator(EggTokenizerOperator::Dot)) {
      // Expect <expression> '.' <identifer>
      EggParserBacktrackMark mark(this->backtrack);
      auto& p1 = mark.peek(1);
      if (p1.kind != EggTokenizerKind::Identifier) {
        this->unexpected("Expected field name to follow '.' operator", p1);
      }
      auto field = std::make_unique<EggSyntaxNode_Identifier>(p1.value.s);
      mark.accept(2);
      expr = std::make_unique<EggSyntaxNode_BinaryOperator>(EggTokenizerOperator::Dot, std::move(expr), std::move(field));
    } else if (p0.isOperator(EggTokenizerOperator::Query)) {
      // Expect <expression> '?.' <identifer>
      EggParserBacktrackMark mark(this->backtrack);
      auto& p1 = mark.peek(1);
      // We use contiguous sequential operators to disambiguate "a?...x:y" from "a?.b"
      if (!p1.isOperator(EggTokenizerOperator::Dot) || !p1.contiguous) {
        break;
      }
      auto& p2 = mark.peek(2);
      if (p2.kind != EggTokenizerKind::Identifier) {
        this->unexpected("Expected field name to follow '?.' operator", p2);
      }
      auto field = std::make_unique<EggSyntaxNode_Identifier>(p2.value.s);
      mark.accept(3);
      expr = std::make_unique<EggSyntaxNode_BinaryOperator>(EggTokenizerOperator::Query, std::move(expr), std::move(field));
    } else {
      // No postfix operator, return just the expression
      break;
    }
  }
  return std::move(expr);
}

std::unique_ptr<IEggSyntaxNode> EggParserContext::parseExpressionPrimary(const char* expected) {
  /*
      primary-expression ::= identifier
                           | constant-literal
                           | object-value
                           | array-value
                           | lambda-value
                           | '(' expression ')'
                           | cast-specifier '(' expression ')'
  */
  EggParserBacktrackMark mark(this->backtrack);
  auto& p0 = mark.peek(0);
  switch (p0.kind) {
  case EggTokenizerKind::Integer:
  case EggTokenizerKind::Float:
  case EggTokenizerKind::String:
    mark.accept(1);
    return std::make_unique<EggSyntaxNode_Literal>(p0.kind, p0.value);
  case EggTokenizerKind::Identifier:
    mark.accept(1);
    return std::make_unique<EggSyntaxNode_Identifier>(p0.value.s);
  case EggTokenizerKind::Keyword:
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

std::unique_ptr<IEggSyntaxNode> EggParserContext::parseConditionGuarded(const char* expected) {
  /*
      guarded-condition ::= expression -- TODO
  */
  auto expr = this->parseExpression(expected);
  assert(expr != nullptr);
  return expr;
}

std::unique_ptr<IEggSyntaxNode> EggParserContext::parseConditionUnguarded(const char* expected) {
  /*
      unguarded-condition ::= expression
  */
  auto expr = this->parseExpression(expected);
  assert(expr != nullptr);
  return expr;
}

void EggParserContext::parseExpressionList(std::function<void(std::unique_ptr<IEggSyntaxNode>&& node)> adder, const char* expected0, const char* expected1) {
  /*
      expression-list ::= expression
                        | expression-list ',' expression
  */
  EggParserBacktrackMark mark(this->backtrack);
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

void EggParserContext::parseParameterList(std::function<void(std::unique_ptr<IEggSyntaxNode>&& node)> adder) {
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
  EggParserBacktrackMark mark(this->backtrack);
  assert(mark.peek(0).isOperator(EggTokenizerOperator::ParenthesisLeft));
  if (mark.peek(1).isOperator(EggTokenizerOperator::ParenthesisRight)) {
    // This is an empty parameter list: '(' ')'
    mark.accept(2);
  } else {
    // Don't worry about the order of positional and named parameters at this stage
    const EggTokenizerItem* p0;
    do {
      mark.advance(1);
      p0 = &mark.peek(0);
      if ((p0->kind == EggTokenizerKind::Identifier) && mark.peek(1).isOperator(EggTokenizerOperator::Colon)) {
        // Expect <identifier> ':' <expression>
        mark.advance(2);
        auto expr = this->parseExpression("Expected expression for named function call parameter value");
        auto named = std::make_unique<EggSyntaxNode_Named>(p0->value.s, std::move(expr));
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
    mark.accept(1);
  }
}

std::unique_ptr<IEggSyntaxNode> EggParserContext::parseStatementAssignment(std::unique_ptr<IEggSyntaxNode>&& lhs) {
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
  EggParserBacktrackMark mark(this->backtrack);
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
  if (!px.isOperator(EggTokenizerOperator::Semicolon)) {
    this->unexpected("Expected semicolon after assignment statement", px);
  }
  mark.accept(1);
  return std::make_unique<EggSyntaxNode_Assignment>(p0.value.o, std::move(lhs), std::move(rhs));
}

std::unique_ptr<IEggSyntaxNode> EggParserContext::parseStatementBreak() {
  /*
      break-statement ::= 'break' ';'
  */
  assert(this->backtrack.peek(0).isKeyword(EggTokenizerKeyword::Break));
  auto& p1 = this->backtrack.peek(1);
  if (!p1.isOperator(EggTokenizerOperator::Semicolon)) {
    this->unexpected("Expected semicolon after 'break' keyword", p1);
  }
  this->backtrack.advance(2);
  return std::make_unique<EggSyntaxNode_Break>();
}

std::unique_ptr<IEggSyntaxNode> EggParserContext::parseStatementCase() {
  /*
      case-statement ::= 'case' <expression> ':'
  */
  EggParserBacktrackMark mark(this->backtrack);
  assert(mark.peek(0).isKeyword(EggTokenizerKeyword::Case));
  mark.advance(1);
  auto expr = this->parseExpression("Expected expression after 'case' keyword");
  auto& px = mark.peek(0);
  if (!px.isOperator(EggTokenizerOperator::Colon)) {
    this->unexpected("Expected colon after 'case' expression", px);
  }
  mark.accept(1);
  return std::make_unique<EggSyntaxNode_Case>(std::move(expr));
}

std::unique_ptr<IEggSyntaxNode> EggParserContext::parseStatementContinue() {
  /*
      continue-statement ::= 'continue' ';'
  */
  assert(this->backtrack.peek(0).isKeyword(EggTokenizerKeyword::Continue));
  auto& p1 = this->backtrack.peek(1);
  if (!p1.isOperator(EggTokenizerOperator::Semicolon)) {
    this->unexpected("Expected semicolon after 'continue' keyword", p1);
  }
  this->backtrack.advance(2);
  return std::make_unique<EggSyntaxNode_Continue>();
}

std::unique_ptr<IEggSyntaxNode> EggParserContext::parseStatementDecrementIncrement(EggTokenizerOperator op, const std::string& what, const char* expected) {
  /*
      assignment-statement ::= assignment-list assignment-operator expression-list
                             | '++' assignment-target
                             | '--' assignment-target
  */
  EggParserBacktrackMark mark(this->backtrack);
  assert(mark.peek(0).isOperator(op));
  mark.advance(1);
  auto expr = this->parseExpression(expected);
  auto& px = mark.peek(0);
  if (!px.isOperator(EggTokenizerOperator::Semicolon)) {
    this->unexpected("Expected semicolon after " + what + " statement", px);
  }
  mark.accept(1);
  return std::make_unique<EggSyntaxNode_Mutate>(op, std::move(expr));
}

std::unique_ptr<IEggSyntaxNode> EggParserContext::parseStatementDefault() {
  /*
      default-statement ::= 'default' ':'
  */
  assert(this->backtrack.peek(0).isKeyword(EggTokenizerKeyword::Default));
  auto& p1 = this->backtrack.peek(1);
  if (!p1.isOperator(EggTokenizerOperator::Colon)) {
    this->unexpected("Expected colon after 'default' keyword", p1);
  }
  this->backtrack.advance(2);
  return std::make_unique<EggSyntaxNode_Default>();
}

std::unique_ptr<IEggSyntaxNode> EggParserContext::parseStatementDo() {
  /*
      do-statement ::= 'do' <compound-statement> 'while' '(' <expression> ')' ';'
  */
  EggParserBacktrackMark mark(this->backtrack);
  assert(mark.peek(0).isKeyword(EggTokenizerKeyword::Do));
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
  return std::make_unique<EggSyntaxNode_Do>(std::move(expr), std::move(block));
}

std::unique_ptr<IEggSyntaxNode> EggParserContext::parseStatementExpression(std::unique_ptr<IEggSyntaxNode>&& expr) {
  // Expect <lhs> <assignment-operator> <rhs> ';'
  //     or <function-call> ';'
  if (!this->backtrack.peek(0).isOperator(EggTokenizerOperator::Semicolon)) {
    return this->parseStatementAssignment(std::move(expr));
  }
  if (expr->getKind() != EggSyntaxNodeKind::Call) {
    this->unexpected("Expected assignment operator after expression, not end of statement");
  }
  EggParserBacktrackMark mark(this->backtrack);
  mark.accept(1);
  return std::move(expr);
}

std::unique_ptr<IEggSyntaxNode> EggParserContext::parseStatementFor() {
  TODO();
}

std::unique_ptr<IEggSyntaxNode> EggParserContext::parseStatementIf() {
  /*
      if-statement ::= 'if' '(' <condition-expression> ')' <compound-statement> <else-clause>?

      else-clause ::= 'else' <compound-statement>
  */
  EggParserBacktrackMark mark(this->backtrack);
  assert(mark.peek(0).isKeyword(EggTokenizerKeyword::If));
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
  auto result = std::make_unique<EggSyntaxNode_If>(std::move(expr), std::move(block));
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

std::unique_ptr<IEggSyntaxNode> EggParserContext::parseStatementReturn() {
  /*
      return-statement ::= 'return' expression-list? ';'
                         | 'return' '...' expression ';'
  */
  EggParserBacktrackMark mark(this->backtrack);
  assert(mark.peek(0).isKeyword(EggTokenizerKeyword::Return));
  auto results = std::make_unique<EggSyntaxNode_Return>();
  if (mark.peek(1).isOperator(EggTokenizerOperator::Ellipsis)) {
    mark.advance(2);
    auto expr = this->parseExpression("Expected expression after '...' in 'return' statement");
    auto ellipsis = std::make_unique<EggSyntaxNode_UnaryOperator>(EggTokenizerOperator::Ellipsis, std::move(expr));
    results->addChild(std::move(ellipsis));
  } else {
    mark.advance(1);
    this->parseExpressionList([&results](auto&& node) { results->addChild(std::move(node)); }, nullptr, "Expected expression after ',' in 'return' statement");
  }
  auto& px = mark.peek(0);
  if (!px.isOperator(EggTokenizerOperator::Semicolon)) {
    this->unexpected("Expected semicolon at end of 'return' statement", px);
  }
  mark.accept(1);
  return std::move(results);
}

std::unique_ptr<IEggSyntaxNode> EggParserContext::parseStatementSwitch() {
  /*
      switch-statement ::= 'switch' '(' <expression> ')' <compound-statement>
  */
  EggParserBacktrackMark mark(this->backtrack);
  assert(mark.peek(0).isKeyword(EggTokenizerKeyword::Switch));
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
  return std::make_unique<EggSyntaxNode_Switch>(std::move(expr), std::move(block));
}

std::unique_ptr<IEggSyntaxNode> EggParserContext::parseStatementTry() {
  TODO();
}

std::unique_ptr<IEggSyntaxNode> EggParserContext::parseStatementType(std::unique_ptr<IEggSyntaxNode>&& type) {
  EggParserBacktrackMark mark(this->backtrack);
  // Already consumed <type>
  auto& p0 = mark.peek(0);
  if (p0.kind == EggTokenizerKind::Identifier) {
    auto& p1 = mark.peek(1);
    if (p1.isOperator(EggTokenizerOperator::Semicolon)) {
      // Found <type> <identifier> ';'
      mark.accept(2);
      return std::make_unique<EggSyntaxNode_VariableDeclaration>(p0.value.s, std::move(type));
    }
    if (p1.isOperator(EggTokenizerOperator::Equal)) {
      // Expect <type> <identifier> = <expression> ';'
      mark.advance(2);
      auto expr = this->parseExpression("Expected expression after assignment operator '='");
      if (!mark.peek(0).isOperator(EggTokenizerOperator::Semicolon)) {
        this->unexpected("Expected semicolon at end of initialization statement");
      }
      mark.accept(1);
      return std::make_unique<EggSyntaxNode_VariableInitialization>(p0.value.s, std::move(type), std::move(expr));
    }
    this->unexpected("Malformed variable declaration or initialization");
  }
  this->unexpected("Expected variable identifier after type", p0);
  return nullptr;
}

std::unique_ptr<IEggSyntaxNode> EggParserContext::parseStatementWhile() {
  /*
      do-statement ::= 'while' '(' <condition-expression> ')' <compound-statement>
  */
  EggParserBacktrackMark mark(this->backtrack);
  assert(mark.peek(0).isKeyword(EggTokenizerKeyword::While));
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
  return std::make_unique<EggSyntaxNode_While>(std::move(expr), std::move(block));
}

std::unique_ptr<IEggSyntaxNode> EggParserContext::parseStatementYield() {
  /*
      yield-statement ::= 'yield' expression ';'
                        | 'yield' '...' expression ';'
  */
  EggParserBacktrackMark mark(this->backtrack);
  std::unique_ptr<IEggSyntaxNode> expr;
  assert(mark.peek(0).isKeyword(EggTokenizerKeyword::Yield));
  if (mark.peek(1).isOperator(EggTokenizerOperator::Ellipsis)) {
    mark.advance(2);
    auto ellipsis = this->parseExpression("Expected expression after '...' in 'yield' statement");
    expr = std::make_unique<EggSyntaxNode_UnaryOperator>(EggTokenizerOperator::Ellipsis, std::move(ellipsis));
  } else {
    mark.advance(1);
    expr = this->parseExpression("Expected expression in 'yield' statement");
  }
  auto& px = mark.peek(0);
  if (!px.isOperator(EggTokenizerOperator::Semicolon)) {
    this->unexpected("Expected semicolon at end of 'yield' statement", px);
  }
  mark.accept(1);
  return std::make_unique<EggSyntaxNode_Yield>(std::move(expr));
}

std::unique_ptr<IEggSyntaxNode> EggParserContext::parseType() {
  auto& p0 = this->backtrack.peek(0);
  if (p0.isKeyword(EggTokenizerKeyword::Var)) {
    return parseTypeSimple(EggSyntaxNode_Type::Inferred);
  }
  if (p0.isKeyword(EggTokenizerKeyword::Bool)) {
    return parseTypeSimple(EggSyntaxNode_Type::Bool);
  }
  if (p0.isKeyword(EggTokenizerKeyword::Int)) {
    return parseTypeSimple(EggSyntaxNode_Type::Int);
  }
  if (p0.isKeyword(EggTokenizerKeyword::Float)) {
    return parseTypeSimple(EggSyntaxNode_Type::Float);
  }
  if (p0.isKeyword(EggTokenizerKeyword::String)) {
    return parseTypeSimple(EggSyntaxNode_Type::String);
  }
  if (p0.isKeyword(EggTokenizerKeyword::Object)) {
    return parseTypeSimple(EggSyntaxNode_Type::Object);
  }
  if (p0.isKeyword(EggTokenizerKeyword::Any)) {
    auto any = EggSyntaxNode_Type::Allowed(EggSyntaxNode_Type::Bool | EggSyntaxNode_Type::Int | EggSyntaxNode_Type::Float | EggSyntaxNode_Type::String | EggSyntaxNode_Type::Object);
    return parseTypeSimple(any);
  }
  return nullptr; // TODO
}

std::unique_ptr<IEggSyntaxNode> EggParserContext::parseTypeSimple(EggSyntaxNode_Type::Allowed allowed) {
  // Expect <simple-type> '?'?
  EggParserBacktrackMark mark(this->backtrack);
  if (mark.peek(1).isOperator(EggTokenizerOperator::Query)) {
    allowed = EggSyntaxNode_Type::Allowed(allowed | EggSyntaxNode_Type::Void);
    mark.accept(2);
  } else {
    mark.accept(1);
  }
  return std::make_unique<EggSyntaxNode_Type>(allowed);
}

std::unique_ptr<IEggSyntaxNode> EggParserContext::parseTypeDefinition() {
  TODO();
}

std::shared_ptr<egg::yolk::IEggParser> egg::yolk::EggParserFactory::createModuleParser() {
  return std::make_shared<EggParserModule>();
}

std::shared_ptr<egg::yolk::IEggParser> egg::yolk::EggParserFactory::createStatementParser() {
  return std::make_shared<EggParserStatement>();
}

std::shared_ptr<egg::yolk::IEggParser> egg::yolk::EggParserFactory::createExpressionParser() {
  return std::make_shared<EggParserExpression>();
}
