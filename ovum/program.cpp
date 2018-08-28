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
    explicit RuntimeException(const INode* node, ARGS&&... args)
      : std::runtime_error("RuntimeException"),
        message(StringBuilder::concat(std::forward<ARGS>(args)...)),
        node(node) {
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
    bool remove(const String& name) {
      // Return true iff the removal occurred
      auto retval = this->table.erase(name);
      return retval == 1;
    }
    void softVisitLinks(const Visitor& visitor) const {
      for (auto& entry : this->table) {
        entry.second.value.softVisitLink(visitor);
      }
    }
  };

  class IProgramBlock : public IProgram {
  public:
    virtual Variant declare(const INode& node, const Type& type, const String& name, const Variant& init) = 0;
    virtual void undeclare(const String& name) = 0;
  };

  class Block {
    Block(const Block&) = delete;
    Block& operator=(const Block&) = delete;
  private:
    IProgramBlock& program;
    std::set<String> declared;
  public:
    explicit Block(IProgramBlock& program) : program(program) {
    }
    virtual ~Block() {
      // Undeclare all the names successfully declared by this block
      for (auto& i : this->declared) {
        this->program.undeclare(i);
      }
    }
    Variant declare(const INode& node, const Type& type, const String& name, const Variant& init) {
      // Keep track of names successfully declared in this block
      auto retval = this->program.declare(node, type, name, init);
      if (!retval.hasFlowControl()) {
        this->declared.insert(name);
      }
      return retval;
    }
  };

  class ProgramDefault final : public HardReferenceCounted<IProgramBlock> {
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
    // Inherited from IProgramBlock
    virtual Variant declare(const INode& node, const Type& type, const String& name, const Variant& init) override {
      if (!this->symtable->add(type, name, init)) {
        throw RuntimeException(&node, "Duplicate variable name: '", name, "'");
      }
      return Variant::Void;
    }
    virtual void undeclare(const String& name) override {
      if (!this->symtable->remove(name)) {
        throw RuntimeException(nullptr, "Orphaned variable name: '", name, "'");
      }
    }
  private:
    Variant executeRoot(const INode& node) {
      if (node.getOpcode() != OPCODE_MODULE) {
        throw this->unexpectedOpcode("module", node);
      }
      this->validateOpcode(node);
      Block block(*this);
      return executeBlock(block, node.getChild(0));
    }
    Variant executeBlock(Block& inner, const INode& node) {
      if (node.getOpcode() != OPCODE_BLOCK) {
        throw this->unexpectedOpcode("block", node);
      }
      this->validateOpcode(node);
      size_t n = node.getChildren();
      for (size_t i = 0; i < n; ++i) {
        auto retval = this->statement(inner, node.getChild(i));
        if (!retval.isVoid()) {
          return retval;
        }
      }
      return Variant::Void;
    }
    // Statements
    Variant statement(Block& block, const INode& node) {
      EGG_WARNING_SUPPRESS_SWITCH_BEGIN
      auto opcode = node.getOpcode();
      switch (opcode) {
      case OPCODE_NOOP:
        return Variant::Void;
      case OPCODE_BLOCK:
        return this->statementBlock(node);
      case OPCODE_ASSIGN:
        return this->statementAssign(node);
      case OPCODE_CALL:
        return this->statementCall(node);
      case OPCODE_DECLARE:
        return this->statementDeclare(block, node);
      case OPCODE_FOR:
        return this->statementFor(node);
      case OPCODE_MUTATE:
        return this->statementMutate(node);
      }
      EGG_WARNING_SUPPRESS_SWITCH_END
      throw this->unexpectedOpcode("statement", node);
    }
    Variant statementAssign(const INode& node) {
      assert(node.getOpcode() == OPCODE_ASSIGN);
      assert(node.getChildren() == 2);
      return this->assign(node.getChild(0), node.getChild(1));
    }
    Variant statementBlock(const INode& node) {
      assert(node.getOpcode() == OPCODE_BLOCK);
      Block block(*this);
      return executeBlock(block, node);
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
    Variant statementDeclare(Block& block, const INode& node) {
      assert(node.getOpcode() == OPCODE_DECLARE);
      auto vtype = type(node.getChild(0));
      auto vname = identifier(node.getChild(1));
      if (node.getChildren() == 2) {
        // No initializer
        return block.declare(node, vtype, vname, Variant::Void);
      }
      assert(node.getChildren() == 3);
      auto vinit = expression(node.getChild(2));
      if (vinit.hasFlowControl()) {
        return vinit;
      }
      vinit.soften(*this->basket);
      return block.declare(node, vtype, vname, vinit);
    }
    Variant statementFor(const INode& node) {
      assert(node.getOpcode() == OPCODE_FOR);
      auto& pre = node.getChild(0);
      auto* cond = &node.getChild(1);
      EGG_WARNING_SUPPRESS_SWITCH_BEGIN
      switch (cond->getOpcode()) {
      case OPCODE_NOOP:
      case OPCODE_TRUE:
        // No condition: 'for(a; ; c) {}' or 'for(a; true; c) {}'
        cond = nullptr;
        break;
      }
      EGG_WARNING_SUPPRESS_SWITCH_END
      auto& post = node.getChild(2);
      auto& loop = node.getChild(3);
      Block inner(*this);
      auto retval = this->statement(inner, pre);
      if (!retval.isVoid()) {
        return retval;
      }
      do {
        if (cond != nullptr) {
          retval = this->condition(*cond);
          if (!retval.isBool()) {
            // Problem evaluating the condition
            return retval;
          }
          if (!retval.getBool()) {
            // Condition evaluated to 'false'
            return Variant::Void;
          }
        }
        retval = this->executeBlock(inner, loop);
        if (!retval.isVoid()) {
          if (retval.is(VariantBits::Break)) {
            // Break from the loop
            return Variant::Void;
          }
          if (!retval.is(VariantBits::Continue)) {
            // Some other flow control
            return retval;
          }
        }
        retval = this->statement(inner, post);
      } while (retval.isVoid());
      return retval;
    }
    Variant statementMutate(const INode& node) {
      assert(node.getOpcode() == OPCODE_MUTATE);
      assert(node.getChildren() == 2);
      auto oper = node.getOperator();
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
      case OPCODE_FALSE:
        return Variant::False;
      case OPCODE_TRUE:
        return Variant::True;
      case OPCODE_UNARY:
        return expressionUnary(node);
      case OPCODE_BINARY:
        return expressionBinary(node);
      case OPCODE_TERNARY:
        return expressionTernary(node);
      case OPCODE_COMPARE:
        return expressionCompare(node);
      case OPCODE_AVALUE:
        return expressionAvalue(node);
      case OPCODE_IVALUE:
        return expressionIvalue(node);
      case OPCODE_OVALUE:
        return expressionOvalue(node);
      case OPCODE_CALL:
        return expressionCall(node);
      }
      EGG_WARNING_SUPPRESS_SWITCH_END
      throw this->unexpectedOpcode("expression", node);
    }
    Variant expressionUnary(const INode& node) {
      assert(node.getOpcode() == OPCODE_UNARY);
      assert(node.getChildren() == 1);
      auto oper = node.getOperator();
      assert(OperatorProperties::from(oper).opclass == Opclass::OPCLASS_UNARY);
      auto message = StringBuilder::concat("WIBBLE: ", Node::toString(&node));
      this->logger.log(ILogger::Source::User, ILogger::Severity::Verbose, message.toUTF8());
      return Variant::Void; // WIBBLE
    }
    Variant expressionBinary(const INode& node) {
      assert(node.getOpcode() == OPCODE_BINARY);
      assert(node.getChildren() == 2);
      auto oper = node.getOperator();
      assert(OperatorProperties::from(oper).opclass == Opclass::OPCLASS_BINARY);
      auto message = StringBuilder::concat("WIBBLE: ", Node::toString(&node));
      this->logger.log(ILogger::Source::User, ILogger::Severity::Verbose, message.toUTF8());
      return Variant::Void; // WIBBLE
    }
    Variant expressionTernary(const INode& node) {
      assert(node.getOpcode() == OPCODE_TERNARY);
      assert(node.getChildren() == 3);
      auto oper = node.getOperator();
      assert(OperatorProperties::from(oper).opclass == Opclass::OPCLASS_TERNARY);
      auto message = StringBuilder::concat("WIBBLE: ", Node::toString(&node));
      this->logger.log(ILogger::Source::User, ILogger::Severity::Verbose, message.toUTF8());
      return Variant::Void; // WIBBLE
    }
    Variant expressionCompare(const INode& node) {
      assert(node.getOpcode() == OPCODE_COMPARE);
      assert(node.getChildren() == 2);
      auto oper = node.getOperator();
      assert(OperatorProperties::from(oper).opclass == Opclass::OPCLASS_COMPARE);
      auto message = StringBuilder::concat("WIBBLE: ", Node::toString(&node));
      this->logger.log(ILogger::Source::User, ILogger::Severity::Verbose, message.toUTF8());
      return Variant::Void; // WIBBLE
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
    Variant expressionIvalue(const INode& node) {
      assert(node.getOpcode() == OPCODE_IVALUE);
      assert(node.getChildren() == 0);
      return Variant(node.getInt());
    }
    Variant expressionOvalue(const INode& node) {
      assert(node.getOpcode() == OPCODE_OVALUE);
      auto object = ObjectFactory::createVanillaObject(this->allocator);
      auto n = node.getChildren();
      for (size_t i = 0; i < n; ++i) {
        auto& named = node.getChild(i);
        assert(named.getOpcode() == OPCODE_NAMED);
        // WIBBLE add expr to array
      }
      return Variant(object);
    }
    Variant expressionCall(const INode& node) {
      assert(node.getOpcode() == OPCODE_CALL);
      return Variant::Void; // WIBBLE
    }
    // Implementation
    Variant assign(const INode& lhs, const INode& rhs) {
      auto message = StringBuilder::concat("WIBBLE: assign'", Node::toString(&lhs), "' '", Node::toString(&rhs), "'");
      this->logger.log(ILogger::Source::User, ILogger::Severity::Verbose, message.toUTF8());
      return Variant::Void;
    }
    Variant mutate(Operator oper, const INode& lhs, const INode& rhs) {
      auto message = StringBuilder::concat("WIBBLE: mutate '", OperatorProperties::from(oper).name, "' '", Node::toString(&lhs), "' '", Node::toString(&rhs), "'");
      this->logger.log(ILogger::Source::User, ILogger::Severity::Verbose, message.toUTF8());
      return Variant::Void;
    }
    Variant condition(const INode& node) {
      auto value = this->expression(node);
      if (!value.hasAny(VariantBits::FlowControl | VariantBits::Bool)) {
        throw RuntimeException(&node, "Expected 'for' statement condition to evaluate to a Boolean value");
      }
      return value;
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
        throw RuntimeException(&node, "Corrupt opcode: '", properties.name, "'");
      }
    }
    RuntimeException unexpectedOpcode(const char* expected, const INode& node) {
      auto opcode = node.getOpcode();
      auto name = OpcodeProperties::from(opcode).name;
      if (name == nullptr) {
        return RuntimeException(&node, "Unknown ", expected, " opcode: '<", std::to_string(opcode), ">'");
      }
      return RuntimeException(&node, "Unexpected ", expected, " opcode: '", name, "'");
    }
  };
}

egg::ovum::Program egg::ovum::ProgramFactory::create(IAllocator& allocator, ILogger& logger) {
  auto basket = BasketFactory::createBasket(allocator);
  return allocator.make<ProgramDefault>(*basket, logger);
}
