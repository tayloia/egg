#include "ovum/ovum.h"
#include "ovum/node.h"
#include "ovum/module.h"
#include "ovum/program.h"

namespace {
  using namespace egg::ovum;

  class RuntimeException : public std::runtime_error {
  public:
    String message;
    Node node;
    template<typename... ARGS>
    explicit RuntimeException(const INode& node, ARGS&&... args)
      : std::runtime_error("RuntimeException"),
        message(StringBuilder::concat(std::forward<ARGS>(args)...)),
        node(&node) {
    }
  };

  struct Symbol {
    Type type;
    String name;
    Variant value;
    Symbol(const Type& type, const String& name, const Variant& value)
      : type(type), name(name), value(value) {
      assert(this->value.validate(true));
    }
  };

  class SymbolTable : public SoftReferenceCounted<ICollectable> {
    SymbolTable(const SymbolTable&) = delete;
    SymbolTable& operator=(const SymbolTable&) = delete;
  private:
    std::map<String, Symbol> table;
  public:
    explicit SymbolTable(IAllocator& allocator)
      : SoftReferenceCounted(allocator) {
    }
    bool add(const Type& type, const String& name, const Variant& init) {
      // Return true iff the insertion occurred
      Symbol symbol(type, name, init);
      auto retval = this->table.insert(std::make_pair(name, std::move(symbol)));
      return retval.second;
    }
    void softVisitLinks(const Visitor& visitor) const {
      for (auto& entry : this->table) {
        entry.second.value.softVisitLink(visitor);
      }
    }
  };

  class ProgramDefault final : public HardReferenceCounted<IProgram> {
    ProgramDefault(const ProgramDefault&) = delete;
    ProgramDefault& operator=(const ProgramDefault&) = delete;
  private:
    ILogger& logger;
    Basket basket;
    HardPtr<SymbolTable> symtable;
  public:
    ProgramDefault(IAllocator& allocator, IBasket& basket, ILogger& logger)
      : HardReferenceCounted(allocator, 0),
        logger(logger),
        basket(&basket),
        symtable(allocator.make<SymbolTable>()) {
      this->basket->take(*this->symtable);
    }
    virtual ~ProgramDefault() {
      this->symtable.set(nullptr);
      this->basket->collect();
    }
    virtual Variant run(const IModule& module) override {
      try {
        return this->executeRoot(module.getRootNode());
      } catch (RuntimeException& exception) {
        this->logger.log(egg::ovum::ILogger::Source::Runtime, egg::ovum::ILogger::Severity::Error, exception.message.toUTF8());
        return Variant::Rethrow;
      }
    }
  private:
    Variant executeRoot(const INode& node) {
      if (node.getOpcode() != OPCODE_MODULE) {
        throw this->unexpectedOpcode("module", node);
      }
      this->validateOpcode(node);
      return executeBlock(node.getChild(0));
    }
    Variant executeBlock(const INode& node) {
      if (node.getOpcode() != OPCODE_BLOCK) {
        throw this->unexpectedOpcode("block", node);
      }
      this->validateOpcode(node);
      size_t n = node.getChildren();
      for (size_t i = 0; i < n; ++i) {
        auto retval = statement(node.getChild(i));
        if (!retval.isVoid()) {
          return retval;
        }
      }
      return Variant::Void;
    }
    // Statements
    Variant statement(const INode& node) {
      EGG_WARNING_SUPPRESS_SWITCH_BEGIN
      auto opcode = node.getOpcode();
      switch (opcode) {
      case OPCODE_NOOP:
        return Variant::Void;
      case OPCODE_BLOCK:
        return this->executeBlock(node);
      case OPCODE_CALL:
        return this->statementCall(node);
      case OPCODE_DECLARE:
        return this->statementDeclare(node);
      case OPCODE_MUTATE:
        return this->statementMutate(node);
      }
      EGG_WARNING_SUPPRESS_SWITCH_END
      throw this->unexpectedOpcode("statement", node);
    }
    Variant statementCall(const INode& node) {
      assert(node.getOpcode() == OPCODE_CALL);
      auto retval = this->expressionCall(node);
      if (retval.hasAny(VariantBits::FlowControl | VariantBits::Void)) {
        return retval;
      }
      auto message = StringBuilder::concat("Discarding call return value: '", retval.toString(), "'");
      this->logger.log(ILogger::Source::Runtime, ILogger::Severity::Warning, message.toUTF8());
      return Variant::Void;
    }
    Variant statementDeclare(const INode& node) {
      assert(node.getOpcode() == OPCODE_DECLARE);
      auto vtype = type(node.getChild(0));
      auto vname = identifier(node.getChild(1));
      if (node.getChildren() == 2) {
        // No initializer
        return this->declare(node, vtype, vname, Variant::Void);
      }
      assert(node.getChildren() == 3);
      auto vinit = expression(node.getChild(2));
      if (vinit.hasFlowControl()) {
        return vinit;
      }
      vinit.soften(*this->basket);
      return this->declare(node, vtype, vname, vinit);
    }
    Variant statementMutate(const INode& node) {
      assert(node.getOpcode() == OPCODE_MUTATE);
      assert(node.getChildren() == 2);
      auto oper = Operator(node.getInt());
      assert(OperatorProperties::from(oper).opclass == Opclass::OPCLASS_BINARY);
      return this->mutate(oper, node.getChild(0), node.getChild(1));
    }
    // Expressions
    Variant expression(const INode& node) {
      EGG_WARNING_SUPPRESS_SWITCH_BEGIN
      auto opcode = node.getOpcode();
      switch (opcode) {
      case OPCODE_NULL:
        return Variant::Null;
      case OPCODE_AVALUE:
        return expressionAvalue(node);
      case OPCODE_CALL:
        return expressionCall(node);
      case OPCODE_IVALUE:
        return expressionIvalue(node);
      }
      EGG_WARNING_SUPPRESS_SWITCH_END
      throw this->unexpectedOpcode("expression", node);
    }
    Variant expressionAvalue(const INode& node) {
      assert(node.getOpcode() == OPCODE_AVALUE);
      auto n = node.getChildren();
      auto array = ObjectFactory::createVanillaArray(this->allocator, n);
      for (size_t i = 0; i < n; ++i) {
        auto expr = expression(node.getChild(i));
        if (expr.hasFlowControl()) {
          return expr;
        }
        // WIBBLE add expr to array
      }
      return Variant(array);
    }
    Variant expressionCall(const INode& node) {
      assert(node.getOpcode() == OPCODE_CALL);
      return Variant::Void; // WIBBLE
    }
    Variant expressionIvalue(const INode& node) {
      assert(node.getOpcode() == OPCODE_IVALUE);
      assert(node.getChildren() == 0);
      return Variant(node.getInt());
    }
    // Implementation
    Variant declare(const INode& node, const Type& type, const String& name, const Variant& init) {
      if (!symtable->add(type, name, init)) {
        throw RuntimeException(node, "Duplicate variable name: '", name, "'");
      }
      return Variant::Void;
    }
    Variant mutate(Operator oper, const INode& lhs, const INode& rhs) {
      auto message = StringBuilder::concat("WIBBLE: mutate '", OperatorProperties::from(oper).name, "' '", Node::toString(&lhs), "' '", Node::toString(&rhs), "'");
      this->logger.log(ILogger::Source::User, ILogger::Severity::Verbose, message.toUTF8());
      return Variant::Void;
    }
    String identifier(const INode& node) {
      if (node.getOpcode() != OPCODE_IDENTIFIER) {
        throw this->unexpectedOpcode("module", node);
      }
      auto& child = node.getChild(0);
      if (child.getOpcode() != OPCODE_SVALUE) {
        throw this->unexpectedOpcode("svalue", child);
      }
      return child.getString();
    }
    Type type(const INode&) {
      // WIBBLE
      return Type::AnyQ;
    }
    // Error handling
    void validateOpcode(const INode& node) const {
      auto& properties = OpcodeProperties::from(node.getOpcode());
      if (!properties.validate(node.getChildren(), node.getOperand() != INode::Operand::None)) {
        assert(properties.name != nullptr);
        throw RuntimeException(node, "Corrupt opcode: '", properties.name, "'");
      }
    }
    RuntimeException unexpectedOpcode(const char* expected, const INode& node) {
      auto opcode = node.getOpcode();
      auto name = OpcodeProperties::from(opcode).name;
      if (name == nullptr) {
        return RuntimeException(node, "Unknown ", expected, " opcode: '<", std::to_string(opcode), ">'");
      }
      return RuntimeException(node, "Unexpected ", expected, " opcode: '", name, "'");
    }
  };
}

egg::ovum::Program egg::ovum::ProgramFactory::create(IAllocator& allocator, ILogger& logger) {
  auto basket = BasketFactory::createBasket(allocator);
  return allocator.make<ProgramDefault>(*basket, logger);
}
