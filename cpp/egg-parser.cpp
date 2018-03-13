#include "yolk.h"
#include "lexers.h"
#include "egg-tokenizer.h"
#include "egg-syntax.h"
#include "egg-parser.h"

namespace {
  using namespace egg::yolk;

  const EggParserType typeVoid(egg::lang::TypeStorage::Void);

  void tagToStringComponent(std::string& dst, const char* text, bool bit) {
    if (bit) {
      if (!dst.empty()) {
        dst.append("|");
      }
      dst.append(text);
    }
  }

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
    ParserDump& add(const std::shared_ptr<IEggParserNode>& child) {
      if (child != nullptr) {
        *this->os << ' ';
        child->dump(*this->os);
      }
      return *this;
    }
    ParserDump& add(const std::vector<std::shared_ptr<IEggParserNode>>& children) {
      for (auto& child : children) {
        this->add(child);
      }
      return *this;
    }
    template<size_t N>
    ParserDump& add(const std::shared_ptr<IEggParserNode>(&children)[N]) {
      // TODO remove?
      for (auto& child : children) {
        this->add(child);
      }
      return *this;
    }
  };

  class EggParserNodeBase : public IEggParserNode {
  public:
    virtual ~EggParserNodeBase() {
    }
    virtual const EggParserType& getType() const override {
      return typeVoid;
    }
    static std::string unaryToString(EggParserUnary op);
    static std::string binaryToString(EggParserBinary op);
  };

  class EggParserNode_Empty : public EggParserNodeBase {
  public:
    virtual void dump(std::ostream & os) const override {
      ParserDump(os, "");
    }
  };

  class EggParserNode_Module : public EggParserNodeBase {
  private:
    std::vector<std::shared_ptr<IEggParserNode>> child;
  public:
    virtual void dump(std::ostream & os) const override {
      ParserDump(os, "module").add(this->child);
    }
    void addChild(const std::shared_ptr<IEggParserNode>& statement) {
      assert(statement != nullptr);
      this->child.push_back(statement);
    }
  };

  class EggParserNode_Block : public EggParserNodeBase {
  private:
    std::vector<std::shared_ptr<IEggParserNode>> child;
  public:
    virtual void dump(std::ostream & os) const override {
      ParserDump(os, "block").add(this->child);
    }
    void addChild(const std::shared_ptr<IEggParserNode>& statement) {
      assert(statement != nullptr);
      this->child.push_back(statement);
    }
  };

  class EggParserNode_Type : public EggParserNodeBase {
  private:
    EggParserType type;
  public:
    explicit EggParserNode_Type(EggParserType type)
      : type(type) {
    }
    virtual const EggParserType& getType() const override {
      return this->type;
    }
    virtual void dump(std::ostream & os) const override {
      ParserDump(os, "type").add(this->type.tagToString());
    }
  };

  class EggParserNode_Declare : public EggParserNodeBase {
  private:
    std::string name;
    std::shared_ptr<IEggParserNode> type;
    std::shared_ptr<IEggParserNode> init;
  public:
    EggParserNode_Declare(const std::string& name, const std::shared_ptr<IEggParserNode>& type, const std::shared_ptr<IEggParserNode>& init = nullptr)
      : name(name), type(type), init(init) {
      assert(type != nullptr);
    }
    virtual void dump(std::ostream & os) const override {
      ParserDump(os, "declare").add(this->name).add(this->type).add(this->init);
    }
  };

  class EggParserNode_Assignment : public EggParserNodeBase {
  private:
    EggParserBinary op;
    std::shared_ptr<IEggParserNode> lhs;
    std::shared_ptr<IEggParserNode> rhs;
  public:
    EggParserNode_Assignment(EggParserBinary op, const std::shared_ptr<IEggParserNode>& lhs, const std::shared_ptr<IEggParserNode>& rhs)
      : op(op), lhs(lhs), rhs(rhs) {
      assert(lhs != nullptr);
      assert(rhs != nullptr);
    }
    virtual void dump(std::ostream & os) const override {
      ParserDump(os, "assign").add(EggParserNodeBase::binaryToString(this->op)).add(this->lhs).add(this->rhs);
    }
  };

  class EggParserNode_Identifier : public EggParserNodeBase {
  private:
    std::string name;
  public:
    explicit EggParserNode_Identifier(const std::string& name)
      :name(name) {
    }
    virtual void dump(std::ostream & os) const override {
      ParserDump(os, "identifier").add(this->name);
    }
  };

  class EggParserContext : public IEggParserContext {
  public:
    virtual bool isAllowed(EggParserAllowed) override {
      return false; // TODO
    }
  };

  class EggParserModule : public IEggParser {
  public:
    virtual std::shared_ptr<IEggParserNode> parse(IEggTokenizer& tokenizer) override {
      auto syntax = EggParserFactory::createModuleSyntaxParser();
      auto ast = syntax->parse(tokenizer);
      EggParserContext context;
      return ast->promote(context);
    }
  };
}

std::string egg::yolk::EggParserType::tagToString(Tag tag) {
  using egg::lang::TypeStorage;
  if (tag == TypeStorage::Inferred) {
    return "var";
  }
  if (tag == TypeStorage::Void) {
    return "void";
  }
  if (tag == TypeStorage::Any) {
    return "any";
  }
  if (tag == (TypeStorage::Null | TypeStorage::Any)) {
    return "any?";
  }
  std::string result;
  tagToStringComponent(result, "bool", tag.hasBit(TypeStorage::Bool));
  tagToStringComponent(result, "int", tag.hasBit(TypeStorage::Int));
  tagToStringComponent(result, "float", tag.hasBit(TypeStorage::Float));
  tagToStringComponent(result, "string", tag.hasBit(TypeStorage::String));
  tagToStringComponent(result, "type", tag.hasBit(TypeStorage::Type));
  tagToStringComponent(result, "object", tag.hasBit(TypeStorage::Object));
  if (tag.hasBit(TypeStorage::Void)) {
    result.append("?");
  }
  return result;
}

#define EGG_PARSER_OPERATOR_STRING(name, text) text,

std::string EggParserNodeBase::unaryToString(egg::yolk::EggParserUnary op) {
  static const char* const table[] = {
    EGG_PARSER_UNARY_OPERATORS(EGG_PARSER_OPERATOR_STRING)
  };
  auto index = static_cast<size_t>(op);
  assert(index < EGG_NELEMS(table));
  return table[index];
}

std::string EggParserNodeBase::binaryToString(egg::yolk::EggParserBinary op) {
  static const char* const table[] = {
    EGG_PARSER_BINARY_OPERATORS(EGG_PARSER_OPERATOR_STRING)
  };
  auto index = static_cast<size_t>(op);
  assert(index < EGG_NELEMS(table));
  return table[index];
}

std::shared_ptr<egg::yolk::IEggParserNode> egg::yolk::EggSyntaxNode_Empty::promote(egg::yolk::IEggParserContext&) {
  EGG_THROW(__FUNCTION__ " TODO"); // TODO
}

std::shared_ptr<egg::yolk::IEggParserNode> egg::yolk::EggSyntaxNode_Module::promote(egg::yolk::IEggParserContext& context) {
  auto module = std::make_shared<EggParserNode_Module>();
  for (auto& statement : this->child) {
    module->addChild(statement->promote(context));
  }
  return module;
}

std::shared_ptr<egg::yolk::IEggParserNode> egg::yolk::EggSyntaxNode_Block::promote(egg::yolk::IEggParserContext& context) {
  auto module = std::make_shared<EggParserNode_Block>();
  for (auto& statement : this->child) {
    module->addChild(statement->promote(context));
  }
  return module;
}

std::shared_ptr<egg::yolk::IEggParserNode> egg::yolk::EggSyntaxNode_Type::promote(egg::yolk::IEggParserContext&) {
  auto type = std::make_shared<EggParserNode_Type>(EggParserType(this->tag));
  return type;
}

std::shared_ptr<egg::yolk::IEggParserNode> egg::yolk::EggSyntaxNode_VariableDeclaration::promote(egg::yolk::IEggParserContext& context) {
  return std::make_shared<EggParserNode_Declare>(this->name, this->child->promote(context));
}

std::shared_ptr<egg::yolk::IEggParserNode> egg::yolk::EggSyntaxNode_VariableInitialization::promote(egg::yolk::IEggParserContext& context) {
  return std::make_shared<EggParserNode_Declare>(this->name, this->child[0]->promote(context), this->child[1]->promote(context));
}

std::shared_ptr<egg::yolk::IEggParserNode> egg::yolk::EggSyntaxNode_Assignment::promote(egg::yolk::IEggParserContext& context) {
  return std::make_shared<EggParserNode_Assignment>(EggParserBinary::Equal /*TODO*/, this->child[0]->promote(context), this->child[1]->promote(context));
}

std::shared_ptr<egg::yolk::IEggParserNode> egg::yolk::EggSyntaxNode_Mutate::promote(egg::yolk::IEggParserContext&) {
  EGG_THROW(__FUNCTION__ " TODO"); // TODO
}

std::shared_ptr<egg::yolk::IEggParserNode> egg::yolk::EggSyntaxNode_Break::promote(egg::yolk::IEggParserContext&) {
  EGG_THROW(__FUNCTION__ " TODO"); // TODO
}

std::shared_ptr<egg::yolk::IEggParserNode> egg::yolk::EggSyntaxNode_Case::promote(egg::yolk::IEggParserContext&) {
  EGG_THROW(__FUNCTION__ " TODO"); // TODO
}

std::shared_ptr<egg::yolk::IEggParserNode> egg::yolk::EggSyntaxNode_Catch::promote(egg::yolk::IEggParserContext&) {
  EGG_THROW(__FUNCTION__ " TODO"); // TODO
}

std::shared_ptr<egg::yolk::IEggParserNode> egg::yolk::EggSyntaxNode_Continue::promote(egg::yolk::IEggParserContext&) {
  EGG_THROW(__FUNCTION__ " TODO"); // TODO
}

std::shared_ptr<egg::yolk::IEggParserNode> egg::yolk::EggSyntaxNode_Default::promote(egg::yolk::IEggParserContext&) {
  EGG_THROW(__FUNCTION__ " TODO"); // TODO
}

std::shared_ptr<egg::yolk::IEggParserNode> egg::yolk::EggSyntaxNode_Do::promote(egg::yolk::IEggParserContext&) {
  EGG_THROW(__FUNCTION__ " TODO"); // TODO
}

std::shared_ptr<egg::yolk::IEggParserNode> egg::yolk::EggSyntaxNode_If::promote(egg::yolk::IEggParserContext&) {
  EGG_THROW(__FUNCTION__ " TODO"); // TODO
}

std::shared_ptr<egg::yolk::IEggParserNode> egg::yolk::EggSyntaxNode_Finally::promote(egg::yolk::IEggParserContext&) {
  EGG_THROW(__FUNCTION__ " TODO"); // TODO
}

std::shared_ptr<egg::yolk::IEggParserNode> egg::yolk::EggSyntaxNode_For::promote(egg::yolk::IEggParserContext&) {
  EGG_THROW(__FUNCTION__ " TODO"); // TODO
}

std::shared_ptr<egg::yolk::IEggParserNode> egg::yolk::EggSyntaxNode_Foreach::promote(egg::yolk::IEggParserContext&) {
  EGG_THROW(__FUNCTION__ " TODO"); // TODO
}

std::shared_ptr<egg::yolk::IEggParserNode> egg::yolk::EggSyntaxNode_Return::promote(egg::yolk::IEggParserContext&) {
  EGG_THROW(__FUNCTION__ " TODO"); // TODO
}

std::shared_ptr<egg::yolk::IEggParserNode> egg::yolk::EggSyntaxNode_Switch::promote(egg::yolk::IEggParserContext&) {
  EGG_THROW(__FUNCTION__ " TODO"); // TODO
}

std::shared_ptr<egg::yolk::IEggParserNode> egg::yolk::EggSyntaxNode_Throw::promote(egg::yolk::IEggParserContext&) {
  EGG_THROW(__FUNCTION__ " TODO"); // TODO
}

std::shared_ptr<egg::yolk::IEggParserNode> egg::yolk::EggSyntaxNode_Try::promote(egg::yolk::IEggParserContext&) {
  EGG_THROW(__FUNCTION__ " TODO"); // TODO
}

std::shared_ptr<egg::yolk::IEggParserNode> egg::yolk::EggSyntaxNode_Using::promote(egg::yolk::IEggParserContext&) {
  EGG_THROW(__FUNCTION__ " TODO"); // TODO
}

std::shared_ptr<egg::yolk::IEggParserNode> egg::yolk::EggSyntaxNode_While::promote(egg::yolk::IEggParserContext&) {
  EGG_THROW(__FUNCTION__ " TODO"); // TODO
}

std::shared_ptr<egg::yolk::IEggParserNode> egg::yolk::EggSyntaxNode_Yield::promote(egg::yolk::IEggParserContext&) {
  EGG_THROW(__FUNCTION__ " TODO"); // TODO
}

std::shared_ptr<egg::yolk::IEggParserNode> egg::yolk::EggSyntaxNode_UnaryOperator::promote(egg::yolk::IEggParserContext&) {
  EGG_THROW(__FUNCTION__ " TODO"); // TODO
}

std::shared_ptr<egg::yolk::IEggParserNode> egg::yolk::EggSyntaxNode_BinaryOperator::promote(egg::yolk::IEggParserContext&) {
  EGG_THROW(__FUNCTION__ " TODO"); // TODO
}

std::shared_ptr<egg::yolk::IEggParserNode> egg::yolk::EggSyntaxNode_TernaryOperator::promote(egg::yolk::IEggParserContext&) {
  EGG_THROW(__FUNCTION__ " TODO"); // TODO
}

std::shared_ptr<egg::yolk::IEggParserNode> egg::yolk::EggSyntaxNode_Call::promote(egg::yolk::IEggParserContext&) {
  EGG_THROW(__FUNCTION__ " TODO"); // TODO
}

std::shared_ptr<egg::yolk::IEggParserNode> egg::yolk::EggSyntaxNode_Named::promote(egg::yolk::IEggParserContext&) {
  EGG_THROW(__FUNCTION__ " TODO"); // TODO
}

std::shared_ptr<egg::yolk::IEggParserNode> egg::yolk::EggSyntaxNode_Identifier::promote(egg::yolk::IEggParserContext&) {
  return std::make_shared<EggParserNode_Identifier>(this->name);
}

std::shared_ptr<egg::yolk::IEggParserNode> egg::yolk::EggSyntaxNode_Literal::promote(egg::yolk::IEggParserContext&) {
  EGG_THROW(__FUNCTION__ " TODO"); // TODO
}

std::shared_ptr<egg::yolk::IEggParser> egg::yolk::EggParserFactory::createModuleParser() {
  return std::make_shared<EggParserModule>();
}
