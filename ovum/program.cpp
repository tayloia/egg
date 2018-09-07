#include "ovum/ovum.h"
#include "ovum/node.h"
#include "ovum/module.h"
#include "ovum/program.h"
#include "ovum/function.h"
#include "ovum/utf.h"

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
      location.formatSourceString(sb);
      sb.add(':', ' ', std::forward<ARGS>(args)...);
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
        retval = Binary::unexpected(b, "Expected right-hand side of ", operation, " to be an 'int' or 'float'");
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
        retval = Binary::unexpected(b, "Expected right-hand side of ", operation, " to be an 'int' or 'float'");
        return Match::Mismatch;
      }
      retval = Binary::unexpected(a, "Expected left-hand side of ", operation, " to be an 'int' or 'float'");
      return Match::Mismatch;
    }
    static Match bitwise(const char* operation, const Variant& a, const Variant& b, Variant& retval) {
      // Accept matching ints or bools
      if (a.isInt()) {
        if (b.isInt()) {
          return Match::Int;
        }
        retval = Binary::unexpected(b, "Expected right-hand side of ", operation, " to be an 'int'");
        return Match::Mismatch;
      }
      if (a.isBool()) {
        if (b.isBool()) {
          return Match::Bool;
        }
        retval = Binary::unexpected(b, "Expected right-hand side of ", operation, " to be a 'bool'");
        return Match::Mismatch;
      }
      retval = Binary::unexpected(a, "Expected left-hand side of ", operation, " to be a 'bool' or 'int'");
      return Match::Mismatch;
    }
    static Match logical(const char* operation, const Variant& a, const Variant& b, Variant& retval) {
      // Accept only bools
      if (!a.isBool()) {
        retval = Binary::unexpected(a, "Expected left-hand side of ", operation, " to be a 'bool'");
        return Match::Mismatch;
      }
      if (!b.isBool()) {
        retval = Binary::unexpected(b, "Expected right-hand side of ", operation, " to be a 'bool'");
        return Match::Mismatch;
      }
      return Match::Bool;
    }
    static Match shift(const char* operation, const Variant& a, const Variant& b, uint64_t& ub, Variant& retval) {
      // Accept only bools
      if (!a.isInt()) {
        retval = Binary::unexpected(a, "Expected left-hand side of ", operation, " to be an 'int'");
        return Match::Mismatch;
      }
      if (!b.isInt()) {
        retval = Binary::unexpected(b, "Expected right-hand side of ", operation, " to be an 'int'");
        return Match::Mismatch;
      }
      auto ib = b.getInt();
      if (ib < 0) {
        retval = Binary::raise("Expected right-hand side of ", operation, " to be a non-negative 'int', but got ", std::to_string(ib), " instead");
        return Match::Mismatch;
      }
      ub = uint64_t(ib);
      return Match::Int;
    }
    template<typename... ARGS>
    static Variant unexpected(const Variant& value, ARGS&&... args) {
      return Binary::raise(std::forward<ARGS>(args)..., ", but got '", value.getRuntimeType().toString(), "' instead");
    }
    template<typename... ARGS>
    static Variant raise(ARGS&&... args) {
      Variant exception{ StringBuilder::concat(std::forward<ARGS>(args)...) };
      return VariantFactory::createException(std::move(exception));
    }
  };

  struct Symbol {
    Type type;
    String name;
    LocationSource source;
    Variant value;
    Symbol(const Type& type, const String& name, const LocationSource& source)
      : type(type),
        name(name),
        source(source),
        value() {
      assert(type != nullptr);
    }
    Variant tryAssign(IExecution& execution, const Variant& rvalue) {
      auto retval = this->type->tryAssign(execution, this->value, rvalue);
      if (!retval.hasFlowControl()) {
        this->value.soften(execution.getBasket());
        assert(this->value.validate(true));
      }
      return retval;
    }
    void softVisitLink(const ICollectable::Visitor& visitor) const {
      this->value.softVisitLink(visitor);
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
    HardPtr<SymbolTable> cloneIndirect() const {
      // WIBBLE not needed once specific capture is implemented
      auto clone = this->allocator.make<SymbolTable>();
      for (auto& src : this->table) {
        auto dst = clone->table.insert(src);
        dst.first->second.value.indirect(this->allocator, *this->basket);
      }
      return clone;
    }
    void softVisitLinks(const Visitor& visitor) const {
      for (auto& entry : this->table) {
        entry.second.softVisitLink(visitor);
      }
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

  class Parameters : public IParameters {
    Parameters(const Parameters&) = delete;
    Parameters& operator=(const Parameters&) = delete;
  private:
    std::vector<Variant> positional;
    std::vector<LocationSource> location;
  public:
    Parameters() = default;
    void addPositional(const LocationSource& source, Variant&& value) {
      this->positional.emplace_back(std::move(value));
      this->location.emplace_back(source);
    }
    virtual size_t getPositionalCount() const override {
      return this->positional.size();
    }
    virtual Variant getPositional(size_t index) const override {
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
    virtual Variant getNamed(const String&) const override {
      return Variant::Void;
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
    virtual Variant toString() const override {
      return "<predicate>";
    }
    virtual Type getRuntimeType() const override {
      return Type::Object;
    }
    virtual Variant call(IExecution& execution, const IParameters& parameters) override;
    virtual Variant getProperty(IExecution& execution, const String&) override {
      return execution.raise("Internal runtime error: Predicates do not support properties");
    }
    virtual Variant setProperty(IExecution& execution, const String&, const Variant&) override {
      return execution.raise("Internal runtime error: Predicates do not support properties");
    }
    virtual Variant getIndex(IExecution& execution, const Variant&) override {
      return execution.raise("Internal runtime error: Predicates do not support indexing");
    }
    virtual Variant setIndex(IExecution& execution, const Variant&, const Variant&) override {
      return execution.raise("Internal runtime error: Predicates do not support indexing");
    }
    virtual Variant iterate(IExecution& execution) override {
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
    SoftPtr<SymbolTable> captured;
  public:
    UserFunction(IAllocator& allocator, ProgramDefault& program, const LocationSource& location, const Type& type, const String& name, const INode& block)
      : SoftReferenceCounted(allocator),
        program(program),
        location(location),
        type(type),
        block(&block) {
      assert(type != nullptr);
      assert(!name.empty());
    }
    void setCaptured(const HardPtr<SymbolTable>& symtable) {
      assert(this->captured == nullptr);
      this->captured.set(*this, symtable.get());
      assert(this->captured != nullptr);
    }
    virtual void softVisitLinks(const Visitor& visitor) const override {
      this->captured.visit(visitor);
    }
    virtual Variant toString() const override {
      return "<predicate>";
    }
    virtual Type getRuntimeType() const override {
      return this->type;
    }
    virtual Variant call(IExecution& execution, const IParameters& parameters) override;
    virtual Variant getProperty(IExecution& execution, const String& property) override {
      return this->raise(execution, "does not support properties such as '", property, "'");
    }
    virtual Variant setProperty(IExecution& execution, const String& property, const Variant&) override {
      return this->raise(execution, "does not support properties such as '", property, "'");
    }
    virtual Variant getIndex(IExecution& execution, const Variant&) override {
      return this->raise(execution, "does not support indexing");
    }
    virtual Variant setIndex(IExecution& execution, const Variant&, const Variant&) override {
      return this->raise(execution, "does not support indexing");
    }
    virtual Variant iterate(IExecution& execution) override {
      return this->raise(execution, "does not support iteration");
    }
    template<typename... ARGS>
    Variant raise(IExecution& execution, ARGS&&... args) {
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
  public:
    enum Flavour { Failed, Identifier, Index, Pointer, Property };
  private:
    Flavour flavour;
    ProgramDefault& program;
    const INode& node;
    Variant a;
    Variant b;
  public:
    Target(ProgramDefault& program, const INode& node, Block* block = nullptr);
    Variant check() const;
    Variant assign(const Variant& rhs) const;
    Variant nudge(Int rhs) const;
    Variant mutate(const INode& opnode, const INode& rhs) const;
  private:
    Variant* ref() const;
    Variant get() const;
    Variant set(const Variant& value) const;
    Variant apply(const INode& opnode, Variant& lvalue, const INode& rhs) const;
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
    Variant declare(const LocationSource& source, const Type& type, const String& name, const Variant* init = nullptr);
    Variant guard(const LocationSource& source, const Type& type, const String& name, const Variant& init);
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
      static const LocationSource source("<builtin>", 0, 0);
      auto symbol = this->symtable->add(value.getRuntimeType(), name, source);
      if (symbol.first) {
        // Don't use tryAssign for builtins; it always fails
        auto& added = symbol.second->value;
        added = value;
        added.soften(*this->basket);
        return true;
      }
      return false;
    }
    virtual Variant run(const IModule& module) override {
      try {
        this->location.file = module.getResourceName();
        this->location.line = 0;
        this->location.column = 0;
        return this->executeRoot(module.getRootNode());
      } catch (RuntimeException& exception) {
        this->logger.log(ILogger::Source::Runtime, ILogger::Severity::Error, exception.message.toUTF8());
        return Variant::Rethrow;
      }
    }
    // Inherited from IExecution
    virtual IAllocator& getAllocator() const override {
      return this->allocator;
    }
    virtual IBasket& getBasket() const override {
      return *this->basket;
    }
    virtual Variant raise(const String& message) override {
      return VariantFactory::createException(this->allocator, this->location, message);
    }
    virtual Variant assertion(const Variant& predicate) override {
      if (predicate.hasObject()) {
        // Predicates can be functions that throw exceptions, as well as 'bool' values
        auto object = predicate.getObject();
        auto type = object->getRuntimeType();
        if (type->callable() != nullptr) {
          // Call the predicate directly
          return object->call(*this, Function::NoParameters);
        }
      }
      if (!predicate.isBool()) {
        return this->raiseFormat("'assert()' expects its parameter to be a 'bool' or 'void()', but got '", predicate.getRuntimeType().toString(), "' instead");
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
    Symbol* blockDeclare(const LocationSource& source, const Type& type, const String& name) {
      auto symbol = this->symtable->add(type, name, source);
      if (!symbol.first) {
        StringBuilder sb;
        symbol.second->source.formatSourceString(sb);
        sb.add(": Previous declaration of '", name, "'");
        this->logger.log(ILogger::Source::Runtime, ILogger::Severity::Warning, sb.toUTF8());
        return nullptr;
      }
      return symbol.second;
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
      auto vname = this->identifier(node.getChild(1));
      auto vtype = this->type(node.getChild(0), vname);
      this->updateLocation(node);
      a = block.declare(this->location, vtype, vname);
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
    Target::Flavour targetPointer(const INode& node, Variant& a) {
      assert(node.getOpcode() == OPCODE_UNARY);
      assert(node.getOperator() == OPERATOR_DEREF);
      assert(node.getChildren() == 1);
      a = this->expression(node.getChild(0)).direct();
      if (!a.hasPointer()) {
        a = this->raiseNode(node, "Expected target to be a pointer, but got '", a.getRuntimeType().toString(), "' instead");
      }
      return Target::Flavour::Pointer;
    }
    Target::Flavour targetProperty(const INode& node, Variant& a, Variant& b) {
      assert(node.getOpcode() == OPCODE_PROPERTY);
      assert(node.getChildren() == 2);
      a = this->expression(node.getChild(0));
      if (a.hasFlowControl()) {
        return Target::Flavour::Failed;
      }
      auto& property = node.getChild(1);
      if (property.getOpcode() == OPCODE_IDENTIFIER) {
        // Handle explicit identifiers
        b = this->identifier(property);
      } else {
        b = this->expression(node.getChild(1));
        if (!b.isString()) {
          if (b.hasFlowControl()) {
            std::swap(a, b); // swap the error into 'a'
          } else {
            a = this->raiseNode(property, "Expected target property name to be a 'string', but got '", b.getRuntimeType().toString(), "' instead");
          }
          return Target::Flavour::Failed;
        }
      }
      if (!a.hasObject()) {
        if (a.hasString()) {
          a = this->raiseNode(node, "Strings do not support modification through properties such as '", b.getString(), "'");
        } else {
          a = this->raiseNode(node, "Values of type '", a.getRuntimeType().toString(), "' do not support modification of properties such as '", b.getString(), "'");
        }
        return Target::Flavour::Failed;
      }
      return Target::Flavour::Property;
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
    // Functions
    Variant executePredicate(const LocationSource& source, const INode& compare) {
      // We have to be careful to get the location correct
      assert(compare.getOpcode() == OPCODE_COMPARE);
      Variant lhs, rhs;
      auto retval = this->operatorCompare(compare, lhs, rhs);
      if (retval.hasFlowControl()) {
        if (retval.is(VariantBits::Break)) {
          return this->raiseLocation(source, "Internal runtime error: Unsupported predicate comparison");
        }
        return retval;
      }
      if (!retval.isBool()) {
        return this->raiseLocation(source, "Internal runtime error: Expected predicate to return a 'bool'");
      }
      if (retval.getBool()) {
        // The assertion has passed
        return Variant::Void;
      }
      auto exception = this->raiseLocation(source, "Assertion is untrue: ", lhs.toString(), ' ', OperatorProperties::str(compare.getOperator()), ' ', rhs.toString());
      if (exception.hasObject()) {
        // Augment the exception with the actual evaluation
        auto object = exception.getObject();
        (void)object->setProperty(*this, "left", lhs);
        (void)object->setProperty(*this, "operator", OperatorProperties::str(compare.getOperator()));
        (void)object->setProperty(*this, "right", rhs);
      }
      return exception;
    }
    Variant executeCall(const LocationSource& source, const IFunctionSignature& signature, const IParameters& runtime, const INode& block, SymbolTable& captured) {
      // We have to be careful to get the location correct
      assert(block.getOpcode() == OPCODE_BLOCK);
      if (runtime.getNamedCount() > 0) {
        return this->raiseFormat(Function::signatureToString(signature), ": Named parameters are not yet supported"); // TODO
      }
      auto maxPositional = signature.getParameterCount();
      auto minPositional = maxPositional;
      while ((minPositional > 0) && !Bits::hasAnySet(signature.getParameter(minPositional - 1).getFlags(), IFunctionSignatureParameter::Flags::Required)) {
        minPositional--;
      }
      auto actual = runtime.getPositionalCount();
      if (actual < minPositional) {
        if (minPositional == 1) {
          return this->raiseFormat(Function::signatureToString(signature), ": At least 1 parameter was expected");
        }
        return this->raiseFormat(Function::signatureToString(signature), ": At least ", minPositional, " parameters were expected, not ", actual);
      }
      if ((maxPositional > 0) && Bits::hasAnySet(signature.getParameter(maxPositional - 1).getFlags(), IFunctionSignatureParameter::Flags::Variadic)) {
        // TODO Variadic
      } else if (actual > maxPositional) {
        // Not variadic
        if (maxPositional == 1) {
          return this->raiseFormat(Function::signatureToString(signature), ": Only 1 parameter was expected, not ", actual);
        }
        return this->raiseFormat(Function::signatureToString(signature), ": No more than ", maxPositional, " parameters were expected, not ", actual);
      }
      CallStack stack(this->symtable, &captured);
      Block scope(*this);
      for (size_t i = 0; i < maxPositional; ++i) {
        auto& sigparam = signature.getParameter(i);
        auto pname = sigparam.getName();
        assert(!pname.empty());
        auto position = sigparam.getPosition();
        assert(position < actual);
        auto ptype = sigparam.getType();
        auto pvalue = runtime.getPositional(position);
        auto retval = scope.guard(source, ptype, pname, pvalue);
        if (retval.hasFlowControl()) {
          return retval;
        }
        assert(retval.isBool());
        if (!retval.getBool()) {
          // Type mismatch on parameter
          auto* plocation = runtime.getPositionalLocation(position);
          if (plocation != nullptr) {
            // Update our current source location (it will be restored when expressionCall returns)
            this->location = *plocation;
          }
          return this->raiseFormat("Type mismatch for parameter '", pname, "': Expected '", ptype.toString(), "', but got '", pvalue.getRuntimeType().toString(), "' instead");
        }
      }
      auto retval = this->executeBlock(scope, block);
      if (retval.stripFlowControl(VariantBits::Return) || retval.hasThrow()) {
        // We got a return value or exception
        return retval;
      }
      if (retval.isVoid()) {
        // Ran off the bottom of the function
        auto rettype = signature.getReturnType();
        assert(rettype != nullptr);
        if (rettype->hasBasalType(BasalBits::Void)) {
          // Implicit 'return;' at end of function
          return retval;
        }
      }
      throw RuntimeException(this->location, "Expected function to return, but got '", retval.getRuntimeType().toString(), "' instead");
    }
    // Builtins
    void addBuiltins() {
      this->builtin("assert", VariantFactory::createBuiltinAssert(this->allocator));
      this->builtin("print", VariantFactory::createBuiltinPrint(this->allocator));
      this->builtin("string", VariantFactory::createBuiltinString(this->allocator));
      this->builtin("type", VariantFactory::createBuiltinType(this->allocator));
    }
  private:
    Variant executeRoot(const INode& node) {
      if (this->validateOpcode(node) != OPCODE_MODULE) {
        throw this->unexpectedOpcode("module", node);
      }
      Block block(*this);
      return this->executeBlock(block, node.getChild(0));
    }
    Variant executeBlock(Block& inner, const INode& node) {
      this->updateLocation(node);
      if (this->validateOpcode(node) != OPCODE_BLOCK) {
        throw this->unexpectedOpcode("block", node);
      }
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
      this->updateLocation(node);
      auto opcode = this->validateOpcode(node);
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
      case OPCODE_RETURN:
        return this->statementReturn(node);
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
      return lvalue.assign(rvalue);
    }
    Variant statementBlock(const INode& node) {
      assert(node.getOpcode() == OPCODE_BLOCK);
      Block block(*this);
      return this->executeBlock(block, node);
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
      if (rvalue.hasString()) {
        // Iterate around the codepoints in the string
        return this->stringForeach(rvalue.getString(), inner, lvalue, node.getChild(2));
      }
      if (!rvalue.hasObject()) {
        return this->raiseFormat("The 'for' statement expected to iterate a 'string' or 'object', but got '", rvalue.getRuntimeType().toString(), "' instead");
      }
      auto object = rvalue.getObject();
      assert(object != nullptr);
      auto iterate = object->iterate(*this);
      if (iterate.hasFlowControl()) {
        return iterate;
      }
      if (!iterate.hasObject()) {
        return this->raiseFormat("The 'for' statement expected an iterator, but got '", iterate.getRuntimeType().toString(), "' instead");
      }
      object = iterate.getObject();
      assert(object != nullptr);
      auto& block = node.getChild(2);
      for (;;) {
        retval = object->call(*this, Function::NoParameters);
        if (retval.hasAny(VariantBits::FlowControl | VariantBits::Void)) {
          break;
        }
        retval = lvalue.assign(retval);
        if (retval.hasFlowControl()) {
          break;
        }
        retval = this->executeBlock(inner, block);
        if (retval.hasFlowControl()) {
          if (retval.is(VariantBits::Break)) {
            // Break from the loop
            return Variant::Void;
          }
          if (!retval.is(VariantBits::Continue)) {
            // Some other flow control
            break;
          }
        }
      }
      return retval;
    }
    Variant statementFunction(Block& block, const INode& node) {
      assert(node.getOpcode() == OPCODE_FUNCTION);
      assert(node.getChildren() == 3);
      auto fname = this->identifier(node.getChild(2));
      auto ftype = this->type(node.getChild(0), fname);
      auto& fblock = node.getChild(1);
      this->updateLocation(node);
      auto function = this->allocator.make<UserFunction>(*this, this->location, ftype, fname, fblock);
      // We have to be careful to ensure the function is declared before capturing symbols so that recursion works correctly
      Variant fvalue{ Object(*function) };
      auto retval = block.declare(this->location, ftype, fname, &fvalue);
      function->setCaptured(this->symtable->cloneIndirect()); // TODO don't capture everything WIBBLE
      return retval;
    }
    Variant statementIf(const INode& node) {
      assert(node.getOpcode() == OPCODE_IF);
      auto n = &node;
      Block block(*this);
      do {
        assert(n->getOpcode() == OPCODE_IF);
        assert((n->getChildren() == 2) || (n->getChildren() == 3));
        auto retval = this->condition(n->getChild(0), &block);
        if (!retval.isBool()) {
          // Problem with the condition
          return retval;
        }
        if (retval.getBool()) {
          // Execute the 'then' clause
          return this->executeBlock(block, n->getChild(1));
        }
        if (n->getChildren() == 2) {
          // No 'else' clause
          return Variant::Void;
        }
        // Execute the 'else' clause
        n = &n->getChild(2);
      } while (n->getOpcode() == OPCODE_IF);
      assert(block.empty());
      return this->executeBlock(block, *n);
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
    Variant statementReturn(const INode& node) {
      assert(node.getOpcode() == OPCODE_RETURN);
      assert(node.getChildren() <= 1);
      if (node.getChildren() == 0) {
        // A naked return
        return Variant::ReturnVoid;
      }
      auto retval = this->expression(node.getChild(0));
      if (!retval.hasFlowControl()) {
        // Convert the expression to a return
        assert(!retval.isVoid());
        retval.addFlowControl(VariantBits::Return);
      }
      return retval;
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
      Block block(*this);
      for (size_t i = 2; i < n; ++i) {
        // Look for the first matching catch clause
        auto& clause = node.getChild(i);
        assert(clause.getOpcode() == OPCODE_CATCH);
        assert(clause.getChildren() == 3);
        this->updateLocation(clause);
        auto cname = this->identifier(clause.getChild(1));
        auto ctype = this->type(clause.getChild(0), cname);
        auto retval = block.guard(this->location, ctype, cname, exception);
        if (retval.hasFlowControl()) {
          // Couldn't define the exception variable
          return this->statementTryFinally(node, retval);
        }
        assert(retval.isBool());
        if (retval.getBool()) {
          retval = this->executeBlock(block, clause.getChild(2));
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
      auto opcode = this->validateOpcode(node);
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
      case OPCODE_PREDICATE:
        return this->expressionPredicate(node);
      case OPCODE_PROPERTY:
        return this->expressionProperty(node);
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
      case Operator::OPERATOR_LOGNOT:
        return this->operatorLogicalNot(node.getChild(0));
      case Operator::OPERATOR_DEREF:
        return this->operatorDeref(node.getChild(0));
      case Operator::OPERATOR_REF:
        return this->operatorRef(node.getChild(0));
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
      case Operator::OPERATOR_SUB:
      case Operator::OPERATOR_MUL:
      case Operator::OPERATOR_DIV:
      case Operator::OPERATOR_REM:
        return this->operatorArithmetic(node, oper, node.getChild(0), node.getChild(1));
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
      Variant lhs, rhs;
      auto retval = this->operatorCompare(node, lhs, rhs);
      if (retval.is(VariantBits::Break)) {
        throw this->unexpectedOperator("compare", node);
      }
      return retval;
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
      if (!callee.hasObject()) {
        return this->raiseNode(node, "Expected function-like expression to be an 'object', but got '", callee.getRuntimeType().toString(), "' instead");
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
      auto retval = callee.getObject()->call(*this, parameters);
      this->location = before;
      return retval;
    }
    Variant expressionGuard(const INode& node, Block* block) {
      assert(node.getOpcode() == OPCODE_GUARD);
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
    Variant expressionIdentifier(const INode& node) {
      assert(node.getOpcode() == OPCODE_IDENTIFIER);
      auto name = this->identifier(node);
      auto* symbol = this->symtable->get(name);
      if (symbol == nullptr) {
        return this->raiseNode(node, "Unknown identifier in expression: '", name, "'");
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
      auto& index = node.getChild(1);
      auto rhs = this->expression(index);
      if (rhs.hasFlowControl()) {
        return rhs;
      }
      this->updateLocation(index);
      if (lhs.hasObject()) {
        auto object = lhs.getObject();
        return object->getIndex(*this, rhs);
      }
      if (lhs.hasString()) {
        auto string = lhs.getString();
        return this->stringIndex(string, rhs);
      }
      return this->raiseFormat("Values of type '", lhs.getRuntimeType().toString(), "' do not support the indexing '[]' operator");
    }
    Variant expressionPredicate(const INode& node) {
      assert(node.getOpcode() == OPCODE_PREDICATE);
      assert(node.getChildren() == 1);
      auto& child = node.getChild(0);
      if (child.getOperand() == INode::Operand::Operator) {
        auto oper = child.getOperator();
        if (OperatorProperties::from(oper).opclass == OPCLASS_COMPARE) {
          // We only support predicates for comparisons
          this->updateLocation(node);
          return VariantFactory::createObject<PredicateFunction>(this->allocator, *this, this->location, child);
        }
      }
      return this->expression(child);
    }
    Variant expressionProperty(const INode& node) {
      assert(node.getOpcode() == OPCODE_PROPERTY);
      assert(node.getChildren() == 2);
      auto lhs = this->expression(node.getChild(0));
      if (lhs.hasFlowControl()) {
        return lhs;
      }
      auto& pnode = node.getChild(1);
      String pname;
      if (pnode.getOpcode() == OPCODE_IDENTIFIER) {
        pname = this->identifier(pnode);
      } else {
        auto rhs = this->expression(pnode);
        if (rhs.hasFlowControl()) {
          return rhs;
        }
        if (!rhs.hasString()) {
          return this->raiseNode(node, "Expected property name to be a 'string', but got '", rhs.getRuntimeType().toString(), "' instead");
        }
        pname = rhs.getString();
      }
      if (lhs.hasObject()) {
        auto object = lhs.getObject();
        auto value = object->getProperty(*this, pname);
        if (value.isVoid()) {
          return this->raiseNode(node, "Object does not have a property named '", pname, "'");
        }
        return value;
      }
      if (lhs.hasString()) {
        auto string = lhs.getString();
        return this->stringProperty(string, pname);
      }
      return this->raiseNode(node, "Values of type '", lhs.getRuntimeType().toString(), "' do not support properties");
    }
    // Strings
    Variant stringForeach(const String& string, Block& block, Target& target, const INode& statements) {
      // Iterate around the codepoints of the string
      auto* p = string.get();
      if (p != nullptr) {
        UTF8 reader(p->begin(), p->end(), 0);
        char32_t codepoint;
        while (reader.forward(codepoint)) {
          auto retval = target.assign(StringFactory::fromCodePoint(this->allocator, codepoint));
          if (retval.hasFlowControl()) {
            return retval;
          }
          retval = this->executeBlock(block, statements);
          if (retval.hasFlowControl()) {
            return retval;
          }
        }
      }
      return Variant::Void;
    }
    Variant stringIndex(const String& string, const Variant& index) {
      if (!index.isInt()) {
        return this->raiseFormat("String indexing '[]' only supports indices of type 'int', but got '", index.getRuntimeType().toString(), "' instead");
      }
      auto i = index.getInt();
      auto c = string.codePointAt(size_t(i));
      if (c < 0) {
        // Invalid index
        auto n = string.length();
        if ((i < 0) || (size_t(i) >= n)) {
          return this->raiseFormat("String index ", i, " is out of range for a string of length ", n);
        }
        return this->raiseFormat("Cannot index a malformed string");
      }
      return Variant{ StringFactory::fromCodePoint(this->allocator, char32_t(c)) };
    }
    Variant stringProperty(const String& string, const String& property) {
      auto retval = VariantFactory::createStringProperty(this->allocator, string, property);
      if (retval.isVoid()) {
        return this->raiseFormat("Unknown property for 'string' value: '", property, "'");
      }
      return retval;
    }
    // Operators
    Variant operatorCompare(const INode& node, Variant& lhs, Variant& rhs) {
      // Returns 'break' for unknown operator
      assert(node.getOpcode() == OPCODE_COMPARE);
      assert(node.getChildren() == 2);
      auto oper = node.getOperator();
      assert(OperatorProperties::from(oper).opclass == Opclass::OPCLASS_COMPARE);
      EGG_WARNING_SUPPRESS_SWITCH_BEGIN();
      switch (oper) {
      case Operator::OPERATOR_EQ:
        // (a == b) === (a == b)
        return this->operatorEquals(node.getChild(0), node.getChild(1), lhs, rhs, false);
      case Operator::OPERATOR_GE:
        // (a >= b) === !(a < b)
        return this->operatorLessThan(node.getChild(0), node.getChild(1), lhs, rhs, true, oper);
      case Operator::OPERATOR_GT:
        // (a > b) === (b < a)
        return this->operatorLessThan(node.getChild(1), node.getChild(0), rhs, lhs, false, oper);
      case Operator::OPERATOR_LE:
        // (a <= b) === !(b < a)
        return this->operatorLessThan(node.getChild(1), node.getChild(0), rhs, lhs, true, oper);
      case Operator::OPERATOR_LT:
        // (a < b) === (a < b)
        return this->operatorLessThan(node.getChild(0), node.getChild(1), lhs, rhs, false, oper);
      case Operator::OPERATOR_NE:
        // (a != b) === !(a == b)
        return this->operatorEquals(node.getChild(0), node.getChild(1), lhs, rhs, true);
      }
      EGG_WARNING_SUPPRESS_SWITCH_END();
      return Variant::Break;
    }
    Variant operatorEquals(const INode& a, const INode& b, Variant& va, Variant& vb, bool invert) {
      // Care with IEEE NaNs
      va = this->expression(a);
      if (va.hasFlowControl()) {
        return va;
      }
      vb = this->expression(b);
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
      return bool(Variant::equals(va, vb) ^ invert); // need to force bool-ness
    }
    Variant operatorLessThan(const INode& a, const INode& b, Variant& va, Variant& vb, bool invert, Operator oper) {
      // Care with IEEE NaNs
      va = this->expression(a);
      if (va.hasFlowControl()) {
        return va;
      }
      vb = this->expression(b);
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
        auto side = ((oper == OPERATOR_GT) || (oper == OPERATOR_LE)) ? "left" : "right";
        auto sign = OperatorProperties::str(oper);
        return this->raiseNode(b, "Expected ", side, "-hand side of comparison '", sign, "' to be an 'int' or 'float', but got '", vb.getRuntimeType().toString(), "' instead");
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
        auto side = ((oper == OPERATOR_GT) || (oper == OPERATOR_LE)) ? "left" : "right";
        auto sign = OperatorProperties::str(oper);
        return this->raiseNode(b, "Expected ", side, "-hand side of comparison '", sign, "' to be an 'int' or 'float', but got '", vb.getRuntimeType().toString(), "' instead");
      }
      auto side = ((oper == OPERATOR_GT) || (oper == OPERATOR_LE)) ? "right" : "left";
      auto sign = OperatorProperties::str(oper);
      return this->raiseNode(a, "Expected ", side, "-hand side of comparison '", sign, "' to be an 'int' or 'float', but got '", va.getRuntimeType().toString(), "' instead");
    }
    Variant operatorArithmetic(const INode& node, Operator oper, const INode& a, const INode& b) {
      auto lhs = this->expression(a);
      if (lhs.hasFlowControl()) {
        return lhs;
      }
      auto rhs = this->expression(b);
      if (rhs.hasFlowControl()) {
        return rhs;
      }
      if (lhs.isFloat()) {
        if (rhs.isFloat()) {
          // Both floats
          return this->operatorBinaryFloat(node, oper, lhs.getFloat(), rhs.getFloat());
        }
        if (rhs.isInt()) {
          // Promote rhs
          return this->operatorBinaryFloat(node, oper, lhs.getFloat(), Float(rhs.getInt()));
        }
        return this->raiseNode(b, "Expected right-hand side of arithmetic '" + OperatorProperties::str(oper), "' operator to be an 'int' or 'float', but got '", rhs.getRuntimeType().toString(), "' instead");
      }
      if (lhs.isInt()) {
        if (rhs.isFloat()) {
          // Promote lhs
          return this->operatorBinaryFloat(node, oper, Float(lhs.getInt()), rhs.getFloat());
        }
        if (rhs.isInt()) {
          // Both ints
          return this->operatorBinaryInt(node, oper, lhs.getInt(), rhs.getInt());
        }
        return this->raiseNode(b, "Expected right-hand side of arithmetic '" + OperatorProperties::str(oper), "' operator to be an 'int' or 'float', but got '", rhs.getRuntimeType().toString(), "' instead");
      }
      return this->raiseNode(b, "Expected left-hand side of arithmetic '" + OperatorProperties::str(oper), "' operator to be an 'int' or 'float', but got '", rhs.getRuntimeType().toString(), "' instead");
    }
    Variant operatorLogicalNot(const INode& a) {
      auto value = this->expression(a);
      if (value.hasFlowControl()) {
        return value;
      }
      if (!value.isBool()) {
        return this->raiseNode(a, "Expected operand of unary '!' operator to be a 'bool', but got '", value.getRuntimeType().toString(), "' instead");
      }
      return !value.getBool();
    }
    Variant operatorDeref(const INode& a) {
      auto value = this->expression(a);
      if (value.hasFlowControl()) {
        return value;
      }
      if (!value.hasPointer()) {
        return this->raiseNode(a, "Expected operand of unary '*' operator to be a pointer, but got '", value.getRuntimeType().toString(), "' instead");
      }
      return value.getPointee();
    }
    Variant operatorRef(const INode& a) {
      if (a.getOpcode() != OPCODE_IDENTIFIER) {
        return this->raiseNode(a, "Expected operand of unary '&' operator to be a variable identifier");
      }
      auto name = this->identifier(a);
      auto* symbol = this->symtable->get(name);
      if (symbol == nullptr) {
        return this->raiseNode(a, "Unknown identifier for unary '&' operator: '", name, "'");
      }
      symbol->value.indirect(this->allocator, *this->basket);
      return symbol->value.address();
    }
    Int operatorBinaryInt(const INode& node, Operator oper, Int lhs, Int rhs) {
      EGG_WARNING_SUPPRESS_SWITCH_BEGIN();
      switch (oper) {
      case OPERATOR_ADD:
        return lhs + rhs;
      case OPERATOR_SUB:
        return lhs - rhs;
      case OPERATOR_MUL:
        return lhs * rhs;
      case OPERATOR_DIV:
        return lhs / rhs;
      case OPERATOR_REM:
        return lhs % rhs;
      }
      EGG_WARNING_SUPPRESS_SWITCH_END();
      throw this->unexpectedOperator("binary int", node);
    }
    Float operatorBinaryFloat(const INode& node, Operator oper, Float lhs, Float rhs) {
      EGG_WARNING_SUPPRESS_SWITCH_BEGIN();
      switch (oper) {
      case OPERATOR_ADD:
        return lhs + rhs;
      case OPERATOR_SUB:
        return lhs - rhs;
      case OPERATOR_MUL:
        return lhs * rhs;
      case OPERATOR_DIV:
        return lhs / rhs;
      case OPERATOR_REM:
        return std::remainder(lhs, rhs);
      }
      EGG_WARNING_SUPPRESS_SWITCH_END();
      throw this->unexpectedOperator("binary float", node);
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
        return this->raiseNode(node, "Expected condition to evaluate to a 'bool' value, but got '", value.getRuntimeType().toString(), "' instead");
      }
      return value;
    }
    // Error handling
    template<typename... ARGS>
    Variant raiseNode(const INode& node, ARGS&&... args) {
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
      if (node.getOpcode() != OPCODE_IDENTIFIER) {
        throw this->unexpectedOpcode("identifier", node);
      }
      auto& child = node.getChild(0);
      if (child.getOpcode() != OPCODE_SVALUE) {
        throw this->unexpectedOpcode("svalue", child);
      }
      return child.getString();
    }
    Type type(const INode& node, const String& name = String()) {
      auto children = node.getChildren();
      EGG_WARNING_SUPPRESS_SWITCH_BEGIN();
      switch (node.getOpcode()) {
      case OPCODE_INFERRED:
        assert(children == 0);
        return nullptr;
      case OPCODE_VOID:
        assert(children == 0);
        return Type::Void;
      case OPCODE_NULL:
        assert(children == 0);
        return Type::Null;
      case OPCODE_BOOL:
        if (children == 0) {
          return Type::Bool;
        }
        break;
      case OPCODE_INT:
        if (children == 0) {
          return Type::Int;
        }
        break;
      case OPCODE_FLOAT:
        if (children == 0) {
          return Type::Float;
        }
        break;
      case OPCODE_STRING:
        if (children == 0) {
          return Type::String;
        }
        break;
      case OPCODE_OBJECT:
        if (children == 0) {
          return Type::Object;
        }
        if (children == 1) {
          auto& callable = node.getChild(0);
          if (callable.getOpcode() == OPCODE_CALLABLE) {
            return UserFunction::makeType(this->allocator, *this, name, callable);
          }
        }
        break;
      case OPCODE_ANY:
        if (children == 0) {
          return Type::Any;
        }
        break;
      case OPCODE_ANYQ:
        if (children == 0) {
          return Type::AnyQ;
        }
        break;
      case OPCODE_POINTER:
        if (children == 1) {
          auto pointee = this->type(node.getChild(0));
          return Type::makePointer(this->allocator, *pointee);
        }
        break;
      case OPCODE_UNION:
        if (children >= 1) {
          auto result = this->type(node.getChild(0));
          for (size_t i = 1; i < children; ++i) {
            result = Type::makeUnion(this->allocator, *result, *this->type(node.getChild(i)));
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
    template<typename... ARGS>
    Variant raiseLocation(const LocationSource& where, ARGS&&... args) const {
      return VariantFactory::createException(this->allocator, where, std::forward<ARGS>(args)...);
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
  case OPCODE_PROPERTY:
    this->flavour = program.targetProperty(node, this->a, this->b);
    return;
  case OPCODE_UNARY:
    if (node.getOperator() != OPERATOR_DEREF) {
      throw program.unexpectedOperator("target", node);
    }
    this->flavour = program.targetPointer(node, this->a);
    return;
  }
  EGG_WARNING_SUPPRESS_SWITCH_END();
  throw program.unexpectedOpcode("target", node);
}

Variant PredicateFunction::call(IExecution&, const IParameters& parameters) {
  assert(parameters.getNamedCount() == 0);
  assert(parameters.getPositionalCount() == 0);
  (void)parameters; // ignore the parameters
  return this->program.executePredicate(this->location, *this->node);
}

Variant UserFunction::call(IExecution&, const IParameters& parameters) {
  auto signature = this->type->callable();
  assert(signature != nullptr);
  assert(this->captured != nullptr);
  return this->program.executeCall(this->location, *signature, parameters, *this->block, *this->captured);
}

Variant Target::check() const {
  if (this->flavour == Flavour::Failed) {
    // Return the problem we saved in the constructor
    return this->a;
  }
  return Variant::Void;
}

Variant Target::assign(const Variant& rhs) const {
  return this->set(rhs);
}

Variant Target::nudge(Int rhs) const {
  assert(rhs != 0);
  Variant v;
  auto* p = this->ref();
  if (p != nullptr) {
    if (p->isInt()) {
      // Nudge directly
      *p = p->getInt() + rhs;
      return Variant::Void;
    }
  } else {
    // Need to get/nudge/set
    v = this->get();
    if (v.hasFlowControl()) {
      return v;
    }
    if (v.isInt()) {
      return this->set(v.getInt() + rhs);
    }
    p = &v;
  }
  if (rhs < 0) {
    return Binary::unexpected(*p, "Expected decrement '--' operation to be applied to an 'int' value");
  }
  return Binary::unexpected(*p, "Expected increment '++' operation to be applied to an 'int' value");
}

Variant Target::mutate(const INode& opnode, const INode& rhs) const {
  auto* p = this->ref();
  if (p != nullptr) {
    // Mutate directly
    return this->apply(opnode, *p, rhs);
  }
  // Need to get/mutate/set
  auto v = this->get();
  if (v.hasFlowControl()) {
    return v;
  }
  auto retval = this->apply(opnode, v, rhs);
  if (retval.hasFlowControl()) {
    return retval;
  }
  return this->set(v);
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
  auto rvalue = this->program.targetExpression(rhs);
  if (rvalue.hasFlowControl()) {
    return rvalue;
  }
  auto result = Binary::apply(oper, lvalue, rvalue);
  if (result.is(VariantBits::Break)) {
    throw this->program.unexpectedOperator("target binary", opnode);
  }
  return result;
}

Variant* Target::ref() const {
  // Return null iff we cannot modify in-place
  switch (this->flavour) {
  case Flavour::Identifier:
    break;
  case Flavour::Pointer:
    return &this->a.getPointee();
  case Flavour::Index:
  case Flavour::Property:
  case Flavour::Failed:
    return nullptr;
  }
  auto* value = &this->program.targetSymbol(this->node, this->a.getString()).value;
  if (value->hasIndirect()) {
    return &value->getPointee();
  }
  return value;
}

Variant Target::get() const {
  switch (this->flavour) {
  case Flavour::Identifier:
    return this->program.targetSymbol(this->node, this->a.getString()).value.direct();
  case Flavour::Index:
    return this->a.getObject()->getIndex(this->program, this->b).direct();
  case Flavour::Pointer:
    return this->a.getPointee();
  case Flavour::Property:
    return this->a.getObject()->getProperty(this->program, this->b.getString()).direct();
  case Flavour::Failed:
    break;
  }
  // Return the problem we saved in the constructor
  return this->a;
}

Variant Target::set(const Variant& value) const {
  switch (this->flavour) {
  case Flavour::Identifier:
    return this->program.targetSymbol(this->node, this->a.getString()).tryAssign(this->program, value);
  case Flavour::Index:
    return this->a.getObject()->setIndex(this->program, this->b, value);
  case Flavour::Pointer:
    this->a.getPointee() = value;
    return Variant::Void; // TODO always succeeds?
  case Flavour::Property:
    return this->a.getObject()->setProperty(this->program, this->b.getString(), value);
  case Flavour::Failed:
    break;
  }
  // Return the problem we saved in the constructor
  return this->a;
}

Block::~Block() {
  // Undeclare all the names successfully declared by this block
  for (auto& i : this->declared) {
    this->program.blockUndeclare(i);
  }
}

egg::ovum::Variant Block::declare(const LocationSource& source, const Type& type, const String& name, const Variant* init) {
  // Keep track of names successfully declared in this block
  auto* symbol = this->program.blockDeclare(source, type, name);
  if (symbol == nullptr) {
    return this->program.raiseLocation(source, "Duplicate name in declaration: '", name, "'");
  }
  if (init != nullptr) {
    auto retval = symbol->tryAssign(this->program, *init);
    if (!retval.isVoid()) {
      this->program.blockUndeclare(name);
      return retval;
    }
  }
  this->declared.insert(name);
  return Variant::Void;
}

egg::ovum::Variant Block::guard(const LocationSource& source, const Type& type, const String& name, const Variant& init) {
  // Keep track of names successfully declared in this block
  auto* symbol = this->program.blockDeclare(source, type, name);
  if (symbol == nullptr) {
    return this->program.raiseLocation(source, "Duplicate name in declaration: '", name, "'");
  }
  auto retval = symbol->tryAssign(this->program, init);
  if (!retval.isVoid()) {
    this->program.blockUndeclare(name);
    return Variant::False;
  }
  this->declared.insert(name);
  return Variant::True;
}

egg::ovum::Type UserFunction::makeType(IAllocator& allocator, ProgramDefault& program, const String& name, const INode& callable) {
  // Create a type appropriate for a standard "user" function
  assert(callable.getOpcode() == OPCODE_CALLABLE);
  auto n = callable.getChildren();
  assert(n >= 1);
  auto rettype = program.type(callable.getChild(0));
  assert(rettype != nullptr);
  auto function = allocator.make<FunctionType>(name, rettype);
  for (size_t i = 1; i < n; ++i) {
    auto& parameter = callable.getChild(i);
    auto c = parameter.getChildren();
    size_t pindex = i - 1;
    IFunctionSignatureParameter::Flags pflags;
    EGG_WARNING_SUPPRESS_SWITCH_BEGIN();
    switch (parameter.getOpcode()) {
    case OPCODE_REQUIRED:
      assert((c == 1) || (c == 2));
      pflags = IFunctionSignatureParameter::Flags::Required;
      break;
    case OPCODE_OPTIONAL:
      assert((c == 1) || (c == 2));
      pflags = IFunctionSignatureParameter::Flags::None;
      break;
    case OPCODE_VARARGS:
      assert(c >= 2);
      pflags = IFunctionSignatureParameter::Flags::Variadic;
      if (c > 2) {
        // TODO We assume the constraint is "length > 0" meaning required
        pflags = Bits::set(pflags, IFunctionSignatureParameter::Flags::Required);
      }
      break;
    case OPCODE_BYNAME:
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
