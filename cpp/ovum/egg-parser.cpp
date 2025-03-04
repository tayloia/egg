#include "ovum/ovum.h"
#include "ovum/lexer.h"
#include "ovum/egg-tokenizer.h"
#include "ovum/egg-parser.h"

#include <deque>

namespace {
  using namespace egg::ovum;

  const char* describe(IEggParser::Node::Kind flavour) {
    if (flavour == IEggParser::Node::Kind::TypeInfer) {
      return "var";
    }
    if (flavour == IEggParser::Node::Kind::TypeInferQ) {
      return "var?";
    }
    return "<unknown>";
  }

  int precedence(ValueBinaryOp op) {
    // See egg/www/v1/syntax/syntax.html#binary-operator
    // OPTIMIZE
    switch (op) {
    case ValueBinaryOp::IfVoid:
    case ValueBinaryOp::IfNull:
      return 1;
    case ValueBinaryOp::IfFalse:
      return 2;
    case ValueBinaryOp::IfTrue:
      return 3;
    case ValueBinaryOp::BitwiseOr:
      return 4;
    case ValueBinaryOp::BitwiseXor:
      return 5;
    case ValueBinaryOp::BitwiseAnd:
      return 6;
    case ValueBinaryOp::Equal:
    case ValueBinaryOp::NotEqual:
      return 7;
    case ValueBinaryOp::LessThan:
    case ValueBinaryOp::LessThanOrEqual:
    case ValueBinaryOp::GreaterThanOrEqual:
    case ValueBinaryOp::GreaterThan:
      return 8;
    case ValueBinaryOp::Minimum:
    case ValueBinaryOp::Maximum:
      // TODO add these (and '!!') to documentation
      return 9;
    case ValueBinaryOp::ShiftLeft:
    case ValueBinaryOp::ShiftRight:
    case ValueBinaryOp::ShiftRightUnsigned:
      return 10;
    case ValueBinaryOp::Add:
    case ValueBinaryOp::Subtract:
      return 11;
    case ValueBinaryOp::Multiply:
    case ValueBinaryOp::Divide:
    case ValueBinaryOp::Remainder:
      return 12;
    }
    return 0;
  }

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
    String resource() const {
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
    IAllocator& allocator;
    EggParserTokens tokens;
    std::vector<Issue> issues;
  public:
    EggParser(IAllocator& allocator, const std::shared_ptr<IEggTokenizer>& tokenizer)
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
        const auto& reason = exception.get("reason");
        auto message = String::fromUTF8(this->allocator, reason.data(), reason.size());
        this->issues.emplace_back(Issue::Severity::Error, message, exception.range());
        root = nullptr;
      }
      return { root, std::move(this->issues) };
    }
    virtual String resource() const override {
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
      bool ambiguous;
      Partial(const Context& context, std::unique_ptr<Node>&& node, size_t tokensAfter, size_t issuesAfter, bool ambiguous);
      bool succeeded() const {
        return this->node != nullptr;
      }
      bool skipped() const {
        return (this->node == nullptr) && (this->issuesBefore == this->issuesAfter);
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
        this->issuesAfter = this->parser.issues.size();
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
        auto wrapper = this->parser.makeNode(kind, this->node->range);
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
      Partial success(std::unique_ptr<Node>&& node, size_t tokidx, bool ambiguous = false) const {
        return Partial(*this, std::move(node), tokidx, this->parser.issues.size(), ambiguous);
      }
      Partial skip() const {
        this->parser.issues.resize(this->issuesBefore);
        return Partial(*this, nullptr, this->tokensBefore, this->issuesBefore, false);
      }
      template<typename... ARGS>
      void warning(size_t tokensBefore, size_t tokensAfter, ARGS&&... args) const {
        this->parser.issues.push_back(this->parser.createIssue(Issue::Severity::Warning, tokensBefore, tokensAfter, std::forward<ARGS>(args)...));
      }
      Partial failed() const {
        assert(!this->parser.issues.empty());
        return Partial(*this, nullptr, this->tokensBefore, this->parser.issues.size(), false);
      }
      Partial failed(const Issue& issue) const {
        this->parser.issues.push_back(issue);
        return this->failed();
      }
      Partial failed(Issue::Severity severity, String message, const SourceRange& range) const {
        this->parser.issues.emplace_back(severity, message, range);
        return this->failed();
      }
      template<typename... ARGS>
      Partial failed(size_t tokensAfter, ARGS&&... args) const {
        return this->failed(this->parser.createIssue(Issue::Severity::Error, this->tokensBefore, tokensAfter, std::forward<ARGS>(args)...));
      }
      template<typename... ARGS>
      Partial expected(size_t tokensAfter, ARGS&&... args) const {
        auto actual = this->parser.getAbsolute(tokensAfter).toString();
        return this->failed(tokensAfter, "Expected ", std::forward<ARGS>(args)..., ", but instead got ", actual);
      }
    };
    template<typename... ARGS>
    Issue createIssue(Issue::Severity severity, size_t tokensBefore, size_t tokensAfter, ARGS&&... args) {
      assert(tokensBefore <= tokensAfter);
      auto message = StringBuilder::concat(this->allocator, std::forward<ARGS>(args)...);
      auto& item0 = this->getAbsolute(tokensBefore);
      SourceLocation location0{ item0.line, item0.column };
      auto& item1 = this->getAbsolute(tokensAfter);
      SourceLocation location1{ item1.line, item1.column + item1.width };
      return { severity, message, { location0, location1 } };
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
        root.children.emplace_back(std::move(partial.node));
        tokidx = partial.tokensAfter;
      }
      return true;
    }
    Partial parseModuleStatement(size_t tokidx) {
      // TODO: Module-level attributes
      return this->parseStatement(tokidx);
    }
    Partial parseStatement(size_t tokidx) {
      auto function = this->parseStatementFunction(tokidx);
      if (!function.skipped()) {
        return function;
      }
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
          case EggTokenizerKeyword::Static:
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
      auto simple = this->parseStatementSimple(tokidx);
      if (!simple.skipped()) {
        if (simple.succeeded()) {
          // Swallow the semicolon
          if (!simple.after(0).isOperator(EggTokenizerOperator::Semicolon)) {
            return context.expected(simple.tokensAfter, "';' after statement");
          }
          simple.tokensAfter++;
        }
        return simple;
      }
      if (context[0].isOperator(EggTokenizerOperator::CurlyLeft)) {
        // We've ruled out a primary expression (object literal) above, so this should be a statement block
        return this->parseStatementBlock(tokidx);
      }
      return context.expected(tokidx, "statement");
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
      if (context[1].isOperator(EggTokenizerOperator::Semicolon)) {
        // break ;
        auto stmt = this->makeNode(Node::Kind::StmtBreak, context[0]);
        return context.success(std::move(stmt), tokidx + 2);
      }
      return context.expected(tokidx + 1, "';' after 'break' statement");
    }
    Partial parseStatementCase(size_t tokidx) {
      Context context(*this, tokidx);
      assert(context[0].isKeyword(EggTokenizerKeyword::Case));
      auto expr = this->parseValueExpression(tokidx + 1);
      if (expr.succeeded()) {
        // case <expr> :
        if (!expr.after(0).isOperator(EggTokenizerOperator::Colon)) {
          return context.expected(expr.tokensAfter, "':' after expression in 'case' statement");
        }
        auto stmt = this->makeNode(Node::Kind::StmtCase, context[0]);
        stmt->children.emplace_back(std::move(expr.node));
        return context.success(std::move(stmt), expr.tokensAfter + 1);
      }
      return expr;
    }
    Partial parseStatementCatch(size_t tokidx) {
      Context context(*this, tokidx);
      assert(context[0].isKeyword(EggTokenizerKeyword::Catch));
      return context.failed(tokidx, "Unexpected 'catch' without preceding 'try' statement");
    }
    Partial parseStatementContinue(size_t tokidx) {
      Context context(*this, tokidx);
      assert(context[0].isKeyword(EggTokenizerKeyword::Continue));
      if (context[1].isOperator(EggTokenizerOperator::Semicolon)) {
        // continue ;
        auto stmt = this->makeNode(Node::Kind::StmtContinue, context[0]);
        return context.success(std::move(stmt), tokidx + 2);
      }
      return context.expected(tokidx + 1, "';' after 'continue' statement");
    }
    Partial parseStatementDefault(size_t tokidx) {
      Context context(*this, tokidx);
      assert(context[0].isKeyword(EggTokenizerKeyword::Default));
      if (context[1].isOperator(EggTokenizerOperator::Colon)) {
        // default :
        auto stmt = this->makeNode(Node::Kind::StmtDefault, context[0]);
        return context.success(std::move(stmt), tokidx + 2);
      }
      return context.expected(tokidx + 1, "':' after 'default' statement");
    }
    Partial parseStatementDo(size_t tokidx) {
      Context context(*this, tokidx);
      assert(context[0].isKeyword(EggTokenizerKeyword::Do));
      if (!context[1].isOperator(EggTokenizerOperator::CurlyLeft)) {
        return context.expected(tokidx + 1, "'{' after keyword 'do'");
      }
      auto block = this->parseStatementBlock(tokidx + 1);
      if (!block.succeeded()) {
        return block;
      }
      if (!block.after(0).isKeyword(EggTokenizerKeyword::While)) {
        return context.expected(block.tokensAfter, "'while' after '}' in 'do' statement");
      }
      if (!block.after(1).isOperator(EggTokenizerOperator::ParenthesisLeft)) {
        return context.expected(block.tokensAfter + 1, "'(' after 'while' in 'do' statement");
      }
      auto condition = this->parseValueExpression(block.tokensAfter + 2);
      if (!condition.succeeded()) {
        return condition;
      }
      if (!condition.after(0).isOperator(EggTokenizerOperator::ParenthesisRight)) {
        return context.expected(condition.tokensAfter, "')' after 'while' condition in 'do' statement");
      }
      if (!condition.after(1).isOperator(EggTokenizerOperator::Semicolon)) {
        return context.expected(condition.tokensAfter + 1, "';' after ')' in 'while' condition of 'do' statement");
      }
      auto stmt = this->makeNode(Node::Kind::StmtDo, context[0]);
      stmt->children.emplace_back(std::move(block.node));
      stmt->children.emplace_back(std::move(condition.node));
      return context.success(std::move(stmt), condition.tokensAfter + 2);
    }
    Partial parseStatementElse(size_t tokidx) {
      Context context(*this, tokidx);
      assert(context[0].isKeyword(EggTokenizerKeyword::Else));
      return context.failed(tokidx, "Unexpected 'else' without preceding 'if' statement");
    }
    Partial parseStatementFinally(size_t tokidx) {
      Context context(*this, tokidx);
      assert(context[0].isKeyword(EggTokenizerKeyword::Finally));
      return context.failed(tokidx, "Unexpected 'finally' without preceding 'try' statement");
    }
    Partial parseStatementFor(size_t tokidx) {
      Context context(*this, tokidx);
      assert(context[0].isKeyword(EggTokenizerKeyword::For));
      auto& next = this->getAbsolute(tokidx + 1);
      if (!next.isOperator(EggTokenizerOperator::ParenthesisLeft)) {
        return context.expected(tokidx + 1, "'(' after keyword 'for'");
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
        // for ( <type> <target> : <expr> ) { <bloc> }
        auto type = this->parseTypeExpression(tokidx + 2);
        if (!type.succeeded()) {
          return context.skip();
        }
        if (type.after(0).kind != EggTokenizerKind::Identifier) {
          if (type.ambiguous) {
            return context.skip();
          }
          return context.expected(type.tokensAfter, "identifier after type in 'for' statement");
        }
        return this->parseStatementForEachIdentifier(type);
      } else if (!context[3].isOperator(EggTokenizerOperator::Query)) {
        // for ( var <identifier> : <expr> ) { <bloc> }
        if (context[3].kind != EggTokenizerKind::Identifier) {
          return context.expected(tokidx + 3, "identifier after 'var' in 'for' statement");
        }
        auto node = this->makeNode(Node::Kind::TypeInfer, context[0]);
        auto type = context.success(std::move(node), tokidx + 3);
        return this->parseStatementForEachIdentifier(type);
      } else {
        // for ( var ? <identifier> : <expr> ) { <bloc> }
        if (context[4].kind != EggTokenizerKind::Identifier) {
          return context.expected(tokidx + 4, "identifier after 'var?' in 'for' statement");
        }
        auto node = this->makeNode(Node::Kind::TypeInferQ, context[0]);
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
      stmt->range.end = expr.node->range.end;
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
      auto init = this->parseStatementSimpleOptional(tokidx + 2, EggTokenizerOperator::Semicolon);
      if (!init.succeeded()) {
        return init;
      }
      if (!init.after(0).isOperator(EggTokenizerOperator::Semicolon)) {
        return context.expected(init.tokensAfter, "';' after first clause of 'for' loop statement");
      }
      auto cond = this->parseValueExpressionOptional(init.tokensAfter + 1, EggTokenizerOperator::Semicolon);
      if (!cond.succeeded()) {
        return cond;
      }
      if (!cond.after(0).isOperator(EggTokenizerOperator::Semicolon)) {
        return context.expected(cond.tokensAfter, "';' after condition clause of 'for' loop statement");
      }
      auto adva = this->parseStatementSimpleOptional(cond.tokensAfter + 1, EggTokenizerOperator::ParenthesisRight);
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
    Partial parseStatementFunction(size_t tokidx) {
      Context context(*this, tokidx);
      auto type = this->parseTypeExpression(tokidx);
      if (!type.succeeded()) {
        return context.skip();
      }
      auto& fname = type.after(0);
      if (fname.kind != EggTokenizerKind::Identifier) {
        return context.skip();
      }
      if (!type.after(1).isOperator(EggTokenizerOperator::ParenthesisLeft)) {
        return context.skip();
      }
      // <type> <identifier> ( <parameters> ) { <block> }
      auto signature = this->parseTypeFunctionSignature(type, fname, type.tokensAfter + 1);
      if (!signature.succeeded()) {
        return signature;
      }
      if (!signature.after(0).isOperator(EggTokenizerOperator::CurlyLeft)) {
        return context.expected(signature.tokensAfter, "'{' after ')' in definition of function '", fname.value.s, "'");
      }
      auto block = this->parseStatementBlock(signature.tokensAfter);
      if (!block.succeeded()) {
        return block;
      }
      auto stmt = this->makeNodeString(Node::Kind::StmtDefineFunction, fname);
      stmt->children.emplace_back(std::move(signature.node));
      stmt->children.emplace_back(std::move(block.node));
      return context.success(std::move(stmt), block.tokensAfter);
    }
    Partial parseStatementIf(size_t tokidx) {
      Context context(*this, tokidx);
      assert(context[0].isKeyword(EggTokenizerKeyword::If));
      if (!context[1].isOperator(EggTokenizerOperator::ParenthesisLeft)) {
        return context.expected(tokidx + 1, "'(' after keyword 'if'");
      }
      auto condition = this->parseGuardExpression(tokidx + 2);
      if (!condition.succeeded()) {
        return condition;
      }
      if (!condition.after(0).isOperator(EggTokenizerOperator::ParenthesisRight)) {
        return context.expected(condition.tokensAfter, "')' after condition in 'if' statement");
      }
      if (!condition.after(1).isOperator(EggTokenizerOperator::CurlyLeft)) {
        return context.expected(condition.tokensAfter + 1, "'{' after ')' in 'if' statement");
      }
      auto truthy = this->parseStatementBlock(condition.tokensAfter + 1);
      if (!truthy.succeeded()) {
        return truthy;
      }
      if (truthy.after(0).isKeyword(EggTokenizerKeyword::Else)) {
        // There is an 'else' clause
        if (truthy.after(1).isKeyword(EggTokenizerKeyword::If)) {
          // It's a chained 'if () {} else if ...'
          auto chain = this->parseStatementIf(truthy.tokensAfter + 1);
          if (!chain.succeeded()) {
            return chain;
          }
          auto stmt = this->makeNode(Node::Kind::StmtIf, context[0]);
          stmt->children.emplace_back(std::move(condition.node));
          stmt->children.emplace_back(std::move(truthy.node));
          stmt->children.emplace_back(std::move(chain.node));
          return context.success(std::move(stmt), chain.tokensAfter);
        }
        if (!truthy.after(1).isOperator(EggTokenizerOperator::CurlyLeft)) {
          return context.expected(truthy.tokensAfter + 1, "'{' after 'else' in 'if' statement");
        }
        auto falsy = this->parseStatementBlock(truthy.tokensAfter + 1);
        if (!falsy.succeeded()) {
          return falsy;
        }
        auto stmt = this->makeNode(Node::Kind::StmtIf, context[0]);
        stmt->children.emplace_back(std::move(condition.node));
        stmt->children.emplace_back(std::move(truthy.node));
        stmt->children.emplace_back(std::move(falsy.node));
        return context.success(std::move(stmt), falsy.tokensAfter);
      } else {
        // There is no 'else' clause
        auto stmt = this->makeNode(Node::Kind::StmtIf, context[0]);
        stmt->children.emplace_back(std::move(condition.node));
        stmt->children.emplace_back(std::move(truthy.node));
        return context.success(std::move(stmt), truthy.tokensAfter);
      }
    }
    Partial parseStatementReturn(size_t tokidx) {
      Context context(*this, tokidx);
      assert(context[0].isKeyword(EggTokenizerKeyword::Return));
      if (context[1].isOperator(EggTokenizerOperator::Semicolon)) {
        // return ;
        auto stmt = this->makeNode(Node::Kind::StmtReturn, context[0]);
        return context.success(std::move(stmt), tokidx + 2);
      }
      auto expr = this->parseValueExpression(tokidx + 1);
      if (expr.succeeded()) {
        // return <expr> ;
        if (!expr.after(0).isOperator(EggTokenizerOperator::Semicolon)) {
          return context.expected(expr.tokensAfter, "';' after 'return' statement");
        }
        auto stmt = this->makeNode(Node::Kind::StmtReturn, context[0]);
        stmt->children.emplace_back(std::move(expr.node));
        return context.success(std::move(stmt), expr.tokensAfter + 1);
      }
      return expr;
    }
    Partial parseStatementSwitch(size_t tokidx) {
      Context context(*this, tokidx);
      assert(context[0].isKeyword(EggTokenizerKeyword::Switch));
      if (!context[1].isOperator(EggTokenizerOperator::ParenthesisLeft)) {
        return context.expected(tokidx + 1, "'(' after keyword 'switch'");
      }
      auto condition = this->parseGuardExpression(tokidx + 2);
      if (!condition.succeeded()) {
        return condition;
      }
      if (!condition.after(0).isOperator(EggTokenizerOperator::ParenthesisRight)) {
        return context.expected(condition.tokensAfter, "')' after condition in 'switch' statement");
      }
      if (!condition.after(1).isOperator(EggTokenizerOperator::CurlyLeft)) {
        return context.expected(condition.tokensAfter + 1, "'{' after ')' in 'switch' statement");
      }
      auto block = this->parseStatementBlock(condition.tokensAfter + 1);
      if (!block.succeeded()) {
        return block;
      }
      auto stmt = this->makeNode(Node::Kind::StmtSwitch, context[0]);
      stmt->children.emplace_back(std::move(condition.node));
      stmt->children.emplace_back(std::move(block.node));
      return context.success(std::move(stmt), block.tokensAfter);
    }
    Partial parseStatementThrow(size_t tokidx) {
      Context context(*this, tokidx);
      assert(context[0].isKeyword(EggTokenizerKeyword::Throw));
      if (context[1].isOperator(EggTokenizerOperator::Semicolon)) {
        // throw ;
        auto stmt = this->makeNode(Node::Kind::StmtThrow, context[0]);
        return context.success(std::move(stmt), tokidx + 2);
      }
      auto expr = this->parseValueExpression(tokidx + 1);
      if (expr.succeeded()) {
        // throw <expr> ;
        if (!expr.after(0).isOperator(EggTokenizerOperator::Semicolon)) {
          return context.expected(expr.tokensAfter, "';' after 'throw' statement");
        }
        auto stmt = this->makeNode(Node::Kind::StmtThrow, context[0]);
        stmt->children.emplace_back(std::move(expr.node));
        return context.success(std::move(stmt), expr.tokensAfter + 1);
      }
      return expr;
    }
    Partial parseStatementTry(size_t tokidx) {
      Context context(*this, tokidx);
      assert(context[0].isKeyword(EggTokenizerKeyword::Try));
      if (!context[1].isOperator(EggTokenizerOperator::CurlyLeft)) {
        return context.expected(tokidx + 1, "'{' after keyword 'try'");
      }
      auto tried = this->parseStatementBlock(tokidx + 1);
      if (!tried.succeeded()) {
        return tried;
      }
      auto stmt = this->makeNode(Node::Kind::StmtTry, context[0]);
      stmt->children.emplace_back(std::move(tried.node));
      auto partial = context.success(std::move(stmt), tried.tokensAfter);
      while (partial.after(0).isKeyword(EggTokenizerKeyword::Catch)) {
        if (!partial.after(1).isOperator(EggTokenizerOperator::ParenthesisLeft)) {
          return context.expected(partial.tokensAfter + 1, "'(' after 'catch' in 'try' statement");
        }
        auto type = this->parseTypeExpression(partial.tokensAfter + 2);
        if (!type.succeeded()) {
          return type;
        }
        auto& name = type.after(0);
        if (name.kind != EggTokenizerKind::Identifier) {
          // Note we DON'T allow keywords
          return context.expected(type.tokensAfter, "identifier after type 'catch' statement");
        }
        if (!type.after(1).isOperator(EggTokenizerOperator::ParenthesisRight)) {
          return context.expected(type.tokensAfter + 1, "')' after identifier in 'catch' statement");
        }
        if (!type.after(2).isOperator(EggTokenizerOperator::CurlyLeft)) {
          return context.expected(type.tokensAfter + 2, "'{' after ')' in 'catch' statement");
        }
        auto block = this->parseStatementBlock(type.tokensAfter + 2);
        if (!block.succeeded()) {
          return block;
        }
        auto caught = this->makeNodeString(Node::Kind::StmtCatch, name);
        caught->children.emplace_back(std::move(type.node));
        caught->children.emplace_back(std::move(block.node));
        partial.node->children.emplace_back(std::move(caught));
        partial.tokensAfter = block.tokensAfter;
      }
      if (partial.after(0).isKeyword(EggTokenizerKeyword::Finally)) {
        if (!partial.after(1).isOperator(EggTokenizerOperator::CurlyLeft)) {
          return context.expected(partial.tokensAfter + 1, "'{' after 'finally' in 'try' statement");
        }
        auto block = this->parseStatementBlock(partial.tokensAfter + 1);
        if (!block.succeeded()) {
          return block;
        }
        if (block.after(0).isKeyword(EggTokenizerKeyword::Catch)) {
          return context.failed(block.tokensAfter, "Unexpected 'catch' after 'finally' block in 'try' statement");
        }
        if (block.after(0).isKeyword(EggTokenizerKeyword::Finally)) {
          return context.failed(block.tokensAfter, "Unexpected second 'finally' in 'try' statement");
        }
        auto final = this->makeNode(Node::Kind::StmtFinally, partial.after(0));
        final->children.emplace_back(std::move(block.node));
        partial.node->children.emplace_back(std::move(final));
        partial.tokensAfter = block.tokensAfter;
      }
      if (partial.node->children.size() < 2) {
        return context.expected(partial.tokensAfter, "'catch' or 'finally' after 'try' block");
      }
      return partial;
    }
    Partial parseStatementWhile(size_t tokidx) {
      Context context(*this, tokidx);
      assert(context[0].isKeyword(EggTokenizerKeyword::While));
      if (!context[1].isOperator(EggTokenizerOperator::ParenthesisLeft)) {
        return context.expected(tokidx + 1, "'(' after keyword 'while'");
      }
      auto condition = this->parseGuardExpression(tokidx + 2);
      if (!condition.succeeded()) {
        return condition;
      }
      if (!condition.after(0).isOperator(EggTokenizerOperator::ParenthesisRight)) {
        return context.expected(condition.tokensAfter, "')' after condition in 'while' statement");
      }
      if (!condition.after(1).isOperator(EggTokenizerOperator::CurlyLeft)) {
        return context.expected(condition.tokensAfter + 1, "'{' after ')' in 'while' statement");
      }
      auto block = this->parseStatementBlock(condition.tokensAfter + 1);
      if (!block.succeeded()) {
        return block;
      }
      auto stmt = this->makeNode(Node::Kind::StmtWhile, context[0]);
      stmt->children.emplace_back(std::move(condition.node));
      stmt->children.emplace_back(std::move(block.node));
      return context.success(std::move(stmt), block.tokensAfter);
    }
    Partial parseStatementYield(size_t tokidx) {
      Context context(*this, tokidx);
      assert(context[0].isKeyword(EggTokenizerKeyword::Yield));
      if (context[1].isOperator(EggTokenizerOperator::Semicolon)) {
        return context.expected(tokidx + 1, "expression, 'break' or 'continue' after keyword 'yield'");
      }
      if (context[1].isKeyword(EggTokenizerKeyword::Break)) {
        // yield break ;
        if (!context[2].isOperator(EggTokenizerOperator::Semicolon)) {
          return context.expected(tokidx + 2, "';' after 'yield break' statement");
        }
        auto stmt = this->makeNode(Node::Kind::StmtYield, context[0]);
        stmt->children.emplace_back(std::move(this->makeNode(Node::Kind::StmtBreak, context[1])));
        return context.success(std::move(stmt), tokidx + 3);
      }
      if (context[1].isKeyword(EggTokenizerKeyword::Continue)) {
        // yield continue ;
        if (!context[2].isOperator(EggTokenizerOperator::Semicolon)) {
          return context.expected(tokidx + 2, "';' after 'yield continue' statement");
        }
        auto stmt = this->makeNode(Node::Kind::StmtYield, context[0]);
        stmt->children.emplace_back(std::move(this->makeNode(Node::Kind::StmtContinue, context[1])));
        return context.success(std::move(stmt), tokidx + 3);
      }
      if (context[1].isOperator(EggTokenizerOperator::Ellipsis)) {
        // yield ... <expr> ;
        auto expr = this->parseValueExpression(tokidx + 2);
        if (expr.succeeded()) {
          if (!expr.after(0).isOperator(EggTokenizerOperator::Semicolon)) {
            return context.expected(expr.tokensAfter, "';' after 'yield ...' statement");
          }
          auto ellipsis = this->makeNode(Node::Kind::ExprEllipsis, context[1]);
          ellipsis->children.emplace_back(std::move(expr.node));
          auto stmt = this->makeNode(Node::Kind::StmtYield, context[0]);
          stmt->children.emplace_back(std::move(ellipsis));
          return context.success(std::move(stmt), expr.tokensAfter + 1);
        }
        return expr;
      }
      auto expr = this->parseValueExpression(tokidx + 1);
      if (expr.succeeded()) {
        // return <expr> ;
        if (!expr.after(0).isOperator(EggTokenizerOperator::Semicolon)) {
          return context.expected(expr.tokensAfter, "';' after 'yield' statement");
        }
        auto stmt = this->makeNode(Node::Kind::StmtYield, context[0]);
        stmt->children.emplace_back(std::move(expr.node));
        return context.success(std::move(stmt), expr.tokensAfter + 1);
      }
      return expr;
    }
    Partial parseStatementSimple(size_t tokidx) {
      Context context(*this, tokidx);
      auto discard = this->parseStatementDiscard(tokidx);
      if (!discard.skipped()) {
        return discard;
      }
      auto define = this->parseStatementDefine(tokidx);
      if (!define.skipped()) {
        return define;
      }
      auto mutate = this->parseStatementMutate(tokidx);
      if (!mutate.skipped()) {
        return mutate;
      }
      auto expr = this->parseValueExpressionPrimary(tokidx, "statement");
      if (expr.succeeded()) {
        // The whole statement is actually an expression
        if (expr.node->kind == Node::Kind::ExprCall) {
          return expr;
        }
        return context.skip();
      }
      if (context[0].isOperator(EggTokenizerOperator::CurlyLeft)) {
        // Edge case: '{}' is an object expression and an empty statement block
        return context.skip();
      }
      return expr;
    }
    Partial parseStatementSimpleOptional(size_t tokidx, EggTokenizerOperator terminal) {
      Context context(*this, tokidx);
      if (context[0].isOperator(terminal)) {
        // Missing statement
        auto stmt = this->makeNode(Node::Kind::Missing, context[0]);
        return context.success(std::move(stmt), tokidx);
      }
      return this->parseStatementSimple(tokidx);
    }
    Partial parseStatementDiscard(size_t tokidx) {
      // void ( <expr> )
      Context context(*this, tokidx);
      if (context[0].isKeyword(EggTokenizerKeyword::Void) && context[1].isOperator(EggTokenizerOperator::ParenthesisLeft)) {
        auto expr = this->parseValueExpression(tokidx + 2);
        if (expr.succeeded() && expr.after(0).isOperator(EggTokenizerOperator::ParenthesisRight)) {
          auto call = this->makeNode(Node::Kind::ExprCall, context[0]);
          auto vtype = this->makeNode(Node::Kind::TypeVoid, context[0]);
          call->children.emplace_back(std::move(vtype));
          call->children.emplace_back(std::move(expr.node));
          return context.success(std::move(call), expr.tokensAfter + 1);
        }
      }
      return context.skip();
    }
    Partial parseStatementDefine(size_t tokidx) {
      Context context(*this, tokidx);
      if (context[0].isKeyword(EggTokenizerKeyword::Type)) {
        // Type definition
        return this->parseStatementDefineType(tokidx);
      }
      if (context[0].isKeyword(EggTokenizerKeyword::Var)) {
        // Inferred type
        if (context[1].isOperator(EggTokenizerOperator::Query)) {
          return this->parseStatementDefineVariableInferred(tokidx + 2, context[0], Node::Kind::TypeInferQ);
        }
        return this->parseStatementDefineVariableInferred(tokidx + 1, context[0], Node::Kind::TypeInfer);
      }
      auto partial = this->parseTypeExpression(tokidx);
      if (partial.succeeded()) {
        return this->parseStatementDefineVariableExplicit(partial.tokensAfter, partial.node, partial.ambiguous);
      }
      return context.skip();
    }
    Partial parseStatementDefineType(size_t tokidx) {
      Context context(*this, tokidx);
      assert(context[0].isKeyword(EggTokenizerKeyword::Type));
      if (context[1].kind != EggTokenizerKind::Identifier) {
        return context.expected(tokidx, "identifier after 'type' in type definition");
      }
      if (context[2].isOperator(EggTokenizerOperator::Equal)) {
        // type <identifier> = <type>
        auto type = this->parseTypeExpression(tokidx + 3);
        if (type.succeeded()) {
          auto stmt = this->makeNodeString(Node::Kind::StmtDefineType, context[1]);
          stmt->range.end = type.node->range.end;
          stmt->children.emplace_back(std::move(type.node));
          return context.success(std::move(stmt), type.tokensAfter);
        }
        return type;
      }
      if (context[2].isOperator(EggTokenizerOperator::CurlyLeft)) {
        // type <identifier> { <definition> }
        auto type = this->parseTypeSpecification(tokidx + 2, context[1].value.s);
        assert(!type.skipped());
        if (type.succeeded()) {
          auto stmt = this->makeNodeString(Node::Kind::StmtDefineType, context[1]);
          stmt->range.end = type.node->range.end;
          stmt->children.emplace_back(std::move(type.node));
          return context.success(std::move(stmt), type.tokensAfter);
        }
        return type;
      }
      return context.expected(tokidx + 2, "'=' or '{' after identifier '", context[1].value.s, "' in type definition");
    }
    Partial parseStatementDefineVariableInferred(size_t tokidx, const EggTokenizerItem& var, Node::Kind flavour) {
      Context context(*this, tokidx);
      if (context[0].kind != EggTokenizerKind::Identifier) {
        return context.expected(tokidx, "identifier after '", describe(flavour), "' in variable definition");
      }
      if (!context[1].isOperator(EggTokenizerOperator::Equal)) {
        return context.failed(tokidx, "Cannot declare variable '", context[0].value.s, "' using '", describe(flavour), "' without an initial value");
      }
      // var[?] <identifier> = <expr>
      auto expr = this->parseValueExpression(tokidx + 2);
      if (expr.succeeded()) {
        auto type = this->makeNode(flavour, var);
        auto stmt = this->makeNodeString(Node::Kind::StmtDefineVariable, context[0]);
        stmt->range.end = expr.node->range.end;
        stmt->children.emplace_back(std::move(type));
        stmt->children.emplace_back(std::move(expr.node));
        return context.success(std::move(stmt), expr.tokensAfter);
      }
      return expr;
    }
    Partial parseStatementDefineVariableExplicit(size_t tokidx, std::unique_ptr<Node>& ptype, bool ambiguous) {
      assert(ptype != nullptr);
      Context context(*this, tokidx);
      if (context[0].kind != EggTokenizerKind::Identifier) {
        if (ambiguous) {
          return context.skip();
        }
        return context.expected(tokidx, "identifier after type in variable declaration");
      }
      if (context[1].isOperator(EggTokenizerOperator::ParenthesisLeft)) {
        // <type> <identifier> (
        return context.skip();
      }
      if (context[1].isOperator(EggTokenizerOperator::Equal)) {
        // <type> <identifier> = <expr>
        auto expr = this->parseValueExpression(tokidx + 2);
        if (expr.succeeded()) {
          auto stmt = this->makeNodeString(Node::Kind::StmtDefineVariable, context[0]);
          stmt->range.end = expr.node->range.end;
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
    Partial parseStatementMutate(size_t tokidx) {
      Context context(*this, tokidx);
      if (context[0].isOperator(EggTokenizerOperator::PlusPlus)) {
        // ++<target>
        auto target = this->parseTarget(tokidx + 1);
        if (target.succeeded()) {
          target.wrap(Node::Kind::StmtMutate);
          target.node->range.begin = { context[0].line, context[0].column };
          target.node->op.valueMutationOp = ValueMutationOp::Increment;
        }
        return target;
      } else if (context[0].isOperator(EggTokenizerOperator::MinusMinus)) {
        // --<target>
        auto target = this->parseTarget(tokidx + 1);
        if (target.succeeded()) {
          target.wrap(Node::Kind::StmtMutate);
          target.node->range.begin = { context[0].line, context[0].column };
          target.node->op.valueMutationOp = ValueMutationOp::Decrement;
        }
        return target;
      } else {
        auto target = this->parseTarget(tokidx);
        if (target.succeeded()) {
          auto& next = target.after(0);
          if (next.kind == EggTokenizerKind::Operator) {
            switch (next.value.o) {
            case EggTokenizerOperator::Equal: // '='
              return this->parseStatementMutateOperator(target, ValueMutationOp::Assign);
            case EggTokenizerOperator::BangBangEqual: // '!!='
              return this->parseStatementMutateOperator(target, ValueMutationOp::IfVoid);
            case EggTokenizerOperator::PercentEqual: // '%='
              return this->parseStatementMutateOperator(target, ValueMutationOp::Remainder);
            case EggTokenizerOperator::AmpersandAmpersandEqual: // '&&='
              return this->parseStatementMutateOperator(target, ValueMutationOp::IfTrue);
            case EggTokenizerOperator::AmpersandEqual: // '&='
              return this->parseStatementMutateOperator(target, ValueMutationOp::BitwiseAnd);
            case EggTokenizerOperator::StarEqual: // '*='
              return this->parseStatementMutateOperator(target, ValueMutationOp::Multiply);
            case EggTokenizerOperator::PlusEqual: // '+='
              return this->parseStatementMutateOperator(target, ValueMutationOp::Add);
            case EggTokenizerOperator::MinusEqual: // '-='
              return this->parseStatementMutateOperator(target, ValueMutationOp::Subtract);
            case EggTokenizerOperator::SlashEqual: // '/='
              return this->parseStatementMutateOperator(target, ValueMutationOp::Divide);
            case EggTokenizerOperator::ShiftLeftEqual: // '<<='
              return this->parseStatementMutateOperator(target, ValueMutationOp::ShiftLeft);
            case EggTokenizerOperator::LessBarEqual: // '<|='
              return this->parseStatementMutateOperator(target, ValueMutationOp::Minimum);
            case EggTokenizerOperator::ShiftRightEqual: // '>>='
              return this->parseStatementMutateOperator(target, ValueMutationOp::ShiftRight);
            case EggTokenizerOperator::ShiftRightUnsignedEqual: // '>>>='
              return this->parseStatementMutateOperator(target, ValueMutationOp::ShiftRightUnsigned);
            case EggTokenizerOperator::GreaterBarEqual: // '>|='
              return this->parseStatementMutateOperator(target, ValueMutationOp::Maximum);
            case EggTokenizerOperator::QueryQueryEqual: // '??='
              return this->parseStatementMutateOperator(target, ValueMutationOp::IfNull);
            case EggTokenizerOperator::CaretEqual: // '^='
              return this->parseStatementMutateOperator(target, ValueMutationOp::BitwiseXor);
            case EggTokenizerOperator::BarEqual: // '|='
              return this->parseStatementMutateOperator(target, ValueMutationOp::BitwiseOr);
            case EggTokenizerOperator::BarBarEqual: // '||='
              return this->parseStatementMutateOperator(target, ValueMutationOp::IfFalse);
            case EggTokenizerOperator::Bang: // '!'
            case EggTokenizerOperator::BangBang: // '!!'
            case EggTokenizerOperator::BangEqual: // '!='
            case EggTokenizerOperator::Percent: // '%'
            case EggTokenizerOperator::Ampersand: // '&'
            case EggTokenizerOperator::AmpersandAmpersand: // '&&'
            case EggTokenizerOperator::ParenthesisLeft: // '('
            case EggTokenizerOperator::ParenthesisRight: // ')'
            case EggTokenizerOperator::Star: // '*'
            case EggTokenizerOperator::Plus: // '+'
            case EggTokenizerOperator::PlusPlus: // '++'
            case EggTokenizerOperator::Comma: // ','
            case EggTokenizerOperator::Minus: // '-'
            case EggTokenizerOperator::MinusMinus: // '--'
            case EggTokenizerOperator::Lambda: // '->'
            case EggTokenizerOperator::Dot: // '.'
            case EggTokenizerOperator::Ellipsis: // '...'
            case EggTokenizerOperator::Slash: // '/'
            case EggTokenizerOperator::Colon: // ':'
            case EggTokenizerOperator::Semicolon: // ';'
            case EggTokenizerOperator::Less: // '<'
            case EggTokenizerOperator::ShiftLeft: // '<<'
            case EggTokenizerOperator::LessEqual: // '<='
            case EggTokenizerOperator::LessBar: // '<='
            case EggTokenizerOperator::EqualEqual: // '=='
            case EggTokenizerOperator::Greater: // '>'
            case EggTokenizerOperator::GreaterEqual: // '>='
            case EggTokenizerOperator::ShiftRight: // '>>'
            case EggTokenizerOperator::ShiftRightUnsigned: // '>>>'
            case EggTokenizerOperator::GreaterBar: // '<='
            case EggTokenizerOperator::Query: // '?'
            case EggTokenizerOperator::QueryQuery: // '??'
            case EggTokenizerOperator::BracketLeft: // '['
            case EggTokenizerOperator::BracketRight: // ']'
            case EggTokenizerOperator::Caret: // '^'
            case EggTokenizerOperator::CurlyLeft: // '{'
            case EggTokenizerOperator::Bar: // '|'
            case EggTokenizerOperator::BarBar: // '||'
            case EggTokenizerOperator::CurlyRight: // '}'
            case EggTokenizerOperator::Tilde: // '~'
            default:
              break;
            }
          }
        }
      }
      return context.skip();
    }
    Partial parseStatementMutateOperator(Partial& lhs, ValueMutationOp op) {
      assert(lhs.succeeded());
      auto rhs = this->parseValueExpression(lhs.tokensAfter + 1);
      if (!rhs.succeeded()) {
        return rhs;
      }
      lhs.wrap(Node::Kind::StmtMutate);
      lhs.node->range.end = rhs.node->range.end;
      lhs.node->children.emplace_back(std::move(rhs.node));
      lhs.node->op.valueMutationOp = op;
      lhs.tokensAfter = rhs.tokensAfter;
      return std::move(lhs);
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
        // Type union
        auto rhs = this->parseTypeExpression(lhs.tokensAfter + 1);
        if (rhs.succeeded()) {
          lhs.wrap(Node::Kind::TypeBinary);
          lhs.node->range.end = rhs.node->range.end;
          lhs.node->children.emplace_back(std::move(rhs.node));
          lhs.node->op.typeBinaryOp = TypeBinaryOp::Union;
          lhs.tokensAfter = rhs.tokensAfter;
          lhs.ambiguous |= rhs.ambiguous;
          return lhs;
        }
        return rhs;
      }
      if (lhs.after(0).isOperator(EggTokenizerOperator::Dot)) {
        // Type property access
        auto& property = lhs.after(1);
        if (!property.isPropertyName()) {
          return context.expected(lhs.tokensAfter + 1, "type property name after '.'");
        }
        auto rhs = this->makeNodeString(Node::Kind::Literal, property);
        lhs.wrap(Node::Kind::ExprProperty);
        lhs.node->children.emplace_back(std::move(rhs));
        lhs.node->range.end = { property.line, property.column + property.width };
        lhs.tokensAfter += 2;
        lhs.ambiguous = true;
        return lhs;
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
          if ((partial.node->kind == Node::Kind::TypeUnary) && (partial.node->op.typeUnaryOp == TypeUnaryOp::Nullable)) {
            context.warning(partial.tokensAfter, partial.tokensAfter, "Redundant repetition of type suffix '?'");
          } else {
            partial.wrap(Node::Kind::TypeUnary);
            partial.node->range.end = { next.line, next.column + 1 };
            partial.node->op.typeUnaryOp = TypeUnaryOp::Nullable;
          }
          partial.tokensAfter++;
        } else if (next.isOperator(EggTokenizerOperator::QueryQuery)) {
          // type??
          context.warning(partial.tokensAfter, partial.tokensAfter, "Redundant repetition of type suffix '?'");
          if ((partial.node->kind != Node::Kind::TypeUnary) || (partial.node->op.typeUnaryOp != TypeUnaryOp::Nullable)) {
            partial.wrap(Node::Kind::TypeUnary);
            partial.node->range.end = { next.line, next.column + 1 };
            partial.node->op.typeUnaryOp = TypeUnaryOp::Nullable;
          }
          partial.tokensAfter++;
        } else if (next.isOperator(EggTokenizerOperator::Star)) {
          // type*
          partial.wrap(Node::Kind::TypeUnary);
          partial.node->range.end = { next.line, next.column + 1 };
          partial.node->op.typeUnaryOp = TypeUnaryOp::Pointer;
          partial.tokensAfter++;
        } else if (next.isOperator(EggTokenizerOperator::Bang)) {
          // type!
          partial.wrap(Node::Kind::TypeUnary);
          partial.node->range.end = { next.line, next.column + 1 };
          partial.node->op.typeUnaryOp = TypeUnaryOp::Iterator;
          partial.tokensAfter++;
          partial.ambiguous = false;
        } else if (next.isOperator(EggTokenizerOperator::BangBang)) {
          // type!!
          partial.wrap(Node::Kind::TypeUnary);
          partial.node->range.end = { next.line, next.column + 1 };
          partial.node->op.typeUnaryOp = TypeUnaryOp::Iterator;
          partial.wrap(Node::Kind::TypeUnary);
          partial.node->range.end = { next.line, next.column + 2 };
          partial.node->op.typeUnaryOp = TypeUnaryOp::Iterator;
          partial.tokensAfter++;
        } else if (next.isOperator(EggTokenizerOperator::BracketLeft)) {
          auto& last = partial.after(1);
          if (last.isOperator(EggTokenizerOperator::BracketRight)) {
            // type[]
            partial.wrap(Node::Kind::TypeUnary);
            partial.node->range.end = { last.line, last.column + 1 };
            partial.node->op.typeUnaryOp = TypeUnaryOp::Array;
            partial.tokensAfter += 2;
            partial.ambiguous = false;
          } else {
            // type[indextype]
            auto index = this->parseTypeExpression(partial.tokensAfter + 1);
            if (!index.succeeded()) {
              return index;
            }
            auto& terminal = index.after(0);
            if (!terminal.isOperator(EggTokenizerOperator::BracketRight)) {
              return context.expected(index.tokensAfter, "']' after index type in map type");
            }
            partial.wrap(Node::Kind::TypeBinary);
            partial.node->range.end = { terminal.line, terminal.column };
            partial.node->children.emplace_back(std::move(index.node));
            partial.node->op.typeBinaryOp = TypeBinaryOp::Map;
            partial.tokensAfter = index.tokensAfter + 1;
            partial.ambiguous |= index.ambiguous;
          }
        } else if (next.isOperator(EggTokenizerOperator::ParenthesisLeft)) {
          auto& last = partial.after(1);
          if (last.isOperator(EggTokenizerOperator::ParenthesisRight)) {
            // type()
            partial.wrap(Node::Kind::TypeFunctionSignature);
            partial.node->range.end = { last.line, last.column + 1 };
            partial.tokensAfter += 2;
          } else {
            // TODO type(<parameters>)
            return context.failed(partial.tokensAfter + 1, "Function parameters not yet supported");
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
      if (next.isOperator(EggTokenizerOperator::ParenthesisLeft)) {
        auto partial = this->parseTypeExpression(tokidx + 1);
        if (!partial.succeeded()) {
          return partial;
        }
        if (!partial.after(0).isOperator(EggTokenizerOperator::ParenthesisRight)) {
          return context.expected(partial.tokensAfter, "')' after type expression");
        }
        partial.tokensAfter++;
        return partial;
      }
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
          return this->parseTypeExpressionPrimaryKeyword(context, Node::Kind::TypeType);
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
        case EggTokenizerKeyword::Static:
        case EggTokenizerKeyword::Switch:
        case EggTokenizerKeyword::Throw:
        case EggTokenizerKeyword::True:
        case EggTokenizerKeyword::Try:
        case EggTokenizerKeyword::Var:
        case EggTokenizerKeyword::While:
        case EggTokenizerKeyword::Yield:
          break;
        }
        return context.skip();
      }
      if (next.kind == EggTokenizerKind::Identifier) {
        // Assume the identifier is a type name
        auto node = this->makeNodeString(Node::Kind::Variable, next);
        return context.success(std::move(node), tokidx + 1, true);
      }
      return context.skip();
    }
    Partial parseTypeExpressionPrimaryKeyword(Context& context, Node::Kind kind) {
      auto node = this->makeNode(kind, context[0]);
      return context.success(std::move(node), context.tokensBefore + 1);
    }
    Partial parseTypeSpecification(size_t tokidx, const String& tname) {
      // Type definition: { <clause> ... }
      Context context(*this, tokidx);
      auto& curly = context[0];
      assert(curly.isOperator(EggTokenizerOperator::CurlyLeft));
      auto description = ValueFactory::createString(this->allocator, tname);
      auto definition = this->makeNodeValue(Node::Kind::TypeSpecification, curly, description);
      auto nxtidx = tokidx + 1;
      while (!this->getAbsolute(nxtidx).isOperator(EggTokenizerOperator::CurlyRight)) {
        auto inner = this->parseTypeSpecificationClause(nxtidx, tname);
        assert(!inner.skipped());
        if (!inner.succeeded()) {
          return inner;
        }
        definition->children.emplace_back(std::move(inner.node));
        nxtidx = inner.tokensAfter;
      }
      auto outer = context.success(std::move(definition), tokidx);
      outer.tokensAfter = nxtidx + 1;
      return outer;
    }
    Partial parseTypeSpecificationClause(size_t tokidx, const String& tname) {
      Context context(*this, tokidx);
      auto isstatic = context[0].isKeyword(EggTokenizerKeyword::Static);
      auto type = this->parseTypeExpression(tokidx + size_t(isstatic));
      if (!type.succeeded()) {
        if (type.skipped()) {
          return context.expected(tokidx, "type definition clause for '", tname, "'");
        }
        return type;
      }
      // TODO handle generators
      auto& identifier = type.after(0);
      if (identifier.kind != EggTokenizerKind::Identifier) {
        return context.expected(type.tokensAfter, "identifier after type in type definition of '", tname, "'");
      }
      auto& next = type.after(1);
      if (next.isOperator(EggTokenizerOperator::Semicolon)) {
        // [static] <type> <identifier> ;
        if (isstatic) {
          return context.failed(type.tokensAfter + 1, "Forward declaration of static property '", identifier.value.s, "' not yet supported");
        }
        return this->parseTypeSpecificationAccess(type, identifier);
      }
      if (!isstatic && next.isOperator(EggTokenizerOperator::CurlyLeft)) {
        // <type> <identifier> {
        return this->parseTypeSpecificationAccess(type, identifier);
      }
      if (next.isOperator(EggTokenizerOperator::ParenthesisLeft)) {
        // [static] <type> <identifier> (
        auto signature = this->parseTypeFunctionSignature(type, identifier, type.tokensAfter + 1);
        if (!signature.succeeded()) {
          return signature;
        }
        if (signature.after(0).isOperator(EggTokenizerOperator::Semicolon)) {
          if (isstatic) {
            // static <type> <identifier> ( ... ) ;
            return context.failed(signature.tokensAfter, "Forward declaration of static function '", identifier.value.s, "' not yet supported");
          }
          // <type> <identifier> ( ... ) ;
          auto stmt = this->makeNodeString(Node::Kind::TypeSpecificationInstanceFunction, identifier);
          stmt->children.emplace_back(std::move(signature.node));
          return context.success(std::move(stmt), signature.tokensAfter + 1);
        }
        if (!isstatic) {
          // <type> <identifier> ( ... ) ...
          return context.expected(signature.tokensAfter, "';' after ')' in declaration of non-static function '", identifier.value.s, "'");
        }
        if (!signature.after(0).isOperator(EggTokenizerOperator::CurlyLeft)) {
          return context.expected(signature.tokensAfter, "'{' after ')' in definition of static function '", identifier.value.s, "'");
        }
        // static <type> <identifier> ( ... ) {
        auto block = this->parseStatementBlock(signature.tokensAfter);
        if (!block.succeeded()) {
          return block;
        }
        auto stmt = this->makeNodeString(Node::Kind::TypeSpecificationStaticFunction, identifier);
        stmt->children.emplace_back(std::move(signature.node));
        stmt->children.emplace_back(std::move(block.node));
        return context.success(std::move(stmt), block.tokensAfter);
      }
      if (!isstatic) {
        // <type> <identifier>
        return context.expected(type.tokensAfter + 1, "';' after identifier '", identifier.value.s, "' in declaration of property");
      }
      // static <type> <identifier>
      if (next.isOperator(EggTokenizerOperator::Equal)) {
        // static <type> <identifier> =
        auto expr = this->parseValueExpression(type.tokensAfter + 2);
        if (!expr.succeeded()) {
          return expr;
        }
        if (!expr.after(0).isOperator(EggTokenizerOperator::Semicolon)) {
          return context.expected(expr.tokensAfter, "';' after value of static property '", identifier.value.s, "'");
        }
        auto stmt = this->makeNodeString(Node::Kind::TypeSpecificationStaticData, identifier);
        stmt->children.emplace_back(std::move(type.node));
        stmt->children.emplace_back(std::move(expr.node));
        return context.success(std::move(stmt), expr.tokensAfter + 1);
      }
      return context.expected(type.tokensAfter + 1, "'=' after identifier '", identifier.value.s, "' in definition of static property");
    }
    Partial parseTypeSpecificationAccess(Partial& partial, const EggTokenizerItem& identifier) {
      // 'partial' is the successful parsing of <type> before the <identifier>
      assert(partial.succeeded());
      auto stmt = this->makeNodeString(Node::Kind::TypeSpecificationInstanceData, identifier);
      stmt->children.emplace_back(std::move(partial.node));
      Context context(*this, partial.tokensBefore);
      auto nxtidx = partial.tokensAfter + 1;
      auto* next = &this->getAbsolute(nxtidx);
      if (next->isOperator(EggTokenizerOperator::Semicolon)) {
        // <type> <identifier> ;
        ++nxtidx;
      } else {
        // <type> <identifier> {
        assert(next->isOperator(EggTokenizerOperator::CurlyLeft));
        auto curly = nxtidx;
        std::map<String, Accessability> known{
          { String::fromUTF8(this->allocator, "get"), Accessability::Get },
          { String::fromUTF8(this->allocator, "set"), Accessability::Set },
          { String::fromUTF8(this->allocator, "mut"), Accessability::Mut },
          { String::fromUTF8(this->allocator, "ref"), Accessability::Ref },
          { String::fromUTF8(this->allocator, "del"), Accessability::Del }
        };
        next = &this->getAbsolute(++nxtidx);
        auto empty = true;
        while (!next->isOperator(EggTokenizerOperator::CurlyRight)) {
          if (next->kind == EggTokenizerKind::Identifier) {
            auto found = known.find(next->value.s);
            if (found != known.end()) {
              auto access = this->makeNodeString(Node::Kind::TypeSpecificationAccess, *next);
              access->op.accessability = found->second;
              stmt->children.emplace_back(std::move(access));
              if (!this->getAbsolute(nxtidx + 1).isOperator(EggTokenizerOperator::Semicolon)) {
                context.tokensBefore = nxtidx;
                return context.expected(nxtidx + 1, "';' after '", next->value.s, "' in access clause of declaration of property '", identifier.value.s, "'");
              }
              nxtidx += 2;
              next = &this->getAbsolute(nxtidx);
              empty = false;
              continue;
            }
          }
          context.tokensBefore = nxtidx;
          return context.expected(nxtidx, "'get', 'set', 'mut', 'ref' or 'del' in access clause of declaration of property '", identifier.value.s, "'");
        }
        if (empty) {
          context.tokensBefore = curly;
          return context.failed(nxtidx, "Expected at least one 'get', 'set', 'mut', 'ref' or 'del' in access clause of declaration of property '", identifier.value.s, "'");
        }
        ++nxtidx;
      }
      return context.success(std::move(stmt), nxtidx);
    }
    Partial parseTypeFunctionSignature(Partial& rtype, const EggTokenizerItem& fname, size_t tokidx) {
      assert(rtype.succeeded());
      Context context(*this, tokidx);
      assert(context[0].isOperator(EggTokenizerOperator::ParenthesisLeft));
      auto signature = this->makeNodeString(Node::Kind::TypeFunctionSignature, fname);
      signature->range.begin = rtype.node->range.begin;
      signature->children.emplace_back(std::move(rtype.node));
      if (context[1].isOperator(EggTokenizerOperator::ParenthesisRight)) {
        // No parameters
        return context.success(std::move(signature), tokidx + 2);
      }
      auto nxtidx = tokidx + 1;
      auto ambiguous = false;
      for (;;) {
        // Parse the parameters
        auto parameter = this->parseTypeFunctionSignatureParameter(nxtidx);
        if (!parameter.succeeded()) {
          return parameter;
        }
        ambiguous |= parameter.ambiguous;
        nxtidx = parameter.tokensAfter;
        auto& next = parameter.after(0);
        signature->children.emplace_back(std::move(parameter.node));
        if (next.isOperator(EggTokenizerOperator::ParenthesisRight)) {
          signature->range.end = { next.line, next.column + 1 };
          return context.success(std::move(signature), nxtidx + 1);
        }
        if (!next.isOperator(EggTokenizerOperator::Comma)) {
          return context.expected(nxtidx, "',' between parameters in definition of function '", fname.value.s, "'");
        }
        nxtidx++;
      }
    }
    Partial parseTypeFunctionSignatureParameter(size_t tokidx) {
      Context context(*this, tokidx);
      auto type = this->parseTypeExpression(tokidx);
      if (!type.succeeded()) {
        return type;
      }
      auto& pname = type.after(0);
      if (pname.kind != EggTokenizerKind::Identifier) {
        // Note we don't allow keywords
        return context.expected(type.tokensAfter, "parameter name");
      }
      if (type.after(1).isOperator(EggTokenizerOperator::Equal)) {
        // <type> <name> = null
        if (!type.after(2).isKeyword(EggTokenizerKeyword::Null)) {
          return context.expected(type.tokensAfter + 2, "'null' as default value after '=' in function parameter definition");
        }
        auto optional = this->makeNodeString(Node::Kind::TypeFunctionSignatureParameter, pname);
        optional->op.parameterOp = Node::ParameterOp::Optional;
        optional->children.emplace_back(std::move(type.node));
        return context.success(std::move(optional), type.tokensAfter + 3);
      }
      // <type> <name>
      auto required = this->makeNodeString(Node::Kind::TypeFunctionSignatureParameter, pname);
      required->op.parameterOp = Node::ParameterOp::Required;
      required->children.emplace_back(std::move(type.node));
      return context.success(std::move(required), type.tokensAfter + 1);
    }
    Partial parseGuardExpression(size_t tokidx) {
      Context context(*this, tokidx);
      if (context[0].isKeyword(EggTokenizerKeyword::Var)) {
        // Inferred type
        if (context[1].isOperator(EggTokenizerOperator::Query)) {
          auto varq = this->makeNode(Node::Kind::TypeInferQ, context[0]);
          return this->parseGuardExpressionIdentifier(tokidx + 2, varq, "'var?'", false);
        }
        auto var = this->makeNode(Node::Kind::TypeInfer, context[0]);
        return this->parseGuardExpressionIdentifier(tokidx + 1, var, "'var'", false);
      }
      auto partial = this->parseTypeExpression(tokidx);
      if (partial.succeeded()) {
        auto guarded = this->parseGuardExpressionIdentifier(partial.tokensAfter, partial.node, "type", partial.ambiguous);
        if (!guarded.skipped()) {
          return guarded;
        }
      }
      return this->parseValueExpression(tokidx);
    }
    Partial parseGuardExpressionIdentifier(size_t tokidx, std::unique_ptr<Node>& ptype, const char* what, bool ambiguous) {
      assert(ptype != nullptr);
      Context context(*this, tokidx);
      if (context[0].kind != EggTokenizerKind::Identifier) {
        if (ambiguous) {
          return context.skip();
        }
        return context.expected(tokidx, "identifier after ", what, " in guard expression");
      }
      if (!context[1].isOperator(EggTokenizerOperator::Equal)) {
        return context.expected(tokidx + 1, "'=' after identifier '", context[0].value.s, "' in guard expression");
      }
      // <type> <identifier> = <expr>
      auto expr = this->parseValueExpression(tokidx + 2);
      if (expr.succeeded()) {
        auto stmt = this->makeNodeString(Node::Kind::ExprGuard, context[0]);
        stmt->children.emplace_back(std::move(ptype));
        stmt->children.emplace_back(std::move(expr.node));
        return context.success(std::move(stmt), expr.tokensAfter);
      }
      return expr;
    }
    Partial parseValueExpression(size_t tokidx) {
      return this->parseValueExpressionTernary(tokidx);
    }
    Partial parseValueExpressionOptional(size_t tokidx, EggTokenizerOperator terminal) {
      Context context(*this, tokidx);
      if (context[0].isOperator(terminal)) {
        // Missing expression
        auto stmt = this->makeNode(Node::Kind::Missing, context[0]);
        return context.success(std::move(stmt), tokidx);
      }
      return this->parseValueExpression(tokidx);
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
            lhs.node->range.end = rhs.node->range.end;
            lhs.node->children.emplace_back(std::move(mid.node));
            lhs.node->children.emplace_back(std::move(rhs.node));
            lhs.node->op.valueTernaryOp = ValueTernaryOp::IfThenElse;
            lhs.tokensAfter = rhs.tokensAfter;
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
          return this->parseValueExpressionBinaryOperator(lhs, ValueBinaryOp::Remainder);
        case EggTokenizerOperator::Ampersand: // "&"
          return this->parseValueExpressionBinaryOperator(lhs, ValueBinaryOp::BitwiseAnd);
        case EggTokenizerOperator::AmpersandAmpersand: // "&&"
          return this->parseValueExpressionBinaryOperator(lhs, ValueBinaryOp::IfTrue);
        case EggTokenizerOperator::BangBang: // "!!"
          return this->parseValueExpressionBinaryOperator(lhs, ValueBinaryOp::IfVoid);
        case EggTokenizerOperator::BangEqual: // "!="
          return this->parseValueExpressionBinaryOperator(lhs, ValueBinaryOp::NotEqual);
        case EggTokenizerOperator::Star: // "*"
          return this->parseValueExpressionBinaryOperator(lhs, ValueBinaryOp::Multiply);
        case EggTokenizerOperator::Plus: // "+"
          return this->parseValueExpressionBinaryOperator(lhs, ValueBinaryOp::Add);
        case EggTokenizerOperator::Slash: // "/"
          return this->parseValueExpressionBinaryOperator(lhs, ValueBinaryOp::Divide);
        case EggTokenizerOperator::Minus: // "-"
          return this->parseValueExpressionBinaryOperator(lhs, ValueBinaryOp::Subtract);
        case EggTokenizerOperator::Less: // "<"
          return this->parseValueExpressionBinaryOperator(lhs, ValueBinaryOp::LessThan);
        case EggTokenizerOperator::ShiftLeft: // "<<"
          return this->parseValueExpressionBinaryOperator(lhs, ValueBinaryOp::ShiftLeft);
        case EggTokenizerOperator::LessEqual: // "<="
          return this->parseValueExpressionBinaryOperator(lhs, ValueBinaryOp::LessThanOrEqual);
        case EggTokenizerOperator::LessBar: // "<|"
          return this->parseValueExpressionBinaryOperator(lhs, ValueBinaryOp::Minimum);
        case EggTokenizerOperator::EqualEqual: // "=="
          return this->parseValueExpressionBinaryOperator(lhs, ValueBinaryOp::Equal);
        case EggTokenizerOperator::Greater: // ">"
          return this->parseValueExpressionBinaryOperator(lhs, ValueBinaryOp::GreaterThan);
        case EggTokenizerOperator::GreaterEqual: // ">="
          return this->parseValueExpressionBinaryOperator(lhs, ValueBinaryOp::GreaterThanOrEqual);
        case EggTokenizerOperator::GreaterBar: // ">|"
          return this->parseValueExpressionBinaryOperator(lhs, ValueBinaryOp::Maximum);
        case EggTokenizerOperator::ShiftRight: // ">>"
          return this->parseValueExpressionBinaryOperator(lhs, ValueBinaryOp::ShiftRight);
        case EggTokenizerOperator::ShiftRightUnsigned: // ">>>"
          return this->parseValueExpressionBinaryOperator(lhs, ValueBinaryOp::ShiftRightUnsigned);
        case EggTokenizerOperator::QueryQuery: // "??"
          return this->parseValueExpressionBinaryOperator(lhs, ValueBinaryOp::IfNull);
        case EggTokenizerOperator::Caret: // "^"
          return this->parseValueExpressionBinaryOperator(lhs, ValueBinaryOp::BitwiseXor);
        case EggTokenizerOperator::Bar: // "|"
          return this->parseValueExpressionBinaryOperator(lhs, ValueBinaryOp::BitwiseOr);
        case EggTokenizerOperator::BarBar: // "||"
          return this->parseValueExpressionBinaryOperator(lhs, ValueBinaryOp::IfFalse);
        case EggTokenizerOperator::CurlyLeft: // "{"
          return this->parseObjectSpecification(lhs);
        case EggTokenizerOperator::Bang: // "!"
        case EggTokenizerOperator::BangBangEqual: // "!!="
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
        case EggTokenizerOperator::LessBarEqual: // "<|="
        case EggTokenizerOperator::Equal: // "="
        case EggTokenizerOperator::ShiftRightEqual: // ">>="
        case EggTokenizerOperator::ShiftRightUnsignedEqual: // ">>>="
        case EggTokenizerOperator::GreaterBarEqual: // ">|="
        case EggTokenizerOperator::Query: // "?"
        case EggTokenizerOperator::QueryQueryEqual: // "??="
        case EggTokenizerOperator::BracketLeft: // "["
        case EggTokenizerOperator::BracketRight: // "]"
        case EggTokenizerOperator::CaretEqual: // "^="
        case EggTokenizerOperator::BarEqual: // "|="
        case EggTokenizerOperator::BarBarEqual: // "||="
        case EggTokenizerOperator::CurlyRight: // "}"
        case EggTokenizerOperator::Tilde: // "~"
          break;
        }
      }
      return lhs;
    }
    Partial parseValueExpressionBinaryOperator(Partial& lhs, ValueBinaryOp op) {
      assert(lhs.succeeded());
      auto rhs = this->parseValueExpression(lhs.tokensAfter + 1);
      if (!rhs.succeeded()) {
        return rhs;
      }
      if (rhs.node->kind == Node::Kind::ExprBinary) {
        // Need to worry about operator precedence
        auto precedence1 = precedence(op);
        assert(precedence1 > 0);
        auto precedence2 = precedence(rhs.node->op.valueBinaryOp);
        assert(precedence2 > 0);
        if (precedence1 > precedence2) {
          // OPTIMIZE
          // e.g. 'a*b+c' needs to parse to '[[a*b]+c]' not '[a*[b+c]]'
          rhs.node->range.begin = lhs.node->range.begin;
          auto& head = rhs.node->children[0];
          auto mid = this->makeNode(Node::Kind::ExprBinary, { lhs.node->range.begin, head->range.end });
          mid->children.emplace_back(std::move(lhs.node));
          mid->children.emplace_back(std::move(head));
          mid->op.valueBinaryOp = op;
          rhs.node->children[0] = std::move(mid);
          return rhs;
        }
      }
      lhs.wrap(Node::Kind::ExprBinary);
      lhs.node->range.end = rhs.node->range.end;
      lhs.node->children.emplace_back(std::move(rhs.node));
      lhs.node->op.valueBinaryOp = op;
      lhs.tokensAfter = rhs.tokensAfter;
      return std::move(lhs);
    }
    Partial parseValueExpressionUnary(size_t tokidx) {
      Context context(*this, tokidx);
      const auto& op = context[0];
      if (op.kind == EggTokenizerKind::Operator) {
        switch (op.value.o) {
        case EggTokenizerOperator::Bang: // "!"
          return this->parseValueExpressionUnaryOperator(tokidx, ValueUnaryOp::LogicalNot);
        case EggTokenizerOperator::Minus: // "-"
          return this->parseValueExpressionUnaryOperator(tokidx, ValueUnaryOp::Negate);
        case EggTokenizerOperator::Tilde: // "~"
          return this->parseValueExpressionUnaryOperator(tokidx, ValueUnaryOp::BitwiseNot);
        case EggTokenizerOperator::Star: // "*"
          return this->parseValueExpressionPrimaryWrap(tokidx, Node::Kind::ExprDereference);
        case EggTokenizerOperator::Ampersand: // "&"
          return this->parseValueExpressionPrimaryWrap(tokidx, Node::Kind::ExprReference);
        case EggTokenizerOperator::PlusPlus: // "++"
          return context.failed(tokidx, "Increment operator '++' cannot be used in expressions");
        case EggTokenizerOperator::MinusMinus: // "--"
          return context.failed(tokidx, "Decrement operator '--' cannot be used in expressions");
        case EggTokenizerOperator::ParenthesisLeft: // "("
        case EggTokenizerOperator::BracketLeft: // "["
        case EggTokenizerOperator::CurlyLeft: // "{"
          break;
        case EggTokenizerOperator::BangBang: // "!!"
        case EggTokenizerOperator::BangBangEqual: // "!!="
        case EggTokenizerOperator::BangEqual: // "!="
        case EggTokenizerOperator::Percent: // "%"
        case EggTokenizerOperator::PercentEqual: // "%="
        case EggTokenizerOperator::AmpersandAmpersand: // "&&"
        case EggTokenizerOperator::AmpersandAmpersandEqual: // "&&="
        case EggTokenizerOperator::AmpersandEqual: // "&="
        case EggTokenizerOperator::ParenthesisRight: // ")"
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
        case EggTokenizerOperator::LessBar: // "<|"
        case EggTokenizerOperator::LessBarEqual: // "<|="
        case EggTokenizerOperator::Equal: // "="
        case EggTokenizerOperator::EqualEqual: // "=="
        case EggTokenizerOperator::Greater: // ">"
        case EggTokenizerOperator::GreaterEqual: // ">="
        case EggTokenizerOperator::ShiftRight: // ">>"
        case EggTokenizerOperator::ShiftRightEqual: // ">>="
        case EggTokenizerOperator::ShiftRightUnsigned: // ">>>"
        case EggTokenizerOperator::ShiftRightUnsignedEqual: // ">>>="
        case EggTokenizerOperator::GreaterBar: // ">|"
        case EggTokenizerOperator::GreaterBarEqual: // ">|="
        case EggTokenizerOperator::Query: // "?"
        case EggTokenizerOperator::QueryQuery: // "??"
        case EggTokenizerOperator::QueryQueryEqual: // "??="
        case EggTokenizerOperator::BracketRight: // "]"
        case EggTokenizerOperator::Caret: // "^"
        case EggTokenizerOperator::CaretEqual: // "^="
        case EggTokenizerOperator::Bar: // "|"
        case EggTokenizerOperator::BarEqual: // "|="
        case EggTokenizerOperator::BarBar: // "||"
        case EggTokenizerOperator::BarBarEqual: // "||="
        case EggTokenizerOperator::CurlyRight: // "}"
          return context.expected(tokidx, "unary prefix operator");
        }
      }
      return this->parseValueExpressionPrimary(tokidx, "expression");
    }
    Partial parseValueExpressionUnaryOperator(size_t tokidx, ValueUnaryOp op) {
      auto rhs = this->parseValueExpressionPrimaryWrap(tokidx, Node::Kind::ExprUnary);
      if (rhs.succeeded()) {
        rhs.node->op.valueUnaryOp = op;
      }
      return rhs;
    }
    Partial parseValueExpressionPrimaryWrap(size_t tokidx, Node::Kind kind) {
      auto rhs = this->parseValueExpressionPrimary(tokidx + 1, "expression");
      if (rhs.succeeded()) {
        auto& prefix = this->getAbsolute(tokidx);
        rhs.wrap(kind);
        rhs.node->range.begin = { prefix.line, prefix.column };
      }
      return rhs;
    }
    Partial parseValueExpressionPrimary(size_t tokidx, const char* expected) {
      auto partial = this->parseValueExpressionPrimaryPrefix(tokidx, expected);
      while (partial.succeeded()) {
        if (!this->parseValueExpressionPrimarySuffix(partial)) {
          break;
        }
      }
      return partial;
    }
    Partial parseValueExpressionPrimaryPrefix(size_t tokidx, const char* expected) {
      std::unique_ptr<Node> node;
      Context context(*this, tokidx);
      auto& next = context[0];
      switch (next.kind) {
      case EggTokenizerKind::Integer:
        node = this->makeNodeInt(Node::Kind::Literal, next);
        return context.success(std::move(node), tokidx + 1);
      case EggTokenizerKind::Float:
        node = this->makeNodeFloat(Node::Kind::Literal, next);
        return context.success(std::move(node), tokidx + 1);
      case EggTokenizerKind::String:
        node = this->makeNodeString(Node::Kind::Literal, next);
        return context.success(std::move(node), tokidx + 1);
      case EggTokenizerKind::Identifier:
        node = this->makeNodeString(Node::Kind::Variable, next);
        return context.success(std::move(node), tokidx + 1);
      case EggTokenizerKind::Keyword:
        return this->parseValueExpressionPrimaryPrefixKeyword(tokidx);
      case EggTokenizerKind::Operator:
        if (next.isOperator(EggTokenizerOperator::ParenthesisLeft)) {
          return this->parseValueExpressionParentheses(tokidx);
        }
        if (next.isOperator(EggTokenizerOperator::BracketLeft)) {
          return this->parseValueExpressionArray(tokidx);
        }
        if (next.isOperator(EggTokenizerOperator::CurlyLeft)) {
          return this->parseValueExpressionEon(tokidx);
        }
        break;
      case EggTokenizerKind::Attribute:
      case EggTokenizerKind::EndOfFile:
        break;
      }
      return context.expected(tokidx, expected);
    }
    Partial parseValueExpressionPrimaryPrefixKeyword(size_t tokidx) {
      Context context(*this, tokidx);
      assert(context[0].kind == EggTokenizerKind::Keyword);
      switch (context[0].value.k) {
      case EggTokenizerKeyword::Any:
        return this->parseValueExpressionPrimaryPrefixKeywordManifestation(context, Node::Kind::TypeAny);
      case EggTokenizerKeyword::Bool:
        return this->parseValueExpressionPrimaryPrefixKeywordManifestation(context, Node::Kind::TypeBool);
      case EggTokenizerKeyword::Float:
        return this->parseValueExpressionPrimaryPrefixKeywordManifestation(context, Node::Kind::TypeFloat);
      case EggTokenizerKeyword::Int:
        return this->parseValueExpressionPrimaryPrefixKeywordManifestation(context, Node::Kind::TypeInt);
      case EggTokenizerKeyword::Object:
        return this->parseValueExpressionPrimaryPrefixKeywordManifestation(context, Node::Kind::TypeObject);
      case EggTokenizerKeyword::String:
        return this->parseValueExpressionPrimaryPrefixKeywordManifestation(context, Node::Kind::TypeString);
      case EggTokenizerKeyword::Void:
        return this->parseValueExpressionPrimaryPrefixKeywordManifestation(context, Node::Kind::TypeVoid);
      case EggTokenizerKeyword::Type:
        return this->parseValueExpressionPrimaryPrefixKeywordManifestation(context, Node::Kind::TypeType);
      case EggTokenizerKeyword::False:
        return this->parseValueExpressionPrimaryPrefixKeywordLiteral(context, HardValue::False);
      case EggTokenizerKeyword::Null:
        return this->parseValueExpressionPrimaryPrefixKeywordLiteral(context, HardValue::Null);
      case EggTokenizerKeyword::True:
        return this->parseValueExpressionPrimaryPrefixKeywordLiteral(context, HardValue::True);
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
      case EggTokenizerKeyword::Static:
      case EggTokenizerKeyword::Switch:
      case EggTokenizerKeyword::Throw:
      case EggTokenizerKeyword::Try:
      case EggTokenizerKeyword::Var:
      case EggTokenizerKeyword::While:
      case EggTokenizerKeyword::Yield:
        break;
      }
      return context.expected(tokidx, "expression");
    }
    Partial parseValueExpressionPrimaryPrefixKeywordManifestation(Context& context, Node::Kind kind) {
      assert(context[0].kind == EggTokenizerKind::Keyword);
      auto node = this->makeNode(kind, context[0]);
      return context.success(std::move(node), context.tokensBefore + 1);
    }
    Partial parseValueExpressionPrimaryPrefixKeywordLiteral(Context& context, const HardValue& value) {
      assert(context[0].kind == EggTokenizerKind::Keyword);
      auto node = this->makeNodeValue(Node::Kind::Literal, context[0], value);
      return context.success(std::move(node), context.tokensBefore + 1);
    }
    bool parseValueExpressionPrimarySuffix(Partial& partial) {
      assert(partial.succeeded());
      auto* next = &partial.after(0);
      if (next->isOperator(EggTokenizerOperator::ParenthesisLeft)) {
        // Function call
        partial.wrap(Node::Kind::ExprCall);
        partial.tokensAfter++;
        next = &partial.after(0);
        if (next->isOperator(EggTokenizerOperator::ParenthesisRight)) {
          // No arguments
          partial.node->range.end = { next->line, next->column + 1 };
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
          next = &argument.after(0);
          partial.node->children.emplace_back(std::move(argument.node));
          partial.tokensAfter = argument.tokensAfter + 1;
          if (next->isOperator(EggTokenizerOperator::ParenthesisRight)) {
            break;
          }
          if (!next->isOperator(EggTokenizerOperator::Comma)) {
            partial.fail("Expected ',' between function call arguments, but instead got ", next->toString());
            return false;
          }
        }
        partial.node->range.end = { next->line, next->column + 1 };
        return true;
      }
      if (next->isOperator(EggTokenizerOperator::Dot)) {
        // Property access
        auto& property = partial.after(1);
        if (!property.isPropertyName()) {
          partial.fail("Expected property name after '.', but instead got ", property.toString());
          return false;
        }
        auto rhs = this->makeNodeString(Node::Kind::Literal, property);
        partial.wrap(Node::Kind::ExprProperty);
        partial.node->children.emplace_back(std::move(rhs)); 
        partial.node->range.end = { property.line, property.column + property.width };
        partial.tokensAfter += 2;
        return true;
      }
      if (next->isOperator(EggTokenizerOperator::BracketLeft)) {
        // Indexing
        auto index = this->parseValueExpression(partial.tokensAfter + 1);
        if (!index.succeeded()) {
          partial.fail(index);
          return false;
        }
        next = &index.after(0);
        if (!next->isOperator(EggTokenizerOperator::BracketRight)) {
          partial.fail("Expected ']' after index, but instead got ", next->toString());
          return false;
        }
        partial.wrap(Node::Kind::ExprIndex);
        partial.node->range.end = { next->line, next->column + 1 };
        partial.node->children.emplace_back(std::move(index.node));
        partial.tokensAfter = index.tokensAfter + 1;
        return true;
      }
      return false;
    }
    Partial parseValueExpressionParentheses(size_t tokidx) {
      // Parenthesis: (expression)
      Context context(*this, tokidx);
      assert(context[0].isOperator(EggTokenizerOperator::ParenthesisLeft));
      auto partial = this->parseValueExpression(tokidx + 1);
      if (!partial.succeeded()) {
        return partial;
      }
      if (!partial.after(0).isOperator(EggTokenizerOperator::ParenthesisRight)) {
        return context.expected(partial.tokensAfter, "')' after parenthesised expression");
      }
      partial.tokensBefore--;
      partial.tokensAfter++;
      return partial;
    }
    Partial parseValueExpressionArray(size_t tokidx) {
      // Array literal: [a, ...b, c]
      Context context(*this, tokidx);
      auto& bracket = context[0];
      assert(bracket.isOperator(EggTokenizerOperator::BracketLeft));
      auto array = this->makeNode(Node::Kind::ExprArray, bracket);
      auto partial = context.success(std::move(array), tokidx + 1);
      size_t index = 0;
      while (this->parseValueExpressionArrayElement(partial, index)) {
        ++index;
      }
      return partial;
    }
    bool parseValueExpressionArrayElement(Partial& partial, size_t index) {
      assert(partial.succeeded());
      auto* next = &partial.after(0);
      if (next->isOperator(EggTokenizerOperator::BracketRight)) {
        partial.node->range.end = { next->line, next->column + 1 };
        partial.tokensAfter++;
        return false;
      }
      if (index > 0) {
        if (!next->isOperator(EggTokenizerOperator::Comma)) {
          partial.fail("Expected ',' between array elements, but instead got ", next->toString());
          return false;
        }
        partial.tokensAfter++;
      }
      auto expr = this->parseValueExpression(partial.tokensAfter);
      if (!expr.succeeded()) {
        partial.fail(expr);
        return false;
      }
      partial.node->children.emplace_back(std::move(expr.node));
      partial.tokensAfter = expr.tokensAfter;
      return true;
    }
    Partial parseValueExpressionEon(size_t tokidx) {
      // Eon literal: {a:x, ...b, c:y}
      Context context(*this, tokidx);
      auto& curly = context[0];
      assert(curly.isOperator(EggTokenizerOperator::CurlyLeft));
      auto array = this->makeNode(Node::Kind::ExprEon, curly);
      auto partial = context.success(std::move(array), tokidx + 1);
      size_t index = 0;
      while (this->parseValueExpressionEonElement(partial, index)) {
        ++index;
      }
      return partial;
    }
    bool parseValueExpressionEonElement(Partial& partial, size_t index) {
      assert(partial.succeeded());
      auto* next = &partial.after(0);
      if (next->isOperator(EggTokenizerOperator::CurlyRight)) {
        partial.node->range.end = { next->line, next->column + 1 };
        partial.tokensAfter++;
        return false;
      }
      if (index > 0) {
        if (!next->isOperator(EggTokenizerOperator::Comma)) {
          partial.fail("Expected ',' between object elements, but instead got ", next->toString());
          return false;
        }
        partial.tokensAfter++;
      }
      next = &partial.after(0);
      const EggTokenizerItem* name = nullptr;
      switch (next->kind) {
      case EggTokenizerKind::String:
        // Allow quoted property names
        name = next;
        break;
      case EggTokenizerKind::Identifier:
      case EggTokenizerKind::Keyword:
        // Note we allow identifiers and keywords
        name = next;
        break;
      case EggTokenizerKind::Integer:
      case EggTokenizerKind::Float:
      case EggTokenizerKind::Attribute:
      case EggTokenizerKind::Operator:
      case EggTokenizerKind::EndOfFile:
        partial.fail("Expected object element name, but instead got ", next->toString());
        return false;
      }
      partial.tokensAfter++;
      next = &partial.after(0);
      if (!next->isOperator(EggTokenizerOperator::Colon)) {
        partial.fail("Expected ':' after object element name, but instead got ", next->toString());
        return false;
      }
      auto expr = this->parseValueExpression(partial.tokensAfter + 1);
      if (!expr.succeeded()) {
        partial.fail(expr);
        return false;
      }
      partial.node->range.end = expr.node->range.end;
      assert(name != nullptr);
      auto named = this->makeNodeString(Node::Kind::Named, *name);
      named->children.emplace_back(std::move(expr.node));
      partial.node->children.emplace_back(std::move(named));
      partial.tokensAfter = expr.tokensAfter;
      return true;
    }
    Partial parseObjectSpecification(Partial& partial) {
      // 'object {' or '<type-expression> {'
      assert(partial.succeeded());
      Context context(*this, partial.tokensAfter);
      assert(context[0].isOperator(EggTokenizerOperator::CurlyLeft));
      partial.wrap(Node::Kind::ExprObject);
      auto nxtidx = partial.tokensAfter + 1;
      while (!this->getAbsolute(nxtidx).isOperator(EggTokenizerOperator::CurlyRight)) {
        auto inner = this->parseObjectSpecificationClause(nxtidx);
        assert(!inner.skipped());
        if (!inner.succeeded()) {
          return inner;
        }
        partial.node->children.emplace_back(std::move(inner.node));
        nxtidx = inner.tokensAfter;
      }
      partial.tokensAfter = nxtidx + 1;
      return std::move(partial);
    }
    Partial parseObjectSpecificationClause(size_t tokidx) {
      Context context(*this, tokidx);
      auto type = this->parseTypeExpression(tokidx);
      if (!type.succeeded()) {
        if (type.skipped()) {
          return context.expected(tokidx, "property definition");
        }
        return type;
      }
      // TODO handle generators
      auto& identifier = type.after(0);
      if (identifier.kind != EggTokenizerKind::Identifier) {
        return context.expected(type.tokensAfter, "identifier after type in property definition");
      }
      auto& next = type.after(1);
      if (next.isOperator(EggTokenizerOperator::ParenthesisLeft)) {
        // <type> <identifier> (
        auto signature = this->parseTypeFunctionSignature(type, identifier, type.tokensAfter + 1);
        if (!signature.succeeded()) {
          return signature;
        }
        if (!signature.after(0).isOperator(EggTokenizerOperator::CurlyLeft)) {
          return context.expected(signature.tokensAfter, "'{' after ')' in definition of property function '", identifier.value.s, "'");
        }
        // <type> <identifier> ( ... ) {
        auto block = this->parseStatementBlock(signature.tokensAfter);
        if (!block.succeeded()) {
          return block;
        }
        auto stmt = this->makeNodeString(Node::Kind::ObjectSpecificationFunction, identifier);
        stmt->children.emplace_back(std::move(signature.node));
        stmt->children.emplace_back(std::move(block.node));
        return context.success(std::move(stmt), block.tokensAfter);
      }
      if (next.isOperator(EggTokenizerOperator::Equal)) {
        // static <type> <identifier> =
        auto expr = this->parseValueExpression(type.tokensAfter + 2);
        if (!expr.succeeded()) {
          return expr;
        }
        if (!expr.after(0).isOperator(EggTokenizerOperator::Semicolon)) {
          return context.expected(expr.tokensAfter, "';' after value of static property '", identifier.value.s, "'");
        }
        auto stmt = this->makeNodeString(Node::Kind::ObjectSpecificationData, identifier);
        stmt->children.emplace_back(std::move(type.node));
        stmt->children.emplace_back(std::move(expr.node));
        return context.success(std::move(stmt), expr.tokensAfter + 1);
      }
      return context.expected(type.tokensAfter + 1, "'=' after identifier '", identifier.value.s, "' in definition of property");
    }
    std::unique_ptr<Node> makeNode(Node::Kind kind, const SourceRange& range) {
      auto node = std::make_unique<Node>(kind);
      node->range = range;
      return node;
    }
    std::unique_ptr<Node> makeNode(Node::Kind kind, const EggTokenizerItem& item) {
      auto node = std::make_unique<Node>(kind);
      node->range.begin.line = item.line;
      node->range.begin.column = item.column;
      if (item.width > 0) {
        node->range.end.line = item.line;
        node->range.end.column = item.column + item.width;
      } else {
        node->range.end.line = 0;
        node->range.end.column = 0;
      }
      return node;
    }
    std::unique_ptr<Node> makeNodeValue(Node::Kind kind, const EggTokenizerItem& item, const HardValue& value) {
      auto node = this->makeNode(kind, item);
      node->value = value;
      return node;
    }
    std::unique_ptr<Node> makeNodeInt(Node::Kind kind, const EggTokenizerItem& item) {
      return this->makeNodeValue(kind, item, ValueFactory::createInt(this->allocator, item.value.i));
    }
    std::unique_ptr<Node> makeNodeFloat(Node::Kind kind, const EggTokenizerItem& item) {
      return this->makeNodeValue(kind, item, ValueFactory::createFloat(this->allocator, item.value.f));
    }
    std::unique_ptr<Node> makeNodeString(Node::Kind kind, const EggTokenizerItem& item) {
      assert((kind == Node::Kind::Literal) || !item.value.s.empty());
      return this->makeNodeValue(kind, item, ValueFactory::createString(this->allocator, item.value.s));
    }
  };

  EggParser::Partial::Partial(const Context& context, std::unique_ptr<Node>&& node, size_t tokensAfter, size_t issuesAfter, bool ambiguous)
    : parser(context.parser),
      node(std::move(node)),
      tokensBefore(context.tokensBefore),
      issuesBefore(context.issuesBefore),
      tokensAfter(tokensAfter),
      issuesAfter(issuesAfter),
      ambiguous(ambiguous) {
      assert(this->tokensBefore <= this->tokensAfter);
      assert(this->issuesBefore <= this->issuesAfter);
  }
}

std::shared_ptr<IEggParser> EggParserFactory::createFromTokenizer(egg::ovum::IAllocator& allocator, const std::shared_ptr<IEggTokenizer>& tokenizer) {
  return std::make_shared<EggParser>(allocator, tokenizer);
}
