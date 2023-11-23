#include "yolk/yolk.h"
#include "yolk/lexers.h"
#include "yolk/egg-tokenizer.h"
#include "yolk/egg-parser.h"

#include <iostream>
#define PARSE_TODO(tokidx, ...) context.todo(tokidx, __FILE__, __LINE__, __VA_ARGS__)

using namespace egg::yolk;

namespace {
  class EggParserTokens {
    EggParserTokens(const EggParserTokens&) = delete;
    EggParserTokens& operator=(const EggParserTokens&) = delete;
  private:
    std::shared_ptr<IEggTokenizer> tokenizer;
    bool exhausted;
    size_t absolute;
    std::deque<EggTokenizerItem> items;
  public:
    explicit EggParserTokens(const std::shared_ptr<IEggTokenizer>& tokenizer)
      : tokenizer(tokenizer),
        exhausted(false),
        absolute(0) {
      assert(this->tokenizer != nullptr);
    }
    egg::ovum::String resource() const {
      return this->tokenizer->resource();
    }
    const EggTokenizerItem& getAbsolute(size_t absidx) {
      assert(absidx >= this->absolute);
      auto relidx = absidx - this->absolute;
      if (relidx < this->items.size()) {
        return this->items[relidx];
      }
      auto remaining = relidx - this->items.size() + 1;
      do {
        if (!this->fetch()) {
          // Already beyond the end of file
          break;
        }
      } while (--remaining > 0);
      return this->items.back();
    }
  private:
    bool fetch() {
      if (this->exhausted) {
        assert(!this->items.empty());
        assert(this->items.back().kind == EggTokenizerKind::EndOfFile);
        this->items.push_back(this->items.back());
        return false;
      }
      EggTokenizerItem item;
      if (this->tokenizer->next(item) == EggTokenizerKind::EndOfFile) {
        this->exhausted = true;
      }
      this->items.push_back(item);
      return !this->exhausted;
    }
  };

  class EggParser : public IEggParser {
    EggParser(const EggParser&) = delete;
    EggParser& operator=(const EggParser&) = delete;
  private:
    egg::ovum::IAllocator& allocator;
    EggParserTokens tokens;
    std::vector<Issue> issues;
  public:
    EggParser(egg::ovum::IAllocator& allocator, const std::shared_ptr<IEggTokenizer>& tokenizer)
      : allocator(allocator),
        tokens(tokenizer) {
    }
    virtual Result parse() override {
      assert(this->issues.empty());
      auto root = std::make_shared<Node>();
      root->kind = Node::Kind::ModuleRoot;
      try {
        if (!this->parseModule(*root)) {
          root = nullptr;
        }
      }
      catch (SyntaxException& exception) {
        const auto& reason = exception.reason();
        auto message = egg::ovum::String::fromUTF8(this->allocator, reason.data(), reason.size());
        const auto& begin = exception.location().begin;
        const auto& end = exception.location().end;
        this->issues.emplace_back(Issue::Severity::Error, message, Location{ begin.line, begin.column }, Location{ end.line, end.column });
        root = nullptr;
      }
      return { root, std::move(this->issues) };
    }
    virtual egg::ovum::String resource() const override {
      return this->tokens.resource();
    }
    const EggTokenizerItem& getAbsolute(size_t absidx) {
      return this->tokens.getAbsolute(absidx);
    }
  private:
    struct Context;
    struct Partial {
      Partial(Partial&&) = default;
      Partial(Partial&) = delete;
      Partial& operator=(Partial&) = delete;
      EggParser& parser;
      std::unique_ptr<Node> node;
      size_t tokensBefore;
      size_t issuesBefore;
      size_t tokensAfter;
      size_t issuesAfter;
      Partial(const Context& context, std::unique_ptr<Node>&& node, size_t tokensAfter, size_t issuesAfter);
      bool succeeded() const {
        return this->node != nullptr;
      }
      size_t accept() {
        assert(this->node != nullptr);
        return this->tokensAfter;
      }
      size_t accept(std::unique_ptr<Node>& target) {
        assert(this->node != nullptr);
        target = std::move(this->node);
        return this->tokensAfter;
      }
      size_t accept(std::vector<std::unique_ptr<Node>>& target) {
        assert(this->node != nullptr);
        target.emplace_back(std::move(this->node));
        return this->tokensAfter;
      }
      const EggTokenizerItem& before(size_t offset) {
        return this->parser.getAbsolute(this->tokensBefore + offset);
      }
      const EggTokenizerItem& after(size_t offset) {
        assert(this->node != nullptr);
        return this->parser.getAbsolute(this->tokensAfter + offset);
      }
      template<typename... ARGS>
      void fail(ARGS&&... args) {
        auto issue = this->parser.createIssue(this->tokensBefore, this->tokensAfter, std::forward<ARGS>(args)...);
        this->parser.issues.push_back(issue);
        this->node.reset();
      }
      void fail(Partial& failed) {
        assert(&failed.parser == &this->parser);
        assert(failed.node == nullptr);
        assert(failed.tokensBefore >= this->tokensBefore);
        assert(failed.tokensAfter >= failed.tokensBefore);
        assert(failed.issuesBefore >= this->issuesBefore);
        assert(failed.issuesAfter >= failed.issuesBefore);
        this->node.reset();
        this->tokensAfter = failed.tokensAfter; 
        this->issuesAfter = failed.issuesAfter;
      }
      void wrap(Node::Kind kind) {
        assert(this->node != nullptr);
        auto wrapper = this->parser.makeNode(kind, this->node->begin, this->node->end);
        wrapper->children.emplace_back(std::move(this->node));
        assert(this->node == nullptr);
        this->node = std::move(wrapper);
      }
    };
    struct Context {
      Context(Context&) = delete;
      Context& operator=(Context&) = delete;
      EggParser& parser;
      size_t tokensBefore;
      size_t issuesBefore;
      Context(EggParser& parser, size_t tokidx)
        : parser(parser),
          tokensBefore(tokidx),
          issuesBefore(parser.issues.size()) {
      }
      const EggTokenizerItem& operator[](size_t offset) const {
        return this->parser.getAbsolute(this->tokensBefore + offset);
      }
      Partial success(std::unique_ptr<Node>&& node, size_t tokidx) const {
        return Partial(*this, std::move(node), tokidx, this->parser.issues.size());
      }
      Partial failed() const {
        assert(!this->parser.issues.empty());
        return Partial(*this, nullptr, this->tokensBefore, this->parser.issues.size());
      }
      Partial failed(const Issue& issue) const {
        this->parser.issues.push_back(issue);
        return this->failed();
      }
      Partial failed(Issue::Severity severity, egg::ovum::String message, const Location& begin, const Location& end) const {
        this->parser.issues.emplace_back(severity, message, begin, end);
        return this->failed();
      }
      template<typename... ARGS>
      Partial failed(size_t tokensAfter, ARGS&&... args) const {
        return this->failed(parser.createIssue(this->tokensBefore, tokensAfter, std::forward<ARGS>(args)...));
      }
      template<typename... ARGS>
      Partial todo(size_t tokensAfter, const char* file, size_t line, ARGS&&... args) const {
        // TODO: remove
        egg::ovum::StringBuilder sb;
        std::cout << file << '(' << line << "): PARSE_TODO: " << sb.add(std::forward<ARGS>(args)...).toUTF8() << std::endl;
        return this->failed(parser.createIssue(this->tokensBefore, tokensAfter, "PARSE_TODO: ", std::forward<ARGS>(args)...));
      }
      void rollback() {
        this->parser.issues.resize(this->issuesBefore);
      }
    };
    template<typename... ARGS>
    Issue createIssue(size_t tokensBefore, size_t tokensAfter, ARGS&&... args) {
      assert(tokensBefore <= tokensAfter);
      auto message = egg::ovum::StringBuilder::concat(this->allocator, std::forward<ARGS>(args)...);
      auto& item0 = this->getAbsolute(tokensBefore);
      Location location0{ item0.line, item0.column };
      auto& item1 = this->getAbsolute(tokensAfter);
      Location location1{ item1.line, item1.column + item1.width() };
      return { Issue::Severity::Error, message, location0, location1 };
    }
    bool parseModule(Node& root) {
      assert(this->issues.empty());
      assert(root.kind == Node::Kind::ModuleRoot);
      size_t tokidx = 0;
      while (this->getAbsolute(tokidx).kind != EggTokenizerKind::EndOfFile) {
        auto partial = this->parseModuleStatement(tokidx);
        if (!partial.succeeded()) {
          return false;
        }
        tokidx = partial.accept(root.children);
      }
      return true;
    }
    Partial parseModuleStatement(size_t tokidx) {
      // TODO: Module-level attributes
      return this->parseStatement(tokidx);
    }
    Partial parseStatement(size_t tokidx) {
      Context context(*this, tokidx);
      auto& next = context[0];
      if (next.kind == EggTokenizerKind::Keyword) {
        switch (next.value.k) {
          case EggTokenizerKeyword::Any:
          case EggTokenizerKeyword::Bool:
          case EggTokenizerKeyword::Float:
          case EggTokenizerKeyword::Int:
          case EggTokenizerKeyword::Object:
          case EggTokenizerKeyword::String:
          case EggTokenizerKeyword::Void:
          case EggTokenizerKeyword::Var:
          case EggTokenizerKeyword::Type:
            // Probably a simple statement introducing a new variable or type
            break;
          case EggTokenizerKeyword::False:
          case EggTokenizerKeyword::Null:
          case EggTokenizerKeyword::True:
            // Probably an error, but let the simple statement code generate the message
            break;
          case EggTokenizerKeyword::Break:
          case EggTokenizerKeyword::Case:
          case EggTokenizerKeyword::Catch:
          case EggTokenizerKeyword::Continue:
          case EggTokenizerKeyword::Default:
          case EggTokenizerKeyword::Do:
          case EggTokenizerKeyword::Else:
          case EggTokenizerKeyword::Finally:
          case EggTokenizerKeyword::For:
          case EggTokenizerKeyword::If:
          case EggTokenizerKeyword::Return:
          case EggTokenizerKeyword::Switch:
          case EggTokenizerKeyword::Throw:
          case EggTokenizerKeyword::Try:
          case EggTokenizerKeyword::While:
          case EggTokenizerKeyword::Yield:
            return PARSE_TODO(tokidx, "statement keyword: ", next.toString());
        }
      }
      auto partial = this->parseStatementSimple(tokidx);
      if (partial.succeeded()) {
        if (partial.after(0).isOperator(EggTokenizerOperator::Semicolon)) {
          // Swallow the semicolon
          partial.tokensAfter++;
          return partial;
        }
        return PARSE_TODO(partial.tokensAfter, "expected ';' after simple statement, but got ", partial.after(0).toString());
      }
      return partial;
    }
    Partial parseStatementSimple(size_t tokidx) {
      Context context(*this, tokidx);
      auto defn = this->parseDefinitionVariable(tokidx);
      if (defn.succeeded()) {
        return defn;
      }
      context.rollback();
      auto expr = this->parseValueExpressionPrimary(tokidx);
      if (expr.succeeded()) {
        // The whole statement is actually an expression
        if (expr.node->kind == Node::Kind::ExprCall) {
          // Wrap in a statement
          expr.wrap(Node::Kind::StmtCall);
          return expr;
        }
        return PARSE_TODO(tokidx, "non-function statement simple");
      }
      return PARSE_TODO(tokidx, "statement simple");

    }
    Partial parseDefinitionVariable(size_t tokidx) {
      Context context(*this, tokidx);
      if (context[0].isKeyword(EggTokenizerKeyword::Var)) {
        // Inferred type
        if (context[1].isOperator(EggTokenizerOperator::Query)) {
          return this->parseDefinitionVariableInferred(tokidx + 2, true);
        }
        return this->parseDefinitionVariableInferred(tokidx + 1, false);
      }
      auto partial = this->parseTypeExpression(tokidx);
      if (partial.succeeded()) {
        return this->parseDefinitionVariableExplicit(partial.tokensAfter, partial.node);
      }
      return partial;
    }
    Partial parseDefinitionVariableInferred(size_t tokidx, bool nullable) {
      Context context(*this, tokidx);
      return PARSE_TODO(tokidx, "definition variable inferred: nullable=", nullable);
    }
    Partial parseDefinitionVariableExplicit(size_t tokidx, std::unique_ptr<Node>& ptype) {
      assert(ptype != nullptr);
      Context context(*this, tokidx);
      if (context[0].kind != EggTokenizerKind::Identifier) {
        return context.failed(tokidx, "expected identifier after type in variable definition, but got ", context[0].toString());
      }
      if (context[1].isOperator(EggTokenizerOperator::Equal)) {
        // <type> <identifier> = <expr>
        auto expr = parseValueExpression(tokidx + 2);
        if (expr.succeeded()) {
          auto stmt = this->makeNodeString(Node::Kind::StmtDefineVar, context[0]);
          stmt->children.emplace_back(std::move(ptype));
          stmt->children.emplace_back(std::move(expr.node));
          return context.success(std::move(stmt), expr.tokensAfter);
        }
        return expr;
      }
      // <type> <identifier>
      auto stmt = this->makeNodeString(Node::Kind::StmtDeclareVar, context[0]);
      stmt->children.emplace_back(std::move(ptype));
      return context.success(std::move(stmt), tokidx + 1);
    }
    Partial parseTypeExpression(size_t tokidx) {
      return this->parseTypeExpressionBinary(tokidx);
    }
    Partial parseTypeExpressionBinary(size_t tokidx) {
      Context context(*this, tokidx);
      auto lhs = this->parseTypeExpressionUnary(tokidx);
      if (!lhs.succeeded()) {
        return lhs;
      }
      if (lhs.after(0).isOperator(EggTokenizerOperator::Bar)) {
        auto rhs = this->parseTypeExpression(lhs.tokensAfter + 1);
        if (rhs.succeeded()) {
          lhs.wrap(Node::Kind::TypeBinary);
          lhs.tokensAfter = rhs.tokensAfter;
          lhs.node->children.emplace_back(std::move(rhs.node));
          lhs.node->op.typeBinaryOp = egg::ovum::TypeBinaryOp::Union;
          return lhs;
        }
        return rhs;
      }
      return lhs;
    }
    Partial parseTypeExpressionUnary(size_t tokidx) {
      Context context(*this, tokidx);
      auto partial = this->parseTypeExpressionPrimary(tokidx);
      while (partial.succeeded()) {
        if (partial.after(0).isOperator(EggTokenizerOperator::Query)) {
          return PARSE_TODO(tokidx, "WIBBLE query");
        } else {
          break;
        }
      }
      return partial;
    }
    Partial parseTypeExpressionPrimary(size_t tokidx) {
      Context context(*this, tokidx);
      auto& next = context[0];
      if (next.kind == EggTokenizerKind::Keyword) {
        switch (next.value.k) {
        case EggTokenizerKeyword::Any:
          return this->parseTypeExpressionPrimaryKeyword(context, Node::Kind::TypeAny);
        case EggTokenizerKeyword::Void:
          return this->parseTypeExpressionPrimaryKeyword(context, Node::Kind::TypeVoid);
        case EggTokenizerKeyword::Bool:
          return this->parseTypeExpressionPrimaryKeyword(context, Node::Kind::TypeBool);
        case EggTokenizerKeyword::Float:
          return this->parseTypeExpressionPrimaryKeyword(context, Node::Kind::TypeFloat);
        case EggTokenizerKeyword::Int:
          return this->parseTypeExpressionPrimaryKeyword(context, Node::Kind::TypeInt);
        case EggTokenizerKeyword::String:
          return this->parseTypeExpressionPrimaryKeyword(context, Node::Kind::TypeString);
        case EggTokenizerKeyword::Object:
          return this->parseTypeExpressionPrimaryKeyword(context, Node::Kind::TypeObject);
        case EggTokenizerKeyword::Type:
          return PARSE_TODO(tokidx, "type expression 'type' keyword");
        case EggTokenizerKeyword::Var:
          return PARSE_TODO(tokidx, "type expression 'var' keyword");
        case EggTokenizerKeyword::Break:
        case EggTokenizerKeyword::Case:
        case EggTokenizerKeyword::Catch:
        case EggTokenizerKeyword::Continue:
        case EggTokenizerKeyword::Default:
        case EggTokenizerKeyword::Do:
        case EggTokenizerKeyword::Else:
        case EggTokenizerKeyword::False:
        case EggTokenizerKeyword::Finally:
        case EggTokenizerKeyword::For:
        case EggTokenizerKeyword::If:
        case EggTokenizerKeyword::Null:
        case EggTokenizerKeyword::Return:
        case EggTokenizerKeyword::Switch:
        case EggTokenizerKeyword::Throw:
        case EggTokenizerKeyword::True:
        case EggTokenizerKeyword::Try:
        case EggTokenizerKeyword::While:
        case EggTokenizerKeyword::Yield:
          break;
        }
        return PARSE_TODO(tokidx, "type expression primary keyword: ", next.toString());
      }
      return PARSE_TODO(tokidx, "bad type expression primary");
    }
    Partial parseTypeExpressionPrimaryKeyword(Context& context, Node::Kind kind) {
      auto node = this->makeNode(kind, context[0]);
      return context.success(std::move(node), context.tokensBefore + 1);
    }
    Partial parseValueExpression(size_t tokidx) {
      return this->parseValueExpressionTernary(tokidx);
    }
    Partial parseValueExpressionTernary(size_t tokidx) {
      Context context(*this, tokidx);
      auto lhs = this->parseValueExpressionBinary(tokidx);
      if (lhs.succeeded()) {
        if (lhs.after(0).isOperator(EggTokenizerOperator::Query)) {
          auto mid = this->parseValueExpression(lhs.tokensAfter + 1);
          if (!mid.succeeded()) {
            return mid;
          }
          if (mid.after(0).isOperator(EggTokenizerOperator::Colon)) {
            auto rhs = this->parseValueExpression(mid.tokensAfter + 1);
            if (!rhs.succeeded()) {
              return rhs;
            }
            lhs.wrap(Node::Kind::ExprTernary);
            lhs.tokensAfter = rhs.tokensAfter;
            lhs.node->children.emplace_back(std::move(mid.node));
            lhs.node->children.emplace_back(std::move(rhs.node));
            lhs.node->op.valueTernaryOp = egg::ovum::ValueTernaryOp::IfThenElse;
          }
        }
      }
      return lhs;
    }
    Partial parseValueExpressionBinary(size_t tokidx) {
      Context context(*this, tokidx);
      auto lhs = this->parseValueExpressionUnary(tokidx);
      if (!lhs.succeeded()) {
        return lhs;
      }
      const auto& op = lhs.after(0);
      if (op.kind == EggTokenizerKind::Operator) {
        switch (op.value.o) {
        case EggTokenizerOperator::Percent: // "%"
          break;
        case EggTokenizerOperator::Ampersand: // "&"
          break;
        case EggTokenizerOperator::AmpersandAmpersand: // "&&"
          break;
        case EggTokenizerOperator::Star: // "*"
          return this->parseValueExpressionBinaryOperator(lhs, egg::ovum::ValueBinaryOp::Multiply);
        case EggTokenizerOperator::Plus: // "+"
          return this->parseValueExpressionBinaryOperator(lhs, egg::ovum::ValueBinaryOp::Add);
        case EggTokenizerOperator::Slash: // "/"
          return this->parseValueExpressionBinaryOperator(lhs, egg::ovum::ValueBinaryOp::Divide);
        case EggTokenizerOperator::Minus: // "-"
          return this->parseValueExpressionBinaryOperator(lhs, egg::ovum::ValueBinaryOp::Subtract);
        case EggTokenizerOperator::Less: // "<"
          return this->parseValueExpressionBinaryOperator(lhs, egg::ovum::ValueBinaryOp::LessThan);
        case EggTokenizerOperator::ShiftLeft: // "<<"
          return this->parseValueExpressionBinaryOperator(lhs, egg::ovum::ValueBinaryOp::ShiftLeft);
        case EggTokenizerOperator::EqualEqual: // "=="
          return this->parseValueExpressionBinaryOperator(lhs, egg::ovum::ValueBinaryOp::Equal);
        case EggTokenizerOperator::Greater: // ">"
          return this->parseValueExpressionBinaryOperator(lhs, egg::ovum::ValueBinaryOp::GreaterThan);
        case EggTokenizerOperator::GreaterEqual: // ">="
          return this->parseValueExpressionBinaryOperator(lhs, egg::ovum::ValueBinaryOp::GreaterThanOrEqual);
        case EggTokenizerOperator::ShiftRight: // ">>"
          return this->parseValueExpressionBinaryOperator(lhs, egg::ovum::ValueBinaryOp::ShiftRight);
        case EggTokenizerOperator::ShiftRightUnsigned: // ">>>"
          return this->parseValueExpressionBinaryOperator(lhs, egg::ovum::ValueBinaryOp::ShiftRightUnsigned);
        case EggTokenizerOperator::QueryQuery: // "??"
          return this->parseValueExpressionBinaryOperator(lhs, egg::ovum::ValueBinaryOp::IfNull);
        case EggTokenizerOperator::Caret: // "^"
          return this->parseValueExpressionBinaryOperator(lhs, egg::ovum::ValueBinaryOp::BitwiseXor);
        case EggTokenizerOperator::Bar: // "|"
          return this->parseValueExpressionBinaryOperator(lhs, egg::ovum::ValueBinaryOp::BitwiseOr);
        case EggTokenizerOperator::BarBar: // "||"
          return this->parseValueExpressionBinaryOperator(lhs, egg::ovum::ValueBinaryOp::IfTrue);
        case EggTokenizerOperator::Bang: // "!"
        case EggTokenizerOperator::BangEqual: // "!="
        case EggTokenizerOperator::PercentEqual: // "%="
        case EggTokenizerOperator::AmpersandAmpersandEqual: // "&&="
        case EggTokenizerOperator::AmpersandEqual: // "&="
        case EggTokenizerOperator::ParenthesisLeft: // "("
        case EggTokenizerOperator::ParenthesisRight: // ")"
        case EggTokenizerOperator::StarEqual: // "*="
        case EggTokenizerOperator::PlusPlus: // "++"
        case EggTokenizerOperator::PlusEqual: // "+="
        case EggTokenizerOperator::Comma: // ","
        case EggTokenizerOperator::MinusMinus: // "--"
        case EggTokenizerOperator::MinusEqual: // "-="
        case EggTokenizerOperator::Lambda: // "->"
        case EggTokenizerOperator::Dot: // "."
        case EggTokenizerOperator::Ellipsis: // "..."
        case EggTokenizerOperator::SlashEqual: // "/="
        case EggTokenizerOperator::Colon: // ":"
        case EggTokenizerOperator::Semicolon: // ";"
        case EggTokenizerOperator::ShiftLeftEqual: // "<<="
        case EggTokenizerOperator::LessEqual: // "<="
        case EggTokenizerOperator::Equal: // "="
        case EggTokenizerOperator::ShiftRightEqual: // ">>="
        case EggTokenizerOperator::ShiftRightUnsignedEqual: // ">>>="
        case EggTokenizerOperator::Query: // "?"
        case EggTokenizerOperator::QueryQueryEqual: // "??="
        case EggTokenizerOperator::BracketLeft: // "["
        case EggTokenizerOperator::BracketRight: // "]"
        case EggTokenizerOperator::CaretEqual: // "^="
        case EggTokenizerOperator::CurlyLeft: // "{"
        case EggTokenizerOperator::BarEqual: // "|="
        case EggTokenizerOperator::BarBarEqual: // "||="
        case EggTokenizerOperator::CurlyRight: // "}"
        case EggTokenizerOperator::Tilde: // "~"
          return lhs;
        default:
          break;
        }
        return PARSE_TODO(tokidx, "bad binary expression operator: ", op.toString());
      }
      return lhs;
    }
    Partial parseValueExpressionBinaryOperator(Partial& lhs, egg::ovum::ValueBinaryOp op) {
      // WIBBLE precedence
      auto rhs = this->parseValueExpression(lhs.tokensAfter + 1);
      if (rhs.succeeded()) {
        lhs.wrap(Node::Kind::ExprBinary);
        lhs.tokensAfter = rhs.tokensAfter;
        lhs.node->children.emplace_back(std::move(rhs.node));
        lhs.node->op.valueBinaryOp = op;
        return std::move(lhs);
      }
      return rhs;
    }
    Partial parseValueExpressionUnary(size_t tokidx) {
      Context context(*this, tokidx);
      const auto& op = context[0];
      if (op.kind == EggTokenizerKind::Operator) {
        switch (op.value.o) {
        case EggTokenizerOperator::Bang: // "!"
          return this->parseValueExpressionUnaryOperator(tokidx + 1, egg::ovum::ValueUnaryOp::LogicalNot);
        case EggTokenizerOperator::Minus: // "-"
          return this->parseValueExpressionUnaryOperator(tokidx + 1, egg::ovum::ValueUnaryOp::Negate);
        case EggTokenizerOperator::Tilde: // "~"
          return this->parseValueExpressionUnaryOperator(tokidx + 1, egg::ovum::ValueUnaryOp::BitwiseNot);
        case EggTokenizerOperator::BangEqual: // "!="
        case EggTokenizerOperator::Percent: // "%"
        case EggTokenizerOperator::PercentEqual: // "%="
        case EggTokenizerOperator::Ampersand: // "&"
        case EggTokenizerOperator::AmpersandAmpersand: // "&&"
        case EggTokenizerOperator::AmpersandAmpersandEqual: // "&&="
        case EggTokenizerOperator::AmpersandEqual: // "&="
        case EggTokenizerOperator::ParenthesisLeft: // "("
        case EggTokenizerOperator::ParenthesisRight: // ")"
        case EggTokenizerOperator::Star: // "*"
        case EggTokenizerOperator::StarEqual: // "*="
        case EggTokenizerOperator::Plus: // "+"
        case EggTokenizerOperator::PlusPlus: // "++"
        case EggTokenizerOperator::PlusEqual: // "+="
        case EggTokenizerOperator::Comma: // ","
        case EggTokenizerOperator::MinusMinus: // "--"
        case EggTokenizerOperator::MinusEqual: // "-="
        case EggTokenizerOperator::Lambda: // "->"
        case EggTokenizerOperator::Dot: // "."
        case EggTokenizerOperator::Ellipsis: // "..."
        case EggTokenizerOperator::Slash: // "/"
        case EggTokenizerOperator::SlashEqual: // "/="
        case EggTokenizerOperator::Colon: // ":"
        case EggTokenizerOperator::Semicolon: // ";"
        case EggTokenizerOperator::Less: // "<"
        case EggTokenizerOperator::ShiftLeft: // "<<"
        case EggTokenizerOperator::ShiftLeftEqual: // "<<="
        case EggTokenizerOperator::LessEqual: // "<="
        case EggTokenizerOperator::Equal: // "="
        case EggTokenizerOperator::EqualEqual: // "=="
        case EggTokenizerOperator::Greater: // ">"
        case EggTokenizerOperator::GreaterEqual: // ">="
        case EggTokenizerOperator::ShiftRight: // ">>"
        case EggTokenizerOperator::ShiftRightEqual: // ">>="
        case EggTokenizerOperator::ShiftRightUnsigned: // ">>>"
        case EggTokenizerOperator::ShiftRightUnsignedEqual: // ">>>="
        case EggTokenizerOperator::Query: // "?"
        case EggTokenizerOperator::QueryQuery: // "??"
        case EggTokenizerOperator::QueryQueryEqual: // "??="
        case EggTokenizerOperator::BracketLeft: // "["
        case EggTokenizerOperator::BracketRight: // "]"
        case EggTokenizerOperator::Caret: // "^"
        case EggTokenizerOperator::CaretEqual: // "^="
        case EggTokenizerOperator::CurlyLeft: // "{"
        case EggTokenizerOperator::Bar: // "|"
        case EggTokenizerOperator::BarEqual: // "|="
        case EggTokenizerOperator::BarBar: // "||"
        case EggTokenizerOperator::BarBarEqual: // "||="
        case EggTokenizerOperator::CurlyRight: // "}"
          return context.failed(tokidx, "bad unary expression operator: ", op.toString());
        default:
          break;
        }
        return PARSE_TODO(tokidx, "invalid unary expression operator: ", op.toString());
      }
      return this->parseValueExpressionPrimary(tokidx);
    }
    Partial parseValueExpressionUnaryOperator(size_t tokidx, egg::ovum::ValueUnaryOp op) {
      auto rhs = this->parseValueExpression(tokidx);
      if (rhs.succeeded()) {
        rhs.wrap(Node::Kind::ExprUnary);
        rhs.node->op.valueUnaryOp = op;
      }
      return rhs;
    }
    Partial parseValueExpressionPrimary(size_t tokidx) {
      Context context(*this, tokidx);
      auto partial = this->parseValueExpressionPrimaryPrefix(tokidx);
      while (partial.succeeded()) {
        if (!this->parseValueExpressionPrimarySuffix(partial)) {
          break;
        }
      }
      return partial;
    }
    Partial parseValueExpressionPrimaryPrefix(size_t tokidx) {
      std::unique_ptr<Node> child;
      Context context(*this, tokidx);
      switch (context[0].kind) {
      case EggTokenizerKind::Integer:
        child = this->makeNodeInt(Node::Kind::Literal, context[0]);
        return context.success(std::move(child), tokidx + 1);
      case EggTokenizerKind::Float:
        child = this->makeNodeFloat(Node::Kind::Literal, context[0]);
        return context.success(std::move(child), tokidx + 1);
      case EggTokenizerKind::String:
        // TODO: join contiguous strings
        child = this->makeNodeString(Node::Kind::Literal, context[0]);
        return context.success(std::move(child), tokidx + 1);
      case EggTokenizerKind::Identifier:
        child = this->makeNodeString(Node::Kind::ExprVar, context[0]);
        return context.success(std::move(child), tokidx + 1);
      case EggTokenizerKind::Keyword:
        child = this->makeNodeString(Node::Kind::ExprVar, context[0]);
        return context.success(std::move(child), tokidx + 1);
      case EggTokenizerKind::Attribute:
        return PARSE_TODO(tokidx, "bad expression attribute");
      case EggTokenizerKind::Operator:
        return PARSE_TODO(tokidx, "bad expression operator");
      case EggTokenizerKind::EndOfFile:
        return context.failed(tokidx, "Expected expression, but got end-of-file");
      }
      return PARSE_TODO(tokidx, "bad expression primary prefix");
    }
    bool parseValueExpressionPrimarySuffix(Partial& partial) {
      assert(partial.succeeded());
      auto& next = partial.after(0);
      if (next.isOperator(EggTokenizerOperator::ParenthesisLeft)) {
        // Function call
        partial.wrap(Node::Kind::ExprCall);
        partial.tokensAfter++;
        if (partial.after(0).isOperator(EggTokenizerOperator::ParenthesisRight)) {
          // No arguments
          partial.tokensAfter++;
          return true;
        }
        for (;;) {
          // Parse the arguments
          auto argument = this->parseValueExpression(partial.tokensAfter);
          if (!argument.succeeded()) {
            partial.fail(argument);
            return false;
          }
          auto& separator = argument.after(0);
          partial.node->children.emplace_back(std::move(argument.node));
          partial.tokensAfter = argument.tokensAfter + 1;
          if (separator.isOperator(EggTokenizerOperator::ParenthesisRight)) {
            return true;
          }
          if (!separator.isOperator(EggTokenizerOperator::Comma)) {
            partial.fail("TODO: Expected ',' between function call arguments, but got ", separator.toString());
            return false;
          }
        }
      }
      if (next.isOperator(EggTokenizerOperator::Dot)) {
        partial.fail("TODO: '.' expression suffix not yet implemented");
        return false;
      }
      if (next.isOperator(EggTokenizerOperator::BracketLeft)) {
        partial.fail("TODO: '[' expression suffix not yet implemented");
        return false;
      }
      return false;
    }
    std::unique_ptr<Node> makeNode(Node::Kind kind, const Location& begin, const Location& end) {
      auto node = std::make_unique<Node>(kind);
      node->begin = begin;
      node->end = end;
      return node;
    }
    std::unique_ptr<Node> makeNode(Node::Kind kind, const EggTokenizerItem& item) {
      auto node = std::make_unique<Node>(kind);
      node->begin.line = item.line;
      node->begin.column = item.column;
      auto width = item.width();
      if (width > 0) {
        node->end.line = item.line;
        node->end.column = item.column + width;
      } else {
        node->end.line = 0;
        node->end.column = 0;
      }
      return node;
    }
    std::unique_ptr<Node> makeNodeInt(Node::Kind kind, const EggTokenizerItem& item) {
      auto node = this->makeNode(kind, item);
      node->value = egg::ovum::ValueFactory::createInt(this->allocator, item.value.i);
      return node;
    }
    std::unique_ptr<Node> makeNodeFloat(Node::Kind kind, const EggTokenizerItem& item) {
      auto node = this->makeNode(kind, item);
      node->value = egg::ovum::ValueFactory::createFloat(this->allocator, item.value.f);
      return node;
    }
    std::unique_ptr<Node> makeNodeString(Node::Kind kind, const EggTokenizerItem& item) {
      auto node = this->makeNode(kind, item);
      node->value = egg::ovum::ValueFactory::createString(this->allocator, item.value.s);
      return node;
    }
  };

  EggParser::Partial::Partial(const Context& context, std::unique_ptr<Node>&& node, size_t tokensAfter, size_t issuesAfter)
    : parser(context.parser),
      node(std::move(node)),
      tokensBefore(context.tokensBefore),
      issuesBefore(context.issuesBefore),
      tokensAfter(tokensAfter),
      issuesAfter(issuesAfter) {
      assert(this->tokensBefore <= this->tokensAfter);
      assert(this->issuesBefore <= this->issuesAfter);
  }
}

std::shared_ptr<IEggParser> EggParserFactory::createFromTokenizer(egg::ovum::IAllocator& allocator, const std::shared_ptr<IEggTokenizer>& tokenizer) {
  return std::make_shared<EggParser>(allocator, tokenizer);
}
