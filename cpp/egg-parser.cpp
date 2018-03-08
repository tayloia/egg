#include "yolk.h"

#define TODO_LINE(line) #line
#define TODO_LOCATION(file, line) file " at line " TODO_LINE(line)
#define TODO() EGG_THROW(TODO_LOCATION(__FILE__, __LINE__) ": " __FUNCTION__ " TODO")

// TODO stop creating strings that are only used on error
#define PARSE_BINARY_LTR(parent, child, condition) \
  std::unique_ptr<IEggSyntaxNode> parent(const char* expected) { \
    EggParserBacktrackMark mark(this->backtrack); \
    auto expr = this->child(expected); \
    for (auto* token = &mark.peek(0); expr && (condition); token = &mark.peek(0)) { \
      std::string expectation = "Expected expression after binary " + token->to_string(); \
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
    size_t advance(size_t count) {
      // Called by EggParserBacktrackMark advance
      this->cursor += count;
      return this->cursor;
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
      this->unexpected(message, this->backtrack.peek(0));
    }
    void unexpected(const std::string& message, const EggTokenizerItem& item) {
      throw Exception(message, this->backtrack.resource(), item.line, item.column);
    }
    void unexpected(const EggTokenizerItem& item) {
      throw Exception("Unexpected " + item.to_string(), this->backtrack.resource(), item.line, item.column);
    }
    std::unique_ptr<IEggSyntaxNode> parseCompoundStatement();
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
    PARSE_BINARY4_LTR(parseExpressionAdditive, parseExpressionMultiplicative, EggTokenizerOperator::Plus, EggTokenizerOperator::Minus, EggTokenizerOperator::PlusPlus, EggTokenizerOperator::MinusMinus)
    PARSE_BINARY3_LTR(parseExpressionMultiplicative, parseExpressionUnary, EggTokenizerOperator::Star, EggTokenizerOperator::Slash, EggTokenizerOperator::Percent)
    std::unique_ptr<IEggSyntaxNode> parseExpressionUnary(const char* expected);
    std::unique_ptr<IEggSyntaxNode> parseExpressionPostfix(const char* expected);
    std::unique_ptr<IEggSyntaxNode> parseExpressionPrimary(const char* expected);
    std::unique_ptr<IEggSyntaxNode> parseModule();
    std::unique_ptr<IEggSyntaxNode> parseStatement();
    std::unique_ptr<IEggSyntaxNode> parseStatementBreak();
    std::unique_ptr<IEggSyntaxNode> parseStatementCase();
    std::unique_ptr<IEggSyntaxNode> parseStatementContinue();
    std::unique_ptr<IEggSyntaxNode> parseStatementDecrement();
    std::unique_ptr<IEggSyntaxNode> parseStatementDefault();
    std::unique_ptr<IEggSyntaxNode> parseStatementDo();
    std::unique_ptr<IEggSyntaxNode> parseStatementExpression(std::unique_ptr<IEggSyntaxNode>&& expr);
    std::unique_ptr<IEggSyntaxNode> parseStatementFor();
    std::unique_ptr<IEggSyntaxNode> parseStatementIf();
    std::unique_ptr<IEggSyntaxNode> parseStatementIncrement();
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
      return context.parseStatement();
    }
  };

  class EggParserExpression : public IEggParser {
  public:
    virtual std::shared_ptr<IEggSyntaxNode> parse(IEggTokenizer& tokenizer) override {
      EggParserContext context(tokenizer);
      return context.parseExpression("Expression expected");
    }
  };
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
      this->unexpected("Internal egg parser error (statement keyword)");
    }
    break;
  case EggTokenizerKind::Operator:
    // Handle spcial cases for prefix decrement/increment and compound statements
    if (p0.value.o == EggTokenizerOperator::MinusMinus) {
      return this->parseStatementDecrement();
    } else if (p0.value.o == EggTokenizerOperator::PlusPlus) {
      return this->parseStatementIncrement();
    } else if (p0.value.o == EggTokenizerOperator::CurlyLeft) {
      return this->parseCompoundStatement();
    }
    break;
  case EggTokenizerKind::Identifier:
    break;
  case EggTokenizerKind::Attribute:
    this->unexpected("TODO Attribute"); // TODO
  case EggTokenizerKind::EndOfFile:
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
  this->unexpected(p0); // TODO
  return nullptr;
}

std::unique_ptr<IEggSyntaxNode> EggParserContext::parseCompoundStatement() {
  TODO();
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
        this->unexpected("Expected ':' as part of ternary operator '?:', not " + mark.peek(0).to_string());
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
  // '-' is never seen because of prefix '--' confusion
  auto& p0 = this->backtrack.peek(0);
  assert(!p0.isOperator(EggTokenizerOperator::Minus));
  if (p0.isOperator(EggTokenizerOperator::Ampersand)) {
    expected = "Expected expression after prefix '&' operator";
  } else if (p0.isOperator(EggTokenizerOperator::Star)) {
    expected = "Expected expression after prefix '*' operator";
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
  /*
      postfix-expression ::= primary-expression
                           | null-detecting-expression '[' expression ']'
                           | null-detecting-expression '(' parameter-list? ')'
                           | null-detecting-expression '.' identifier

      null-detecting-expression ::= postfix-expression '?'?
  */
  EggParserBacktrackMark mark(this->backtrack);
  auto expr = this->parseExpressionPrimary(expected);
  if (expr) {
    auto& p0 = mark.peek(0);
    if (p0.isOperator(EggTokenizerOperator::BracketLeft)) {
      // Expect <expression> '[' <expression> ']' 
      TODO();
    } else if (p0.isOperator(EggTokenizerOperator::ParenthesisLeft)) {
      // Expect <expression> '(' <expression> ')' 
      TODO();
    } else if (p0.isOperator(EggTokenizerOperator::Dot)) {
      // Expect <expression> '.' <identifer>
      TODO();
    }
    mark.accept(0);
  }
  return expr;
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
      this->unexpected(std::string(expected) + ", not " + p0.to_string(), p0);
    }
    return nullptr;
  }
}

std::unique_ptr<IEggSyntaxNode> EggParserContext::parseStatementBreak() {
  TODO();
}

std::unique_ptr<IEggSyntaxNode> EggParserContext::parseStatementCase() {
  TODO();
}

std::unique_ptr<IEggSyntaxNode> EggParserContext::parseStatementContinue() {
  TODO();
}

std::unique_ptr<IEggSyntaxNode> EggParserContext::parseStatementDecrement() {
  TODO();
}

std::unique_ptr<IEggSyntaxNode> EggParserContext::parseStatementDefault() {
  TODO();
}

std::unique_ptr<IEggSyntaxNode> EggParserContext::parseStatementDo() {
  TODO();
}

std::unique_ptr<IEggSyntaxNode> EggParserContext::parseStatementExpression(std::unique_ptr<IEggSyntaxNode>&& expr) {
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
    this->unexpected("Expected assignment operator after expression, not " + p0.to_string(), p0);
  }
  mark.advance(1);
  auto rhs = this->parseExpression(expected);
  auto& px = mark.peek(0);
  if (!px.isOperator(EggTokenizerOperator::Semicolon)) {
    this->unexpected("Expected semicolon after assignment statement, not " + px.to_string(), px);
  }
  mark.accept(1);
  return std::make_unique<EggSyntaxNode_Assignment>(p0.value.o, std::move(expr), std::move(rhs));
}

std::unique_ptr<IEggSyntaxNode> EggParserContext::parseStatementFor() {
  TODO();
}

std::unique_ptr<IEggSyntaxNode> EggParserContext::parseStatementIf() {
  TODO();
}

std::unique_ptr<IEggSyntaxNode> EggParserContext::parseStatementIncrement() {
  TODO();
}

std::unique_ptr<IEggSyntaxNode> EggParserContext::parseStatementReturn() {
  TODO();
}

std::unique_ptr<IEggSyntaxNode> EggParserContext::parseStatementSwitch() {
  TODO();
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
    this->unexpected("Malformed variable declaration or initialization", p1);
  }
  this->unexpected("Malformed statement", p0);
  return nullptr;
}

std::unique_ptr<IEggSyntaxNode> EggParserContext::parseStatementWhile() {
  TODO();
}

std::unique_ptr<IEggSyntaxNode> EggParserContext::parseStatementYield() {
  TODO();
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
