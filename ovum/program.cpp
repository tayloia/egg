#include "ovum/ovum.h"
#include "ovum/node.h"
#include "ovum/module.h"
#include "ovum/program.h"

#include <cmath>

namespace {
  using namespace egg::ovum;

  // Forward declarations
  class Block;
  class ProgramDefault;

  class RuntimeException : public std::runtime_error {
  public:
    String message;
    template<typename... ARGS>
    explicit RuntimeException(const LocationSource& location, ARGS&&... args)
      : std::runtime_error("RuntimeException") {
      StringBuilder sb;
      sb.add(location.file);
      if (location.line > 0) {
        sb.add('(', location.line);
        if (location.column > 0) {
          sb.add(',', location.column);
        }
        sb.add("): ");
      } else if (!location.file.empty()) {
        sb.add(": ");
      }
      sb.add(std::forward<ARGS>(args)...);
      this->message = sb.str();
    }
  };

  class Binary {
  public:
    enum class Match { Bool, Int, Float, Mismatch };
    static Variant apply(Operator oper, Variant& lvalue, const Variant& rvalue) {
      // Returns 'Break' if operator not known
      auto retval = Variant::Break;
      Float lfloat, rfloat;
      uint64_t uvalue;
      EGG_WARNING_SUPPRESS_SWITCH_BEGIN();
      switch (oper) {
      case OPERATOR_ADD:
        switch (Binary::arithmetic("addition", lvalue, rvalue, lfloat, rfloat, retval)) {
        case Match::Float:
          lvalue = lfloat + rfloat;
          return Variant::Void;
        case Match::Int:
          lvalue = lvalue.getInt() + rvalue.getInt();
          return Variant::Void;
        }
        break;
      case OPERATOR_SUB:
        switch (Binary::arithmetic("subtraction", lvalue, rvalue, lfloat, rfloat, retval)) {
        case Match::Float:
          lvalue = lfloat - rfloat;
          return Variant::Void;
        case Match::Int:
          lvalue = lvalue.getInt() - rvalue.getInt();
          return Variant::Void;
        }
        break;
      case OPERATOR_MUL:
        switch (Binary::arithmetic("multiplication", lvalue, rvalue, lfloat, rfloat, retval)) {
        case Match::Float:
          lvalue = lfloat * rfloat;
          return Variant::Void;
        case Match::Int:
          lvalue = lvalue.getInt() * rvalue.getInt();
          return Variant::Void;
        }
        break;
      case OPERATOR_DIV:
        switch (Binary::arithmetic("division", lvalue, rvalue, lfloat, rfloat, retval)) {
        case Match::Float:
          lvalue = lfloat / rfloat;
          return Variant::Void;
        case Match::Int:
          lvalue = lvalue.getInt() / rvalue.getInt();
          return Variant::Void;
        }
        break;
      case OPERATOR_REM:
        switch (Binary::arithmetic("remainder", lvalue, rvalue, lfloat, rfloat, retval)) {
        case Match::Float:
          lvalue = std::remainder(lfloat, rfloat);
          return Variant::Void;
        case Match::Int:
          lvalue = lvalue.getInt() % rvalue.getInt();
          return Variant::Void;
        }
        break;
      case OPERATOR_BITAND:
        switch (Binary::bitwise("bitwise-and", lvalue, rvalue, retval)) {
        case Match::Int:
          lvalue = lvalue.getInt() & rvalue.getInt();
          return Variant::Void;
        case Match::Bool:
          lvalue = lvalue.getBool() && rvalue.getBool();
          return Variant::Void;
        }
        break;
      case OPERATOR_BITOR:
        switch (Binary::bitwise("bitwise-or", lvalue, rvalue, retval)) {
        case Match::Int:
          lvalue = lvalue.getInt() | rvalue.getInt();
          return Variant::Void;
        case Match::Bool:
          lvalue = lvalue.getBool() || rvalue.getBool();
          return Variant::Void;
        }
        break;
      case OPERATOR_BITXOR:
        switch (Binary::bitwise("bitwise-xor", lvalue, rvalue, retval)) {
        case Match::Int:
          lvalue = lvalue.getInt() ^ rvalue.getInt();
          return Variant::Void;
        case Match::Bool:
          lvalue = bool(lvalue.getBool() ^ rvalue.getBool());
          return Variant::Void;
        }
        break;
      case OPERATOR_LOGAND:
        // Note that short-circuiting is handled previously in Target::apply()
        switch (Binary::logical("logical-and", lvalue, rvalue, retval)) {
        case Match::Bool:
          lvalue = lvalue.getBool() && rvalue.getBool();
          return Variant::Void;
        }
        break;
      case OPERATOR_LOGOR:
        // Note that short-circuiting is handled previously in Target::apply()
        switch (Binary::logical("logical-or", lvalue, rvalue, retval)) {
        case Match::Bool:
          lvalue = lvalue.getBool() || rvalue.getBool();
          return Variant::Void;
        }
        break;
      case OPERATOR_SHIFTL:
        switch (Binary::shift("left-shift", lvalue, rvalue, uvalue, retval)) {
        case Match::Int:
          lvalue = lvalue.getInt() << uvalue;
          return Variant::Void;
        }
        break;
      case OPERATOR_SHIFTR:
        switch (Binary::shift("right-shift", lvalue, rvalue, uvalue, retval)) {
        case Match::Int:
          lvalue = lvalue.getInt() >> uvalue;
          return Variant::Void;
        }
        break;
      case OPERATOR_SHIFTU:
        switch (Binary::shift("unsigned-right-shift", lvalue, rvalue, uvalue, retval)) {
        case Match::Int:
          lvalue = Int(uint64_t(lvalue.getInt()) >> uvalue);
          return Variant::Void;
        }
        break;
      case OPERATOR_IFNULL:
        if (lvalue.isNull()) {
          lvalue = rvalue;
        }
        return Variant::Void;
      }
      EGG_WARNING_SUPPRESS_SWITCH_END();
      return retval;
    }
    static Match arithmetic(const char* operation, const Variant& a, const Variant& b, Float& fa, Float& fb, Variant& retval) {
      // Promote integers to floats where appropriate
      if (a.isFloat()) {
        if (b.isFloat()) {
          fa = a.getFloat();
          fb = b.getFloat();
          return Match::Float;
        } else if (b.isInt()) {
          // TODO overflow
          fa = a.getFloat();
          fb = Float(b.getInt());
          return Match::Float;
        }
        retval = Binary::unexpected(b, "Expected right-hand side of ", operation, " to be 'int' or 'float'");
        return Match::Mismatch;
      }
      if (a.isInt()) {
        if (b.isFloat()) {
          // TODO overflow
          fa = Float(a.getInt());
          fb = b.getFloat();
          return Match::Float;
        } else if (b.isInt()) {
          return Match::Int;
        }
        retval = Binary::unexpected(b, "Expected right-hand side of ", operation, " to be 'int' or 'float'");
        return Match::Mismatch;
      }
      retval = Binary::unexpected(a, "Expected left-hand side of ", operation, " to be 'int' or 'float'");
      return Match::Mismatch;
    }
    static Match bitwise(const char* operation, const Variant& a, const Variant& b, Variant& retval) {
      // Accept matching ints or bools
      if (a.isInt()) {
        if (b.isInt()) {
          return Match::Int;
        }
        retval = Binary::unexpected(b, "Expected right-hand side of ", operation, " to be 'int'");
        return Match::Mismatch;
      }
      if (a.isBool()) {
        if (b.isBool()) {
          return Match::Bool;
        }
        retval = Binary::unexpected(b, "Expected right-hand side of ", operation, " to be 'bool'");
        return Match::Mismatch;
      }
      retval = Binary::unexpected(a, "Expected left-hand side of ", operation, " to be 'bool' or 'int'");
      return Match::Mismatch;
    }
    static Match logical(const char* operation, const Variant& a, const Variant& b, Variant& retval) {
      // Accept only bools
      if (!a.isBool()) {
        retval = Binary::unexpected(a, "Expected left-hand side of ", operation, " to be 'bool'");
        return Match::Mismatch;
      }
      if (!b.isBool()) {
        retval = Binary::unexpected(b, "Expected right-hand side of ", operation, " to be 'bool'");
        return Match::Mismatch;
      }
      return Match::Bool;
    }
    static Match shift(const char* operation, const Variant& a, const Variant& b, uint64_t& ub, Variant& retval) {
      // Accept only bools
      if (!a.isInt()) {
        retval = Binary::unexpected(a, "Expected left-hand side of ", operation, " to be 'int'");
        return Match::Mismatch;
      }
      if (!b.isInt()) {
        retval = Binary::unexpected(b, "Expected right-hand side of ", operation, " to be 'int'");
        return Match::Mismatch;
      }
      auto ib = b.getInt();
      if (ib < 0) {
        retval = Binary::raise("Expected right-hand side of ", operation, " to be a non-negative 'int', not ", std::to_string(ib));
        return Match::Mismatch;
      }
      ub = uint64_t(ib);
      return Match::Int;
    }
    template<typename... ARGS>
    static Variant unexpected(const Variant& value, ARGS&&... args) {
      return Binary::raise(std::forward<ARGS>(args)..., ", not '", value.getRuntimeType().toString(), "'");
    }
    template<typename... ARGS>
    static Variant raise(ARGS&&... args) {
      Variant exception{ StringBuilder::concat(std::forward<ARGS>(args)...) };
      exception.addFlowControl(VariantBits::Throw);
      return exception;
    }
  };

  class Parameters : public IParameters {
    Parameters(const Parameters&) = delete;
    Parameters& operator=(const Parameters&) = delete;
  private:
    std::vector<Variant> positional;
  public:
    Parameters() = default;
    void addPositional(Variant&& value) {
      this->positional.emplace_back(std::move(value));
    }
    virtual size_t getPositionalCount() const override {
      return this->positional.size();
    }
    virtual Variant getPositional(size_t index) const override {
      return this->positional[index];
    }
    virtual const LocationSource* getPositionalLocation(size_t) const override {
      // TODO remove
      return nullptr;
    }
    virtual size_t getNamedCount() const override {
      return 0;
    }
    virtual String getName(size_t) const override {
      return String();
    }
    virtual Variant getNamed(const String&) const override {
      return Variant::Void;
    }
    virtual const LocationSource* getNamedLocation(const String&) const override {
      // TODO remove
      return nullptr;
    }
  };

  struct Symbol {
    Type type;
    String name;
    Variant value;
    Symbol(IBasket* basket, const Type& type, const String& name, const Variant& value)
      : type(type), name(name), value(value) {
      assert(basket != nullptr);
      this->value.soften(*basket);
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
      Symbol symbol(this->softGetBasket(), type, name, init);
      auto retval = this->table.insert(std::make_pair(name, std::move(symbol)));
      return retval.second;
    }
    bool remove(const String& name) {
      // Return true iff the removal occurred
      auto retval = this->table.erase(name);
      return retval == 1;
    }
    Symbol* get(const String& name) {
      // Return a pointer to the symbol or null
      auto retval = this->table.find(name);
      if (retval == this->table.end()) {
        return nullptr;
      }
      return &retval->second;
    }
    void softVisitLinks(const Visitor& visitor) const {
      for (auto& entry : this->table) {
        entry.second.value.softVisitLink(visitor);
      }
    }
  };

  class Target {
    Target(const Target&) = delete;
    Target& operator=(const Target&) = delete;
  public:
    enum Flavour { Failed, Identifier, Index };
  private:
    Flavour flavour;
    ProgramDefault& program;
    const INode& node;
    Variant a;
    Variant b;
  public:
    Target(ProgramDefault& program, const INode& node, Block* block = nullptr);
    Variant check() const;
    Variant assign(Variant&& rhs) const;
    Variant nudge(Int rhs) const;
    Variant mutate(const INode& opnode, const INode& rhs) const;
  private:
    Variant& identifier() const;
    Variant getIndex() const;
    Variant setIndex(const Variant& value) const;
    Variant apply(const INode& opnode, Variant& lvalue, const INode& rhs) const;
    Variant expression(const INode& node) const;
  };

  class Block final {
    Block(const Block&) = delete;
    Block& operator=(const Block&) = delete;
  private:
    ProgramDefault& program;
    std::set<String> declared;
  public:
    explicit Block(ProgramDefault& program) : program(program) {
    }
    ~Block();
    Variant declare(const INode& node, const Type& type, const String& name, const Variant& init);
  };

  class ProgramDefault final : public HardReferenceCounted<IProgram>, public IExecution {
    ProgramDefault(const ProgramDefault&) = delete;
    ProgramDefault& operator=(const ProgramDefault&) = delete;
  private:
    ILogger& logger;
    Basket basket;
    HardPtr<SymbolTable> symtable;
    LocationSource location;
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
    // Inherited from IProgram
    virtual bool builtin(const String& name, const Variant& value) override {
      return this->symtable->add(value.getRuntimeType(), name, value);
    }
    virtual Variant run(const IModule& module) override {
      try {
        this->location.file = module.getResourceName();
        this->location.line = 0;
        this->location.column = 0;
        return this->executeRoot(module.getRootNode());
      } catch (RuntimeException& exception) {
        this->logger.log(egg::ovum::ILogger::Source::Runtime, egg::ovum::ILogger::Severity::Error, exception.message.toUTF8());
        return Variant::Rethrow;
      }
    }
    // Inherited from IExecution
    virtual IAllocator& getAllocator() const override {
      return this->allocator;
    }
    virtual Variant raise(const String& message) override {
      StringBuilder sb;
      sb.add(this->location.file);
      if (this->location.line > 0) {
        sb.add('(', this->location.line);
        if (this->location.column > 0) {
          sb.add(',', this->location.column);
        }
        sb.add("): ", message);
      } else {
        sb.add(": ", message);
      }
      Variant exception{ sb.str() };
      exception.addFlowControl(VariantBits::Throw);
      return exception;
    }
    virtual Variant assertion(const Variant& predicate) override {
      if (!predicate.isBool()) {
        return this->raiseFormat("Builtin 'assert()' expects its parameter to be a 'bool' value, not ", predicate.getRuntimeType().toString());
      }
      if (predicate.getBool()) {
        return Variant::Void;
      }
      return this->raise("Assertion is untrue");
    }
    virtual void print(const std::string& utf8) override {
      this->logger.log(ILogger::Source::User, ILogger::Severity::Information, utf8);
    }
    // Called by Block
    Variant blockDeclare(const INode& node, const Type& type, const String& name, const Variant& init) {
      if (!this->symtable->add(type, name, init)) {
        // TODO display the other location too
        auto opcode = node.getOpcode();
        if (opcode != OPCODE_DECLARE) {
          auto decl = OpcodeProperties::from(node.getOpcode()).name;
          if (decl != nullptr) {
            return this->raiseNode(node, "Duplicate name in ", decl, " declaration: '", name, "'");
          }
        }
        return this->raiseNode(node, "Duplicate name in declaration: '", name, "'");
      }
      return Variant::Void;
    }
    void blockUndeclare(const String& name) {
      if (!this->symtable->remove(name)) {
        auto message = StringBuilder().add("Failed to remove name from symbol table: '", name, "'").toUTF8();
        this->logger.log(ILogger::Source::Runtime, ILogger::Severity::Warning, message);
      }
    }
    // Called by Target construction
    Target::Flavour targetDeclare(const INode& node, Variant& a, Block& block) {
      assert(node.getOpcode() == OPCODE_DECLARE);
      assert(node.getChildren() == 2);
      auto vtype = this->type(node.getChild(0));
      auto vname = this->identifier(node.getChild(1));
      a = block.declare(node, vtype, vname, Variant::Void);
      if (a.hasFlowControl()) {
        // Couldn't define the variable; leave the error in 'a'
        return Target::Flavour::Failed;
      }
      a = vname;
      return Target::Flavour::Identifier;
    }
    Target::Flavour targetIdentifier(const INode& node, Variant& a) {
      assert(node.getOpcode() == OPCODE_IDENTIFIER);
      a = this->identifier(node);
      return Target::Flavour::Identifier;
    }
    Target::Flavour targetIndex(const INode& node, Variant& a, Variant& b) {
      assert(node.getOpcode() == OPCODE_INDEX);
      assert(node.getChildren() == 2);
      a = this->expression(node.getChild(0));
      if (!a.hasFlowControl()) {
        b = this->expression(node.getChild(1));
        if (!b.hasFlowControl()) {
          return Target::Flavour::Index;
        }
        // Swap the values so the error is in 'a'
        std::swap(a, b);
      }
      return Target::Flavour::Failed;
    }
    Symbol& targetSymbol(const INode& node, const String& name) {
      auto* symbol = this->symtable->get(name);
      if (symbol == nullptr) {
        this->updateLocation(node);
        throw RuntimeException(this->location, "Unknown target symbol: '", name, "'");
      }
      return *symbol;
    }
    Variant targetExpression(const INode& node) {
      return this->expression(node);
    }
    // Builtins
    void addBuiltins() {
      this->builtin("assert", VariantFactory::createBuiltinAssert(this->allocator));
      this->builtin("print", VariantFactory::createBuiltinPrint(this->allocator));
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
        if (retval.hasFlowControl()) {
          return retval;
        }
      }
      return Variant::Void;
    }
    // Statements
    Variant statement(Block& block, const INode& node) {
      this->validateOpcode(node);
      auto opcode = node.getOpcode();
      EGG_WARNING_SUPPRESS_SWITCH_BEGIN();
      switch (opcode) {
      case OPCODE_NOOP:
        return Variant::Void;
      case OPCODE_BLOCK:
        return this->statementBlock(node);
      case OPCODE_ASSIGN:
        return this->statementAssign(node);
      case OPCODE_BREAK:
        return Variant::Break;
      case OPCODE_CALL:
        return this->statementCall(node);
      case OPCODE_CONTINUE:
        return Variant::Continue;
      case OPCODE_DECLARE:
        return this->statementDeclare(block, node);
      case OPCODE_DECREMENT:
        return this->statementDecrement(node);
      case OPCODE_DO:
        return this->statementDo(node);
      case OPCODE_FOR:
        return this->statementFor(node);
      case OPCODE_FOREACH:
        return this->statementForeach(node);
      case OPCODE_FUNCTION:
        return this->statementFunction(block, node);
      case OPCODE_IF:
        return this->statementIf(node);
      case OPCODE_INCREMENT:
        return this->statementIncrement(node);
      case OPCODE_MUTATE:
        return this->statementMutate(node);
      case OPCODE_SWITCH:
        return this->statementSwitch(node);
      case OPCODE_THROW:
        return this->statementThrow(node);
      case OPCODE_TRY:
        return this->statementTry(node);
      case OPCODE_WHILE:
        return this->statementWhile(node);
      }
      EGG_WARNING_SUPPRESS_SWITCH_END();
      throw this->unexpectedOpcode("statement", node);
    }
    Variant statementAssign(const INode& node) {
      assert(node.getOpcode() == OPCODE_ASSIGN);
      assert(node.getChildren() == 2);
      Target lvalue(*this, node.getChild(0));
      auto rvalue = this->expression(node.getChild(1));
      if (rvalue.hasFlowControl()) {
        return rvalue;
      }
      return lvalue.assign(std::move(rvalue));
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
      auto message = StringBuilder().add("Discarding call return value: '", retval.toString(), "'").toUTF8();
      this->logger.log(ILogger::Source::Runtime, ILogger::Severity::Warning, message);
      return Variant::Void;
    }
    Variant statementDeclare(Block& block, const INode& node) {
      assert(node.getOpcode() == OPCODE_DECLARE);
      auto vtype = this->type(node.getChild(0));
      auto vname = this->identifier(node.getChild(1));
      if (node.getChildren() == 2) {
        // No initializer
        return block.declare(node, vtype, vname, Variant::Void);
      }
      assert(node.getChildren() == 3);
      auto vinit = this->expression(node.getChild(2));
      if (vinit.hasFlowControl()) {
        return vinit;
      }
      return block.declare(node, vtype, vname, vinit);
    }
    Variant statementDecrement(const INode& node) {
      assert(node.getOpcode() == OPCODE_DECREMENT);
      assert(node.getChildren() == 1);
      Target lvalue(*this, node.getChild(0));
      return lvalue.nudge(-1);
    }
    Variant statementDo(const INode& node) {
      assert(node.getOpcode() == OPCODE_DO);
      assert(node.getChildren() == 2);
      Variant retval;
      do {
        Block block(*this);
        retval = this->executeBlock(block, node.getChild(1));
        if (retval.hasFlowControl()) {
          return retval;
        }
        retval = this->condition(node.getChild(0), nullptr); // don't allow guards
        if (!retval.isBool()) {
          // Problem with the condition
          return retval;
        }
      } while (retval.getBool());
      return Variant::Void;
    }
    Variant statementFor(const INode& node) {
      assert(node.getOpcode() == OPCODE_FOR);
      assert(node.getChildren() == 4);
      auto& pre = node.getChild(0);
      auto* cond = &node.getChild(1);
      EGG_WARNING_SUPPRESS_SWITCH_BEGIN();
      switch (cond->getOpcode()) {
      case OPCODE_NOOP:
      case OPCODE_TRUE:
        // No condition: 'for(a; ; c) {}' or 'for(a; true; c) {}'
        cond = nullptr;
        break;
      }
      EGG_WARNING_SUPPRESS_SWITCH_END();
      auto& post = node.getChild(2);
      auto& loop = node.getChild(3);
      Block inner(*this);
      auto retval = this->statement(inner, pre);
      if (retval.hasFlowControl()) {
        return retval;
      }
      do {
        if (cond != nullptr) {
          retval = this->condition(*cond, &inner);
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
        if (retval.hasFlowControl()) {
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
      } while (!retval.hasFlowControl());
      return retval;
    }
    Variant statementForeach(const INode& node) {
      assert(node.getOpcode() == OPCODE_FOREACH);
      assert(node.getChildren() == 3);
      Block block(*this);
      Target lvalue(*this, node.getChild(0), &block);
      auto retval = lvalue.check();
      if (retval.hasFlowControl()) {
        return retval;
      }
      auto rvalue = this->expression(node.getChild(1), &block);
      if (rvalue.hasFlowControl()) {
        return rvalue;
      }
      size_t WIBBLE = 0;
      do {
        retval = rvalue; // WIBBLE
        if (retval.hasAny(VariantBits::FlowControl | VariantBits::Void)) {
          break;
        }
        retval = lvalue.assign(std::move(retval));
      } while (!retval.hasFlowControl() && (++WIBBLE < 1000));
      return retval;
    }
    Variant statementFunction(Block& block, const INode& node) {
      assert(node.getOpcode() == OPCODE_FUNCTION);
      assert(node.getChildren() == 3);
      auto ftype = this->type(node.getChild(0));
      Node fblock{ &node.getChild(1) };
      auto fname = this->identifier(node.getChild(2));
      Variant fvalue{ ObjectFactory::createVanillaFunction(this->allocator, ftype, fname, fblock) };
      return block.declare(node, ftype, fname, fvalue);
    }
    Variant statementIf(const INode& node) {
      assert(node.getOpcode() == OPCODE_IF);
      assert((node.getChildren() == 2) || (node.getChildren() == 3));
      Block block(*this);
      auto retval = this->condition(node.getChild(0), &block);
      if (!retval.isBool()) {
        // Problem with the condition
        return retval;
      }
      if (retval.getBool()) {
        // Execute the 'then' clause
        return this->executeBlock(block, node.getChild(1));
      }
      if (node.getChildren() > 2) {
        // Execute the 'else' clause
        return this->executeBlock(block, node.getChild(2));
      }
      return Variant::Void;
    }
    Variant statementIncrement(const INode& node) {
      assert(node.getOpcode() == OPCODE_INCREMENT);
      assert(node.getChildren() == 1);
      Target lvalue(*this, node.getChild(0));
      return lvalue.nudge(+1);
    }
    Variant statementMutate(const INode& node) {
      assert(node.getOpcode() == OPCODE_MUTATE);
      assert(node.getChildren() == 2);
      assert(OperatorProperties::from(node.getOperator()).opclass == Opclass::OPCLASS_BINARY);
      Target lvalue(*this, node.getChild(0));
      return lvalue.mutate(node, node.getChild(1));
    }
    Variant statementSwitch(const INode& node) {
      assert(node.getOpcode() == OPCODE_SWITCH);
      auto n = node.getChildren();
      assert(n >= 2);
      auto match = this->expression(node.getChild(0));
      if (match.hasFlowControl()) {
        // Failed to evaluate the value to match
        return match;
      }
      size_t defclause = 0;
      size_t matched = 0;
      for (size_t i = 1; i < n; ++i) {
        // Look for the first matching case clause
        auto& clause = node.getChild(i);
        auto m = clause.getChildren();
        assert(m >= 1);
        assert((clause.getOpcode() == OPCODE_CASE) || (clause.getOpcode() == OPCODE_DEFAULT));
        for (size_t j = 1; !matched && (j < m); ++j) {
          auto expr = this->expression(node.getChild(0));
          if (expr.hasFlowControl()) {
            // Failed to evaluate the value to match
            return expr;
          }
          if (Variant::equals(expr, match)) {
            // We've matched this clause
            matched = j;
            break;
          }
        }
        if (matched != 0) {
          // We matched!
          break;
        }
        if (clause.getOpcode() == OPCODE_DEFAULT) {
          // Remember the default clause
          assert(defclause == 0);
          defclause = i;
        }
      }
      if (matched == 0) {
        // Use the default clause, if any
        matched = defclause;
      }
      if (matched == 0) {
        // No clause to run
        return Variant::Void;
      }
      for (;;) {
        // Run the clauses in round-robin order
        auto retval = this->statementBlock(node.getChild(matched).getChild(0));
        if (retval.is(VariantBits::Break)) {
          // Explicit 'break' terminates the switch statement
          break;
        }
        if (!retval.is(VariantBits::Continue)) {
          // Any other than 'continue' also terminates the switch statement
          return retval;
        }
        if (++matched >= n) {
          // Round robin
          matched = 1;
        }
      }
      return Variant::Void;
    }
    Variant statementThrow(const INode& node) {
      assert(node.getOpcode() == OPCODE_THROW);
      assert(node.getChildren() <= 1);
      if (node.getChildren() == 0) {
        // A naked rethrow
        return Variant::Rethrow;
      }
      auto retval = this->expression(node.getChild(0));
      if (!retval.hasFlowControl()) {
        // Convert the expression to an exception throw
        assert(!retval.isVoid());
        retval.addFlowControl(VariantBits::Throw);
      }
      return retval;
    }
    Variant statementTry(const INode& node) {
      assert(node.getOpcode() == OPCODE_TRY);
      auto n = node.getChildren();
      assert(n >= 2);
      auto exception = this->statementBlock(node.getChild(0));
      if (!exception.stripFlowControl(VariantBits::Throw)) {
        // No exception thrown
        return this->statementTryFinally(node, exception);
      }
      for (size_t i = 2; i < n; ++i) {
        // Look for the first matching catch clause
        auto& clause = node.getChild(i);
        assert(clause.getOpcode() == OPCODE_CATCH);
        assert(clause.getChildren() == 3);
        auto ctype = this->type(clause.getChild(0));
        // WIBBLE if (match exception)
        if (true) {
          Block inner(*this);
          auto cname = this->identifier(clause.getChild(1));
          auto retval = inner.declare(node, ctype, cname, exception);
          if (retval.hasFlowControl()) {
            // Couldn't define the exception variable
            return this->statementTryFinally(node, retval);
          }
          retval = this->executeBlock(inner, clause.getChild(2));
          if (retval.is(VariantBits::Throw | VariantBits::Void)) {
            // This is a rethrow of the original exception
            retval = exception;
            retval.addFlowControl(VariantBits::Throw);
          }
          return this->statementTryFinally(node, retval);
        }
      }
      // Propagate the original exception
      exception.addFlowControl(VariantBits::Throw);
      return this->statementTryFinally(node, exception);
    }
    Variant statementTryFinally(const INode& node, const Variant& retval) {
      assert(node.getOpcode() == OPCODE_TRY);
      assert(node.getChildren() >= 2);
      auto& clause = node.getChild(1);
      if (clause.getOpcode() == OPCODE_NOOP) {
        // No finally clause
        return retval;
      }
      if (retval.hasFlowControl()) {
        // Run the block but return the original result
        (void)this->statementBlock(clause);
        return retval;
      }
      return this->statementBlock(clause);
    }
    Variant statementWhile(const INode& node) {
      assert(node.getOpcode() == OPCODE_WHILE);
      assert(node.getChildren() == 2);
      for (;;) {
        Block block(*this);
        auto retval = this->condition(node.getChild(0), &block);
        if (!retval.isBool()) {
          // Problem with the condition
          return retval;
        }
        if (!retval.getBool()) {
          // Condition is false
          break;
        }
        retval = this->executeBlock(block, node.getChild(1));
        if (retval.hasFlowControl()) {
          return retval;
        }
      }
      return Variant::Void;
    }
    // Expressions
    Variant expression(const INode& node, Block* block = nullptr) {
      auto opcode = node.getOpcode();
      EGG_WARNING_SUPPRESS_SWITCH_BEGIN();
      switch (opcode) {
      case OPCODE_NULL:
        return Variant::Null;
      case OPCODE_FALSE:
        return Variant::False;
      case OPCODE_TRUE:
        return Variant::True;
      case OPCODE_UNARY:
        return this->expressionUnary(node);
      case OPCODE_BINARY:
        return this->expressionBinary(node);
      case OPCODE_TERNARY:
        return this->expressionTernary(node);
      case OPCODE_COMPARE:
        return this->expressionCompare(node);
      case OPCODE_AVALUE:
        return this->expressionAvalue(node);
      case OPCODE_FVALUE:
        return this->expressionFvalue(node);
      case OPCODE_IVALUE:
        return this->expressionIvalue(node);
      case OPCODE_OVALUE:
        return this->expressionOvalue(node);
      case OPCODE_SVALUE:
        return this->expressionSvalue(node);
      case OPCODE_CALL:
        return this->expressionCall(node);
      case OPCODE_GUARD:
        return this->expressionGuard(node, block);
      case OPCODE_IDENTIFIER:
        return this->expressionIdentifier(node);
      case OPCODE_INDEX:
        return this->expressionIndex(node);
      }
      EGG_WARNING_SUPPRESS_SWITCH_END();
      throw this->unexpectedOpcode("expression", node);
    }
    Variant expressionUnary(const INode& node) {
      assert(node.getOpcode() == OPCODE_UNARY);
      assert(node.getChildren() == 1);
      auto oper = node.getOperator();
      assert(OperatorProperties::from(oper).opclass == Opclass::OPCLASS_UNARY);
      EGG_WARNING_SUPPRESS_SWITCH_BEGIN();
      switch (oper) {
      case Operator::OPERATOR_ADD:
        break;
      }
      EGG_WARNING_SUPPRESS_SWITCH_END();
      throw this->unexpectedOperator("unary", node);
    }
    Variant expressionBinary(const INode& node) {
      assert(node.getOpcode() == OPCODE_BINARY);
      assert(node.getChildren() == 2);
      auto oper = node.getOperator();
      assert(OperatorProperties::from(oper).opclass == Opclass::OPCLASS_BINARY);
      EGG_WARNING_SUPPRESS_SWITCH_BEGIN();
      switch (oper) {
      case Operator::OPERATOR_ADD:
        break;
      }
      EGG_WARNING_SUPPRESS_SWITCH_END();
      throw this->unexpectedOperator("binary", node);
    }
    Variant expressionTernary(const INode& node) {
      assert(node.getOpcode() == OPCODE_TERNARY);
      assert(node.getChildren() == 3);
      auto oper = node.getOperator();
      assert(OperatorProperties::from(oper).opclass == Opclass::OPCLASS_TERNARY);
      EGG_WARNING_SUPPRESS_SWITCH_BEGIN();
      switch (oper) {
      case Operator::OPERATOR_TERNARY:
        return this->operatorTernary(node.getChild(0), node.getChild(1), node.getChild(2));
      }
      EGG_WARNING_SUPPRESS_SWITCH_END();
      throw this->unexpectedOperator("ternary", node);
    }
    Variant expressionCompare(const INode& node) {
      assert(node.getOpcode() == OPCODE_COMPARE);
      assert(node.getChildren() == 2);
      auto oper = node.getOperator();
      assert(OperatorProperties::from(oper).opclass == Opclass::OPCLASS_COMPARE);
      EGG_WARNING_SUPPRESS_SWITCH_BEGIN();
      switch (oper) {
      case Operator::OPERATOR_EQ:
        // (a == b) === (a == b)
        return this->operatorEquals(node.getChild(0), node.getChild(1), false);
      case Operator::OPERATOR_GE:
        // (a >= b) === !(a < b)
        return this->operatorLessThan(node.getChild(0), node.getChild(1), false, true);
      case Operator::OPERATOR_GT:
        // (a > b) === (b < a)
        return this->operatorLessThan(node.getChild(1), node.getChild(0), true, false);
      case Operator::OPERATOR_LE:
        // (a <= b) === !(b < a)
        return this->operatorLessThan(node.getChild(1), node.getChild(0), true, true);
      case Operator::OPERATOR_LT:
        // (a < b) === (a < b)
        return this->operatorLessThan(node.getChild(0), node.getChild(1), false, false);
      case Operator::OPERATOR_NE:
        // (a != b) === !(a == b)
        return this->operatorEquals(node.getChild(0), node.getChild(1), true);
      }
      EGG_WARNING_SUPPRESS_SWITCH_END();
      throw this->unexpectedOperator("compare", node);
    }
    Variant expressionAvalue(const INode& node) {
      assert(node.getOpcode() == OPCODE_AVALUE);
      auto n = node.getChildren();
      auto array = ObjectFactory::createVanillaArray(this->allocator, n);
      for (size_t i = 0; i < n; ++i) {
        auto expr = this->expression(node.getChild(i));
        if (expr.hasFlowControl()) {
          return expr;
        }
        expr = array->setIndex(*this, Variant(Int(i)), expr);
        if (expr.hasFlowControl()) {
          return expr;
        }
      }
      return Variant(array);
    }
    Variant expressionFvalue(const INode& node) {
      assert(node.getOpcode() == OPCODE_FVALUE);
      assert(node.getChildren() == 0);
      return Variant(node.getFloat());
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
        // WIBBLE add expr to object
        (void)named;
      }
      return Variant(object);
    }
    Variant expressionSvalue(const INode& node) {
      assert(node.getOpcode() == OPCODE_SVALUE);
      assert(node.getChildren() == 0);
      return Variant(node.getString());
    }
    Variant expressionCall(const INode& node) {
      assert(node.getOpcode() == OPCODE_CALL);
      auto n = node.getChildren();
      assert(n >= 1);
      auto callee = this->expression(node.getChild(0));
      if (callee.hasFlowControl()) {
        return callee;
      }
      Parameters parameters;
      for (size_t i = 1; i < n; ++i) {
        auto expr = this->expression(node.getChild(i));
        if (expr.hasFlowControl()) {
          return expr;
        }
        parameters.addPositional(std::move(expr));
      }
      return this->call(node, callee, parameters);
    }
    Variant expressionGuard(const INode& node, Block* block) {
      // WIBBLE currently always succeeds
      assert(node.getOpcode() == OPCODE_GUARD);
      assert(node.getChildren() == 3);
      if (block == nullptr) {
        throw this->unexpectedOpcode("guard", node);
      }
      auto vtype = this->type(node.getChild(0));
      auto vname = this->identifier(node.getChild(1));
      auto vinit = this->expression(node.getChild(2));
      if (vinit.hasFlowControl()) {
        return vinit;
      }
      return block->declare(node, vtype, vname, vinit);
    }
    Variant expressionIdentifier(const INode& node) {
      assert(node.getOpcode() == OPCODE_IDENTIFIER);
      auto name = this->identifier(node);
      auto* symbol = this->symtable->get(name);
      if (symbol == nullptr) {
        return this->raiseNode(node, "Unknown symbol in expression: '", name, "'");
      }
      return symbol->value.direct();
    }
    Variant expressionIndex(const INode& node) {
      assert(node.getOpcode() == OPCODE_INDEX);
      assert(node.getChildren() == 2);
      auto lhs = this->expression(node.getChild(0));
      if (lhs.hasFlowControl()) {
        return lhs;
      }
      auto rhs = this->expression(node.getChild(1));
      if (rhs.hasFlowControl()) {
        return rhs;
      }
      if (lhs.hasObject()) {
        auto object = lhs.getObject();
        return object->getIndex(*this, rhs);
      }
      return this->raiseNode(node, "WIBBLE: Invalid indexee type: '", lhs.getRuntimeType().toString(), "'");
    }
    // Operators
    Variant operatorEquals(const INode& a, const INode& b, bool invert) {
      // Care with IEEE NaNs
      auto va = this->expression(a);
      if (va.hasFlowControl()) {
        return va;
      }
      auto vb = this->expression(b);
      if (vb.hasFlowControl()) {
        return vb;
      }
      if (va.isFloat() && std::isnan(va.getFloat())) {
        // An IEEE NaN is not equal to anything, even other NaNs
        return invert;
      }
      if (vb.isFloat() && std::isnan(vb.getFloat())) {
        // An IEEE NaN is not equal to anything, even other NaNs
        return invert;
      }
      return Variant::equals(va, vb);
    }
    Variant operatorLessThan(const INode& a, const INode& b, bool swapped, bool invert) {
      // Care with IEEE NaNs
      auto va = this->expression(a);
      if (va.hasFlowControl()) {
        return va;
      }
      auto vb = this->expression(b);
      if (vb.hasFlowControl()) {
        return vb;
      }
      if (va.isFloat()) {
        auto da = va.getFloat();
        if (std::isnan(da)) {
          // An IEEE NaN does not compare with anything, even other NaNs
          return false;
        }
        if (vb.isFloat()) {
          // Comparing float with float
          auto db = vb.getFloat();
          if (std::isnan(db)) {
            // An IEEE NaN does not compare with anything, even other NaNs
            return false;
          }
          return bool((da < db) ^ invert); // need to force bool-ness
        }
        if (vb.isInt()) {
          // Comparing float with int
          auto ib = vb.getInt();
          return bool((da < ib) ^ invert); // need to force bool-ness
        }
        auto side = swapped ? "left" : "right";
        return this->raiseNode(b, "Expected ", side, "-hand side of comparison to be 'float' or 'int', not'", vb.getRuntimeType().toString(), "'");
      }
      if (va.isInt()) {
        auto ia = va.getInt();
        if (vb.isFloat()) {
          // Comparing int with float
          auto db = vb.getFloat();
          if (std::isnan(db)) {
            // An IEEE NaN does not compare with anything
            return false;
          }
          return bool((ia < db) ^ invert); // need to force bool-ness
        }
        if (vb.isInt()) {
          // Comparing int with int
          auto ib = vb.getInt();
          return bool((ia < ib) ^ invert); // need to force bool-ness
        }
        auto side = swapped ? "left" : "right";
        return this->raiseNode(b, "Expected ", side, "-hand side of comparison to be 'float' or 'int', not'", vb.getRuntimeType().toString(), "'");
      }
      auto side = swapped ? "right" : "left";
      return this->raiseNode(a, "Expected ", side, "-hand side of comparison to be 'float' or 'int', not'", va.getRuntimeType().toString(), "'");
    }
    Variant operatorTernary(const INode& a, const INode& b, const INode& c) {
      auto cond = this->condition(a, nullptr);
      if (!cond.isBool()) {
        return cond;
      }
      return cond.getBool() ? this->expression(b) : this->expression(c);
    }
    // Implementation
    Variant condition(const INode& node, Block* block) {
      auto value = this->expression(node, block);
      if (!value.hasAny(VariantBits::FlowControl | VariantBits::Bool)) {
        return this->raiseNode(node, "Expected condition to evaluate to a 'bool' value, not '", value.getRuntimeType().toString(), "'");
      }
      return value;
    }
    String identifier(const INode& node) {
      if (node.getOpcode() != OPCODE_IDENTIFIER) {
        throw this->unexpectedOpcode("identifier", node);
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
    Variant call(const INode& node, const Variant& callee, const IParameters& parameters) {
      assert(!callee.hasFlowControl());
      if (callee.hasObject()) {
        auto object = callee.getObject();
        return object->call(*this, parameters);
      }
      return this->raiseNode(node, "WIBBLE: Invalid callee type: '", callee.getRuntimeType().toString(), "'");
    }
    // Error handling
    template<typename... ARGS>
    Variant raiseNode(const INode& node, ARGS&&... args) {
      this->updateLocation(node);
      return this->raiseLocation(this->location, std::forward<ARGS>(args)...);
    }
    template<typename... ARGS>
    Variant raiseLocation(const LocationSource& where, ARGS&&... args) const {
      StringBuilder sb;
      sb.add(where.file);
      if (where.line > 0) {
        sb.add('(', where.line);
        if (where.column > 0) {
          sb.add(',', where.column);
        }
        sb.add(')');
      }
      sb.add(": ", std::forward<ARGS>(args)...);
      Variant exception{ sb.str() };
      exception.addFlowControl(VariantBits::Throw);
      return exception;
    }
    void updateLocation(const INode& node) {
      auto* source = node.getLocation();
      if (source != nullptr) {
        this->location.line = source->line;
        this->location.column = source->column;
      }
    }
    void validateOpcode(const INode& node) {
      this->updateLocation(node);
      auto& properties = OpcodeProperties::from(node.getOpcode());
      if (!properties.validate(node.getChildren(), node.getOperand() != INode::Operand::None)) {
        assert(properties.name != nullptr);
        throw RuntimeException(this->location, "Corrupt opcode: '", properties.name, "'");
      }
    }
  public:
    RuntimeException unexpectedOpcode(const char* expected, const INode& node) {
      this->updateLocation(node);
      auto opcode = node.getOpcode();
      auto name = OpcodeProperties::from(opcode).name;
      if (name == nullptr) {
        return RuntimeException(this->location, "Unknown ", expected, " opcode: '<", std::to_string(opcode), ">'");
      }
      return RuntimeException(this->location, "Unexpected ", expected, " opcode: '", name, "'");
    }
    RuntimeException unexpectedOperator(const char* expected, const INode& node) {
      this->updateLocation(node);
      auto oper = node.getOperator();
      auto name = OperatorProperties::from(oper).name;
      if (name == nullptr) {
        return RuntimeException(this->location, "Unknown ", expected, " operator: '<", std::to_string(oper), ">'");
      }
      return RuntimeException(this->location, "Unexpected ", expected, " operator: '", name, "'");
    }
  };
}

Target::Target(ProgramDefault& program, const INode& node, Block* block)
  : flavour(Flavour::Failed), program(program), node(node) {
  EGG_WARNING_SUPPRESS_SWITCH_BEGIN();
  switch (node.getOpcode()) {
  case OPCODE_DECLARE:
    if (block != nullptr) {
      // Only allow declaration inside a foreach statement
      this->flavour = program.targetDeclare(node, this->a, *block);
      return;
    }
    break;
  case OPCODE_IDENTIFIER:
    this->flavour = program.targetIdentifier(node, this->a);
    return;
  case OPCODE_INDEX:
    this->flavour = program.targetIndex(node, this->a, this->b);
    return;
  }
  EGG_WARNING_SUPPRESS_SWITCH_END();
  throw program.unexpectedOpcode("target", node);
}

egg::ovum::Variant& Target::identifier() const {
  assert(this->flavour == Flavour::Identifier);
  auto name = this->a.getString();
  return this->program.targetSymbol(this->node, name).value;
}

egg::ovum::Variant Target::getIndex() const {
  assert(this->flavour == Flavour::Index);
  auto object = this->a.getObject();
  return object->getIndex(this->program, this->b);
}

egg::ovum::Variant Target::setIndex(const Variant& value) const {
  assert(this->flavour == Flavour::Index);
  auto object = this->a.getObject();
  return object->setIndex(this->program, this->b, value);
}

Variant Target::check() const {
  if (this->flavour == Flavour::Failed) {
    // Return the problem we saved in the constructor
    return this->a;
  }
  return Variant::Void;
}

Variant Target::assign(Variant&& rhs) const {
  // TODO promotion
  switch (this->flavour) {
  case Flavour::Failed:
    break;
  case Flavour::Identifier:
    this->identifier() = std::move(rhs);
    return Variant::Void;
  case Flavour::Index:
    return this->setIndex(std::move(rhs));
  }
  // Return the problem we saved in the constructor
  return this->a;
}

Variant Target::nudge(Int rhs) const {
  Variant element;
  Variant* slot;
  switch (this->flavour) {
  case Flavour::Failed:
  default:
    // Return the problem we saved in the constructor
    return this->a;
  case Flavour::Identifier:
    slot = &this->identifier();
    if (slot->isInt()) {
      *slot = slot->getInt() + rhs;
      return Variant::Void;
    }
    break;
  case Flavour::Index:
    element = this->getIndex();
    if (element.hasFlowControl()) {
      return element;
    }
    if (element.isInt()) {
      element = element.getInt() + rhs;
      return this->setIndex(element);
    }
    slot = &element;
    break;
  }
  if (rhs < 0) {
    return Binary::unexpected(*slot, "Expected decrement operation to be applied to an 'int' value");
  }
  return Binary::unexpected(*slot, "Expected increment operation to be applied to an 'int' value");
}

Variant Target::mutate(const INode& opnode, const INode& rhs) const {
  switch (this->flavour) {
  case Flavour::Failed:
    break;
  case Flavour::Identifier:
    return this->apply(opnode, this->identifier(), rhs);
  case Flavour::Index:
    // TODO atomic mutation
    auto element = this->getIndex();
    if (element.hasFlowControl()) {
      return element;
    }
    auto result = this->apply(opnode, element, rhs);
    if (result.hasFlowControl()) {
      return result;
    }
    return this->setIndex(element);
  }
  // Return the problem we saved in the constructor
  return this->a;
}

egg::ovum::Variant Target::apply(const INode& opnode, Variant& lvalue, const INode& rhs) const {
  // Handle "short-circuit" cases before evaluating the rhs
  auto oper = opnode.getOperator();
  EGG_WARNING_SUPPRESS_SWITCH_BEGIN();
  switch (oper) {
  case Operator::OPERATOR_IFNULL:
    // Don't evaluate rhs of (lhs ??= rhs) unless lhs is null
    if (!lvalue.isNull()) {
      // Do nothing
      return Variant::Void;
    }
    break;
  case Operator::OPERATOR_LOGAND:
    // Don't evaluate rhs of (lhs &&= rhs) if lhs is false
    if (lvalue.isBool() && !lvalue.getBool()) {
      return Variant::Void;
    }
    break;
  case Operator::OPERATOR_LOGOR:
    // Don't evaluate rhs of (lhs ||= rhs) if lhs is true
    if (lvalue.isBool() && lvalue.getBool()) {
      return Variant::Void;
    }
    break;
  }
  EGG_WARNING_SUPPRESS_SWITCH_END();
  auto rvalue = this->expression(rhs);
  if (rvalue.hasFlowControl()) {
    return rvalue;
  }
  auto result = Binary::apply(oper, lvalue, rvalue);
  if (result.is(VariantBits::Break)) {
    throw this->program.unexpectedOperator("target binary", opnode);
  }
  return result;
}

Block::~Block() {
  // Undeclare all the names successfully declared by this block
  for (auto& i : this->declared) {
    this->program.blockUndeclare(i);
  }
}

egg::ovum::Variant Block::declare(const INode& node, const Type& type, const String& name, const Variant& init) {
  // Keep track of names successfully declared in this block
  auto retval = this->program.blockDeclare(node, type, name, init);
  if (!retval.hasFlowControl()) {
    this->declared.insert(name);
  }
  return retval;
}

egg::ovum::Variant Target::expression(const INode& value) const {
  return this->program.targetExpression(value);
}

egg::ovum::Program egg::ovum::ProgramFactory::createProgram(IAllocator& allocator, ILogger& logger) {
  auto basket = BasketFactory::createBasket(allocator);
  auto program = allocator.make<ProgramDefault>(*basket, logger);
  program->addBuiltins();
  return program;
}
