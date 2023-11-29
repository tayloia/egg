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
      bool skipped() const {
        return (this->node == nullptr) && (this->issuesBefore == this->issuesAfter);
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
        auto issue = this->parser.createIssue(Issue::Severity::Error, this->tokensBefore, this->tokensAfter, std::forward<ARGS>(args)...);
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
      Partial skip() const {
        this->parser.issues.resize(this->issuesBefore);
        return Partial(*this, nullptr, this->tokensBefore, this->issuesBefore);
      }
      template<typename... ARGS>
      void warning(size_t tokensBefore, size_t tokensAfter, ARGS&&... args) const {
        this->parser.issues.push_back(this->parser.createIssue(Issue::Severity::Warning, tokensBefore, tokensAfter, std::forward<ARGS>(args)...));
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
        return this->failed(this->parser.createIssue(Issue::Severity::Error, this->tokensBefore, tokensAfter, std::forward<ARGS>(args)...));
      }
      template<typename... ARGS>
      Partial expected(size_t tokensAfter, ARGS&&... args) const {
        auto actual = this->parser.getAbsolute(tokensAfter).toString();
        return this->failed(tokensAfter, "Expected ", std::forward<ARGS>(args)..., ", but got ", actual);
      }
      template<typename... ARGS>
      Partial todo(size_t tokensAfter, const char* file, size_t line, ARGS&&... args) const {
        // TODO: remove
        egg::ovum::StringBuilder sb;
        std::cout << file << '(' << line << "): PARSE_TODO: " << sb.add(std::forward<ARGS>(args)...).toUTF8() << std::endl;
        return this->failed(tokensAfter, "PARSE_TODO: ", std::forward<ARGS>(args)...);
      }
    };
    template<typename... ARGS>
    Issue createIssue(Issue::Severity severity, size_t tokensBefore, size_t tokensAfter, ARGS&&... args) {
      assert(tokensBefore <= tokensAfter);
      auto message = egg::ovum::StringBuilder::concat(this->allocator, std::forward<ARGS>(args)...);
      auto& item0 = this->getAbsolute(tokensBefore);
      Location location0{ item0.line, item0.column };
      auto& item1 = this->getAbsolute(tokensAfter);
      Location location1{ item1.line, item1.column + item1.width() };
      return { severity, message, location0, location1 };
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
            return this->parseStatementBreak(tokidx);
          case EggTokenizerKeyword::Case:
            return this->parseStatementCase(tokidx);
          case EggTokenizerKeyword::Catch:
            return this->parseStatementCatch(tokidx);
          case EggTokenizerKeyword::Continue:
            return this->parseStatementContinue(tokidx);
          case EggTokenizerKeyword::Default:
            return this->parseStatementDefault(tokidx);
          case EggTokenizerKeyword::Do:
            return this->parseStatementDo(tokidx);
          case EggTokenizerKeyword::Else:
            return this->parseStatementElse(tokidx);
          case EggTokenizerKeyword::Finally:
            return this->parseStatementFinally(tokidx);
          case EggTokenizerKeyword::For:
            return this->parseStatementFor(tokidx);
          case EggTokenizerKeyword::If:
            return this->parseStatementIf(tokidx);
          case EggTokenizerKeyword::Return:
            return this->parseStatementReturn(tokidx);
          case EggTokenizerKeyword::Switch:
            return this->parseStatementSwitch(tokidx);
          case EggTokenizerKeyword::Throw:
            return this->parseStatementThrow(tokidx);
          case EggTokenizerKeyword::Try:
            return this->parseStatementTry(tokidx);
          case EggTokenizerKeyword::While:
            return this->parseStatementWhile(tokidx);
          case EggTokenizerKeyword::Yield:
            return this->parseStatementYield(tokidx);
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
    Partial parseStatementBlock(size_t tokidx) {
      Context context(*this, tokidx);
      const auto* head = &this->getAbsolute(tokidx);
      assert(head->isOperator(EggTokenizerOperator::CurlyLeft));
      auto block = this->makeNode(Node::Kind::StmtBlock, *head);
      head = &this->getAbsolute(++tokidx);
      while (!head->isOperator(EggTokenizerOperator::CurlyRight)) {
        auto stmt = this->parseStatement(tokidx);
        if (!stmt.succeeded()) {
          return stmt;
        }
        block->children.emplace_back(std::move(stmt.node));
        tokidx = stmt.tokensAfter;
        head = &this->getAbsolute(tokidx);
      }
      return context.success(std::move(block), tokidx + 1);
    }
    Partial parseStatementBreak(size_t tokidx) {
      Context context(*this, tokidx);
      assert(context[0].isKeyword(EggTokenizerKeyword::Break));
      return PARSE_TODO(tokidx, "statement keyword: ", context[0].toString());
    }
    Partial parseStatementCase(size_t tokidx) {
      Context context(*this, tokidx);
      assert(context[0].isKeyword(EggTokenizerKeyword::Case));
      return PARSE_TODO(tokidx, "statement keyword: ", context[0].toString());
    }
    Partial parseStatementCatch(size_t tokidx) {
      Context context(*this, tokidx);
      assert(context[0].isKeyword(EggTokenizerKeyword::Catch));
      return PARSE_TODO(tokidx, "statement keyword: ", context[0].toString());
    }
    Partial parseStatementContinue(size_t tokidx) {
      Context context(*this, tokidx);
      assert(context[0].isKeyword(EggTokenizerKeyword::Continue));
      return PARSE_TODO(tokidx, "statement keyword: ", context[0].toString());
    }
    Partial parseStatementDefault(size_t tokidx) {
      Context context(*this, tokidx);
      assert(context[0].isKeyword(EggTokenizerKeyword::Default));
      return PARSE_TODO(tokidx, "statement keyword: ", context[0].toString());
    }
    Partial parseStatementDo(size_t tokidx) {
      Context context(*this, tokidx);
      assert(context[0].isKeyword(EggTokenizerKeyword::Do));
      return PARSE_TODO(tokidx, "statement keyword: ", context[0].toString());
    }
    Partial parseStatementElse(size_t tokidx) {
      Context context(*this, tokidx);
      assert(context[0].isKeyword(EggTokenizerKeyword::Else));
      return PARSE_TODO(tokidx, "statement keyword: ", context[0].toString());
    }
    Partial parseStatementFinally(size_t tokidx) {
      Context context(*this, tokidx);
      assert(context[0].isKeyword(EggTokenizerKeyword::Finally));
      return PARSE_TODO(tokidx, "statement keyword: ", context[0].toString());
    }
    Partial parseStatementFor(size_t tokidx) {
      Context context(*this, tokidx);
      assert(context[0].isKeyword(EggTokenizerKeyword::For));
      auto& next = this->getAbsolute(tokidx + 1);
      if (!next.isOperator(EggTokenizerOperator::ParenthesisLeft)) {
        return context.failed(tokidx + 1, "'(' after keyword 'for'");
      }
      auto each = this->parseStatementForEach(tokidx);
      if (!each.skipped()) {
        return each;
      }
      return this->parseStatementForLoop(tokidx);
    }
    Partial parseStatementForEach(size_t tokidx) {
      Context context(*this, tokidx);
      assert(context[0].isKeyword(EggTokenizerKeyword::For));
      assert(context[1].isOperator(EggTokenizerOperator::ParenthesisLeft));
      if (!context[2].isKeyword(EggTokenizerKeyword::Var)) {
        // for ( <target> : <expr> ) { <bloc> }
        auto type = this->parseTypeExpression(tokidx + 2);
        if (!type.succeeded()) {
          return context.skip();
        }
        if (type.after(0).kind != EggTokenizerKind::Identifier) {
          return context.expected(type.tokensAfter, "identifier after type in 'for' statement");
        }
        return this->parseStatementForEachIdentifier(type);
      } else if (!context[3].isOperator(EggTokenizerOperator::Query)) {
        // for ( var <identifier> : <expr> ) { <bloc> }
        if (context[3].kind != EggTokenizerKind::Identifier) {
          return context.expected(tokidx + 3, "identifier after 'var' in 'for' statement");
        }
        auto node = this->makeNodeString(Node::Kind::TypeInfer, context[0]);
        auto type = context.success(std::move(node), tokidx + 3);
        return this->parseStatementForEachIdentifier(type);
      } else {
        // for ( var ? <identifier> : <expr> ) { <bloc> }
        if (context[4].kind != EggTokenizerKind::Identifier) {
          return context.expected(tokidx + 4, "identifier after 'var?' in 'for' statement");
        }
        auto node = this->makeNodeString(Node::Kind::TypeInferQ, context[0]);
        auto type = context.success(std::move(node), tokidx + 4);
        return this->parseStatementForEachIdentifier(type);
      }
    }
    Partial parseStatementForEachIdentifier(Partial& type) {
      // <identifier> : <expr> ) { <bloc> }
      assert(type.succeeded());
      Context context(*this, type.tokensAfter);
      assert(context[0].kind == EggTokenizerKind::Identifier);
      if (!context[1].isOperator(EggTokenizerOperator::Colon)) {
        // It's probably a for-loop statement
        return context.skip();
      }
      auto expr = this->parseValueExpression(type.tokensAfter + 2);
      if (!expr.succeeded()) {
        return expr;
      }
      if (!expr.after(0).isOperator(EggTokenizerOperator::ParenthesisRight)) {
        return context.expected(expr.tokensAfter, "')' in 'for' each statement");
      }
      if (!expr.after(1).isOperator(EggTokenizerOperator::CurlyLeft)) {
        return context.expected(expr.tokensAfter + 1, "'{' after ')' in 'for' loop statement");
      }
      auto bloc = this->parseStatementBlock(expr.tokensAfter + 1);
      if (!bloc.succeeded()) {
        return bloc;
      }
      auto stmt = this->makeNodeString(Node::Kind::StmtForEach, context[0]);
      stmt->children.emplace_back(std::move(type.node));
      stmt->children.emplace_back(std::move(expr.node));
      stmt->children.emplace_back(std::move(bloc.node));
      return context.success(std::move(stmt), bloc.tokensAfter);
    }
    Partial parseStatementForLoop(size_t tokidx) {
      // for ( <init> ; <cond> ; <adva> ) { <bloc> }
      Context context(*this, tokidx);
      assert(context[0].isKeyword(EggTokenizerKeyword::For));
      assert(context[1].isOperator(EggTokenizerOperator::ParenthesisLeft));
      auto init = this->parseStatementSimple(tokidx + 2);
      if (!init.succeeded()) {
        return init;
      }
      if (!init.after(0).isOperator(EggTokenizerOperator::Semicolon)) {
        return context.expected(init.tokensAfter, "';' after first clause of 'for' loop statement");
      }
      auto cond = this->parseValueCondition(init.tokensAfter + 1);
      if (!cond.succeeded()) {
        return cond;
      }
      if (!cond.after(0).isOperator(EggTokenizerOperator::Semicolon)) {
        return context.expected(cond.tokensAfter, "';' after condition clause of 'for' loop statement");
      }
      auto adva = this->parseStatementSimple(cond.tokensAfter + 1);
      if (!adva.succeeded()) {
        return adva;
      }
      if (!adva.after(0).isOperator(EggTokenizerOperator::ParenthesisRight)) {
        return context.expected(adva.tokensAfter, "')' after third clause of 'for' loop statement");
      }
      if (!adva.after(1).isOperator(EggTokenizerOperator::CurlyLeft)) {
        return context.expected(adva.tokensAfter + 1, "'{' after ')' in 'for' loop statement");
      }
      auto bloc = this->parseStatementBlock(adva.tokensAfter + 1);
      if (!bloc.succeeded()) {
        return bloc;
      }
      auto stmt = this->makeNode(Node::Kind::StmtForLoop, context[0]);
      stmt->children.emplace_back(std::move(init.node));
      stmt->children.emplace_back(std::move(cond.node));
      stmt->children.emplace_back(std::move(adva.node));
      stmt->children.emplace_back(std::move(bloc.node));
      return context.success(std::move(stmt), bloc.tokensAfter);
    }
    Partial parseStatementIf(size_t tokidx) {
      Context context(*this, tokidx);
      assert(context[0].isKeyword(EggTokenizerKeyword::If));
      return PARSE_TODO(tokidx, "statement keyword: ", context[0].toString());
    }
    Partial parseStatementReturn(size_t tokidx) {
      Context context(*this, tokidx);
      assert(context[0].isKeyword(EggTokenizerKeyword::Return));
      return PARSE_TODO(tokidx, "statement keyword: ", context[0].toString());
    }
    Partial parseStatementSwitch(size_t tokidx) {
      Context context(*this, tokidx);
      assert(context[0].isKeyword(EggTokenizerKeyword::Switch));
      return PARSE_TODO(tokidx, "statement keyword: ", context[0].toString());
    }
    Partial parseStatementThrow(size_t tokidx) {
      Context context(*this, tokidx);
      assert(context[0].isKeyword(EggTokenizerKeyword::Throw));
      return PARSE_TODO(tokidx, "statement keyword: ", context[0].toString());
    }
    Partial parseStatementTry(size_t tokidx) {
      Context context(*this, tokidx);
      assert(context[0].isKeyword(EggTokenizerKeyword::Try));
      return PARSE_TODO(tokidx, "statement keyword: ", context[0].toString());
    }
    Partial parseStatementWhile(size_t tokidx) {
      Context context(*this, tokidx);
      assert(context[0].isKeyword(EggTokenizerKeyword::While));
      return PARSE_TODO(tokidx, "statement keyword: ", context[0].toString());
    }
    Partial parseStatementYield(size_t tokidx) {
      Context context(*this, tokidx);
      assert(context[0].isKeyword(EggTokenizerKeyword::Yield));
      return PARSE_TODO(tokidx, "statement keyword: ", context[0].toString());
    }
    Partial parseStatementSimple(size_t tokidx) {
      Context context(*this, tokidx);
      auto defn = this->parseDefinitionVariable(tokidx);
      if (!defn.skipped()) {
        return defn;
      }
      auto nudge = this->parseStatementNudge(tokidx);
      if (!nudge.skipped()) {
        return nudge;
      }
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
          return this->parseDefinitionVariableInferred(tokidx + 2, context[0], true);
        }
        return this->parseDefinitionVariableInferred(tokidx + 1, context[0], false);
      }
      auto partial = this->parseTypeExpression(tokidx);
      if (partial.succeeded()) {
        return this->parseDefinitionVariableExplicit(partial.tokensAfter, partial.node);
      }
      return partial;
    }
    Partial parseDefinitionVariableInferred(size_t tokidx, const EggTokenizerItem& var, bool nullable) {
      Context context(*this, tokidx);
      if (context[0].kind != EggTokenizerKind::Identifier) {
        return context.expected(tokidx, "identifier after '", nullable ? "var?" : "var", "' in variable definition");
      }
      if (!context[1].isOperator(EggTokenizerOperator::Equal)) {
        return context.failed(tokidx, "Cannot declare variable '", context[0].value.s, "' using '", nullable ? "var?" : "var", "' without an initial value");
      }
      // var? <identifier> = <expr>
      auto expr = this->parseValueExpression(tokidx + 2);
      if (expr.succeeded()) {
        auto type = this->makeNode(nullable ? Node::Kind::TypeInferQ : Node::Kind::TypeInfer, var);
        auto stmt = this->makeNodeString(Node::Kind::StmtDefineVariable, context[0]);
        stmt->children.emplace_back(std::move(type));
        stmt->children.emplace_back(std::move(expr.node));
        return context.success(std::move(stmt), expr.tokensAfter);
      }
      return expr;
    }
    Partial parseDefinitionVariableExplicit(size_t tokidx, std::unique_ptr<Node>& ptype) {
      assert(ptype != nullptr);
      Context context(*this, tokidx);
      if (context[0].kind != EggTokenizerKind::Identifier) {
        return context.expected(tokidx, "identifier after type in variable definition");
      }
      if (context[1].isOperator(EggTokenizerOperator::Equal)) {
        // <type> <identifier> = <expr>
        auto expr = this->parseValueExpression(tokidx + 2);
        if (expr.succeeded()) {
          auto stmt = this->makeNodeString(Node::Kind::StmtDefineVariable, context[0]);
          stmt->children.emplace_back(std::move(ptype));
          stmt->children.emplace_back(std::move(expr.node));
          return context.success(std::move(stmt), expr.tokensAfter);
        }
        return expr;
      }
      // <type> <identifier>
      auto stmt = this->makeNodeString(Node::Kind::StmtDeclareVariable, context[0]);
      stmt->children.emplace_back(std::move(ptype));
      return context.success(std::move(stmt), tokidx + 1);
    }
    Partial parseStatementNudge(size_t tokidx) {
      Context context(*this, tokidx);
      if (context[0].isOperator(EggTokenizerOperator::PlusPlus)) {
        // ++<target>
        auto target = this->parseTarget(tokidx + 1);
        if (target.succeeded()) {
          target.wrap(Node::Kind::StmtMutate);
          target.node->op.valueMutationOp = egg::ovum::ValueMutationOp::Increment;
        }
        return target;
      } else if (context[0].isOperator(EggTokenizerOperator::MinusMinus)) {
        // --<target>
        auto target = this->parseTarget(tokidx + 1);
        if (target.succeeded()) {
          target.wrap(Node::Kind::StmtMutate);
          target.node->op.valueMutationOp = egg::ovum::ValueMutationOp::Decrement;
        }
        return target;
      }
      return context.skip();
    }
    Partial parseTarget(size_t tokidx) {
      // TODO
      return this->parseValueExpression(tokidx);
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
      // Also handles map syntax: type[indextype]
      Context context(*this, tokidx);
      auto partial = this->parseTypeExpressionPrimary(tokidx);
      while (partial.succeeded()) {
        auto& next = partial.after(0);
        if (next.isOperator(EggTokenizerOperator::Query)) {
          // type?
          if ((partial.node->kind == Node::Kind::TypeUnary) && (partial.node->op.typeUnaryOp == egg::ovum::TypeUnaryOp::Nullable)) {
            context.warning(partial.tokensAfter, partial.tokensAfter + 1, "Redundant repetition of type suffix '?'");
          } else {
            partial.wrap(Node::Kind::TypeUnary);
            partial.node->op.typeUnaryOp = egg::ovum::TypeUnaryOp::Nullable;
          }
          partial.tokensAfter++;
        } else if (next.isOperator(EggTokenizerOperator::Star)) {
          // type*
          partial.wrap(Node::Kind::TypeUnary);
          partial.node->op.typeUnaryOp = egg::ovum::TypeUnaryOp::Pointer;
          partial.tokensAfter++;
        } else if (next.isOperator(EggTokenizerOperator::Bang)) {
          // type!
          partial.wrap(Node::Kind::TypeUnary);
          partial.node->op.typeUnaryOp = egg::ovum::TypeUnaryOp::Iterator;
          partial.tokensAfter++;
        } else if (next.isOperator(EggTokenizerOperator::BracketLeft)) {
          if (partial.after(1).isOperator(EggTokenizerOperator::BracketRight)) {
            // type[]
            partial.wrap(Node::Kind::TypeUnary);
            partial.node->op.typeUnaryOp = egg::ovum::TypeUnaryOp::Array;
            partial.tokensAfter += 2;
          } else {
            // type[indextype]
            auto index = this->parseTypeExpression(partial.tokensAfter + 1);
            if (!index.succeeded()) {
              return index;
            }
            if (!index.after(0).isOperator(EggTokenizerOperator::BracketRight)) {
              return context.expected(index.tokensAfter, "']' after index type in map type");
            }
            partial.wrap(Node::Kind::TypeBinary);
            partial.node->children.emplace_back(std::move(index.node));
            partial.node->op.typeBinaryOp = egg::ovum::TypeBinaryOp::Map;
            partial.tokensAfter = index.tokensAfter + 1;
          }
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
      return context.skip();
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
        case EggTokenizerOperator::PlusPlus: // "++"
          return context.failed(tokidx, "'++' operator cannot be used in expressions");
        case EggTokenizerOperator::MinusMinus: // "--"
          return context.failed(tokidx, "'--' operator cannot be used in expressions");
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
        case EggTokenizerOperator::PlusEqual: // "+="
        case EggTokenizerOperator::Comma: // ","
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
      std::unique_ptr<Node> node;
      Context context(*this, tokidx);
      switch (context[0].kind) {
      case EggTokenizerKind::Integer:
        node = this->makeNodeInt(Node::Kind::Literal, context[0]);
        return context.success(std::move(node), tokidx + 1);
      case EggTokenizerKind::Float:
        node = this->makeNodeFloat(Node::Kind::Literal, context[0]);
        return context.success(std::move(node), tokidx + 1);
      case EggTokenizerKind::String:
        // TODO: join contiguous strings?
        node = this->makeNodeString(Node::Kind::Literal, context[0]);
        return context.success(std::move(node), tokidx + 1);
      case EggTokenizerKind::Identifier:
        node = this->makeNodeString(Node::Kind::ExprVariable, context[0]);
        return context.success(std::move(node), tokidx + 1);
      case EggTokenizerKind::Keyword:
        return this->parseValueExpressionPrimaryPrefixKeyword(tokidx);
      case EggTokenizerKind::Attribute:
        return PARSE_TODO(tokidx, "bad expression attribute");
      case EggTokenizerKind::Operator:
        return PARSE_TODO(tokidx, "bad expression operator");
      case EggTokenizerKind::EndOfFile:
        return context.expected(tokidx, "expression");
      }
      return PARSE_TODO(tokidx, "bad expression primary prefix");
    }
    Partial parseValueExpressionPrimaryPrefixKeyword(size_t tokidx) {
      Context context(*this, tokidx);
      assert(context[0].kind == EggTokenizerKind::Keyword);
      switch (context[0].value.k) {
      case EggTokenizerKeyword::Any:
        return this->parseValueExpressionPrimaryPrefixKeywordConstructor(context, Node::Kind::TypeAny);
      case EggTokenizerKeyword::Bool:
        return this->parseValueExpressionPrimaryPrefixKeywordConstructor(context, Node::Kind::TypeBool);
      case EggTokenizerKeyword::Float:
        return this->parseValueExpressionPrimaryPrefixKeywordConstructor(context, Node::Kind::TypeFloat);
      case EggTokenizerKeyword::Int:
        return this->parseValueExpressionPrimaryPrefixKeywordConstructor(context, Node::Kind::TypeInt);
      case EggTokenizerKeyword::Object:
        return this->parseValueExpressionPrimaryPrefixKeywordConstructor(context, Node::Kind::TypeObject);
      case EggTokenizerKeyword::String:
        return this->parseValueExpressionPrimaryPrefixKeywordConstructor(context, Node::Kind::TypeString);
      case EggTokenizerKeyword::Void:
        return this->parseValueExpressionPrimaryPrefixKeywordConstructor(context, Node::Kind::TypeVoid);
      case EggTokenizerKeyword::False:
        return this->parseValueExpressionPrimaryPrefixKeywordLiteral(context, egg::ovum::HardValue::False);
      case EggTokenizerKeyword::Null:
        return this->parseValueExpressionPrimaryPrefixKeywordLiteral(context, egg::ovum::HardValue::Null);
      case EggTokenizerKeyword::True:
        return this->parseValueExpressionPrimaryPrefixKeywordLiteral(context, egg::ovum::HardValue::True);
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
      case EggTokenizerKeyword::Type:
      case EggTokenizerKeyword::Var:
      case EggTokenizerKeyword::While:
      case EggTokenizerKeyword::Yield:
        break;
      }
      return PARSE_TODO(tokidx, "bad expression primary prefix keyword: '", context[0].value.s, "'");
    }
    Partial parseValueExpressionPrimaryPrefixKeywordConstructor(Context& context, Node::Kind kind) {
      assert(context[0].kind == EggTokenizerKind::Keyword);
      auto node = this->makeNode(kind, context[0]);
      return context.success(std::move(node), context.tokensBefore + 1);
    }
    Partial parseValueExpressionPrimaryPrefixKeywordLiteral(Context& context, const egg::ovum::HardValue& value) {
      assert(context[0].kind == EggTokenizerKind::Keyword);
      auto node = this->makeNodeValue(Node::Kind::Literal, context[0], value);
      return context.success(std::move(node), context.tokensBefore + 1);
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
            break;
          }
          if (!separator.isOperator(EggTokenizerOperator::Comma)) {
            partial.fail("TODO: Expected ',' between function call arguments, but got ", separator.toString());
            return false;
          }
        }
        return true;
      }
      if (next.isOperator(EggTokenizerOperator::Dot)) {
        // Property access
        auto& property = partial.after(1);
        switch (property.kind) {
        case EggTokenizerKind::Identifier:
        case EggTokenizerKind::Keyword:
          break;
        case EggTokenizerKind::Integer:
        case EggTokenizerKind::Float:
        case EggTokenizerKind::String:
        case EggTokenizerKind::Operator:
        case EggTokenizerKind::Attribute:
        case EggTokenizerKind::EndOfFile:
          partial.fail("TODO: Expected property name after '.', but got ", property.toString());
          return false;
        }
        auto rhs = this->makeNodeString(Node::Kind::Literal, property);
        partial.wrap(Node::Kind::ExprProperty);
        partial.node->children.emplace_back(std::move(rhs)); 
        partial.tokensAfter += 2;
        return true;
      }
      if (next.isOperator(EggTokenizerOperator::BracketLeft)) {
        // Indexing
        auto index = this->parseValueExpression(partial.tokensAfter + 1);
        if (!index.succeeded()) {
          partial.fail(index);
          return false;
        }
        if (index.after(0).isOperator(EggTokenizerOperator::ParenthesisRight)) {
          partial.fail("TODO: Expected ']' after index, but got ", index.after(0).toString());
          return false;
        }
        partial.wrap(Node::Kind::ExprIndex);
        partial.node->children.emplace_back(std::move(index.node));
        partial.tokensAfter = index.tokensAfter + 1;
        return true;
      }
      return false;
    }
    Partial parseValueCondition(size_t tokidx) {
      // WIBBLE
      return this->parseValueExpression(tokidx);
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
    std::unique_ptr<Node> makeNodeValue(Node::Kind kind, const EggTokenizerItem& item, const egg::ovum::HardValue& value) {
      auto node = this->makeNode(kind, item);
      node->value = value;
      return node;
    }
    std::unique_ptr<Node> makeNodeInt(Node::Kind kind, const EggTokenizerItem& item) {
      return this->makeNodeValue(kind, item, egg::ovum::ValueFactory::createInt(this->allocator, item.value.i));
    }
    std::unique_ptr<Node> makeNodeFloat(Node::Kind kind, const EggTokenizerItem& item) {
      return this->makeNodeValue(kind, item, egg::ovum::ValueFactory::createFloat(this->allocator, item.value.f));
    }
    std::unique_ptr<Node> makeNodeString(Node::Kind kind, const EggTokenizerItem& item) {
      return this->makeNodeValue(kind, item, egg::ovum::ValueFactory::createString(this->allocator, item.value.s));
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
