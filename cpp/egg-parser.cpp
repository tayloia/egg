#include "yolk.h"

namespace {
  using namespace egg::yolk;

  class EggParserLookahead {
    EGG_NO_COPY(EggParserLookahead);
  private:
    IEggTokenizer& tokenizer;
    std::deque<EggTokenizerItem> upcoming;
  public:
    explicit EggParserLookahead(IEggTokenizer& tokenizer)
      : tokenizer(tokenizer) {
    }
    const EggTokenizerItem& peek(size_t index = 0) {
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
      this->tokenizer.next(this->upcoming.emplace_back());
    }
  };

  class EggParserLookaheadWithRewind {
    EGG_NO_COPY(EggParserLookaheadWithRewind);
  private:
    EggParserLookahead lookahead;
    size_t eaten;
  public:
    explicit EggParserLookaheadWithRewind(IEggTokenizer& tokenizer)
      : lookahead(tokenizer), eaten(0) {
    }
    const EggTokenizerItem& peek(size_t index = 0) {
      return this->lookahead.peek(eaten + index);
    }
    void eat(size_t count = 1) {
      assert(count > 0);
      this->eaten += count;
    }
    void commit() {
      if (this->eaten > 0) {
        this->lookahead.pop(this->eaten);
        this->eaten = 0;
      }
    }
    void rewind() {
      this->eaten = 0;
    }
  };

  class EggParserContext {
    EGG_NO_COPY(EggParserContext);
  private:
    EggParserLookaheadWithRewind lookahead;
  public:
    explicit EggParserContext(IEggTokenizer& tokenizer)
      : lookahead(tokenizer) {
    }
    const EggTokenizerItem& peek(size_t index = 0) {
      return this->lookahead.peek(index);
    }
    std::unique_ptr<IEggSyntaxNode> parseStatement() {
      EGG_THROW("Not implemented: parseStatement"); // TODO
    }
  };

  class EggSyntaxNodeModule : public IEggSyntaxNode {
  private:
    std::vector<std::unique_ptr<IEggSyntaxNode>> children;
  public:
    virtual ~EggSyntaxNodeModule() {
    }
    virtual EggSyntaxNodeKind getKind() const override {
      return EggSyntaxNodeKind::Module;
    }
    virtual size_t getChildren() const override {
      return this->children.size();
    }
    virtual IEggSyntaxNode& getChild(size_t index) const override {
      return *this->children[index];
    }
    void parse(EggParserContext& context);
  };

  class EggParserModule : public IEggParser {
  public:
    virtual std::shared_ptr<const IEggSyntaxNode> parse(IEggTokenizer& tokenizer) override {
      EggParserContext context(tokenizer);
      auto module = std::make_shared<EggSyntaxNodeModule>();
      module->parse(context);
      return module;
    }
  };
}

void EggSyntaxNodeModule::parse(EggParserContext& context) {
  while (context.peek().kind != EggTokenizerKind::EndOfFile) {
    this->children.emplace_back(std::move(context.parseStatement()));
  }
}

std::shared_ptr<egg::yolk::IEggParser> egg::yolk::EggParserFactory::create(EggSyntaxNodeKind kind) {
  if (kind != EggSyntaxNodeKind::Module) {
    EGG_THROW("Not implemented: EggParseFactory for that kind"); // TODO
  }
  return std::make_shared<EggParserModule>();
}
