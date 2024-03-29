#include "ovum/ovum.h"
#include "ovum/slot.h"
#include "ovum/node.h"
#include "ovum/module.h"
#include "ovum/program.h"
#include "ovum/utf.h"
#include "ovum/builtin.h"
#include "ovum/function.h"
#include "ovum/vanilla.h"
#include "ovum/forge.h"

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
      location.printSource(sb);
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
    Value declareType(const LocationSource& source, const String& name, const Type& type);
    Value declareVariable(const LocationSource& source, const Type& type, const String& name, const Value* init = nullptr);
    Value guardVariable(const LocationSource& source, const Type& type, const String& name, const Value& init);
  };

  class Symbol {
    Symbol(Symbol&& rhs) noexcept = delete;
    Symbol(const Symbol&) = delete;
    Symbol& operator=(const Symbol&) = delete;
  public:
    enum class Kind {
      Builtin,
      Type,
      Variable
    };
  private:
    LocationSource source;
    Kind kind;
    Type type;
    String name;
    SoftPtr<Slot> slot;
  public:
    Symbol(IBasket& basket, const LocationSource& source, Kind kind, const IType& type, const String& name, Slot& slot)
      : source(source),
        kind(kind),
        type(&type),
        name(name),
        slot(basket, &slot) {
      assert(this->validate(true));
    }
    Type getType() const {
      return this->type;
    }
    Kind getKind() const {
      return this->kind;
    }
    const String& getName() const {
      return this->name;
    }
    HardPtr<Slot> getSlot() const {
      return HardPtr<Slot>(this->slot.get());
    }
    Value getValue() const {
      return this->slot->value(Value::Break);
    }
    void assign(const Value& value) {
      auto* target = this->slot.get();
      assert(target != nullptr);
      auto* basket = target->softGetBasket();
      assert(basket != nullptr);
      Object object;
      if (value->getObject(object)) {
        basket->take(*object);
      }
      target->set(value);
    }
    void clone(IBasket& basket, std::map<String, Symbol>& table) {
      table.emplace(std::piecewise_construct, std::forward_as_tuple(this->name),
                                              std::forward_as_tuple(basket, this->source, this->kind, *this->type, this->name, *this->slot));
    }
    void visit(const ICollectable::Visitor& visitor) const {
      this->slot.visit(visitor);
    }
    bool validate(bool optional) const {
      return (this->type != nullptr) && !this->name.empty() && (slot != nullptr) && slot->validate(optional);
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
    SymbolTable(IAllocator& allocator, IBasket& basket, SymbolTable* parent, SymbolTable* capture)
      : SoftReferenceCounted(allocator) {
      this->parent.set(basket, parent);
      this->capture.set(basket, capture);
      basket.take(*this);
    }
    Symbol* add(const LocationSource& source, Symbol::Kind kind, const Type& type, const String& name) {
      // Return non-null iff the insertion occurred
      auto slot = this->allocator.makeHard<Slot>(*this->basket);
      auto retval = this->table.emplace(std::piecewise_construct,
                                        std::forward_as_tuple(name),
                                        std::forward_as_tuple(*this->basket, source, kind, *type, name, *slot));
      return retval.second ? &retval.first->second : nullptr;
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
    void softVisit(const Visitor& visitor) const override {
      // Visit all elements in this symbol table
      for (auto& each : this->table) {
        each.second.visit(visitor);
      }
      this->parent.visit(visitor);
      this->capture.visit(visitor);
    }
    SymbolTable* push(SymbolTable* captured = nullptr) {
      // Push a table onto the symbol table stack
      return this->allocator.create<SymbolTable>(0, this->allocator, *this->basket, this, captured);
    }
    SymbolTable* pop() {
      // Pop an element from the symbol table stack
      assert(this->parent != nullptr);
      return this->parent.get();
    }
    HardPtr<SymbolTable> clone() {
      // Shallow clone the symbol table
      auto cloned = this->allocator.makeHard<SymbolTable>(*this->basket, nullptr, nullptr);
      assert(cloned->softGetBasket() == this->basket);
      for (auto& each : this->table) {
        each.second.clone(*this->basket, cloned->table);
      }
      return cloned;
    }
    virtual void print(Printer& printer) const override {
      printer.stream() << "WIBBLE" << std::endl;
    }
  };

  struct Target {
    enum class Flavour { Identifier, Property, Index, Pointer } flavour = Flavour::Identifier;
    Type type;
    String identifier;
    Object object;
    Value index;
  };

  class UserFunction : public SoftReferenceCounted<IObject> {
    UserFunction(const UserFunction&) = delete;
    UserFunction& operator=(const UserFunction&) = delete;
  private:
    ProgramDefault& program; // ProgramDefault lifetime guaranteed to be longer than UserFunction instance
    LocationSource location;
    Type type;
    const INode* block;
    SoftPtr<SymbolTable> captured;
  public:
    UserFunction(IAllocator& allocator, ProgramDefault& program, const LocationSource& location, const Type& ftype, const INode& block)
      : SoftReferenceCounted(allocator),
      program(program),
      location(location),
      type(),
      block(&block) {
      assert(block.getOpcode() == Opcode::BLOCK);
      this->initialize(ftype);
      assert(type != nullptr);
    }
    void setCaptured(const HardPtr<SymbolTable>& symtable) {
      assert(this->captured == nullptr);
      this->captured.set(*this->basket, symtable.get());
      assert(this->captured != nullptr);
    }
    virtual void softVisit(const Visitor& visitor) const override {
      this->captured.visit(visitor);
    }
    virtual void print(Printer& printer) const override {
      printer << "<user-function>";
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
    virtual Value mutProperty(IExecution& execution, const String& property, Mutation, const Value&) override {
      return this->raise(execution, "does not support properties such as '", property, "'");
    }
    virtual Value delProperty(IExecution& execution, const String& property) override {
      return this->raise(execution, "does not support properties such as '", property, "'");
    }
    virtual Value refProperty(IExecution& execution, const String& property) override {
      return this->raise(execution, "does not support properties such as '", property, "'");
    }
    virtual Value getIndex(IExecution& execution, const Value&) override {
      return this->raise(execution, "does not support indexing");
    }
    virtual Value setIndex(IExecution& execution, const Value&, const Value&) override {
      return this->raise(execution, "does not support indexing");
    }
    virtual Value mutIndex(IExecution& execution, const Value&, Mutation, const Value&) override {
      return this->raise(execution, "does not support indexing");
    }
    virtual Value delIndex(IExecution& execution, const Value&) override {
      return this->raise(execution, "does not support indexing");
    }
    virtual Value refIndex(IExecution& execution, const Value&) override {
      return this->raise(execution, "does not support indexing");
    }
    virtual Value getPointee(IExecution& execution) override {
      return this->raise(execution, "does not support pointer dereferencing");
    }
    virtual Value setPointee(IExecution& execution, const Value&) override {
      return this->raise(execution, "does not support pointer dereferencing");
    }
    virtual Value mutPointee(IExecution& execution, Mutation, const Value&) override {
      return this->raise(execution, "does not support pointer dereferencing");
    }
    virtual Value iterate(IExecution& execution) override {
      return this->raise(execution, "does not support iteration");
    }
    template<typename... ARGS>
    Value raise(IExecution& execution, ARGS&&... args) {
      auto* signature = this->getFunctionSignature();
      if (signature != nullptr) {
        auto name = signature->getName();
        if (!name.empty()) {
          return execution.raiseFormat("Function '", name, "' ", std::forward<ARGS>(args)...);
        }
      }
      return execution.raiseFormat("Function ", std::forward<ARGS>(args)...);
    }
    static Type makeType(ITypeFactory& factory, ProgramDefault& program, const String& name, const INode& callable);
  private:
    void initialize(const Type& ftype);
    const IFunctionSignature* getFunctionSignature() const {
      assert(this->type->getObjectShapeCount() == 1);
      auto* shape = this->type->getObjectShape(0);
      assert(shape != nullptr);
      return shape->callable;
    }
  };

  class CallStack final {
    CallStack(const CallStack&) = delete;
    CallStack& operator=(const CallStack&) = delete;
  private:
    HardPtr<SymbolTable>& symtable;
  public:
    CallStack(HardPtr<SymbolTable>& symtable, SymbolTable* capture) : symtable(symtable) {
      // Push an element onto the symbol table stack
      this->symtable.set(this->symtable->push(capture));
      assert(this->symtable != nullptr);
    }
    ~CallStack() {
      // Pop an element from the symbol table stack
      this->symtable.set(this->symtable->pop());
      assert(this->symtable != nullptr);
    }
  };

  class ProgramDefault final : public HardReferenceCounted<IProgram>, public IExecution, public IVanillaPredicateCallback {
    ProgramDefault(const ProgramDefault&) = delete;
    ProgramDefault& operator=(const ProgramDefault&) = delete;
  private:
    ILogger& logger;
    ILogger::Severity maxseverity;
    TypeFactory factory;
    Basket basket;
    HardPtr<SymbolTable> symtable;
    LocationSource location;
  public:
    ProgramDefault(IAllocator& allocator, ILogger& logger)
      : HardReferenceCounted(allocator, 0),
        logger(logger),
        maxseverity(ILogger::Severity::None),
        factory(allocator),
        basket(egg::ovum::BasketFactory::createBasket(allocator)),
        symtable(factory.getAllocator().makeHard<SymbolTable>(*basket, nullptr, nullptr)),
        location({}, 0, 0) {
    }
    virtual ~ProgramDefault() {
      this->symtable.set(nullptr);
      this->basket->collect();
      // Check for incomplete collection (usually an indication that soft pointer tracing is incorrect)
      assert(this->basket->collect() == 0);
    }
    virtual bool builtin(const String& name, const Value& value) override {
      static const LocationSource source("<builtin>", 0, 0);
      auto symbol = this->symtable->add(source, Symbol::Kind::Builtin, value->getRuntimeType(), name);
      if (symbol != nullptr) {
        // Don't use assignment for builtins; it always fails
        symbol->assign(value);
        return true;
      }
      return false;
    }
    virtual Value run(const IModule& module, ILogger::Severity* severity) override {
      Value result{ Value::Rethrow };
      try {
        this->location.file = module.getResourceName();
        this->location.line = 0;
        this->location.column = 0;
        result = this->executeRoot(module.getRootNode());
      } catch (RuntimeException& exception) {
        this->log(ILogger::Source::Runtime, ILogger::Severity::Error, exception.message.toUTF8());
      }
      if (severity != nullptr) {
        *severity = this->maxseverity;
      }
      return result;
    }
    virtual IAllocator& getAllocator() const override {
      return this->factory.getAllocator();
    }
    virtual IBasket& getBasket() const override {
      return *this->basket;
    }
    virtual ITypeFactory& getTypeFactory() override {
      return this->factory;
    }
    virtual Value raise(const String& message) override {
      return this->raiseError(this->location, message);
    }
    virtual Value assertion(const Value& predicate) override {
      Object object;
      if (predicate->getObject(object)) {
        // Predicates can be functions that throw exceptions, as well as 'bool' values
        auto type = object->getRuntimeType();
        if (this->factory.queryCallable(type) != nullptr) {
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
      this->log(ILogger::Source::User, ILogger::Severity::None, utf8);
    }
    Value executeCall(const LocationSource& source, const IFunctionSignature& signature, const IParameters& runtime, const INode& block, SymbolTable& captured) {
      // We have to be careful to get the location correct
      assert(block.getOpcode() == Opcode::BLOCK);
      if (runtime.getNamedCount() > 0) {
        return this->raiseFormat(FunctionSignature::toString(signature), ": Named parameters are not yet supported"); // TODO
      }
      auto maxPositional = signature.getParameterCount();
      auto minPositional = maxPositional;
      while ((minPositional > 0) && !Bits::hasAnySet(signature.getParameter(minPositional - 1).getFlags(), IFunctionSignatureParameter::Flags::Required)) {
        minPositional--;
      }
      auto numPositional = runtime.getPositionalCount();
      if (numPositional < minPositional) {
        if (minPositional == 1) {
          return this->raiseFormat(FunctionSignature::toString(signature), ": At least 1 parameter was expected");
        }
        return this->raiseFormat(FunctionSignature::toString(signature), ": At least ", minPositional, " parameters were expected, but got ", numPositional);
      }
      if ((maxPositional > 0) && Bits::hasAnySet(signature.getParameter(maxPositional - 1).getFlags(), IFunctionSignatureParameter::Flags::Variadic)) {
        // TODO Variadic
        maxPositional = numPositional;
      } else if (numPositional > maxPositional) {
        // Not variadic
        if (maxPositional == 1) {
          return this->raiseFormat(FunctionSignature::toString(signature), ": Only 1 parameter was expected, not ", numPositional);
        }
        return this->raiseFormat(FunctionSignature::toString(signature), ": No more than ", maxPositional, " parameters were expected, but got ", numPositional);
      }
      Value retval;
      {
        // This scope pushes/pops a symbol table context
        // TODO rationalize CallStack/Block usage
        CallStack stack(this->symtable, &captured);
        Block scope(*this);
        for (size_t i = 0; i < maxPositional; ++i) {
          auto& sigparam = signature.getParameter(i);
          auto pname = sigparam.getName();
          assert(!pname.empty());
          auto position = sigparam.getPosition();
          assert(position < numPositional);
          auto ptype = sigparam.getType();
          auto pvalue = runtime.getPositional(position);
          auto guarded = scope.guardVariable(source, ptype, pname, pvalue);
          Bool matched;
          if (!guarded->getBool(matched)) {
            return guarded;
          }
          if (!matched) {
            // Type mismatch on parameter
            auto* plocation = runtime.getPositionalLocation(position);
            if (plocation != nullptr) {
              // Update our current source location (it will be restored when expressionCall returns)
              this->location = *plocation;
            }
            return this->raiseFormat("Type mismatch for parameter '", pname, "': Expected '", ptype.toString(), "', but got '", pvalue->getRuntimeType().toString(), "' instead");
          }
        }
        retval = this->executeBlock(scope, block);
      }
      if (retval.hasAnyFlags(ValueFlags::Return)) {
        if (retval->getInner(retval)) {
          // Simple 'return <value>'
          return retval;
        }
        return Value::Void;
      }
      if (retval.hasFlowControl()) {
        // Probably an exception
        return retval;
      }
      if (retval->getVoid()) {
        // Ran off the bottom of the function
        auto rettype = signature.getReturnType();
        assert(rettype != nullptr);
        if (rettype.hasPrimitiveFlag(ValueFlags::Void)) {
          // Implicit 'return;' at end of function
          return retval;
        }
      }
      throw RuntimeException(this->location, "Expected function to return, but got '", retval->getRuntimeType().toString(), "' instead");
    }
    virtual Value predicateCallback(const INode& node) override {
      // We have to be careful to get the location correct
      this->updateLocation(node);
      auto& source = this->location;
      assert(node.getOpcode() == Opcode::COMPARE);
      auto oper = node.getOperator();
      Value lhs, rhs;
      auto retval = this->operatorCompare(node, oper, lhs, rhs);
      if (retval.hasFlowControl()) {
        if (retval->getFlags() == ValueFlags::Break) {
          return this->raiseLocation(source, "Internal runtime error: Unsupported predicate comparison: '", OperatorProperties::str(oper), "'");
        }
        return retval;
      }
      bool passed;
      if (!retval->getBool(passed)) {
        return this->raiseLocation(source, "Internal runtime error: Expected predicate to return a 'bool'");
      }
      if (passed) {
        // The assertion has passed
        return Value::Void;
      }
      auto str = OperatorProperties::str(node.getOperator());
      auto message = StringBuilder::concat("Assertion is untrue: ", lhs.readable(), ' ', str, ' ', rhs.readable());
      auto object = VanillaFactory::createError(this->factory, *this->basket, source, message);
      (void)object->setProperty(*this, "left", lhs);
      (void)object->setProperty(*this, "operator", ValueFactory::createUTF8(this->allocator, str));
      (void)object->setProperty(*this, "right", rhs);
      auto value = ValueFactory::createObject(this->allocator, object);
      return ValueFactory::createFlowControl(this->allocator, ValueFlags::Throw, value);
    }
    // Builtins
    void addBuiltins() {
      this->builtin("assert", BuiltinFactory::createAssertInstance(this->factory, *this->basket));
      this->builtin("print", BuiltinFactory::createPrintInstance(this->factory, *this->basket));
      this->builtin("string", BuiltinFactory::createStringInstance(this->factory, *this->basket));
      this->builtin("type", BuiltinFactory::createTypeInstance(this->factory, *this->basket));
    }
  private:
    void log(ILogger::Source source, ILogger::Severity severity, const std::string& message) {
      this->maxseverity = std::max(this->maxseverity, severity);
      this->logger.log(source, severity, message);
    }
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
      EGG_WARNING_SUPPRESS_SWITCH_BEGIN
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
      case Opcode::TYPEDEF:
        return this->statementTypedef(block, node);
      case Opcode::WHILE:
        return this->statementWhile(node);
      }
      EGG_WARNING_SUPPRESS_SWITCH_END
      throw this->unexpectedOpcode("statement", node);
    }
    Value statementAssign(const INode& node) {
      assert(node.getOpcode() == Opcode::ASSIGN);
      assert(node.getChildren() == 2);
      Target target;
      auto lvalue = this->targetInit(target, node.getChild(0));
      if (lvalue.hasFlowControl()) {
        return lvalue;
      }
      auto rvalue = this->expression(node.getChild(1));
      if (rvalue.hasFlowControl()) {
        return rvalue;
      }
      return this->targetAssign(target, node, rvalue);
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
      StringBuilder sb;
      sb.add("Discarding call return value: ");
      retval->print(sb);
      this->log(ILogger::Source::Runtime, ILogger::Severity::Warning, sb.toUTF8());
      return Value::Void;
    }
    Value statementDeclare(Block& block, const INode& node) {
      assert(node.getOpcode() == Opcode::DECLARE);
      this->updateLocation(node);
      auto vname = this->identifier(node.getChild(1));
      auto vtype = this->type(node.getChild(0), vname);
      if (node.getChildren() == 2) {
        // No initializer
        return block.declareVariable(this->location, vtype, vname);
      }
      assert(node.getChildren() == 3);
      auto vinit = this->expression(node.getChild(2));
      if (vinit.hasFlowControl()) {
        return vinit;
      }
      return block.declareVariable(this->location, vtype, vname, &vinit);
    }
    Value statementDecrement(const INode& node) {
      assert(node.getOpcode() == Opcode::DECREMENT);
      assert(node.getChildren() == 1);
      Target target;
      auto value = this->targetInit(target, node.getChild(0));
      if (value.hasFlowControl()) {
        return value;
      }
      return this->targetMutate(target, node, Mutation::Decrement, Value::Void);
    }
    Value statementDo(const INode& node) {
      assert(node.getOpcode() == Opcode::DO);
      assert(node.getChildren() == 2);
      Value retval;
      auto condition = false;
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
      EGG_WARNING_SUPPRESS_SWITCH_BEGIN
      switch (cond->getOpcode()) {
      case Opcode::NOOP:
      case Opcode::TRUE:
        // No condition: 'for(a; ; c) {}' or 'for(a; true; c) {}'
        cond = nullptr;
        break;
      }
      EGG_WARNING_SUPPRESS_SWITCH_END
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
      Target target;
      auto value = this->targetInit(target, node.getChild(0), &inner);
      if (value.hasFlowControl()) {
        return value;
      }
      auto rvalue = this->expression(node.getChild(1));
      if (rvalue.hasFlowControl()) {
        return rvalue;
      }
      String string;
      if (rvalue->getString(string)) {
        // Iterate around the codepoints in the string
        return this->stringForeach(string, target, node.getChild(2), inner);
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
      Value retval;
      auto& block = node.getChild(2);
      for (;;) {
        retval = object->call(*this, Object::ParametersNone);
        if (retval.hasAnyFlags(ValueFlags::FlowControl | ValueFlags::Void)) {
          break;
        }
        retval = this->targetAssign(target, node, retval);
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
        return this->raise("Generators are not yet supported"); // TODO
      }
      this->updateLocation(node);
      auto function = this->allocator.makeHard<UserFunction>(*this, this->location, ftype, fblock);
      // We have to be careful to ensure the function is declared before capturing symbols so that recursion works correctly
      auto fvalue = ValueFactory::createObject(this->allocator, Object(*function));
      auto retval = block.declareVariable(this->location, ftype, fname, &fvalue);
      function->setCaptured(this->symtable->clone());
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
      Target target;
      auto value = this->targetInit(target, node.getChild(0));
      if (value.hasFlowControl()) {
        return value;
      }
      return this->targetMutate(target, node, Mutation::Increment, Value::Void);
    }
    Value statementMutate(const INode& node) {
      assert(node.getOpcode() == Opcode::MUTATE);
      assert(node.getChildren() == 2);
      assert(OperatorProperties::from(node.getOperator()).opclass == Opclass::BINARY);
      Target target;
      auto value = this->targetInit(target, node.getChild(0));
      if (value.hasFlowControl()) {
        return value;
      }
      Mutation mutation;
      bool logical;
      auto op = node.getOperator();
      EGG_WARNING_SUPPRESS_SWITCH_BEGIN
      switch (op) {
      case Operator::ADD:
        mutation = Mutation::Add;
        break;
      case Operator::BITAND:
        mutation = Mutation::BitwiseAnd;
        break;
      case Operator::BITOR:
        mutation = Mutation::BitwiseOr;
        break;
      case Operator::BITXOR:
        mutation = Mutation::BitwiseXor;
        break;
      case Operator::DIV:
        mutation = Mutation::Divide;
        break;
      case Operator::IFNULL:
        value = this->targetGet(node, target);
        if (value.hasFlowControl()) {
          return value;
        }
        if (!value->getNull()) {
          // Short-circuit
          return value;
        }
        mutation = Mutation::IfNull;
        break;
      case Operator::LOGAND:
        value = this->targetGet(node, target);
        if (value.hasFlowControl()) {
          return value;
        }
        if (value->getBool(logical) && !logical) {
          // Short-circuit
          return Value::False;
        }
        mutation = Mutation::LogicalAnd;
        break;
      case Operator::LOGOR:
        value = this->targetGet(node, target);
        if (value.hasFlowControl()) {
          return value;
        }
        if (value->getBool(logical) && logical) {
          // Short-circuit
          return Value::True;
        }
        mutation = Mutation::LogicalOr;
        break;
      case Operator::MUL:
        mutation = Mutation::Multiply;
        break;
      case Operator::REM:
        mutation = Mutation::Remainder;
        break;
      case Operator::SHIFTL:
        mutation = Mutation::ShiftLeft;
        break;
      case Operator::SHIFTR:
        mutation = Mutation::ShiftRight;
        break;
      case Operator::SHIFTU:
        mutation = Mutation::ShiftRightUnsigned;
        break;
      case Operator::SUB:
        mutation = Mutation::Subtract;
        break;
      default: {
        auto name = OperatorProperties::from(op).name;
        if (name == nullptr) {
          return this->raiseNode(node, "Unknown mutation operator: '<", int(op), ">'");
        }
        return this->raiseNode(node, "Unexpected mutation operator: '", name, "'");
      }
      }
      EGG_WARNING_SUPPRESS_SWITCH_END
      value = this->expression(node.getChild(1));
      if (value.hasFlowControl()) {
        return value;
      }
      return this->targetMutate(target, node, mutation, value);
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
        if (exception->getInner(thrown)) {
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
            auto retval = block.guardVariable(this->location, ctype, cname, thrown);
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
    Value statementTypedef(Block& block, const INode& node) {
      assert(node.getOpcode() == Opcode::TYPEDEF);
      auto children = node.getChildren();
      assert(children >= 1);
      this->updateLocation(node);
      auto tname = this->identifier(node.getChild(0));
      auto builder = this->getTypeFactory().createTypeBuilder(tname, "WIBBLE");
      for (size_t index = 1; index < children; ++index) {
        auto& child = node.getChild(index);
        (void)child; // WIBBLE
      }
      block.declareType(this->location, tname, builder->build());
      return Value::Void;
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
      EGG_WARNING_SUPPRESS_SWITCH_BEGIN
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
      EGG_WARNING_SUPPRESS_SWITCH_END
      throw this->unexpectedOpcode("expression", node);
    }
    Value expressionUnary(const INode& node) {
      assert(node.getOpcode() == Opcode::UNARY);
      assert(node.getChildren() == 1);
      auto oper = node.getOperator();
      assert(OperatorProperties::from(oper).opclass == Opclass::UNARY);
      EGG_WARNING_SUPPRESS_SWITCH_BEGIN
      switch (oper) {
      case Operator::NEG:
        return this->operatorNegate(node.getChild(0));
      case Operator::BITNOT:
        return this->operatorBitwiseNot(node.getChild(0));
      case Operator::LOGNOT:
        return this->operatorLogicalNot(node.getChild(0));
      case Operator::DEREF:
        return this->operatorDeref(node.getChild(0));
      case Operator::REF:
        return this->operatorRef(node.getChild(0));
      }
      EGG_WARNING_SUPPRESS_SWITCH_END
      return this->raiseNode(node, "Internal runtime error: Unsupported unary operator: '", OperatorProperties::str(oper), "'");
    }
    Value expressionBinary(const INode& node) {
      assert(node.getOpcode() == Opcode::BINARY);
      assert(node.getChildren() == 2);
      auto oper = node.getOperator();
      assert(OperatorProperties::from(oper).opclass == Opclass::BINARY);
      EGG_WARNING_SUPPRESS_SWITCH_BEGIN
      switch (oper) {
      case Operator::ADD:
      case Operator::SUB:
      case Operator::MUL:
      case Operator::DIV:
      case Operator::REM:
        return this->operatorArithmetic(node, oper, node.getChild(0), node.getChild(1));
      case Operator::BITAND:
      case Operator::BITOR:
      case Operator::BITXOR:
        return this->operatorBitwise(node, oper, node.getChild(0), node.getChild(1));
      case Operator::IFNULL:
      case Operator::LOGAND:
      case Operator::LOGOR:
        return this->operatorLogical(node, oper, node.getChild(0), node.getChild(1));
      case Operator::SHIFTL:
      case Operator::SHIFTR:
      case Operator::SHIFTU:
        return this->operatorShift(node, oper, node.getChild(0), node.getChild(1));
      }
      EGG_WARNING_SUPPRESS_SWITCH_END
      return this->raiseNode(node, "Internal runtime error: Unsupported binary operator: '", OperatorProperties::str(oper), "'");
    }
    Value expressionTernary(const INode& node) {
      assert(node.getOpcode() == Opcode::TERNARY);
      assert(node.getChildren() == 3);
      auto oper = node.getOperator();
      assert(OperatorProperties::from(oper).opclass == Opclass::TERNARY);
      EGG_WARNING_SUPPRESS_SWITCH_BEGIN
      switch (oper) {
      case Operator::TERNARY:
        return this->operatorTernary(node.getChild(0), node.getChild(1), node.getChild(2));
      }
      EGG_WARNING_SUPPRESS_SWITCH_END
      return this->raiseNode(node, "Internal runtime error: Unsupported ternary operator: '", OperatorProperties::str(oper), "'");
    }
    Value expressionCompare(const INode& node) {
      assert(node.getOpcode() == Opcode::COMPARE);
      auto oper = node.getOperator();
      Value lhs, rhs;
      auto retval = this->operatorCompare(node, oper, lhs, rhs);
      if (retval->getFlags() == ValueFlags::Break) {
        return this->raiseNode(node, "Internal runtime error: Unexpected comparison operator: '", OperatorProperties::str(oper), "'");
      }
      return retval;
    }
    Value expressionAvalue(const INode& node) {
      assert(node.getOpcode() == Opcode::AVALUE);
      auto n = node.getChildren();
      auto array = VanillaFactory::createArray(this->factory, *this->basket, n);
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
      auto object = VanillaFactory::createDictionary(this->factory, *this->basket);
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
      return block->guardVariable(this->location, vtype, vname, vinit);
    }
    Value expressionIdentifier(const INode& node) {
      assert(node.getOpcode() == Opcode::IDENTIFIER);
      auto name = this->identifier(node);
      auto* symbol = this->symtable->get(name);
      if (symbol == nullptr) {
        return this->raiseNode(node, "Unknown identifier in expression: '", name, "'");
      }
      auto value = symbol->getValue();
      if (value.hasFlowControl()) {
        return this->raiseNode(node, "Internal runtime error: Symbol table slot is empty: '", name, "'");
      }
      return value;
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
      return this->raiseFormat(lhs->getRuntimeType().describeValue(), " does not support the indexing '[]' operator");
    }
    Value expressionPredicate(const INode& node) {
      assert(node.getOpcode() == Opcode::PREDICATE);
      assert(node.getChildren() == 1);
      auto& child = node.getChild(0);
      if (child.getOperand() == INode::Operand::Operator) {
        auto oper = child.getOperator();
        if (OperatorProperties::from(oper).opclass == Opclass::COMPARE) {
          // We only support predicates for comparisons
          auto predicate = VanillaFactory::createPredicate(this->factory, *this->basket, *this, child);
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
        auto value = this->stringProperty(string, pname);
        if (value->getVoid()) {
          return this->raiseNode(node, "String does not have a property named '", pname, "'");
        }
        return value;
      }
      return this->raiseNode(node, lhs->getRuntimeType().describeValue(), " does not support properties such as '", pname, "'");
    }
    // Strings
    Value stringForeach(const String& string, const Target& target, const INode& statements, Block& block) {
      // Iterate around the codepoints of the string
      auto* p = string.get();
      if (p != nullptr) {
        UTF8 reader(p->begin(), p->end(), 0);
        char32_t codepoint;
        while (reader.forward(codepoint)) {
          auto rvalue = ValueFactory::createString(this->allocator, StringFactory::fromCodePoint(this->allocator, codepoint));
          auto retval = this->targetAssign(target, statements, rvalue);
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
      auto* found = BuiltinFactory::getStringPropertyByName(this->factory, property);
      if (found == nullptr) {
        return Value::Void;
      }
      return found->createInstance(this->factory, *this->basket, string);
    }
    // Operators
    Value operatorCompare(const INode& node, Operator oper, Value& lhs, Value& rhs) {
      // Returns 'break' for unknown operator
      assert(node.getOpcode() == Opcode::COMPARE);
      assert(node.getChildren() == 2);
      assert(OperatorProperties::from(oper).opclass == Opclass::COMPARE);
      EGG_WARNING_SUPPRESS_SWITCH_BEGIN
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
      EGG_WARNING_SUPPRESS_SWITCH_END
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
          auto result = fa < fb;
          result ^= invert;
          return ValueFactory::create(this->allocator, result);
        }
        Int ib;
        if (vb->getInt(ib)) {
          // Comparing float with int
          auto result = fa < Float(ib);
          result ^= invert;
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
          auto result = Float(ia) < fb;
          result ^= invert;
          return ValueFactory::create(this->allocator, result);
        }
        Int ib;
        if (vb->getInt(ib)) {
          // Comparing int with int
          auto result = ia < ib;
          result ^= invert;
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
    Value operatorBinaryBool(const INode& node, Operator oper, Bool lhs, Bool rhs) {
      EGG_WARNING_SUPPRESS_SWITCH_BEGIN
      switch (oper) {
      case Operator::BITAND:
        return ValueFactory::createBool(lhs && rhs);
      case Operator::BITOR:
        return ValueFactory::createBool(lhs || rhs);
      case Operator::BITXOR:
        return ValueFactory::createBool(lhs ^ rhs);
      }
      EGG_WARNING_SUPPRESS_SWITCH_END
      return this->raiseNode(node, "Internal runtime error: Unexpected binary bool operator: '", OperatorProperties::str(oper), "'");
    }
    Value operatorBinaryInt(const INode& node, Operator oper, Int lhs, Int rhs) {
      // TODO: handle div-by-zero etc more elegantly
      EGG_WARNING_SUPPRESS_SWITCH_BEGIN
      switch (oper) {
      case Operator::ADD:
        return ValueFactory::createInt(this->allocator, lhs + rhs);
      case Operator::SUB:
        return ValueFactory::createInt(this->allocator, lhs - rhs);
      case Operator::MUL:
        return ValueFactory::createInt(this->allocator, lhs * rhs);
      case Operator::DIV:
        return ValueFactory::createInt(this->allocator, lhs / rhs);
      case Operator::REM:
        return ValueFactory::createInt(this->allocator, lhs % rhs);
      case Operator::BITAND:
        return ValueFactory::createInt(this->allocator, lhs & rhs);
      case Operator::BITOR:
        return ValueFactory::createInt(this->allocator, lhs | rhs);
      case Operator::BITXOR:
        return ValueFactory::createInt(this->allocator, lhs ^ rhs);
      case Operator::SHIFTL:
        if (rhs < 0) {
          return ValueFactory::createInt(this->allocator, lhs >> -rhs);
        }
        return ValueFactory::createInt(this->allocator, lhs << rhs);
      case Operator::SHIFTR:
        if (rhs < 0) {
          return ValueFactory::createInt(this->allocator, lhs << -rhs);
        }
        return ValueFactory::createInt(this->allocator, lhs >> rhs);
      case Operator::SHIFTU:
        if (rhs < 0) {
          return ValueFactory::createInt(this->allocator, lhs << -rhs);
        }
        return ValueFactory::createInt(this->allocator, Int(uint64_t(lhs) >> rhs));
      }
      EGG_WARNING_SUPPRESS_SWITCH_END
      return this->raiseNode(node, "Internal runtime error: Unexpected binary integer operator: '", OperatorProperties::str(oper), "'");
    }
    Value operatorBinaryFloat(const INode& node, Operator oper, Float lhs, Float rhs) {
      // TODO: handle div-by-zero etc more elegantly
      EGG_WARNING_SUPPRESS_SWITCH_BEGIN
      switch (oper) {
      case Operator::ADD:
        return ValueFactory::createFloat(this->allocator, lhs + rhs);
      case Operator::SUB:
        return ValueFactory::createFloat(this->allocator, lhs - rhs);
      case Operator::MUL:
        return ValueFactory::createFloat(this->allocator, lhs * rhs);
      case Operator::DIV:
        return ValueFactory::createFloat(this->allocator, lhs / rhs);
      case Operator::REM:
        return ValueFactory::createFloat(this->allocator, std::remainder(lhs, rhs));
      }
      EGG_WARNING_SUPPRESS_SWITCH_END
      return this->raiseNode(node, "Internal runtime error: Unexpected binary float operator: '", OperatorProperties::str(oper), "'");
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
          return this->operatorBinaryFloat(node, oper, flhs, frhs);
        }
        Int irhs;
        if (rhs->getInt(irhs)) {
          // Promote rhs
          return this->operatorBinaryFloat(node, oper, flhs, Float(irhs));
        }
        return this->raiseNode(b, "Expected right-hand side of arithmetic '" + OperatorProperties::str(oper), "' operator to be an 'int' or 'float', but got '", rhs->getRuntimeType().toString(), "' instead");
      }
      Int ilhs;
      if (lhs->getInt(ilhs)) {
        Float frhs;
        if (rhs->getFloat(frhs)) {
          // Promote lhs
          return this->operatorBinaryFloat(node, oper, Float(ilhs), frhs);
        }
        Int irhs;
        if (rhs->getInt(irhs)) {
          // Both ints
          return this->operatorBinaryInt(node, oper, ilhs, irhs);
        }
        return this->raiseNode(b, "Expected right-hand side of arithmetic '" + OperatorProperties::str(oper), "' operator to be an 'int' or 'float', but got '", rhs->getRuntimeType().toString(), "' instead");
      }
      return this->raiseNode(a, "Expected left-hand side of arithmetic '" + OperatorProperties::str(oper), "' operator to be an 'int' or 'float', but got '", lhs->getRuntimeType().toString(), "' instead");
    }
    Value operatorBitwise(const INode& node, Operator oper, const INode& a, const INode& b) {
      auto lhs = this->expression(a);
      if (lhs.hasFlowControl()) {
        return lhs;
      }
      auto rhs = this->expression(b);
      if (rhs.hasFlowControl()) {
        return rhs;
      }
      Int ilhs;
      if (lhs->getInt(ilhs)) {
        Int irhs;
        if (rhs->getInt(irhs)) {
          // Both ints
          return this->operatorBinaryInt(node, oper, ilhs, irhs);
        }
        return this->raiseNode(b, "Expected right-hand side of 'int' '" + OperatorProperties::str(oper), "' operator to be an 'int', but got '", rhs->getRuntimeType().toString(), "' instead");
      }
      Bool blhs;
      if (lhs->getBool(blhs)) {
        Bool brhs;
        if (rhs->getBool(brhs)) {
          // Both bools
          return this->operatorBinaryBool(node, oper, blhs, brhs);
        }
        return this->raiseNode(b, "Expected right-hand side of 'bool' '" + OperatorProperties::str(oper), "' operator to be a 'bool', but got '", rhs->getRuntimeType().toString(), "' instead");
      }
      return this->raiseNode(a, "Expected left-hand side of '" + OperatorProperties::str(oper), "' operator to be a 'bool' or 'int', but got '", rhs->getRuntimeType().toString(), "' instead");
    }
    Value operatorLogical(const INode&, Operator oper, const INode& a, const INode& b) {
      auto lhs = this->expression(a);
      if (lhs.hasFlowControl()) {
        return lhs;
      }
      if (oper == Operator::IFNULL) {
        return lhs->getNull() ? this->expression(b) : lhs;
      }
      Bool blhs;
      if (lhs->getBool(blhs)) {
        if (blhs == Bool(oper == Operator::LOGOR)) {
          // Short circuit
          return lhs;
        }
        auto rhs = this->expression(b);
        if (rhs.hasFlowControl()) {
          return rhs;
        }
        Bool brhs;
        if (rhs->getBool(brhs)) {
          return rhs;
        }
        return this->raiseNode(b, "Expected right-hand side of '" + OperatorProperties::str(oper), "' operator to be a 'bool', but got '", rhs->getRuntimeType().toString(), "' instead");
      }
      return this->raiseNode(a, "Expected left-hand side of '" + OperatorProperties::str(oper), "' operator to be a 'bool', but got '", lhs->getRuntimeType().toString(), "' instead");
    }
    Value operatorShift(const INode& node, Operator oper, const INode& a, const INode& b) {
      auto lhs = this->expression(a);
      if (lhs.hasFlowControl()) {
        return lhs;
      }
      auto rhs = this->expression(b);
      if (rhs.hasFlowControl()) {
        return rhs;
      }
      Int ilhs;
      if (lhs->getInt(ilhs)) {
        Int irhs;
        if (rhs->getInt(irhs)) {
          return operatorBinaryInt(node, oper, ilhs, irhs);
        }
        return this->raiseNode(b, "Expected right-hand side of shift '" + OperatorProperties::str(oper), "' operator to be an 'int', but got '", rhs->getRuntimeType().toString(), "' instead");
      }
      return this->raiseNode(a, "Expected left-hand side of shift '" + OperatorProperties::str(oper), "' operator to be an 'int', but got '", lhs->getRuntimeType().toString(), "' instead");
    }
    Value operatorNegate(const INode& a) {
      auto value = this->expression(a);
      if (value.hasFlowControl()) {
        return value;
      }
      Float fvalue;
      if (value->getFloat(fvalue)) {
        return ValueFactory::createFloat(this->allocator, -fvalue);
      }
      Int ivalue;
      if (value->getInt(ivalue)) {
        return ValueFactory::createInt(this->allocator, -ivalue);
      }
      return this->raiseNode(a, "Expected operand of unary '-' operator to be an 'int' or 'float', but got '", value->getRuntimeType().toString(), "' instead");
    }
    Value operatorBitwiseNot(const INode& a) {
      auto value = this->expression(a);
      if (value.hasFlowControl()) {
        return value;
      }
      Int bits;
      if (!value->getInt(bits)) {
        return this->raiseNode(a, "Expected operand of unary '~' operator to be an 'int', but got '", value->getRuntimeType().toString(), "' instead");
      }
      return ValueFactory::createInt(this->allocator, ~bits);
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
      Object pointer;
      if (!value->getObject(pointer)) {
        return this->raiseNode(a, "Expected operand of unary '*' operator to be a pointer-like object, but got '", value->getRuntimeType().toString(), "' instead");
      }
      return pointer->getPointee(*this);
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
      return this->referenceSymbol(a, *symbol);
    }
    Value operatorTernary(const INode& a, const INode& b, const INode& c) {
      auto cond = this->condition(a, nullptr);
      Bool condition;
      if (!cond->getBool(condition)) {
        return cond;
      }
      return condition ? this->expression(b) : this->expression(c);
    }
    // Error handling
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
      EGG_WARNING_SUPPRESS_SWITCH_BEGIN
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
          auto& child = node.getChild(0);
          if (child.getOpcode() == Opcode::CALLABLE) {
            return UserFunction::makeType(this->factory, *this, name, child);
          }
          if (child.getOpcode() == Opcode::POINTER) {
            auto pointee = this->type(child.getChild(0));
            auto modifiability = Modifiability::READ_WRITE_MUTATE;
            return this->factory.createPointer(pointee, modifiability);
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
      case Opcode::UNION:
        if (children > 0) {
          std::vector<Type> types;
          types.resize(children);
          for (size_t i = 0; i < children; ++i) {
            types[i] = this->type(node.getChild(i));
          }
          return this->factory.createUnion(types);
        }
        return Type::Void;
      default:
        throw this->unexpectedOpcode("type", node);
      }
      EGG_WARNING_SUPPRESS_SWITCH_END
      this->updateLocation(node);
      throw RuntimeException(this->location, "Type constraints not yet supported"); // TODO
    }
    Value condition(const INode& node, Block* block) {
      auto value = this->expression(node, block);
      if (!value.hasAnyFlags(ValueFlags::FlowControl | ValueFlags::Bool)) {
        return this->raiseNode(node, "Expected condition to evaluate to a 'bool' value, but got '", value->getRuntimeType().toString(), "' instead");
      }
      return value;
    }
    static const char* mutationToString(Mutation mutation) {
      switch (mutation) {
      case Mutation::Assign:
        return "=";
      case Mutation::Noop:
        return "<no-op>";
      case Mutation::Decrement:
        return "--";
      case Mutation::Increment:
        return "++";
      case Mutation::Add:
        return "+=";
      case Mutation::BitwiseAnd:
        return "&=";
      case Mutation::BitwiseOr:
        return "|=";
      case Mutation::BitwiseXor:
        return "^=";
      case Mutation::Divide:
        return "/=";
      case Mutation::IfNull:
        return "??=";
      case Mutation::LogicalAnd:
        return "&&=";
      case Mutation::LogicalOr:
        return "||=";
      case Mutation::Multiply:
        return "*=";
      case Mutation::Remainder:
        return "%=";
      case Mutation::ShiftLeft:
        return "<<=";
      case Mutation::ShiftRight:
        return ">>=";
      case Mutation::ShiftRightUnsigned:
        return ">>>=";
      case Mutation::Subtract:
        return "-=";
      };
      assert(false);
      return "<unknown>";
    }
    Value tryInitialize(Symbol& symbol, const Value& value);
    Value tryAssign(Symbol& symbol, const Value& value);
    Value tryMutate(Symbol& symbol, Mutation mutation, const Value& value);
    Symbol* blockDeclare(const LocationSource& source, Symbol::Kind kind, const Type& type, const String& identifier);
    void blockUndeclare(const String& identifier);
    Value targetInit(Target& target, const INode& node, Block* block = nullptr) {
      EGG_WARNING_SUPPRESS_SWITCH_BEGIN
      switch (node.getOpcode()) {
      case Opcode::DECLARE:
        if (block != nullptr) {
          // Only allow declaration inside a foreach statement
          return this->targetDeclare(target, node, *block);
        }
        break;
      case Opcode::IDENTIFIER:
        return this->targetIdentifier(target, node);
      case Opcode::PROPERTY:
        return this->targetProperty(target, node);
      case Opcode::INDEX:
        return this->targetIndex(target, node);
      case Opcode::UNARY:
        if (node.getOperator() == Operator::DEREF) {
          // The only unary operator we support for a target is '*'
          return this->targetPointer(target, node);
        }
        break;
      }
      EGG_WARNING_SUPPRESS_SWITCH_END
      throw this->unexpectedOpcode("target", node);
    }
    Value targetDeclare(Target& target, const INode& node, Block& block) {
      assert(node.getOpcode() == Opcode::DECLARE);
      assert(node.getChildren() == 2);
      target.flavour = Target::Flavour::Identifier;
      target.identifier = this->identifier(node.getChild(1));
      target.type = this->type(node.getChild(0), target.identifier);
      this->updateLocation(node);
      return block.declareVariable(this->location, target.type, target.identifier);
    }
    Value targetIdentifier(Target& target, const INode& node) {
      assert(node.getOpcode() == Opcode::IDENTIFIER);
      target.flavour = Target::Flavour::Identifier;
      target.identifier = this->identifier(node);
      auto* symbol = this->symtable->get(target.identifier);
      if (symbol == nullptr) {
        return this->raiseNode(node, "Unknown identifier: '", target.identifier, "'");
      }
      target.type = symbol->getType();
      return Value::Void;
    }
    Value targetProperty(Target& target, const INode& node) {
      assert(node.getOpcode() == Opcode::PROPERTY);
      assert(node.getChildren() == 2);
      target.flavour = Target::Flavour::Property;
      auto lhs = this->expression(node.getChild(0));
      if (lhs.hasFlowControl()) {
        return lhs;
      }
      auto& pnode = node.getChild(1);
      if (pnode.getOpcode() == Opcode::IDENTIFIER) {
        target.identifier = this->identifier(pnode);
      } else {
        auto rhs = this->expression(pnode);
        if (rhs.hasFlowControl()) {
          return rhs;
        }
        if (!rhs->getString(target.identifier)) {
          return this->raiseNode(node, "Expected property name to be a 'string', but got '", rhs->getRuntimeType().toString(), "' instead");
        }
      }
      if (lhs->getObject(target.object)) {
        auto type = target.object->getRuntimeType();
        auto dotable = this->factory.queryDotable(type);
        if (dotable == nullptr) {
          return this->raiseNode(node, type.describeValue(), " does not support properties such as '", target.identifier, "'");
        }
        auto modifiability = dotable->getModifiability(target.identifier);
        if (modifiability == Modifiability::NONE) {
          return this->raiseNode(node, type.describeValue(), " does not support the property '", target.identifier, "'");
        }
        if (!Bits::hasAnySet(modifiability, Modifiability::WRITE)) { // TODO: Mutate?
          return this->raiseNode(node, type.describeValue(), " does not support modification of the property '", target.identifier, "'");
        }
        target.type = dotable->getType(target.identifier);
        return Value::Void;
      }
      String string;
      if (lhs->getString(string)) {
        return this->raiseNode(node, "Strings do not support modification of properties such as '", target.identifier, "'");
      }
      return this->raiseNode(node, lhs->getRuntimeType().describeValue(), " does not support properties such as '", target.identifier, "'");
    }
    Value targetIndex(Target& target, const INode& node) {
      assert(node.getOpcode() == Opcode::INDEX);
      assert(node.getChildren() == 2);
      target.flavour = Target::Flavour::Index;
      auto lhs = this->expression(node.getChild(0));
      if (lhs.hasFlowControl()) {
        return lhs;
      }
      target.index = this->expression(node.getChild(1));
      if (target.index.hasFlowControl()) {
        return target.index;
      }
      String string;
      if (lhs->getObject(target.object)) {
        auto* indexable = this->factory.queryIndexable(target.object->getRuntimeType());
        if (indexable != nullptr) {
          target.type = indexable->getResultType();
          return Value::Void;
        }
      } else if (lhs->getString(string)) {
        return this->raiseNode(node, "String cannot be modified using the indexing '[]' operator");
      }
      return this->raiseNode(node, lhs->getRuntimeType().describeValue(), " does not support the indexing '[]' operator");
    }
    Value targetPointer(Target& target, const INode& node) {
      assert(node.getOpcode() == Opcode::UNARY);
      assert(node.getOperator() == Operator::DEREF);
      assert(node.getChildren() == 1);
      target.flavour = Target::Flavour::Pointer;
      auto pointer = this->expression(node.getChild(0));
      if (pointer.hasFlowControl()) {
        return pointer;
      }
      if (pointer->getObject(target.object)) {
        auto* pointable = this->factory.queryPointable(target.object->getRuntimeType());
        if (pointable != nullptr) {
          target.type = pointable->getType();
          return Value::Void;
        }
      }
      return this->raiseNode(node, pointer->getRuntimeType().describeValue(), " does not support the pointer dereferencing '*' operator");
    }
    Value targetGet(const INode& node, const Target& target) {
      // Get the value of a target (used only as part of short-circuiting)
      switch (target.flavour) {
      case Target::Flavour::Identifier: {
        auto* symbol = this->symtable->get(target.identifier);
        if (symbol == nullptr) {
          return this->raiseNode(node, "Unknown identifier: '", target.identifier, "'");
        }
        auto value = symbol->getValue();
        if (value.hasFlowControl()) {
          return this->raiseNode(node, "Internal runtime error: Symbol table slot is empty: '", target.identifier, "'");
        }
        return value;
      }
      case Target::Flavour::Property:
        return target.object->getProperty(*this, target.identifier);
      case Target::Flavour::Index:
        return target.object->getIndex(*this, target.index);
      case Target::Flavour::Pointer:
        return target.object->getPointee(*this);
      }
      return this->raiseNode(node, "Internal runtime error: Unknown target flavour");
    }
    Value targetAssign(const Target& target, const INode& node, const Value& rhs) {
      // Set the value of a target
      switch (target.flavour) {
      case Target::Flavour::Identifier: {
        auto* symbol = this->symtable->get(target.identifier);
        if (symbol == nullptr) {
          return this->raiseNode(node, "Unknown identifier: '", target.identifier, "'");
        }
        return this->tryAssign(*symbol, rhs);
      }
      case Target::Flavour::Property:
        return target.object->setProperty(*this, target.identifier, rhs);
      case Target::Flavour::Index:
        return target.object->setIndex(*this, target.index, rhs);
      case Target::Flavour::Pointer:
        return target.object->setPointee(*this, rhs);
      }
      return this->raiseNode(node, "Internal runtime error: Unknown target flavour");
    }
    Value targetMutate(const Target& target, const INode& node, Mutation mutation, const Value& rhs) {
      // Set the value of a target
      switch (target.flavour) {
      case Target::Flavour::Identifier: {
        auto* symbol = this->symtable->get(target.identifier);
        if (symbol == nullptr) {
          return this->raiseNode(node, "Unknown identifier: '", target.identifier, "'");
        }
        return this->tryMutate(*symbol, mutation, rhs);
      }
      case Target::Flavour::Property:
        return target.object->mutProperty(*this, target.identifier, mutation, rhs);
      case Target::Flavour::Index:
        return target.object->mutIndex(*this, target.index, mutation, rhs);
      case Target::Flavour::Pointer:
        return target.object->mutPointee(*this, mutation, rhs);
      }
      return this->raiseNode(node, "Internal runtime error: Unknown target flavour");
    }
    Value referenceSymbol(const INode& node, const Symbol& symbol) {
      auto slot = symbol.getSlot();
      if (slot == nullptr) {
        return this->raiseNode(node, "Internal runtime error: Symbol table slot is empty: '", symbol.getName(), "'");
      }
      assert(slot->softGetBasket() == this->basket.get());
      auto modifiability = Modifiability::READ_WRITE_MUTATE;
      switch (symbol.getKind()) {
      case Symbol::Kind::Builtin:
      case Symbol::Kind::Type:
        modifiability = Modifiability::READ;
        break;
      case Symbol::Kind::Variable:
      default:
        break;
      }
      return slot->reference(this->factory, *this->basket, symbol.getType(), modifiability);
    }
    RuntimeException unexpectedOpcode(const char* expected, const INode& node) {
      this->updateLocation(node);
      auto opcode = node.getOpcode();
      auto name = OpcodeProperties::from(opcode).name;
      if (name == nullptr) {
        return RuntimeException(this->location, "Unknown ", expected, " opcode: '<", std::to_string(int(opcode)), ">'");
      }
      return RuntimeException(this->location, "Unexpected ", expected, " opcode: '", name, "'");
    }
    Value raiseError(const LocationSource& where, const String& message) {
      auto object = VanillaFactory::createError(this->factory, *this->basket, where, message);
      auto value = ValueFactory::createObject(this->allocator, object);
      return ValueFactory::createFlowControl(this->allocator, ValueFlags::Throw, value);
    }
    template<typename... ARGS>
    Value raiseLocation(const LocationSource& where, ARGS&&... args) {
      auto message = StringBuilder::concat(std::forward<ARGS>(args)...);
      return this->raiseError(where, message);
    }
    template<typename... ARGS>
    Value raiseNode(const INode& node, ARGS&&... args) {
      this->updateLocation(node);
      return this->raiseLocation(this->location, std::forward<ARGS>(args)...);
    }
  };
}

void UserFunction::initialize(const Type& ftype) {
  assert(ftype != nullptr);
  this->type = ftype;
  this->program.getBasket().take(*this);
}

Value UserFunction::call(IExecution& execution, const IParameters& parameters) {
  auto signature = this->getFunctionSignature();
  if (signature == nullptr) {
    return execution.raise("Attempt to call a function without a signature");
  }
  assert(this->captured != nullptr);
  return this->program.executeCall(this->location, *signature, parameters, *this->block, *this->captured);
}

Block::~Block() {
  // Undeclare all the names successfully declared by this block
  for (auto& i : this->declared) {
    this->program.blockUndeclare(i);
  }
}

Value ProgramDefault::tryInitialize(Symbol& symbol, const Value& value) {
  assert(symbol.validate(true));
  auto type = symbol.getType();
  assert(type != nullptr);
  Value promoted;
  auto retval = type.promote(this->allocator, value, promoted);
  switch (retval) {
  case Type::Assignment::Success:
    symbol.assign(promoted);
    break;
  case Type::Assignment::Incompatible:
    // Occurs when the RHS *may* be assignable to the LHS at compile-time, but not at runtime
    return this->raiseFormat("Cannot initialize '", symbol.getName(), "' of type '", type.toString(), "' with a value of type '", value->getRuntimeType().toString(), "'");
  case Type::Assignment::Uninitialized:
  case Type::Assignment::BadIntToFloat:
    return this->raiseFormat("Internal error: Cannot initialize '", symbol.getName(), "' of type '", type.toString(), "' with a value of type '", value->getRuntimeType().toString(), "'");
  }
  assert(symbol.validate(false));
  return promoted;
}

Value ProgramDefault::tryAssign(Symbol& symbol, const Value& value) {
  assert(symbol.validate(true));
  auto type = symbol.getType();
  assert(type != nullptr);
  switch (symbol.getKind()) {
  case Symbol::Kind::Builtin:
    return this->raiseFormat("Cannot re-assign built-in value: '", symbol.getName(), "'");
  case Symbol::Kind::Type:
    return this->raiseFormat("Cannot re-assign type identifier: '", symbol.getName(), "'");
  case Symbol::Kind::Variable:
    break;
  }
  Value promoted;
  auto retval = type.promote(this->allocator, value, promoted);
  switch (retval) {
  case Type::Assignment::Success:
    symbol.assign(promoted);
    break;
  case Type::Assignment::Incompatible:
    return this->raiseFormat("Cannot assign '", symbol.getName(), "' of type '", type.toString(), "' with a value of type '", value->getRuntimeType().toString(), "'");
  case Type::Assignment::Uninitialized:
  case Type::Assignment::BadIntToFloat: // TODO
    return this->raiseFormat("Internal error: Cannot assign '", symbol.getName(), "' of type '", type.toString(), "' with a value of type '", value->getRuntimeType().toString(), "'");
  }
  assert(symbol.validate(false));
  return promoted;
}

Value ProgramDefault::tryMutate(Symbol& symbol, Mutation mutation, const Value& value) {
  assert(symbol.validate(true));
  auto type = symbol.getType();
  assert(type != nullptr);
  Value before;
  auto retval = symbol.getSlot()->mutate(type, mutation, value, before);
  switch (retval) {
  case Type::Assignment::Success:
    break;
  case Type::Assignment::Uninitialized:
    return this->raiseFormat("Cannot apply '", mutationToString(mutation), "' to modify uninitialized '", symbol.getName(), "' of type '", type.toString(), "'");
  case Type::Assignment::Incompatible:
  case Type::Assignment::BadIntToFloat: // TODO
    return this->raiseFormat("Internal error: Cannot apply '", mutationToString(mutation), "' to modify '", symbol.getName(), "' of type '", type.toString(), "' with a value of type '", value->getRuntimeType().toString(), "'");
  }
  assert(symbol.validate(false));
  return before;
}

Symbol* ProgramDefault::blockDeclare(const LocationSource& source, Symbol::Kind kind, const Type& type, const String& identifier) {
  return this->symtable->add(source, kind, type, identifier);
}

void ProgramDefault::blockUndeclare(const String& identifier) {
  this->symtable->remove(identifier);
}

egg::ovum::Value Block::declareType(const LocationSource& source, const String& name, const Type& type) {
  // Keep track of names successfully declared in this block
  auto* symbol = this->program.blockDeclare(source, Symbol::Kind::Type, type, name);
  if (symbol == nullptr) {
    return this->program.raiseLocation(source, "Duplicate name in type definition: '", name, "'");
  }
  this->declared.insert(name);
  return Value::Void;
}

egg::ovum::Value Block::declareVariable(const LocationSource& source, const Type& type, const String& name, const Value* init) {
  // Keep track of names successfully declared in this block
  auto* symbol = this->program.blockDeclare(source, Symbol::Kind::Variable, type, name);
  if (symbol == nullptr) {
    return this->program.raiseLocation(source, "Duplicate name in variable declaration: '", name, "'");
  }
  if (init != nullptr) {
    auto retval = this->program.tryInitialize(*symbol, *init);
    if (retval.hasFlowControl()) {
      this->program.blockUndeclare(name);
      return retval;
    }
  }
  this->declared.insert(name);
  return Value::Void;
}

egg::ovum::Value Block::guardVariable(const LocationSource& source, const Type& type, const String& name, const Value& init) {
  // Keep track of names successfully declared in this block
  auto* symbol = this->program.blockDeclare(source, Symbol::Kind::Variable, type, name);
  if (symbol == nullptr) {
    return this->program.raiseLocation(source, "Duplicate name in variable declaration: '", name, "'");
  }
  auto retval = this->program.tryInitialize(*symbol, init);
  if (retval.hasFlowControl()) {
    this->program.blockUndeclare(name);
    return Value::False;
  }
  this->declared.insert(name);
  return Value::True;
}

egg::ovum::Type UserFunction::makeType(ITypeFactory& factory, ProgramDefault& program, const String& name, const INode& callable) {
  // Create a type appropriate for a standard "user" function
  assert(callable.getOpcode() == Opcode::CALLABLE);
  auto n = callable.getChildren();
  assert(n >= 1);
  auto rettype = program.type(callable.getChild(0));
  assert(rettype != nullptr);
  auto description = StringBuilder::concat("Function '", name, "'");
  auto builder = factory.createFunctionBuilder(rettype, name, description);
  for (size_t i = 1; i < n; ++i) {
    auto& parameter = callable.getChild(i);
    auto children = parameter.getChildren();
    auto variadic = false;
    auto named = false;
    auto optional = false;
    EGG_WARNING_SUPPRESS_SWITCH_BEGIN
    switch (parameter.getOpcode()) {
    case Opcode::REQUIRED:
      assert((children == 1) || (children == 2));
      break;
    case Opcode::OPTIONAL:
      assert((children == 1) || (children == 2));
      optional = true;
      break;
    case Opcode::VARARGS:
      assert(children >= 2);
      variadic = true;
      // TODO We assume the constraint is "length > 0" meaning required
      optional = (children <= 2);
      break;
    case Opcode::BYNAME:
      assert(children == 2);
      named = true;
      optional = true;
      break;
    default:
      throw program.unexpectedOpcode("function type parameter", parameter);
    }
    EGG_WARNING_SUPPRESS_SWITCH_END
    String pname;
    if (children > 1) {
      pname = program.identifier(parameter.getChild(1));
    }
    auto ptype = program.type(parameter.getChild(0), pname);
    if (variadic) {
      builder->addVariadicParameter(ptype, pname, optional);
    } else if (named) {
      builder->addNamedParameter(ptype, pname, optional);
    } else {
      builder->addPositionalParameter(ptype, pname, optional);
    }
  }
  return builder->build();
}

egg::ovum::Program egg::ovum::ProgramFactory::createProgram(IAllocator& allocator, ILogger& logger) {
  HardPtr<ProgramDefault> program{ allocator.makeRaw<ProgramDefault>(allocator, logger) };
  program->addBuiltins();
  return program;
}
