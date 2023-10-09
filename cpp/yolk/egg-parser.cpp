#include "yolk/yolk.h"
#include "yolk/lexers.h"
#include "yolk/egg-tokenizer.h"
#include "yolk/egg-parser.h"

using namespace egg::yolk;

namespace {
  class EggParserTokens {
    EggParserTokens(const EggParserTokens&) = delete;
    EggParserTokens& operator=(const EggParserTokens&) = delete;
  private:
    egg::ovum::IAllocator& allocator;
    std::shared_ptr<IEggTokenizer> tokenizer;
    bool exhausted;
    size_t absolute;
    std::deque<EggTokenizerItem> items;
  public:
    EggParserTokens(egg::ovum::IAllocator& allocator, const std::shared_ptr<IEggTokenizer>& tokenizer)
      : allocator(allocator),
      tokenizer(tokenizer),
      exhausted(false),
      absolute(0) {
      assert(this->tokenizer != nullptr);
    }
    egg::ovum::String resource() const {
      return this->tokenizer->resource();
    }
    const EggTokenizerItem& operator[](size_t index) {
      assert(index >= this->absolute);
      index -= this->absolute;
      if (index < this->items.size()) {
        return this->items[index];
      }
      auto remaining = index - this->items.size() + 1;
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
    egg::ovum::IAllocator& allocator; // WIBBLE shared with tokens?
    EggParserTokens tokens;
    std::vector<Issue> issues;
  public:
    EggParser(egg::ovum::IAllocator& allocator, const std::shared_ptr<IEggTokenizer>& tokenizer)
      : allocator(allocator),
      tokens(allocator, tokenizer) {
    }
    virtual Result parse() override {
      // WIBBLE
      assert(this->issues.empty());
      auto root = std::make_shared<Node>();
      root->kind = Node::Kind::ModuleRoot;
      size_t tokidx = 0;
      try {
        while (this->tokens[tokidx].kind != EggTokenizerKind::EndOfFile) {
          auto partial = parseModule(tokidx);
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
  private:
    struct Context;
    struct Partial {
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
      auto& item0 = this->tokens[context.tokensBefore];
      Location location0{ item0.line, item0.column };
      auto& item1 = this->tokens[tokidx];
      Location location1{ item1.line, item1.column + item1.width() };
      return { Issue::Severity::Error, message, location0, location1 };
    }

    Partial parseModule(size_t tokidx) {
      Context context(*this, tokidx);
      if (this->tokens[tokidx + 0].kind == EggTokenizerKind::Identifier &&
          this->tokens[tokidx + 1].isOperator(EggTokenizerOperator::ParenthesisLeft) &&
          this->tokens[tokidx + 2].kind == EggTokenizerKind::String &&
          this->tokens[tokidx + 3].isOperator(EggTokenizerOperator::ParenthesisRight) &&
          this->tokens[tokidx + 4].isOperator(EggTokenizerOperator::Semicolon)) {
        auto child = this->makeNode(Node::Kind::StmtCall);
        auto function = this->makeNodeString(Node::Kind::ExprVar, this->tokens[tokidx].value.s);
        child->children.emplace_back(std::move(function));
        auto argument = this->makeNodeString(Node::Kind::Literal, this->tokens[tokidx + 2].value.s);
        child->children.emplace_back(std::move(argument));
        return context.success(std::move(child), tokidx + 5); // WIBBLE
      }
      return context.failed(tokidx, "WIBBLE");
    }

    std::unique_ptr<Node> makeNode(Node::Kind kind) {
      return std::make_unique<Node>(kind);
    }

    std::unique_ptr<Node> makeNodeString(Node::Kind kind, const egg::ovum::String& svalue) {
      auto node = this->makeNode(kind);
      node->value = egg::ovum::ValueFactory::createString(this->allocator, svalue);
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
