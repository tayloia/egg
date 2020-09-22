#include "ovum/ovum.h"
#include "ovum/slot.h"
#include "ovum/node.h"
#include "ovum/module.h"
#include "ovum/program.h"
#include "ovum/utf.h"

#include <map>
#include <set>

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
      location.formatSourceString(sb);
      sb.add(':', ' ', std::forward<ARGS>(args)...);
      this->message = sb.str();
    }
  };

  class Parameters : public IParameters {
    Parameters(const Parameters&) = delete;
    Parameters& operator=(const Parameters&) = delete;
  private:
    std::vector<Value> positional;
    std::vector<LocationSource> location;
  public:
    Parameters() = default;
    void addPositional(const LocationSource& source, Value&& value) {
      this->positional.emplace_back(std::move(value));
      this->location.emplace_back(source);
    }
    virtual size_t getPositionalCount() const override {
      return this->positional.size();
    }
    virtual Value getPositional(size_t index) const override {
      return this->positional[index];
    }
    virtual const LocationSource* getPositionalLocation(size_t index) const override {
      return &this->location[index];
    }
    virtual size_t getNamedCount() const override {
      return 0;
    }
    virtual String getName(size_t) const override {
      return String();
    }
    virtual Value getNamed(const String&) const override {
      return Value::Void;
    }
    virtual const LocationSource* getNamedLocation(const String&) const override {
      // TODO remove
      return nullptr;
    }
  };

  class PredicateFunction : public SoftReferenceCounted<IObject> {
    PredicateFunction(const PredicateFunction&) = delete;
    PredicateFunction& operator=(const PredicateFunction&) = delete;
  private:
    ProgramDefault& program; // ProgramDefault lifetime guaranteed to be longer than UserFunction instance
    LocationSource location;
    Node node;
  public:
    PredicateFunction(IAllocator& allocator, ProgramDefault& program, const LocationSource& location, const INode& node)
      : SoftReferenceCounted(allocator),
      program(program),
      location(location),
      node(&node) {
    }
    virtual void softVisitLinks(const Visitor&) const override {
      // There are no soft link to visit
    }
    virtual String toString() const override {
      return "<predicate>";
    }
    virtual Type getRuntimeType() const override {
      return Type::Object;
    }
    virtual Value call(IExecution& execution, const IParameters& parameters) override;
    virtual Value getProperty(IExecution& execution, const String&) override {
      return execution.raise("Internal runtime error: Predicates do not support properties");
    }
    virtual Value setProperty(IExecution& execution, const String&, const Value&) override {
      return execution.raise("Internal runtime error: Predicates do not support properties");
    }
    virtual Value getIndex(IExecution& execution, const Value&) override {
      return execution.raise("Internal runtime error: Predicates do not support indexing");
    }
    virtual Value setIndex(IExecution& execution, const Value&, const Value&) override {
      return execution.raise("Internal runtime error: Predicates do not support indexing");
    }
    virtual Value iterate(IExecution& execution) override {
      return execution.raise("Internal runtime error: Predicates do not support iteration");
    }
  };

  class UserFunction : public SoftReferenceCounted<IObject> {
    UserFunction(const UserFunction&) = delete;
    UserFunction& operator=(const UserFunction&) = delete;
  private:
    ProgramDefault& program; // ProgramDefault lifetime guaranteed to be longer than UserFunction instance
    LocationSource location;
    Type type;
    Node block;
  public:
    UserFunction(IAllocator& allocator, ProgramDefault& program, const LocationSource& location, const Type& type, const INode& block)
      : SoftReferenceCounted(allocator),
        program(program),
        location(location),
        type(type),
        block(&block) {
      assert(type != nullptr);
      assert(block.getOpcode() == Opcode::BLOCK);
    }
    virtual void softVisitLinks(const Visitor&) const override {
      // WIBBLE
    }
    virtual String toString() const override {
      return "<user-function>";
    }
    virtual Type getRuntimeType() const override {
      return this->type;
    }
    virtual Value call(IExecution& execution, const IParameters& parameters) override;
    virtual Value getProperty(IExecution& execution, const String& property) override {
      return this->raise(execution, "does not support properties such as '", property, "'");
    }
    virtual Value setProperty(IExecution& execution, const String& property, const Value&) override {
      return this->raise(execution, "does not support properties such as '", property, "'");
    }
    virtual Value getIndex(IExecution& execution, const Value&) override {
      return this->raise(execution, "does not support indexing");
    }
    virtual Value setIndex(IExecution& execution, const Value&, const Value&) override {
      return this->raise(execution, "does not support indexing");
    }
    virtual Value iterate(IExecution& execution) override {
      return this->raise(execution, "does not support iteration");
    }
    template<typename... ARGS>
    Value raise(IExecution& execution, ARGS&&... args) {
      auto* signature = this->type->callable();
      if (signature != nullptr) {
        auto name = signature->getFunctionName();
        if (!name.empty()) {
          return execution.raiseFormat("Function '", name, "' ", std::forward<ARGS>(args)...);
        }
      }
      return execution.raiseFormat("Function ", std::forward<ARGS>(args)...);
    }
    static Type makeType(IAllocator& allocator, ProgramDefault& program, const String& name, const INode& callable);
  };

  class Target {
    Target(const Target&) = delete;
    Target& operator=(const Target&) = delete;
  private:
    ProgramDefault& program;
    const INode& node;
    Slot slot;
    Value failure;
  public:
    Target(ProgramDefault& program, const INode& node, Block* block = nullptr);
    Value check() const;
    Value assign(const Value& rhs) const;
    Value nudge(Int rhs) const;
    Value mutate(const INode& opnode, const INode& rhs) const;
  private:
    bool ref(Value& pointee) const;
    Value get() const;
    Value set(const Value& value) const;
    Value apply(const INode& opnode, Value& lvalue, const INode& rhs) const;
    Value raise(const String& message) const;
    template<typename... ARGS>
    Value unexpected(const Value& value, ARGS&&... args) const {
      return this->raise(StringBuilder::concat(std::forward<ARGS>(args)..., ", but got '", value->getRuntimeType().toString(), "' instead"));
    }
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
    bool empty() const {
      return this->declared.empty();
    }
    Value declare(const LocationSource& source, const Type& type, const String& name, const Value* init = nullptr);
    Value guard(const LocationSource& source, const Type& type, const String& name, const Value& init);
  };

  struct Symbol {
    Type type;
    String name;
    LocationSource source;
    Slot slot;
    Symbol(const Type& type, const String& name, const LocationSource& source)
      : type(type),
        name(name),
        source(source),
        slot() {
      assert(this->validate(true));
    }
    Symbol(Symbol&& rhs) noexcept
      : type(std::move(rhs.type)),
        name(std::move(rhs.name)),
        source(std::move(rhs.source)),
        slot(std::move(rhs.slot)) {
      assert(this->validate(true));
    }
    Symbol(const Symbol&) = delete;
    Symbol& operator=(const Symbol&) = delete;
    bool validate(bool optional) const {
      return (this->type != nullptr) && !this->name.empty() && slot.validate(optional);
    }
  };

  class SymbolTable : public SoftReferenceCounted<ICollectable> {
    SymbolTable(const SymbolTable&) = delete;
    SymbolTable& operator=(const SymbolTable&) = delete;
  private:
    std::map<String, Symbol> table;
    SoftPtr<SymbolTable> parent;
    SoftPtr<SymbolTable> capture;
  public:
    explicit SymbolTable(IAllocator& allocator, SymbolTable* parent = nullptr, SymbolTable* capture = nullptr)
      : SoftReferenceCounted(allocator) {
      this->parent.set(*this, parent);
      this->capture.set(*this, capture);
    }
    std::pair<bool, Symbol*> add(const Type& type, const String& name, const LocationSource& source) {
      // Return true in the first of the pair iff the insertion occurred
      Symbol symbol(type, name, source);
      auto retval = this->table.insert(std::make_pair(name, std::move(symbol)));
      return std::make_pair(retval.second, &retval.first->second);
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
        if (this->capture != nullptr) {
          return this->capture->get(name);
        }
        return nullptr;
      }
      return &retval->second;
    }
    void softVisitLinks(const Visitor& visitor) const {
      // WIBBLE
      this->parent.visit(visitor);
      this->capture.visit(visitor);
    }
    SymbolTable* push(SymbolTable* captured = nullptr) {
      // Push a table onto the symbol table stack
      return this->allocator.create<SymbolTable>(0, this->allocator, this, captured);
    }
    SymbolTable* pop() {
      // Pop an element from the symbol table stack
      assert(this->parent != nullptr);
      return this->parent.get();
    }
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
        symtable(allocator.make<SymbolTable>()),
        location({}, 0, 0) {
      this->basket->take(*this->symtable);
    }
    virtual ~ProgramDefault() {
      this->symtable.set(nullptr);
      this->basket->collect();
    }
    virtual bool builtin(const String& name, const Value& value) override {
      static const LocationSource source("<builtin>", 0, 0);
      auto symbol = this->symtable->add(value->getRuntimeType(), name, source);
      if (symbol.first) {
        // Don't use tryAssign for builtins; it always fails
        symbol.second->slot.set(value);
        return true;
      }
      return false;
    }
    virtual Value run(const IModule& module) override {
      try {
        this->location.file = module.getResourceName();
        this->location.line = 0;
        this->location.column = 0;
        return this->executeRoot(module.getRootNode());
      } catch (RuntimeException& exception) {
        this->logger.log(ILogger::Source::Runtime, ILogger::Severity::Error, exception.message.toUTF8());
        return Value::Rethrow;
      }
    }
    // Inherited from IExecution
    virtual IAllocator& getAllocator() const override {
      return this->allocator;
    }
    virtual IBasket& getBasket() const override {
      return *this->basket;
    }
    virtual Value raise(const String& message) override {
      return ValueFactory::createThrowError(this->allocator, this->location, message);
    }
    virtual Value assertion(const Value& predicate) override {
      Object object;
      if (predicate->getObject(object)) {
        // Predicates can be functions that throw exceptions, as well as 'bool' values
        auto type = object->getRuntimeType();
        if (type->callable() != nullptr) {
          // Call the predicate directly
          return object->call(*this, Object::ParametersNone);
        }
      }
      Bool value;
      if (!predicate->getBool(value)) {
        return this->raiseFormat("'assert()' expects its parameter to be a 'bool' or 'void()', but got '", predicate->getRuntimeType().toString(), "' instead");
      }
      if (value) {
        return Value::Void;
      }
      return this->raise("Assertion is untrue");
    }
    virtual void print(const std::string& utf8) override {
      this->logger.log(ILogger::Source::User, ILogger::Severity::Information, utf8);
    }
    // Builtins
    void addBuiltins() {
      this->builtin("assert", ValueFactory::createBuiltinAssert(this->allocator));
      this->builtin("print", ValueFactory::createBuiltinPrint(this->allocator));
      this->builtin("string", ValueFactory::createBuiltinString(this->allocator));
      this->builtin("type", ValueFactory::createBuiltinType(this->allocator));
    }
  private:
    Value executeRoot(const INode& node) {
      if (this->validateOpcode(node) != Opcode::MODULE) {
        throw this->unexpectedOpcode("module", node);
      }
      Block block(*this);
      return this->executeBlock(block, node.getChild(0));
    }
    Value executeBlock(Block& inner, const INode& node) {
      this->updateLocation(node);
      if (this->validateOpcode(node) != Opcode::BLOCK) {
        throw this->unexpectedOpcode("block", node);
      }
      size_t n = node.getChildren();
      for (size_t i = 0; i < n; ++i) {
        auto retval = this->statement(inner, node.getChild(i));
        if (retval.hasFlowControl()) {
          return retval;
        }
      }
      return Value::Void;
    }
    // Statements
    Value statement(Block& block, const INode& node) {
      this->updateLocation(node);
      auto opcode = this->validateOpcode(node);
      EGG_WARNING_SUPPRESS_SWITCH_BEGIN();
      switch (opcode) {
      case Opcode::NOOP:
        return Value::Void;
      case Opcode::BLOCK:
        return this->statementBlock(node);
      case Opcode::ASSIGN:
        return this->statementAssign(node);
      case Opcode::BREAK:
        return Value::Break;
      case Opcode::CALL:
        return this->statementCall(node);
      case Opcode::CONTINUE:
        return Value::Continue;
      case Opcode::DECLARE:
        return this->statementDeclare(block, node);
      case Opcode::DECREMENT:
        return this->statementDecrement(node);
      case Opcode::DO:
        return this->statementDo(node);
      case Opcode::FOR:
        return this->statementFor(node);
      case Opcode::FOREACH:
        return this->statementForeach(node);
      case Opcode::FUNCTION:
        return this->statementFunction(block, node);
      case Opcode::IF:
        return this->statementIf(node);
      case Opcode::INCREMENT:
        return this->statementIncrement(node);
      case Opcode::MUTATE:
        return this->statementMutate(node);
      case Opcode::RETURN:
        return this->statementReturn(node);
      case Opcode::SWITCH:
        return this->statementSwitch(node);
      case Opcode::THROW:
        return this->statementThrow(node);
      case Opcode::TRY:
        return this->statementTry(node);
      case Opcode::WHILE:
        return this->statementWhile(node);
      }
      EGG_WARNING_SUPPRESS_SWITCH_END();
      throw this->unexpectedOpcode("statement", node);
    }
    Value statementAssign(const INode& node) {
      assert(node.getOpcode() == Opcode::ASSIGN);
      assert(node.getChildren() == 2);
      Target lvalue(*this, node.getChild(0));
      auto rvalue = this->expression(node.getChild(1));
      if (rvalue.hasFlowControl()) {
        return rvalue;
      }
      return lvalue.assign(rvalue);
    }
    Value statementBlock(const INode& node) {
      assert(node.getOpcode() == Opcode::BLOCK);
      Block block(*this);
      return this->executeBlock(block, node);
    }
    Value statementCall(const INode& node) {
      assert(node.getOpcode() == Opcode::CALL);
      auto retval = this->expressionCall(node);
      if (retval.hasAnyFlags(ValueFlags::FlowControl | ValueFlags::Void)) {
        return retval;
      }
      auto message = StringBuilder().add("Discarding call return value: '", retval->toString(), "'").toUTF8();
      this->logger.log(ILogger::Source::Runtime, ILogger::Severity::Warning, message);
      return Value::Void;
    }
    Value statementDeclare(Block& block, const INode& node) {
      assert(node.getOpcode() == Opcode::DECLARE);
      this->updateLocation(node);
      auto vname = this->identifier(node.getChild(1));
      auto vtype = this->type(node.getChild(0), vname);
      if (node.getChildren() == 2) {
        // No initializer
        return block.declare(this->location, vtype, vname);
      }
      assert(node.getChildren() == 3);
      auto vinit = this->expression(node.getChild(2));
      if (vinit.hasFlowControl()) {
        return vinit;
      }
      return block.declare(this->location, vtype, vname, &vinit);
    }
    Value statementDecrement(const INode& node) {
      assert(node.getOpcode() == Opcode::DECREMENT);
      assert(node.getChildren() == 1);
      Target lvalue(*this, node.getChild(0));
      return lvalue.nudge(-1);
    }
    Value statementDo(const INode& node) {
      assert(node.getOpcode() == Opcode::DO);
      assert(node.getChildren() == 2);
      Value retval;
      Bool condition;
      do {
        Block block(*this);
        retval = this->executeBlock(block, node.getChild(1));
        if (retval.hasFlowControl()) {
          return retval;
        }
        retval = this->condition(node.getChild(0), nullptr); // don't allow guards
        if (!retval->getBool(condition)) {
          // Problem with the condition
          return retval;
        }
      } while (condition);
      return Value::Void;
    }
    Value statementFor(const INode& node) {
      assert(node.getOpcode() == Opcode::FOR);
      assert(node.getChildren() == 4);
      auto& pre = node.getChild(0);
      auto* cond = &node.getChild(1);
      EGG_WARNING_SUPPRESS_SWITCH_BEGIN();
      switch (cond->getOpcode()) {
      case Opcode::NOOP:
      case Opcode::TRUE:
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
      Bool condition;
      do {
        if (cond != nullptr) {
          retval = this->condition(*cond, &inner);
          if (!retval->getBool(condition)) {
            // Problem evaluating the condition
            return retval;
          }
          if (!condition) {
            // Condition evaluated to 'false'
            return Value::Void;
          }
        }
        retval = this->executeBlock(inner, loop);
        auto flags = retval->getFlags();
        if (Bits::hasAnySet(flags, ValueFlags::FlowControl)) {
          if (flags == ValueFlags::Break) {
            // Break from the loop
            return Value::Void;
          }
          if (flags != ValueFlags::Continue) {
            // Some other flow control
            return retval;
          }
        }
        retval = this->statement(inner, post);
      } while (!retval.hasFlowControl());
      return retval;
    }
    Value statementForeach(const INode& node) {
      assert(node.getOpcode() == Opcode::FOREACH);
      assert(node.getChildren() == 3);
      Block inner(*this);
      Target lvalue(*this, node.getChild(0), &inner);
      auto retval = lvalue.check();
      if (retval.hasFlowControl()) {
        return retval;
      }
      auto rvalue = this->expression(node.getChild(1));
      if (rvalue.hasFlowControl()) {
        return rvalue;
      }
      String string;
      if (rvalue->getString(string)) {
        // Iterate around the codepoints in the string
        return this->stringForeach(string, inner, lvalue, node.getChild(2));
      }
      Object object;
      if (!rvalue->getObject(object)) {
        return this->raiseFormat("The 'for' statement expected to iterate a 'string' or 'object', but got '", rvalue->getRuntimeType().toString(), "' instead");
      }
      assert(object != nullptr);
      auto iterate = object->iterate(*this);
      if (iterate.hasFlowControl()) {
        return iterate;
      }
      if (!iterate->getObject(object)) {
        return this->raiseFormat("The 'for' statement expected an iterator, but got '", iterate->getRuntimeType().toString(), "' instead");
      }
      assert(object != nullptr);
      auto& block = node.getChild(2);
      for (;;) {
        retval = object->call(*this, Object::ParametersNone);
        if (retval.hasAnyFlags(ValueFlags::FlowControl | ValueFlags::Void)) {
          break;
        }
        retval = lvalue.assign(retval);
        if (retval.hasFlowControl()) {
          break;
        }
        retval = this->executeBlock(inner, block);
        auto flags = retval->getFlags();
        if (Bits::hasAnySet(flags, ValueFlags::FlowControl)) {
          if (flags == ValueFlags::Break) {
            // Break from the loop
            return Value::Void;
          }
          if (flags != ValueFlags::Continue) {
            // Some other flow control
            break;
          }
        }
      }
      return retval;
    }
    Value statementFunction(Block& block, const INode& node) {
      assert(node.getOpcode() == Opcode::FUNCTION);
      assert(node.getChildren() == 3);
      auto fname = this->identifier(node.getChild(2));
      auto ftype = this->type(node.getChild(0), fname);
      auto& fblock = node.getChild(1);
      if (fblock.getOpcode() != Opcode::BLOCK) {
        return this->raise("Generators are not yet supported"); // WIBBLE
      }
      this->updateLocation(node);
      auto function = this->allocator.make<UserFunction>(*this, this->location, ftype, fblock);
      // We have to be careful to ensure the function is declared before capturing symbols so that recursion works correctly
      auto fvalue = ValueFactory::createObject(this->allocator, Object(*function));
      auto retval = block.declare(this->location, ftype, fname, &fvalue);
      // WIBBLE function->setCaptured(this->symtable); // WIBBLE was cloneIndirect
      return retval;
    }
    Value statementIf(const INode& node) {
      assert(node.getOpcode() == Opcode::IF);
      auto n = &node;
      Block block(*this);
      do {
        assert(n->getOpcode() == Opcode::IF);
        assert((n->getChildren() == 2) || (n->getChildren() == 3));
        auto retval = this->condition(n->getChild(0), &block);
        Bool condition;
        if (!retval->getBool(condition)) {
          // Problem with the condition
          return retval;
        }
        if (condition) {
          // Execute the 'then' clause
          return this->executeBlock(block, n->getChild(1));
        }
        if (n->getChildren() == 2) {
          // No 'else' clause
          return Value::Void;
        }
        // Execute the 'else' clause
        n = &n->getChild(2);
      } while (n->getOpcode() == Opcode::IF);
      assert(block.empty());
      return this->executeBlock(block, *n);
    }
    Value statementIncrement(const INode& node) {
      assert(node.getOpcode() == Opcode::INCREMENT);
      assert(node.getChildren() == 1);
      Target lvalue(*this, node.getChild(0));
      return lvalue.nudge(+1);
    }
    Value statementMutate(const INode& node) {
      assert(node.getOpcode() == Opcode::MUTATE);
      assert(node.getChildren() == 2);
      assert(OperatorProperties::from(node.getOperator()).opclass == Opclass::BINARY);
      Target lvalue(*this, node.getChild(0));
      return lvalue.mutate(node, node.getChild(1));
    }
    Value statementReturn(const INode& node) {
      assert(node.getOpcode() == Opcode::RETURN);
      assert(node.getChildren() <= 1);
      if (node.getChildren() == 0) {
        // A naked return
        return ValueFactory::createFlowControl(this->allocator, ValueFlags::Return, Value::Void);
      }
      auto retval = this->expression(node.getChild(0));
      if (!retval.hasFlowControl()) {
        // Convert the expression to a return
        assert(!retval->getVoid());
        return ValueFactory::createFlowControl(this->allocator, ValueFlags::Return, retval);
      }
      return retval;
    }
    Value statementSwitch(const INode& node) {
      assert(node.getOpcode() == Opcode::SWITCH);
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
        assert((clause.getOpcode() == Opcode::CASE) || (clause.getOpcode() == Opcode::DEFAULT));
        for (size_t j = 1; !matched && (j < m); ++j) {
          auto expr = this->expression(node.getChild(0));
          if (expr.hasFlowControl()) {
            // Failed to evaluate the value to match
            return expr;
          }
          if (Value::equals(expr, match, ValueCompare::PromoteInts)) {
            // We've matched this clause
            matched = j;
            break;
          }
        }
        if (matched != 0) {
          // We matched!
          break;
        }
        if (clause.getOpcode() == Opcode::DEFAULT) {
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
        return Value::Void;
      }
      for (;;) {
        // Run the clauses in round-robin order
        auto retval = this->statementBlock(node.getChild(matched).getChild(0));
        auto flags = retval->getFlags();
        if (flags == ValueFlags::Break) {
          // Explicit 'break' terminates the switch statement
          break;
        }
        if (flags != ValueFlags::Continue) {
          // Any other than 'continue' also terminates the switch statement
          return retval;
        }
        if (++matched >= n) {
          // Round robin
          matched = 1;
        }
      }
      return Value::Void;
    }
    Value statementThrow(const INode& node) {
      assert(node.getOpcode() == Opcode::THROW);
      assert(node.getChildren() <= 1);
      if (node.getChildren() == 0) {
        // A naked rethrow
        return Value::Rethrow;
      }
      auto retval = this->expression(node.getChild(0));
      if (!retval.hasFlowControl()) {
        // Convert the expression to an exception throw
        assert(!retval->getVoid());
        return ValueFactory::createFlowControl(this->allocator, ValueFlags::Throw, retval);
      }
      return retval;
    }
    Value statementTry(const INode& node) {
      assert(node.getOpcode() == Opcode::TRY);
      auto n = node.getChildren();
      assert(n >= 2);
      auto exception = this->statementBlock(node.getChild(0));
      if (exception.hasAnyFlags(ValueFlags::Throw)) {
        Value thrown;
        if (exception->getChild(thrown)) {
          // Find a matching catch block
          Block block(*this);
          for (size_t i = 2; i < n; ++i) {
            // Look for the first matching catch clause
            auto& clause = node.getChild(i);
            assert(clause.getOpcode() == Opcode::CATCH);
            assert(clause.getChildren() == 3);
            this->updateLocation(clause);
            auto cname = this->identifier(clause.getChild(1));
            auto ctype = this->type(clause.getChild(0), cname);
            auto retval = block.guard(this->location, ctype, cname, thrown);
            if (retval.hasFlowControl()) {
              // Couldn't define the exception variable
              return this->statementTryFinally(node, retval);
            }
            Bool found;
            if (retval->getBool(found) && found) {
              retval = this->executeBlock(block, clause.getChild(2));
              if (retval->getFlags() == ValueFlags::Throw) {
                // This is a rethrow of the original exception
                return this->statementTryFinally(node, exception);
              }
              return this->statementTryFinally(node, retval);
            }
          }
        }
      }
      return this->statementTryFinally(node, exception);
    }
    Value statementTryFinally(const INode& node, const Value& retval) {
      assert(node.getOpcode() == Opcode::TRY);
      assert(node.getChildren() >= 2);
      auto& clause = node.getChild(1);
      if (clause.getOpcode() == Opcode::NOOP) {
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
    Value statementWhile(const INode& node) {
      assert(node.getOpcode() == Opcode::WHILE);
      assert(node.getChildren() == 2);
      for (;;) {
        Block block(*this);
        auto retval = this->condition(node.getChild(0), &block);
        Bool condition;
        if (!retval->getBool(condition)) {
          // Problem with the condition
          return retval;
        }
        if (!condition) {
          // Condition is false
          break;
        }
        retval = this->executeBlock(block, node.getChild(1));
        if (retval.hasFlowControl()) {
          return retval;
        }
      }
      return Value::Void;
    }
    // Expressions
    Value expression(const INode& node, Block* block = nullptr) {
      auto opcode = this->validateOpcode(node);
      EGG_WARNING_SUPPRESS_SWITCH_BEGIN();
      switch (opcode) {
      case Opcode::NULL_:
        return Value::Null;
      case Opcode::FALSE:
        return Value::False;
      case Opcode::TRUE:
        return Value::True;
      case Opcode::UNARY:
        return this->expressionUnary(node);
      case Opcode::BINARY:
        return this->expressionBinary(node);
      case Opcode::TERNARY:
        return this->expressionTernary(node);
      case Opcode::COMPARE:
        return this->expressionCompare(node);
      case Opcode::AVALUE:
        return this->expressionAvalue(node);
      case Opcode::FVALUE:
        return this->expressionFvalue(node);
      case Opcode::IVALUE:
        return this->expressionIvalue(node);
      case Opcode::OVALUE:
        return this->expressionOvalue(node);
      case Opcode::SVALUE:
        return this->expressionSvalue(node);
      case Opcode::CALL:
        return this->expressionCall(node);
      case Opcode::GUARD:
        return this->expressionGuard(node, block);
      case Opcode::IDENTIFIER:
        return this->expressionIdentifier(node);
      case Opcode::INDEX:
        return this->expressionIndex(node);
      case Opcode::PREDICATE:
        return this->expressionPredicate(node);
      case Opcode::PROPERTY:
        return this->expressionProperty(node);
      }
      EGG_WARNING_SUPPRESS_SWITCH_END();
      throw this->unexpectedOpcode("expression", node);
    }
    Value expressionUnary(const INode& node) {
      assert(node.getOpcode() == Opcode::UNARY);
      assert(node.getChildren() == 1);
      auto oper = node.getOperator();
      assert(OperatorProperties::from(oper).opclass == Opclass::UNARY);
      EGG_WARNING_SUPPRESS_SWITCH_BEGIN();
      switch (oper) {
      case Operator::LOGNOT:
        return this->operatorLogicalNot(node.getChild(0));
      case Operator::DEREF:
        return this->operatorDeref(node.getChild(0));
      case Operator::REF:
        return this->operatorRef(node.getChild(0));
      }
      EGG_WARNING_SUPPRESS_SWITCH_END();
      throw this->unexpectedOperator("unary", node);
    }
    Value expressionBinary(const INode& node) {
      assert(node.getOpcode() == Opcode::BINARY);
      assert(node.getChildren() == 2);
      auto oper = node.getOperator();
      assert(OperatorProperties::from(oper).opclass == Opclass::BINARY);
      EGG_WARNING_SUPPRESS_SWITCH_BEGIN();
      switch (oper) {
      case Operator::ADD:
      case Operator::SUB:
      case Operator::MUL:
      case Operator::DIV:
      case Operator::REM:
        return this->operatorArithmetic(node, oper, node.getChild(0), node.getChild(1));
      }
      EGG_WARNING_SUPPRESS_SWITCH_END();
      throw this->unexpectedOperator("binary", node);
    }
    Value expressionTernary(const INode& node) {
      assert(node.getOpcode() == Opcode::TERNARY);
      assert(node.getChildren() == 3);
      auto oper = node.getOperator();
      assert(OperatorProperties::from(oper).opclass == Opclass::TERNARY);
      EGG_WARNING_SUPPRESS_SWITCH_BEGIN();
      switch (oper) {
      case Operator::TERNARY:
        return this->operatorTernary(node.getChild(0), node.getChild(1), node.getChild(2));
      }
      EGG_WARNING_SUPPRESS_SWITCH_END();
      throw this->unexpectedOperator("ternary", node);
    }
    Value expressionCompare(const INode& node) {
      assert(node.getOpcode() == Opcode::COMPARE);
      Value lhs, rhs;
      auto retval = this->operatorCompare(node, lhs, rhs);
      if (retval->getFlags() == ValueFlags::Break) {
        throw this->unexpectedOperator("compare", node);
      }
      return retval;
    }
    Value expressionAvalue(const INode& node) {
      assert(node.getOpcode() == Opcode::AVALUE);
      auto n = node.getChildren();
      auto array = ObjectFactory::createVanillaArray(this->allocator, n);
      for (size_t i = 0; i < n; ++i) {
        auto expr = this->expression(node.getChild(i));
        if (expr.hasFlowControl()) {
          return expr;
        }
        expr = array->setIndex(*this, ValueFactory::createInt(this->allocator, Int(i)), expr);
        if (expr.hasFlowControl()) {
          return expr;
        }
      }
      return ValueFactory::create(this->allocator, array);
    }
    Value expressionFvalue(const INode& node) {
      assert(node.getOpcode() == Opcode::FVALUE);
      assert(node.getChildren() == 0);
      return ValueFactory::createFloat(this->allocator, node.getFloat());
    }
    Value expressionIvalue(const INode& node) {
      assert(node.getOpcode() == Opcode::IVALUE);
      assert(node.getChildren() == 0);
      return ValueFactory::createInt(this->allocator, node.getInt());
    }
    Value expressionOvalue(const INode& node) {
      assert(node.getOpcode() == Opcode::OVALUE);
      auto object = ObjectFactory::createVanillaObject(this->allocator);
      auto n = node.getChildren();
      for (size_t i = 0; i < n; ++i) {
        auto& named = node.getChild(i);
        assert(named.getOpcode() == Opcode::NAMED);
        auto name = this->identifier(named.getChild(0));
        auto expr = this->expression(named.getChild(1));
        if (expr.hasFlowControl()) {
          return expr;
        }
        expr = object->setProperty(*this, name, expr);
        if (expr.hasFlowControl()) {
          return expr;
        }
      }
      return ValueFactory::createObject(this->allocator, object);
    }
    Value expressionSvalue(const INode& node) {
      assert(node.getOpcode() == Opcode::SVALUE);
      assert(node.getChildren() == 0);
      return ValueFactory::createString(this->allocator, node.getString());
    }
    Value expressionCall(const INode& node) {
      assert(node.getOpcode() == Opcode::CALL);
      auto n = node.getChildren();
      assert(n >= 1);
      auto callee = this->expression(node.getChild(0));
      if (callee.hasFlowControl()) {
        return callee;
      }
      Object object;
      if (!callee->getObject(object)) {
        return this->raiseNode(node, "Expected function-like expression to be an 'object', but got '", callee->getRuntimeType().toString(), "' instead");
      }
      LocationSource before = this->location;
      Parameters parameters;
      for (size_t i = 1; i < n; ++i) {
        auto& pnode = node.getChild(i);
        auto pvalue = this->expression(pnode);
        if (pvalue.hasFlowControl()) {
          return pvalue;
        }
        this->updateLocation(pnode);
        parameters.addPositional(this->location, std::move(pvalue));
      }
      auto retval = object->call(*this, parameters);
      this->location = before;
      return retval;
    }
    Value expressionGuard(const INode& node, Block* block) {
      assert(node.getOpcode() == Opcode::GUARD);
      assert(node.getChildren() == 3);
      if (block == nullptr) {
        throw this->unexpectedOpcode("guard", node);
      }
      auto vname = this->identifier(node.getChild(1));
      auto vtype = this->type(node.getChild(0), vname);
      auto vinit = this->expression(node.getChild(2));
      if (vinit.hasFlowControl()) {
        return vinit;
      }
      this->updateLocation(node);
      return block->guard(this->location, vtype, vname, vinit);
    }
    Value expressionIdentifier(const INode& node) {
      assert(node.getOpcode() == Opcode::IDENTIFIER);
      auto name = this->identifier(node);
      auto* symbol = this->symtable->get(name);
      if (symbol == nullptr) {
        return this->raiseNode(node, "Unknown identifier in expression: '", name, "'");
      }
      return symbol->slot.get();
    }
    Value expressionIndex(const INode& node) {
      assert(node.getOpcode() == Opcode::INDEX);
      assert(node.getChildren() == 2);
      auto lhs = this->expression(node.getChild(0));
      if (lhs.hasFlowControl()) {
        return lhs;
      }
      auto& index = node.getChild(1);
      auto rhs = this->expression(index);
      if (rhs.hasFlowControl()) {
        return rhs;
      }
      this->updateLocation(index);
      Object object;
      if (lhs->getObject(object)) {
        return object->getIndex(*this, rhs);
      }
      String string;
      if (lhs->getString(string)) {
        return this->stringIndex(string, rhs);
      }
      return this->raiseFormat("Values of type '", lhs->getRuntimeType().toString(), "' do not support the indexing '[]' operator");
    }
    Value expressionPredicate(const INode& node) {
      assert(node.getOpcode() == Opcode::PREDICATE);
      assert(node.getChildren() == 1);
      auto& child = node.getChild(0);
      if (child.getOperand() == INode::Operand::Operator) {
        auto oper = child.getOperator();
        if (OperatorProperties::from(oper).opclass == Opclass::COMPARE) {
          // We only support predicates for comparisons
          this->updateLocation(node);
          auto predicate = ObjectFactory::create<PredicateFunction>(this->allocator, *this, this->location, child);
          return ValueFactory::createObject(this->allocator, predicate);
        }
      }
      return this->expression(child);
    }
    Value expressionProperty(const INode& node) {
      assert(node.getOpcode() == Opcode::PROPERTY);
      assert(node.getChildren() == 2);
      auto lhs = this->expression(node.getChild(0));
      if (lhs.hasFlowControl()) {
        return lhs;
      }
      auto& pnode = node.getChild(1);
      String pname;
      if (pnode.getOpcode() == Opcode::IDENTIFIER) {
        pname = this->identifier(pnode);
      } else {
        auto rhs = this->expression(pnode);
        if (rhs.hasFlowControl()) {
          return rhs;
        }
        if (!rhs->getString(pname)) {
          return this->raiseNode(node, "Expected property name to be a 'string', but got '", rhs->getRuntimeType().toString(), "' instead");
        }
      }
      Object object;
      if (lhs->getObject(object)) {
        auto value = object->getProperty(*this, pname);
        if (value->getVoid()) {
          return this->raiseNode(node, "Object does not have a property named '", pname, "'");
        }
        return value;
      }
      String string;
      if (lhs->getString(string)) {
        return this->stringProperty(string, pname);
      }
      return this->raiseNode(node, "Values of type '", lhs->getRuntimeType().toString(), "' do not support properties such as '.", pname, "'");
    }
    // Strings
    Value stringForeach(const String& string, Block& block, Target& target, const INode& statements) {
      // Iterate around the codepoints of the string
      auto* p = string.get();
      if (p != nullptr) {
        UTF8 reader(p->begin(), p->end(), 0);
        char32_t codepoint;
        while (reader.forward(codepoint)) {
          auto retval = target.assign(ValueFactory::createString(this->allocator, StringFactory::fromCodePoint(this->allocator, codepoint)));
          if (retval.hasFlowControl()) {
            return retval;
          }
          retval = this->executeBlock(block, statements);
          if (retval.hasFlowControl()) {
            return retval;
          }
        }
      }
      return Value::Void;
    }
    Value stringIndex(const String& string, const Value& index) {
      Int i;
      if (!index->getInt(i)) {
        return this->raiseFormat("String indexing '[]' only supports indices of type 'int', but got '", index->getRuntimeType().toString(), "' instead");
      }
      auto c = string.codePointAt(size_t(i));
      if (c < 0) {
        // Invalid index
        auto n = string.length();
        if ((i < 0) || (size_t(i) >= n)) {
          return this->raiseFormat("String index ", i, " is out of range for a string of length ", n);
        }
        return this->raiseFormat("Cannot index a malformed string");
      }
      return ValueFactory::createString(this->allocator, StringFactory::fromCodePoint(this->allocator, char32_t(c)));
    }
    Value stringProperty(const String& string, const String& property) {
      auto retval = ValueFactory::createBuiltinStringProperty(this->allocator, string, property);
      if (retval->getVoid()) {
        return this->raiseFormat("Unknown property for 'string' value: '.", property, "'");
      }
      return retval;
    }
    // Operators
    Value operatorCompare(const INode& node, Value& lhs, Value& rhs) {
      // Returns 'break' for unknown operator
      assert(node.getOpcode() == Opcode::COMPARE);
      assert(node.getChildren() == 2);
      auto oper = node.getOperator();
      assert(OperatorProperties::from(oper).opclass == Opclass::COMPARE);
      EGG_WARNING_SUPPRESS_SWITCH_BEGIN();
      switch (oper) {
      case Operator::EQ:
        // (a == b) === (a == b)
        return this->operatorEquals(node.getChild(0), node.getChild(1), lhs, rhs, false);
      case Operator::GE:
        // (a >= b) === !(a < b)
        return this->operatorLessThan(node.getChild(0), node.getChild(1), lhs, rhs, true, oper);
      case Operator::GT:
        // (a > b) === (b < a)
        return this->operatorLessThan(node.getChild(1), node.getChild(0), rhs, lhs, false, oper);
      case Operator::LE:
        // (a <= b) === !(b < a)
        return this->operatorLessThan(node.getChild(1), node.getChild(0), rhs, lhs, true, oper);
      case Operator::LT:
        // (a < b) === (a < b)
        return this->operatorLessThan(node.getChild(0), node.getChild(1), lhs, rhs, false, oper);
      case Operator::NE:
        // (a != b) === !(a == b)
        return this->operatorEquals(node.getChild(0), node.getChild(1), lhs, rhs, true);
      }
      EGG_WARNING_SUPPRESS_SWITCH_END();
      return Value::Break;
    }
    Value operatorEquals(const INode& a, const INode& b, Value& va, Value& vb, bool invert) {
      // Care with IEEE NaNs
      va = this->expression(a);
      if (va.hasFlowControl()) {
        return va;
      }
      vb = this->expression(b);
      if (vb.hasFlowControl()) {
        return vb;
      }
      Float value;
      bool result;
      if (va->getFloat(value) && std::isnan(value)) {
        // An IEEE NaN is not equal to anything, even other NaNs
        result = invert;
      } else if (vb->getFloat(value) && std::isnan(value)) {
        // An IEEE NaN is not equal to anything, even other NaNs
        result = invert;
      } else {
        result = Value::equals(va, vb, ValueCompare::PromoteInts) ^ invert;
      }
      return ValueFactory::create(this->allocator, result);
    }
    Value operatorLessThan(const INode& a, const INode& b, Value& va, Value& vb, bool invert, Operator oper) {
      // Care with IEEE NaNs
      va = this->expression(a);
      if (va.hasFlowControl()) {
        return va;
      }
      vb = this->expression(b);
      if (vb.hasFlowControl()) {
        return vb;
      }
      Float fa;
      if (va->getFloat(fa)) {
        if (std::isnan(fa)) {
          // An IEEE NaN does not compare with anything, even other NaNs
          return Value::False;
        }
        Float fb;
        if (vb->getFloat(fb)) {
          // Comparing float with float
          if (std::isnan(fb)) {
            // An IEEE NaN does not compare with anything, even other NaNs
            return Value::False;
          }
          bool result = (fa < fb) ^ invert; // need to force bool-ness
          return ValueFactory::create(this->allocator, result);
        }
        Int ib;
        if (vb->getInt(ib)) {
          // Comparing float with int
          bool result = (fa < Float(ib)) ^ invert; // need to force bool-ness
          return ValueFactory::create(this->allocator, result);
        }
        auto side = ((oper == Operator::GT) || (oper == Operator::LE)) ? "left" : "right";
        auto sign = OperatorProperties::str(oper);
        return this->raiseNode(b, "Expected ", side, "-hand side of comparison '", sign, "' to be an 'int' or 'float', but got '", vb->getRuntimeType().toString(), "' instead");
      }
      Int ia;
      if (va->getInt(ia)) {
        Float fb;
        if (vb->getFloat(fb)) {
          // Comparing int with float
          if (std::isnan(fb)) {
            // An IEEE NaN does not compare with anything
            return Value::False;
          }
          bool result = (Float(ia) < fb) ^ invert; // need to force bool-ness
          return ValueFactory::create(this->allocator, result);
        }
        Int ib;
        if (vb->getInt(ib)) {
          // Comparing int with int
          bool result = (ia < ib) ^ invert; // need to force bool-ness
          return ValueFactory::create(this->allocator, result);
        }
        auto side = ((oper == Operator::GT) || (oper == Operator::LE)) ? "left" : "right";
        auto sign = OperatorProperties::str(oper);
        return this->raiseNode(b, "Expected ", side, "-hand side of comparison '", sign, "' to be an 'int' or 'float', but got '", vb->getRuntimeType().toString(), "' instead");
      }
      auto side = ((oper == Operator::GT) || (oper == Operator::LE)) ? "right" : "left";
      auto sign = OperatorProperties::str(oper);
      return this->raiseNode(a, "Expected ", side, "-hand side of comparison '", sign, "' to be an 'int' or 'float', but got '", va->getRuntimeType().toString(), "' instead");
    }
    Value operatorArithmetic(const INode& node, Operator oper, const INode& a, const INode& b) {
      auto lhs = this->expression(a);
      if (lhs.hasFlowControl()) {
        return lhs;
      }
      auto rhs = this->expression(b);
      if (rhs.hasFlowControl()) {
        return rhs;
      }
      Float flhs;
      if (lhs->getFloat(flhs)) {
        Float frhs;
        if (rhs->getFloat(frhs)) {
          // Both floats
          return ValueFactory::createFloat(this->allocator, this->operatorBinaryFloat(node, oper, flhs, frhs));
        }
        Int irhs;
        if (rhs->getInt(irhs)) {
          // Promote rhs
          return ValueFactory::createFloat(this->allocator, this->operatorBinaryFloat(node, oper, flhs, Float(irhs)));
        }
        return this->raiseNode(b, "Expected right-hand side of arithmetic '" + OperatorProperties::str(oper), "' operator to be an 'int' or 'float', but got '", rhs->getRuntimeType().toString(), "' instead");
      }
      Int ilhs;
      if (lhs->getInt(ilhs)) {
        Float frhs;
        if (rhs->getFloat(frhs)) {
          // Promote lhs
          return ValueFactory::createFloat(this->allocator, this->operatorBinaryFloat(node, oper, Float(ilhs), frhs));
        }
        Int irhs;
        if (rhs->getInt(irhs)) {
          // Both ints
          return ValueFactory::createInt(this->allocator, this->operatorBinaryInt(node, oper, ilhs, irhs));
        }
        return this->raiseNode(b, "Expected right-hand side of arithmetic '" + OperatorProperties::str(oper), "' operator to be an 'int' or 'float', but got '", rhs->getRuntimeType().toString(), "' instead");
      }
      return this->raiseNode(b, "Expected left-hand side of arithmetic '" + OperatorProperties::str(oper), "' operator to be an 'int' or 'float', but got '", rhs->getRuntimeType().toString(), "' instead");
    }
    Value operatorLogicalNot(const INode& a) {
      auto value = this->expression(a);
      if (value.hasFlowControl()) {
        return value;
      }
      Bool logical;
      if (!value->getBool(logical)) {
        return this->raiseNode(a, "Expected operand of unary '!' operator to be a 'bool', but got '", value->getRuntimeType().toString(), "' instead");
      }
      return ValueFactory::create(this->allocator, !logical);
    }
    Value operatorDeref(const INode& a) {
      auto value = this->expression(a);
      if (value.hasFlowControl()) {
        return value;
      }
      Value pointee;
      if (!value.hasAnyFlags(ValueFlags::Pointer) || !value->getChild(pointee)) {
        return this->raiseNode(a, "Expected operand of unary '*' operator to be a pointer, but got '", value->getRuntimeType().toString(), "' instead");
      }
      return pointee;
    }
    Value operatorRef(const INode& a) {
      if (a.getOpcode() != Opcode::IDENTIFIER) {
        return this->raiseNode(a, "Expected operand of unary '&' operator to be a variable identifier");
      }
      auto name = this->identifier(a);
      auto* symbol = this->symtable->get(name);
      if (symbol == nullptr) {
        return this->raiseNode(a, "Unknown identifier for unary '&' operator: '", name, "'");
      }
      return this->raiseNode(a, "WIBBLE Symbol references not yet supported: '", name, "'");
      // WIBBLE return ValueFactory::createPointer(this->allocator, symbol->value);
    }
    Int operatorBinaryInt(const INode& node, Operator oper, Int lhs, Int rhs) {
      // WIBBLE return Value and handle div-by-zero etc more elegantly
      EGG_WARNING_SUPPRESS_SWITCH_BEGIN();
      switch (oper) {
      case Operator::ADD:
        return lhs + rhs;
      case Operator::SUB:
        return lhs - rhs;
      case Operator::MUL:
        return lhs * rhs;
      case Operator::DIV:
        return lhs / rhs;
      case Operator::REM:
        return lhs % rhs;
      }
      EGG_WARNING_SUPPRESS_SWITCH_END();
      throw this->unexpectedOperator("binary int", node);
    }
    Float operatorBinaryFloat(const INode& node, Operator oper, Float lhs, Float rhs) {
      EGG_WARNING_SUPPRESS_SWITCH_BEGIN();
      switch (oper) {
      case Operator::ADD:
        return lhs + rhs;
      case Operator::SUB:
        return lhs - rhs;
      case Operator::MUL:
        return lhs * rhs;
      case Operator::DIV:
        return lhs / rhs;
      case Operator::REM:
        return std::remainder(lhs, rhs);
      }
      EGG_WARNING_SUPPRESS_SWITCH_END();
      throw this->unexpectedOperator("binary float", node);
    }
    Value operatorTernary(const INode& a, const INode& b, const INode& c) {
      auto cond = this->condition(a, nullptr);
      Bool condition;
      if (!cond->getBool(condition)) {
        return cond;
      }
      return condition ? this->expression(b) : this->expression(c);
    }
    // Implementation
    Value condition(const INode& node, Block* block) {
      auto value = this->expression(node, block);
      if (!value.hasAnyFlags(ValueFlags::FlowControl | ValueFlags::Bool)) {
        return this->raiseNode(node, "Expected condition to evaluate to a 'bool' value, but got '", value->getRuntimeType().toString(), "' instead");
      }
      return value;
    }
    // Error handling
    template<typename... ARGS>
    Value raiseNode(const INode& node, ARGS&&... args) {
      this->updateLocation(node);
      return this->raiseLocation(this->location, std::forward<ARGS>(args)...);
    }
    void updateLocation(const INode& node) {
      auto* source = node.getLocation();
      if (source != nullptr) {
        this->location.line = source->line;
        this->location.column = source->column;
      }
    }
    Opcode validateOpcode(const INode& node) const {
      auto opcode = node.getOpcode();
      auto& properties = OpcodeProperties::from(opcode);
      if (!properties.validate(node.getChildren(), node.getOperand() != INode::Operand::None)) {
        assert(properties.name != nullptr);
        throw RuntimeException(this->location, "Corrupt opcode: '", properties.name, "'");
      }
      return opcode;
    }
  public:
    String identifier(const INode& node) {
      if (node.getOpcode() != Opcode::IDENTIFIER) {
        throw this->unexpectedOpcode("identifier", node);
      }
      auto& child = node.getChild(0);
      if (child.getOpcode() != Opcode::SVALUE) {
        throw this->unexpectedOpcode("svalue", child);
      }
      return child.getString();
    }
    Type type(const INode& node, const String& name = String()) {
      auto children = node.getChildren();
      EGG_WARNING_SUPPRESS_SWITCH_BEGIN();
      switch (node.getOpcode()) {
      case Opcode::INFERRED:
        assert(children == 0);
        return nullptr;
      case Opcode::VOID:
        assert(children == 0);
        return Type::Void;
      case Opcode::NULL_:
        assert(children == 0);
        return Type::Null;
      case Opcode::BOOL:
        if (children == 0) {
          return Type::Bool;
        }
        break;
      case Opcode::INT:
        if (children == 0) {
          return Type::Int;
        }
        break;
      case Opcode::FLOAT:
        if (children == 0) {
          return Type::Float;
        }
        break;
      case Opcode::STRING:
        if (children == 0) {
          return Type::String;
        }
        break;
      case Opcode::OBJECT:
        if (children == 0) {
          return Type::Object;
        }
        if (children == 1) {
          auto& callable = node.getChild(0);
          if (callable.getOpcode() == Opcode::CALLABLE) {
            return UserFunction::makeType(this->allocator, *this, name, callable);
          }
        }
        break;
      case Opcode::ANY:
        if (children == 0) {
          return Type::Any;
        }
        break;
      case Opcode::ANYQ:
        if (children == 0) {
          return Type::AnyQ;
        }
        break;
      case Opcode::POINTER:
        if (children == 1) {
          auto pointee = this->type(node.getChild(0));
          return TypeFactory::createPointer(this->allocator, *pointee);
        }
        break;
      case Opcode::UNION:
        if (children >= 1) {
          auto result = this->type(node.getChild(0));
          for (size_t i = 1; i < children; ++i) {
            result = TypeFactory::createUnion(this->allocator, *result, *this->type(node.getChild(i)));
          }
          return result;
        }
        return Type::Void;
      default:
        throw this->unexpectedOpcode("type", node);
      }
      EGG_WARNING_SUPPRESS_SWITCH_END();
      this->updateLocation(node);
      throw RuntimeException(this->location, "Type constraints not yet supported"); // TODO
    }
    Value tryAssign(Symbol& symbol, const Value& value);
    Symbol* blockDeclare(const LocationSource& source, const Type& type, const String& identifier);
    void blockUndeclare(const String& identifier);
    RuntimeException unexpectedOpcode(const char* expected, const INode& node) {
      this->updateLocation(node);
      auto opcode = node.getOpcode();
      auto name = OpcodeProperties::from(opcode).name;
      if (name == nullptr) {
        return RuntimeException(this->location, "Unknown ", expected, " opcode: '<", std::to_string(int(opcode)), ">'");
      }
      return RuntimeException(this->location, "Unexpected ", expected, " opcode: '", name, "'");
    }
    RuntimeException unexpectedOperator(const char* expected, const INode& node) {
      this->updateLocation(node);
      auto oper = node.getOperator();
      auto name = OperatorProperties::from(oper).name;
      if (name == nullptr) {
        return RuntimeException(this->location, "Unknown ", expected, " operator: '<", std::to_string(int(oper)), ">'");
      }
      return RuntimeException(this->location, "Unexpected ", expected, " operator: '", name, "'");
    }
    template<typename... ARGS>
    Value raiseLocation(const LocationSource& where, ARGS&&... args) const {
      auto message = StringBuilder::concat(std::forward<ARGS>(args)...);
      return ValueFactory::createThrowError(this->allocator, where, message);
    }
  };
}

Target::Target(ProgramDefault& program, const INode& node, Block*)
  : program(program), node(node) {
  // WIBBLE WOBBLE
  throw program.unexpectedOpcode("target", node);
}

Value Target::check() const {
  return this->failure;
}

Value Target::assign(const Value&) const {
  return this->raise("WIBBLE: Target::assign not implemented");
}

Value Target::nudge(Int) const {
  return this->raise("WIBBLE: Target::nudge not implemented");
}

Value Target::mutate(const INode&, const INode&) const {
  return this->raise("WIBBLE: Target::mutate not implemented");
}

#if defined(WIBBLE)
bool Target::ref(Value&) const {
  // WIBBLE
  return false;
}

Value Target::get() const {
  return this->raise("WIBBLE: Target::get not implemented");
}

Value Target::set(const Value&) const {
  return this->raise("WIBBLE: Target::set not implemented");
}

Value Target::apply(const INode&, Value&, const INode&) const {
  return this->raise("WIBBLE: Target::apply not implemented");
}
#endif

Value Target::raise(const String& message) const {
  LocationSource location{ "<unknown>", 0, 0 }; // TODO
  return ValueFactory::createThrowError(this->program.getAllocator(), location, message);
}

Value PredicateFunction::call(IExecution&, const IParameters& parameters) {
  assert(parameters.getNamedCount() == 0);
  assert(parameters.getPositionalCount() == 0);
  (void)parameters; // ignore the parameters
  // WIBBLE return this->program.executePredicate(this->location, *this->node);
  return this->program.raiseLocation(this->location, "WIBBLE: PredicateFunction::call not yet implemented");
}

Value UserFunction::call(IExecution&, const IParameters&) {
  auto signature = this->type->callable();
  assert(signature != nullptr);
  (void)signature;
  // WIBBLE assert(this->captured != nullptr);
  // WIBBLE return this->program.executeCall(this->location, *signature, parameters, *this->block, *this->captured);
  return this->program.raiseLocation(this->location, "WIBBLE: UserFunction::call not yet implemented");
}

Block::~Block() {
  // Undeclare all the names successfully declared by this block
  for (auto& i : this->declared) {
    this->program.blockUndeclare(i);
  }
}

Value ProgramDefault::tryAssign(Symbol& symbol, const Value& value) {
  String failure;
  if (!symbol.type->tryAssign(this->allocator, symbol.slot, value, failure)) {
    return this->raise(failure);
  }
  return Value::Void;
}

Symbol* ProgramDefault::blockDeclare(const LocationSource& source, const Type& type, const String& identifier) {
  auto result = this->symtable->add(type, identifier, source);
  if (result.first) {
    return result.second;
  }
  return nullptr;
}

void ProgramDefault::blockUndeclare(const String& identifier) {
  this->symtable->remove(identifier);
}

egg::ovum::Value Block::declare(const LocationSource& source, const Type& type, const String& name, const Value* init) {
  // Keep track of names successfully declared in this block
  auto* symbol = this->program.blockDeclare(source, type, name);
  if (symbol == nullptr) {
    return this->program.raiseLocation(source, "Duplicate name in declaration: '", name, "'");
  }
  if (init != nullptr) {
    auto retval = this->program.tryAssign(*symbol, *init);
    if (retval.hasFlowControl()) {
      this->program.blockUndeclare(name);
      return retval;
    }
  }
  this->declared.insert(name);
  return Value::Void;
}

egg::ovum::Value Block::guard(const LocationSource& source, const Type& type, const String& name, const Value& init) {
  // Keep track of names successfully declared in this block
  auto* symbol = this->program.blockDeclare(source, type, name);
  if (symbol == nullptr) {
    return this->program.raiseLocation(source, "Duplicate name in declaration: '", name, "'");
  }
  auto retval = this->program.tryAssign(*symbol, init);
  if (retval.hasFlowControl()) {
    this->program.blockUndeclare(name);
    return Value::False;
  }
  this->declared.insert(name);
  return Value::True;
}

egg::ovum::Type UserFunction::makeType(IAllocator& allocator, ProgramDefault& program, const String& name, const INode& callable) {
  // Create a type appropriate for a standard "user" function
  assert(callable.getOpcode() == Opcode::CALLABLE);
  auto n = callable.getChildren();
  assert(n >= 1);
  auto rettype = program.type(callable.getChild(0));
  assert(rettype != nullptr);
  HardPtr<ITypeFunction> function{ TypeFactory::createFunction(allocator, name, rettype) };
  for (size_t i = 1; i < n; ++i) {
    auto& parameter = callable.getChild(i);
    auto c = parameter.getChildren();
    size_t pindex = i - 1;
    IFunctionSignatureParameter::Flags pflags;
    EGG_WARNING_SUPPRESS_SWITCH_BEGIN();
    switch (parameter.getOpcode()) {
    case Opcode::REQUIRED:
      assert((c == 1) || (c == 2));
      pflags = IFunctionSignatureParameter::Flags::Required;
      break;
    case Opcode::OPTIONAL:
      assert((c == 1) || (c == 2));
      pflags = IFunctionSignatureParameter::Flags::None;
      break;
    case Opcode::VARARGS:
      assert(c >= 2);
      pflags = IFunctionSignatureParameter::Flags::Variadic;
      if (c > 2) {
        // TODO We assume the constraint is "length > 0" meaning required
        pflags = Bits::set(pflags, IFunctionSignatureParameter::Flags::Required);
      }
      break;
    case Opcode::BYNAME:
      assert(c == 2);
      pindex = SIZE_MAX;
      pflags = IFunctionSignatureParameter::Flags::None;
      break;
    default:
      throw program.unexpectedOpcode("function type parameter", parameter);
    }
    EGG_WARNING_SUPPRESS_SWITCH_END();
    String pname;
    if (c > 1) {
      pname = program.identifier(parameter.getChild(1));
    }
    auto ptype = program.type(parameter.getChild(0), pname);
    function->addParameter(pname, ptype, pflags, pindex);
  }
  return Type(function.get());
}

egg::ovum::Program egg::ovum::ProgramFactory::createProgram(IAllocator& allocator, ILogger& logger) {
  auto basket = BasketFactory::createBasket(allocator);
  auto program = allocator.make<ProgramDefault>(*basket, logger);
  program->addBuiltins();
  return program;
}
