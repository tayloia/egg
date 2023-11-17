#include "yolk/yolk.h"
#include "yolk/lexers.h"
#include "yolk/egg-tokenizer.h"
#include "yolk/egg-parser.h"

#define PARSE_TODO(tokidx, ...) context.failed(tokidx, "PARSE_TODO: ", __FILE__, "(", __LINE__, "): ", __VA_ARGS__)

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
      size_t tokidx = 0;
      try {
        while (this->tokens.getAbsolute(tokidx).kind != EggTokenizerKind::EndOfFile) {
          auto partial = this->parseModule(tokidx);
          if (!partial.succeeded()) {
            return { nullptr, std::move(this->issues) };
          }
          tokidx = partial.accept(root->children);
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
    };
    template<typename... ARGS>
    Issue createIssue(size_t tokensBefore, size_t tokensAfter, ARGS&&... args) {
      assert(tokensBefore <= tokensAfter);
      egg::ovum::StringBuilder sb;
      auto message = sb.add(std::forward<ARGS>(args)...).build(this->allocator);
      auto& item0 = this->tokens.getAbsolute(tokensBefore);
      Location location0{ item0.line, item0.column };
      auto& item1 = this->tokens.getAbsolute(tokensAfter);
      Location location1{ item1.line, item1.column + item1.width() };
      return { Issue::Severity::Error, message, location0, location1 };
    }
    Partial parseModule(size_t tokidx) {
      Context context(*this, tokidx);
      auto partial = this->parseValueExpressionPrimary(tokidx);
      if (partial.succeeded()) {
        if (partial.after(0).isOperator(EggTokenizerOperator::Semicolon)) {
          // The whole statement is actually an expression
          if (partial.node->kind == Node::Kind::ExprCall) {
            // Wrap in a statement and swallow the semicolon
            partial.wrap(Node::Kind::StmtCall);
            partial.tokensAfter++;
            return partial;
          }
          return PARSE_TODO(tokidx, "non-function expression statement");
        }
        return PARSE_TODO(partial.tokensAfter, "unexpected token after module-level expression: '", partial.after(0).toString(), "'");
      }
      return partial;
    }
    Partial parseValueExpression(size_t tokidx) {
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
            lhs.node->op.ternary = egg::ovum::TernaryOp::IfThenElse;
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
          return this->parseValueExpressionBinaryOperator(lhs, egg::ovum::BinaryOp::Multiply);
        case EggTokenizerOperator::Plus: // "+"
          return this->parseValueExpressionBinaryOperator(lhs, egg::ovum::BinaryOp::Add);
        case EggTokenizerOperator::Slash: // "/"
          return this->parseValueExpressionBinaryOperator(lhs, egg::ovum::BinaryOp::Divide);
        case EggTokenizerOperator::Minus: // "-"
          return this->parseValueExpressionBinaryOperator(lhs, egg::ovum::BinaryOp::Subtract);
        case EggTokenizerOperator::Less: // "<"
          return this->parseValueExpressionBinaryOperator(lhs, egg::ovum::BinaryOp::LessThan);
        case EggTokenizerOperator::ShiftLeft: // "<<"
          return this->parseValueExpressionBinaryOperator(lhs, egg::ovum::BinaryOp::ShiftLeft);
        case EggTokenizerOperator::EqualEqual: // "=="
          return this->parseValueExpressionBinaryOperator(lhs, egg::ovum::BinaryOp::Equal);
        case EggTokenizerOperator::Greater: // ">"
          return this->parseValueExpressionBinaryOperator(lhs, egg::ovum::BinaryOp::GreaterThan);
        case EggTokenizerOperator::GreaterEqual: // ">="
          return this->parseValueExpressionBinaryOperator(lhs, egg::ovum::BinaryOp::GreaterThanOrEqual);
        case EggTokenizerOperator::ShiftRight: // ">>"
          return this->parseValueExpressionBinaryOperator(lhs, egg::ovum::BinaryOp::ShiftRight);
        case EggTokenizerOperator::ShiftRightUnsigned: // ">>>"
          return this->parseValueExpressionBinaryOperator(lhs, egg::ovum::BinaryOp::ShiftRightUnsigned);
        case EggTokenizerOperator::QueryQuery: // "??"
          return this->parseValueExpressionBinaryOperator(lhs, egg::ovum::BinaryOp::IfNull);
        case EggTokenizerOperator::Caret: // "^"
          return this->parseValueExpressionBinaryOperator(lhs, egg::ovum::BinaryOp::BitwiseXor);
        case EggTokenizerOperator::Bar: // "|"
          return this->parseValueExpressionBinaryOperator(lhs, egg::ovum::BinaryOp::BitwiseOr);
        case EggTokenizerOperator::BarBar: // "||"
          return this->parseValueExpressionBinaryOperator(lhs, egg::ovum::BinaryOp::IfTrue);
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
    Partial parseValueExpressionBinaryOperator(Partial& lhs, egg::ovum::BinaryOp op) {
      // WIBBLE precedence
      auto rhs = this->parseValueExpression(lhs.tokensAfter + 1);
      if (rhs.succeeded()) {
        lhs.wrap(Node::Kind::ExprBinary);
        lhs.tokensAfter = rhs.tokensAfter;
        lhs.node->children.emplace_back(std::move(rhs.node));
        lhs.node->op.binary = op;
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
          return this->parseValueExpressionUnaryOperator(tokidx + 1, egg::ovum::UnaryOp::LogicalNot);
        case EggTokenizerOperator::Minus: // "-"
          return this->parseValueExpressionUnaryOperator(tokidx + 1, egg::ovum::UnaryOp::Negate);
        case EggTokenizerOperator::Tilde: // "~"
          return this->parseValueExpressionUnaryOperator(tokidx + 1, egg::ovum::UnaryOp::BitwiseNot);
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
    Partial parseValueExpressionUnaryOperator(size_t tokidx, egg::ovum::UnaryOp op) {
      auto rhs = this->parseValueExpression(tokidx);
      if (rhs.succeeded()) {
        rhs.wrap(Node::Kind::ExprUnary);
        rhs.node->op.unary = op;
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
        // TODO: Multiple arguments
        auto argument = this->parseValueExpression(partial.tokensAfter + 1);
        if (!argument.succeeded()) {
          partial.fail(argument);
          return false;
        }
        if (argument.after(0).isOperator(EggTokenizerOperator::ParenthesisRight)) {
          partial.wrap(Node::Kind::ExprCall);
          partial.tokensAfter = argument.tokensAfter + 1;
          partial.node->children.emplace_back(std::move(argument.node));
          return true;
        }
        partial.fail("TODO: Expected ')' after function call arguments, but got ", argument.after(0).toString());
        return false;
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
