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
      Partial failed(size_t tokidx, ARGS&&... args) const {
        return this->failed(parser.createIssue(*this, tokidx, std::forward<ARGS>(args)...));
      }
    };

    template<typename... ARGS>
    Issue createIssue(const Context& context, size_t tokidx, ARGS&&... args) {
      assert(context.tokensBefore <= tokidx);
      egg::ovum::StringBuilder sb;
      auto message = sb.add(std::forward<ARGS>(args)...).build(this->allocator);
      auto& item0 = this->tokens.getAbsolute(context.tokensBefore);
      Location location0{ item0.line, item0.column };
      auto& item1 = this->tokens.getAbsolute(tokidx);
      Location location1{ item1.line, item1.column + item1.width() };
      return { Issue::Severity::Error, message, location0, location1 };
    }

    Partial parseModule(size_t tokidx) {
      Context context(*this, tokidx);
      auto partial = this->parseExpression(tokidx);
      if (partial.succeeded() && partial.after(0).isOperator(EggTokenizerOperator::Semicolon)) {
        // The whole statement is actually an expression
        if (partial.node->kind == Node::Kind::ExprCall) {
          // Wrap in a statement and swallow the semicolon
          partial.wrap(Node::Kind::StmtCall);
          partial.tokensAfter++;
          return partial;
        }
        return PARSE_TODO(tokidx, "non-function expression statement");
      }
      return PARSE_TODO(tokidx, "bad module-level statement");
    }

    Partial parseExpression(size_t tokidx) {
      return this->parseExpressionPrimary(tokidx);
    }

    Partial parseExpressionPrimary(size_t tokidx) {
      Context context(*this, tokidx);
      auto expr = this->parseExpressionPrimaryPrefix(tokidx);
      if (expr.succeeded()) {
        while (this->parseExpressionPrimarySuffix(expr)) {
          // 'expr' has been updated
        }
      }
      return expr;
    }

    Partial parseExpressionPrimaryPrefix(size_t tokidx) {
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

    bool parseExpressionPrimarySuffix(Partial& prefix) {
      auto& next = prefix.after(0);
      if (next.isOperator(EggTokenizerOperator::ParenthesisLeft)) {
        // TODO: Multiple arguments
        auto argument = this->parseExpression(prefix.tokensAfter + 1);
        if (argument.succeeded()) {
          if (argument.after(0).isOperator(EggTokenizerOperator::ParenthesisRight)) {
            prefix.wrap(Node::Kind::ExprCall);
            prefix.tokensAfter = argument.tokensAfter + 1;
            prefix.node->children.emplace_back(std::move(argument.node));
            return true;
          }
        }
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
