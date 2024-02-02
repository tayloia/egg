#include "ovum/ovum.h"
#include "ovum/operation.h"

#include <deque>
#include <stack>

namespace {
  class VMModule;

  egg::ovum::Type getHardTypeOrNone(const egg::ovum::HardValue& value) {
    egg::ovum::Type type;
    if (value->getHardType(type)) {
      return type;
    }
    assert(false);
    return egg::ovum::Type::None;
  }
}

class egg::ovum::IVMModule::Node : public HardReferenceCounted<IHardAcquireRelease> {
  Node(const Node&) = delete;
  Node& operator=(const Node&) = delete;
private:
  HardPtr<Node> chain; // Internal linked list of all known nodes in this module
public:
  enum class Kind {
    Root,
    ExprUnaryOp,
    ExprBinaryOp,
    ExprTernaryOp,
    ExprPredicateOp,
    ExprLiteral,
    ExprFunctionCall,
    ExprVariableGet,
    ExprPropertyGet,
    ExprIndexGet,
    ExprPointeeGet,
    ExprVariableRef,
    ExprPropertyRef,
    ExprIndexRef,
    ExprArray,
    ExprObject,
    ExprGuard,
    ExprNamed,
    TypeInfer,
    TypeLiteral,
    TypeManifestation,
    StmtBlock,
    StmtFunctionDefine,
    StmtFunctionCapture,
    StmtFunctionInvoke,
    StmtVariableDeclare,
    StmtVariableDefine,
    StmtVariableSet,
    StmtVariableMutate,
    StmtVariableUndeclare,
    StmtPropertySet,
    StmtPropertyMutate,
    StmtIndexMutate,
    StmtPointerMutate,
    StmtIf,
    StmtWhile,
    StmtDo,
    StmtForEach,
    StmtForLoop,
    StmtSwitch,
    StmtCase,
    StmtBreak,
    StmtContinue,
    StmtThrow,
    StmtTry,
    StmtCatch,
    StmtRethrow,
    StmtReturn,
    StmtYield,
    StmtYieldAll,
    StmtYieldBreak,
    StmtYieldContinue
  };
  VMModule& module;
  Kind kind;
  SourceRange range; // May be zero
  HardValue literal; // Only stores simple literals
  union {
    ValueUnaryOp unaryOp;
    ValueBinaryOp binaryOp;
    ValueTernaryOp ternaryOp;
    ValueMutationOp mutationOp;
    ValuePredicateOp predicateOp;
    size_t defaultIndex;
  };
  std::vector<Node*> children; // Reference-counting hard pointers are stored in the chain
  Node(VMModule& module, Kind kind, const SourceRange& range, Node* chain)
    : HardReferenceCounted<IHardAcquireRelease>(),
      chain(chain),
      module(module),
      kind(kind),
      range(range) {
  }
  void addChild(Node& child) {
    this->children.push_back(&child);
  }
  void printLocation(Printer& printer) const;
protected:
  virtual void hardDestroy() const override;
};

namespace {
  using namespace egg::ovum;

  template<typename T>
  std::string describe(const T& value) {
    std::stringstream ss;
    Printer printer{ ss, Print::Options::DEFAULT };
    printer.describe(value);
    return ss.str();
  }

  std::string describe(const HardValue& value) {
    return describe(value.get());
  }

  class VMRunner;

  class VMCallStack : public HardReferenceCountedAllocator<IVMCallStack> {
    VMCallStack(const VMCallStack&) = delete;
    VMCallStack& operator=(const VMCallStack&) = delete;
  public:
    String resource;
    SourceRange range;
    String function;
  public:
    explicit VMCallStack(IAllocator& allocator)
      : HardReferenceCountedAllocator<IVMCallStack>(allocator) {
    }
    virtual const IVMCallStack* getCaller() const override {
      // TODO
      return nullptr;
    }
    virtual String getResource() const override {
      return this->resource;
    }
    virtual const SourceRange* getSourceRange() const override {
      return &this->range;
    }
    virtual String getFunction() const override {
      return this->function;
    }
    virtual void print(Printer& printer) const override {
      if (!this->resource.empty() || !this->range.empty()) {
        printer << this->resource << this->range << ": ";
      }
      if (!this->function.empty()) {
        printer << this->function << ": ";
      }
    }
  };

  template<typename T>
  class VMCommon : public T {
    VMCommon(const VMCommon&) = delete;
    VMCommon& operator=(const VMCommon&) = delete;
  protected:
    IVM& vm;
  public:
    explicit VMCommon(IVM& vm)
      : vm(vm) {
    }
    virtual void log(ILogger::Source source, ILogger::Severity severity, const String& message) {
      this->vm.getLogger().log(source, severity, message);
    }
    virtual String createStringUTF8(const void* utf8, size_t bytes, size_t codepoints) override {
      return this->vm.createStringUTF8(utf8, bytes, codepoints);
    }
    virtual String createStringUTF32(const void* utf32, size_t codepoints) override {
      return this->vm.createStringUTF32(utf32, codepoints);
    }
    virtual HardValue createHardValueVoid() override {
      return this->vm.createHardValueVoid();
    }
    virtual HardValue createHardValueNull() override {
      return this->vm.createHardValueNull();
    }
    virtual HardValue createHardValueBool(Bool value) override {
      return this->vm.createHardValueBool(value);
    }
    virtual HardValue createHardValueInt(Int value) override {
      return this->vm.createHardValueInt(value);
    }
    virtual HardValue createHardValueFloat(Float value) override {
      return this->vm.createHardValueFloat(value);
    }
    virtual HardValue createHardValueString(const String& value) override {
      return this->vm.createHardValueString(value);
    }
    virtual HardValue createHardValueObject(const HardObject& value) override {
      return this->vm.createHardValueObject(value);
    }
    virtual HardValue createHardValueType(const Type& value) override {
      return this->vm.createHardValueType(value);
    }
  };

  template<typename T>
  class VMCollectable : public VMCommon<SoftReferenceCounted<T>> {
    VMCollectable(const VMCollectable&) = delete;
    VMCollectable& operator=(const VMCollectable&) = delete;
  protected:
    explicit VMCollectable(IVM& vm)
      : VMCommon<SoftReferenceCounted<T>>(vm) {
    }
    virtual void hardDestroy() const override {
      this->vm.getAllocator().destroy(this);
    }
  public:
    virtual IAllocator& getAllocator() const override {
      return this->vm.getAllocator();
    }
  };

  template<typename T>
  class VMUncollectable : public VMCommon<HardReferenceCounted<T>> {
    VMUncollectable(const VMUncollectable&) = delete;
    VMUncollectable& operator=(const VMUncollectable&) = delete;
  protected:
    explicit VMUncollectable(IVM& vm)
      : VMCommon<HardReferenceCounted<T>>(vm) {
    }
    virtual void hardDestroy() const override {
      this->vm.getAllocator().destroy(this);
    }
  public:
    virtual IAllocator& getAllocator() const override {
      return this->vm.getAllocator();
    }
  };

  class VMModule;

  class VMProgram : public VMUncollectable<IVMProgram> {
    VMProgram(const VMProgram&) = delete;
    VMProgram& operator=(const VMProgram&) = delete;
  private:
    std::vector<HardPtr<VMModule>> modules;
    std::map<String, Type> builtins;
  public:
    explicit VMProgram(IVM& vm)
      : VMUncollectable(vm) {
    }
    virtual HardPtr<IVMRunner> createRunner() override;
    virtual size_t getModuleCount() const override {
      return this->modules.size();
    }
    virtual HardPtr<IVMModule> getModule(size_t index) const override {
      if (index < this->modules.size()) {
        return this->modules[index];
      }
      return nullptr;
    }
    void addModule(VMModule& module) {
      this->modules.emplace_back(&module);
    }
    bool addBuiltin(const String& symbol, const Type& type) {
      return this->builtins.emplace(symbol, type).second;
    }
    const std::map<String, Type>& getBuiltins() const {
      return this->builtins;
    }
  };

  class VMModule : public VMUncollectable<IVMModule> {
    VMModule(const VMModule&) = delete;
    VMModule& operator=(const VMModule&) = delete;
  private:
    String resource;
    HardPtr<Node> chain; // Head of the linked list of all known nodes in this module
    Node* root;
  public:
    VMModule(IVM& vm, const String& resource)
      : VMUncollectable(vm),
        resource(resource) {
      this->root = this->getAllocator().makeRaw<Node>(*this, Node::Kind::Root, SourceRange{}, nullptr);
      this->chain.set(this->root);
    }
    virtual HardPtr<IVMRunner> createRunner(IVMProgram& program) override;
    const String& getResource() const {
      return this->resource;
    }
    Node& getRoot() const {
      return *this->root;
    }
    Node& createNode(Node::Kind kind, const SourceRange& range) {
      // Make sure we add the node to the internal linked list
      auto* node = this->getAllocator().makeRaw<Node>(*this, kind, range, this->chain.get());
      assert(node != nullptr);
      this->chain.set(node);
      return *node;
    }
  };

  // Only instantiated by composition within 'VMRunner' etc.
  struct VMSymbolTable {
  public:
    using Kind = VMSymbolKind;
    struct Entry {
      Kind kind;
      Type type;
      IValue* soft;
    };
  private:
    struct Table {
      std::map<String, Entry> entries;
      Entry* insert(Kind kind, const String& name, const Type& type, IValue* soft) {
        // Returns any extant entry
        assert(soft != nullptr);
        assert(soft->softGetBasket() != nullptr);
        auto iterator = this->entries.emplace(name, Entry{ kind, type, soft });
        return iterator.second ? nullptr : &iterator.first->second;
      }
      Entry* find(const String& name) {
        auto iterator = this->entries.find(name);
        if (iterator == this->entries.end()) {
          return nullptr;
        }
        return &iterator->second;
      }
      bool remove(const String& name) {
        return this->entries.erase(name) == 1;
      }
    };
    std::deque<Table> stack;
  public:
    void push() {
      this->stack.emplace_front();
    }
    void pop() {
      assert(this->stack.size() > 1);
      this->stack.pop_front();
    }
    void builtin(const String& name, IValue* soft) {
      // You can only add builtins to the base of the chain
      assert(this->stack.size() == 1);
      auto extant = this->stack.front().insert(Kind::Builtin, name, soft->getRuntimeType(), soft);
      assert(extant == nullptr);
      (void)extant;
    }
    Entry* add(Kind kind, const String& name, const Type& type, IValue* soft) {
      // Returns any extant entry
      assert(!this->stack.empty());
      return this->stack.front().insert(kind, name, type, soft);
    }
    bool remove(const String& name) {
      // Only removes from the head of the chain
      assert(!this->stack.empty());
      return this->stack.front().remove(name);
    }
    Entry* find(const String& name) {
      // Searches the chain from front to back
      for (auto& table : this->stack) {
        auto* found = table.find(name);
        if (found != nullptr) {
          return found;
        }
      }
      return nullptr;
    }
    void softVisit(ICollectable::IVisitor& visitor) const {
      for (const auto& table : this->stack) {
        for (const auto& entry : table.entries) {
          assert(entry.second.soft != nullptr);
          visitor.visit(*entry.second.soft);
        }
      }
    }
    void print(Printer& printer) const {
      // TODO debugging only
      auto frame = 0;
      for (const auto& table : this->stack) {
        printer << "=== SYMBOL TABLE FRAME " << frame++ << " (size=" << table.entries.size() << ") ===\n";
        for (const auto& entry : table.entries) {
          auto& name = entry.first;
          auto& value = entry.second;
          switch (value.kind) {
          case Kind::Builtin:
            printer << "   BUILTIN ";
            break;
          case Kind::Variable:
            printer << "  VARIABLE ";
            break;
          case Kind::Type:
            printer << "      TYPE ";
            break;
          }
          printer << value.type << ' ' << name << " = " << value.soft << '\n';
        }
      }
      printer << "=== SYMBOL TABLE END ===";
    }
  };

  // Only instantiated by composition within 'VMRunner' etc.
  class VMExecution final : public VMCommon<IVMExecution> {
    VMExecution(const VMExecution&) = delete;
    VMExecution& operator=(const VMExecution&) = delete;
  public:
    VMRunner* runner;
  public:
    explicit VMExecution(IVM& vm)
      : VMCommon<IVMExecution>(vm),
        runner(nullptr) {
    }
    virtual HardValue raiseException(const HardValue& inner) override {
      auto& allocator = this->vm.getAllocator();
      return ValueFactory::createHardThrow(allocator, inner);
    }
    virtual HardValue raiseRuntimeError(const String& message, const SourceRange* source) override;
    virtual HardValue initiateTailCall(const IFunctionSignature& signature, const ICallArguments& arguments, const IVMModule::Node& definition, const IVMCallCaptures* captures) override;
    virtual bool assignValue(IValue& lhs, const Type& ltype, const IValue& rhs) override {
      // Assign with int-to-float promotion
      assert(ltype != nullptr);
      auto rtype = rhs.getRuntimeType();
      assert(rtype != nullptr);
      if (ltype->isPrimitive()) {
        if (rtype->isPrimitive()) {
          // Assigning a primitive value
          assert(rtype->getPrimitiveFlags() == rhs.getPrimitiveFlag());
          return this->assignPrimitive(lhs, ltype->getPrimitiveFlags(), rhs, rhs.getPrimitiveFlag());
        }
        return this->assignPrimitive(lhs, ltype->getPrimitiveFlags(), rhs, ValueFlags::Object);
      }
      switch (this->vm.getTypeForge().isTypeAssignable(ltype, rtype)) {
      case Assignability::Never:
        return false;
      case Assignability::Always:
        return lhs.set(rhs);
      case Assignability::Sometimes:
        break;
      }
      // TODO
      assert(false);
      return false;
    }
    virtual HardValue getSoftValue(const SoftValue& soft) override {
      return this->vm.getSoftValue(soft);
    }
    virtual bool setSoftValue(SoftValue& lhs, const HardValue& rhs) override {
      return this->vm.setSoftValue(lhs, rhs);
    }
    virtual HardValue mutateSoftValue(SoftValue& lhs, ValueMutationOp mutation, const HardValue& rhs) override {
      return this->vm.mutateSoftValue(lhs, mutation, rhs);
    }
    virtual HardValue evaluateValueUnaryOp(ValueUnaryOp op, const HardValue& arg) override {
      return this->augment(this->unary(op, arg));
    }
    virtual HardValue evaluateValueBinaryOp(ValueBinaryOp op, const HardValue& lhs, const HardValue& rhs) override {
      return this->augment(this->binary(op, lhs, rhs));
    }
    virtual HardValue evaluateValueTernaryOp(ValueTernaryOp op, const HardValue& lhs, const HardValue& mid, const HardValue& rhs) override {
      return this->augment(this->ternary(op, lhs, mid, rhs));
    }
    virtual HardValue precheckValueMutationOp(ValueMutationOp op, HardValue& lhs, ValueFlags rhs) override {
      // Handle short-circuits (returns 'Continue' if rhs should be evaluated)
      return this->augment(this->precheck(op, lhs, rhs));
    }
    virtual HardValue evaluateValueMutationOp(ValueMutationOp op, HardValue& lhs, const HardValue& rhs) override {
      // Return the value before the mutation
      return this->augment(this->mutate(op, lhs, rhs));
    }
    virtual HardValue evaluateValuePredicateOp(ValuePredicateOp op, const HardValue& lhs, const HardValue& rhs) override {
      return this->augment(this->predicate(op, lhs, rhs));
    }
    virtual HardValue debugSymtable() override;
    template<typename... ARGS>
    String concat(ARGS&&... args) {
      return StringBuilder::concat(this->vm.getAllocator(), std::forward<ARGS>(args)...);
    }
  private:
    template<typename... ARGS>
    HardValue panic(ARGS&&... args) {
      auto exception = this->createHardValueString(this->concat(std::forward<ARGS>(args)...));
      return this->raiseException(exception);
    }
    template<typename... ARGS>
    HardValue raise(ARGS&&... args) {
      // TODO: Non-string exception?
      auto exception = this->createHardValueString(this->concat(std::forward<ARGS>(args)...));
      return this->raiseException(exception);
    }
    HardValue augment(const HardValue& value) {
      // TODO what if the user program throws a raw string within an expression?
      if (value->getPrimitiveFlag() == (ValueFlags::Throw | ValueFlags::String)) {
        HardValue inner;
        if (value->getInner(inner)) {
          String message;
          if (inner->getString(message)) {
            return this->raiseRuntimeError(message, nullptr);
          }
        }
      }
      return value;
    }
    HardValue unary(ValueUnaryOp op, const HardValue& arg) {
      Operation::UnaryValue unaryValue;
      switch (op) {
      case ValueUnaryOp::Negate:
        switch (unaryValue.extractArithmetic(arg)) {
        case Operation::UnaryValue::ArithmeticResult::Int:
          return this->createHardValueInt(-unaryValue.i);
        case Operation::UnaryValue::ArithmeticResult::Float:
          return this->createHardValueFloat(-unaryValue.f);
        case Operation::UnaryValue::ArithmeticResult::Mismatch:
          return this->raise("Expected expression after negation operator '-' to be an 'int' or 'float', but instead got ", describe(arg));
        }
        break;
      case ValueUnaryOp::BitwiseNot:
        switch (unaryValue.extractInt(arg)) {
        case Operation::UnaryValue::ExtractResult::Match:
          return this->createHardValueInt(~unaryValue.i);
        case Operation::UnaryValue::ExtractResult::Mismatch:
          return this->raise("Expected expression after bitwise-not operator '~' to be an 'int', but instead got ", describe(arg));
        }
        break;
      case ValueUnaryOp::LogicalNot:
        switch (unaryValue.extractBool(arg)) {
        case Operation::UnaryValue::ExtractResult::Match:
          return this->createHardValueBool(!unaryValue.b);
        case Operation::UnaryValue::ExtractResult::Mismatch:
          return this->raise("Expected expression after of logical-not operator '!' to be a 'bool', but instead got ", describe(arg));
        }
        break;
      }
      return this->panic("Unknown unary operator: ", op);
    }
    HardValue binary(ValueBinaryOp op, const HardValue& lhs, const HardValue& rhs) {
      Operation::BinaryValues binaryValues;
      switch (op) {
      case ValueBinaryOp::Add:
        switch (binaryValues.promote(lhs, rhs)) {
        case Operation::BinaryValues::PromotionResult::Ints:
          return this->createHardValueInt(binaryValues.i[0] + binaryValues.i[1]);
        case Operation::BinaryValues::PromotionResult::Floats:
          return this->createHardValueFloat(binaryValues.f[0] + binaryValues.f[1]);
        case Operation::BinaryValues::PromotionResult::BadLeft:
          return this->raise("Expected left-hand side of addition operator '+' to be an 'int' or 'float', but instead got ", describe(lhs));
        case Operation::BinaryValues::PromotionResult::BadRight:
          return this->raise("Expected right-hand side of addition operator '+' to be an 'int' or 'float', but instead got ", describe(rhs));
        }
        break;
      case ValueBinaryOp::Subtract:
        switch (binaryValues.promote(lhs, rhs)) {
        case Operation::BinaryValues::PromotionResult::Ints:
          return this->createHardValueInt(binaryValues.i[0] - binaryValues.i[1]);
        case Operation::BinaryValues::PromotionResult::Floats:
          return this->createHardValueFloat(binaryValues.f[0] - binaryValues.f[1]);
        case Operation::BinaryValues::PromotionResult::BadLeft:
          return this->raise("Expected left-hand side of subtraction operator '-' to be an 'int' or 'float', but instead got ", describe(lhs));
        case Operation::BinaryValues::PromotionResult::BadRight:
          return this->raise("Expected right-hand side of subtraction operator '-' to be an 'int' or 'float', but instead got ", describe(rhs));
        }
        break;
      case ValueBinaryOp::Multiply:
        switch (binaryValues.promote(lhs, rhs)) {
        case Operation::BinaryValues::PromotionResult::Ints:
          return this->createHardValueInt(binaryValues.i[0] * binaryValues.i[1]);
        case Operation::BinaryValues::PromotionResult::Floats:
          return this->createHardValueFloat(binaryValues.f[0] * binaryValues.f[1]);
        case Operation::BinaryValues::PromotionResult::BadLeft:
          return this->raise("Expected left-hand side of multiplication operator '*' to be an 'int' or 'float', but instead got ", describe(lhs));
        case Operation::BinaryValues::PromotionResult::BadRight:
          return this->raise("Expected right-hand side of multiplication operator '*' to be an 'int' or 'float', but instead got ", describe(rhs));
        }
        break;
      case ValueBinaryOp::Divide:
        switch (binaryValues.promote(lhs, rhs)) {
        case Operation::BinaryValues::PromotionResult::Ints:
          if (binaryValues.i[1] == 0) {
            return this->raise("Integer division by zero in division operator '/'");
          }
          return this->createHardValueInt(binaryValues.i[0] / binaryValues.i[1]);
        case Operation::BinaryValues::PromotionResult::Floats:
          return this->createHardValueFloat(binaryValues.f[0] / binaryValues.f[1]);
        case Operation::BinaryValues::PromotionResult::BadLeft:
          return this->raise("Expected left-hand side of division operator '/' to be an 'int' or 'float', but instead got ", describe(lhs));
        case Operation::BinaryValues::PromotionResult::BadRight:
          return this->raise("Expected right-hand side of division operator '/' to be an 'int' or 'float', but instead got ", describe(rhs));
        }
        break;
      case ValueBinaryOp::Remainder:
        switch (binaryValues.promote(lhs, rhs)) {
        case Operation::BinaryValues::PromotionResult::Ints:
          if (binaryValues.i[1] == 0) {
            return this->raise("Integer division by zero in remainder operator '%'");
          }
          return this->createHardValueInt(binaryValues.i[0] % binaryValues.i[1]);
        case Operation::BinaryValues::PromotionResult::Floats:
          return this->createHardValueFloat(std::fmod(binaryValues.f[0], binaryValues.f[1]));
        case Operation::BinaryValues::PromotionResult::BadLeft:
          return this->raise("Expected left-hand side of remainder operator '%' to be an 'int' or 'float', but instead got ", describe(lhs));
        case Operation::BinaryValues::PromotionResult::BadRight:
          return this->raise("Expected right-hand side of remainder operator '%' to be an 'int' or 'float', but instead got ", describe(rhs));
        }
        break;
      case ValueBinaryOp::LessThan:
        switch (binaryValues.promote(lhs, rhs)) {
        case Operation::BinaryValues::PromotionResult::Ints:
          return this->createHardValueBool(binaryValues.compareInts(Arithmetic::Compare::LessThan));
        case Operation::BinaryValues::PromotionResult::Floats:
          return this->createHardValueBool(binaryValues.compareFloats(Arithmetic::Compare::LessThan, false));
        case Operation::BinaryValues::PromotionResult::BadLeft:
          return this->raise("Expected left-hand side of comparison operator '<' to be an 'int' or 'float', but instead got ", describe(lhs));
        case Operation::BinaryValues::PromotionResult::BadRight:
          return this->raise("Expected right-hand side of comparison operator '<' to be an 'int' or 'float', but instead got ", describe(rhs));
        }
        break;
      case ValueBinaryOp::LessThanOrEqual:
        switch (binaryValues.promote(lhs, rhs)) {
        case Operation::BinaryValues::PromotionResult::Ints:
          return this->createHardValueBool(binaryValues.compareInts(Arithmetic::Compare::LessThanOrEqual));
        case Operation::BinaryValues::PromotionResult::Floats:
          return this->createHardValueBool(binaryValues.compareFloats(Arithmetic::Compare::LessThanOrEqual, false));
        case Operation::BinaryValues::PromotionResult::BadLeft:
          return this->raise("Expected left-hand side of comparison operator '<=' to be an 'int' or 'float', but instead got ", describe(lhs));
        case Operation::BinaryValues::PromotionResult::BadRight:
          return this->raise("Expected right-hand side of comparison operator '<=' to be an 'int' or 'float', but instead got ", describe(rhs));
        }
        break;
      case ValueBinaryOp::Equal:
        // Promote ints to floats but ignore IEEE NaN semantics
        return this->createHardValueBool(Operation::areEqual(lhs, rhs, true, false));
      case ValueBinaryOp::NotEqual:
        // Promote ints to floats but ignore IEEE NaN semantics
        return this->createHardValueBool(!Operation::areEqual(lhs, rhs, true, false));
      case ValueBinaryOp::GreaterThanOrEqual:
        switch (binaryValues.promote(lhs, rhs)) {
        case Operation::BinaryValues::PromotionResult::Ints:
          return this->createHardValueBool(binaryValues.compareInts(Arithmetic::Compare::GreaterThanOrEqual));
        case Operation::BinaryValues::PromotionResult::Floats:
          return this->createHardValueBool(binaryValues.compareFloats(Arithmetic::Compare::GreaterThanOrEqual, false));
        case Operation::BinaryValues::PromotionResult::BadLeft:
          return this->raise("Expected left-hand side of comparison operator '>=' to be an 'int' or 'float', but instead got ", describe(lhs));
        case Operation::BinaryValues::PromotionResult::BadRight:
          return this->raise("Expected right-hand side of comparison operator '>=' to be an 'int' or 'float', but instead got ", describe(rhs));
        }
        break;
      case ValueBinaryOp::GreaterThan:
        switch (binaryValues.promote(lhs, rhs)) {
        case Operation::BinaryValues::PromotionResult::Ints:
          return this->createHardValueBool(binaryValues.compareInts(Arithmetic::Compare::GreaterThan));
        case Operation::BinaryValues::PromotionResult::Floats:
          return this->createHardValueBool(binaryValues.compareFloats(Arithmetic::Compare::GreaterThan, false));
        case Operation::BinaryValues::PromotionResult::BadLeft:
          return this->raise("Expected left-hand side of comparison operator '>' to be an 'int' or 'float', but instead got ", describe(lhs));
        case Operation::BinaryValues::PromotionResult::BadRight:
          return this->raise("Expected right-hand side of comparison operator '>' to be an 'int' or 'float', but instead got ", describe(rhs));
        }
        break;
      case ValueBinaryOp::BitwiseAnd:
        switch (binaryValues.extractBitwise(lhs, rhs)) {
        case Operation::BinaryValues::BitwiseResult::Bools:
          return this->createHardValueBool(bool(binaryValues.b[0] & binaryValues.b[1]));
        case Operation::BinaryValues::BitwiseResult::Ints:
          return this->createHardValueInt(binaryValues.i[0] & binaryValues.i[1]);
        case Operation::BinaryValues::BitwiseResult::BadLeft:
          return this->raise("Expected left-hand side of bitwise-and operator '&' to be a 'bool' or 'int', but instead got ", describe(lhs));
        case Operation::BinaryValues::BitwiseResult::BadRight:
          return this->raise("Expected right-hand side of bitwise-and operator '&' to be a 'bool' or 'int', but instead got ", describe(rhs));
        case Operation::BinaryValues::BitwiseResult::Mismatch:
          return this->raise("Type mismatch in bitwise-and operator '&': left-hand side is ", describe(lhs), ", but right-hand side is ", describe(rhs));
        }
        break;
      case ValueBinaryOp::BitwiseOr:
        switch (binaryValues.extractBitwise(lhs, rhs)) {
        case Operation::BinaryValues::BitwiseResult::Bools:
          return this->createHardValueBool(bool(binaryValues.b[0] | binaryValues.b[1]));
        case Operation::BinaryValues::BitwiseResult::Ints:
          return this->createHardValueInt(binaryValues.i[0] | binaryValues.i[1]);
        case Operation::BinaryValues::BitwiseResult::BadLeft:
          return this->raise("Expected left-hand side of bitwise-or operator '|' to be a 'bool' or 'int', but instead got ", describe(lhs));
        case Operation::BinaryValues::BitwiseResult::BadRight:
          return this->raise("Expected right-hand side of bitwise-or operator '|' to be a 'bool' or 'int', but instead got ", describe(rhs));
        case Operation::BinaryValues::BitwiseResult::Mismatch:
          return this->raise("Type mismatch in bitwise-or operator '|': left-hand side is ", describe(lhs), ", but right-hand side is ", describe(rhs));
        }
        break;
      case ValueBinaryOp::BitwiseXor:
        switch (binaryValues.extractBitwise(lhs, rhs)) {
        case Operation::BinaryValues::BitwiseResult::Bools:
          return this->createHardValueBool(bool(binaryValues.b[0] ^ binaryValues.b[1]));
        case Operation::BinaryValues::BitwiseResult::Ints:
          return this->createHardValueInt(binaryValues.i[0] ^ binaryValues.i[1]);
        case Operation::BinaryValues::BitwiseResult::BadLeft:
          return this->raise("Expected left-hand side of bitwise-xor operator '^' to be a 'bool' or 'int', but instead got ", describe(lhs));
        case Operation::BinaryValues::BitwiseResult::BadRight:
          return this->raise("Expected right-hand side of bitwise-xor operator '^' to be a 'bool' or 'int', but instead got ", describe(rhs));
        case Operation::BinaryValues::BitwiseResult::Mismatch:
          return this->raise("Type mismatch in bitwise-xor operator '^': left-hand side is ", describe(lhs), ", but right-hand side is ", describe(rhs));
        }
        break;
      case ValueBinaryOp::ShiftLeft:
        switch (binaryValues.extractInts(lhs, rhs)) {
        case Operation::BinaryValues::ExtractResult::Match:
          return this->createHardValueInt(binaryValues.shiftInts(Arithmetic::Shift::ShiftLeft));
        case Operation::BinaryValues::ExtractResult::BadLeft:
          return this->raise("Expected left-hand side of left-shift operator '<<' to be an 'int', but instead got ", describe(lhs));
        case Operation::BinaryValues::ExtractResult::BadRight:
          return this->raise("Expected right-hand side of left-shift operator '<<' to be an 'int', but instead got ", describe(rhs));
        }
        break;
      case ValueBinaryOp::ShiftRight:
        switch (binaryValues.extractInts(lhs, rhs)) {
        case Operation::BinaryValues::ExtractResult::Match:
          return this->createHardValueInt(binaryValues.shiftInts(Arithmetic::Shift::ShiftRight));
        case Operation::BinaryValues::ExtractResult::BadLeft:
          return this->raise("Expected left-hand side of right-shift operator '>>' to be an 'int', but instead got ", describe(lhs));
        case Operation::BinaryValues::ExtractResult::BadRight:
          return this->raise("Expected right-hand side of right-shift operator '>>' to be an 'int', but instead got ", describe(rhs));
        }
        break;
      case ValueBinaryOp::ShiftRightUnsigned:
        switch (binaryValues.extractInts(lhs, rhs)) {
        case Operation::BinaryValues::ExtractResult::Match:
          return this->createHardValueInt(binaryValues.shiftInts(Arithmetic::Shift::ShiftRightUnsigned));
        case Operation::BinaryValues::ExtractResult::BadLeft:
          return this->raise("Expected left-hand side of unsigned-shift operator '>>>' to be an 'int', but instead got ", describe(lhs));
        case Operation::BinaryValues::ExtractResult::BadRight:
          return this->raise("Expected right-hand side of unsigned-shift operator '>>>' to be an 'int', but instead got ", describe(rhs));
        }
        break;
      case ValueBinaryOp::IfNull:
        // Too late to truly short-circuit here
        return lhs->getNull() ? rhs : lhs;
      case ValueBinaryOp::IfFalse:
        // Too late to truly short-circuit here
        switch (binaryValues.extractBools(lhs, rhs)) {
        case Operation::BinaryValues::ExtractResult::Match:
          return this->createHardValueBool(binaryValues.b[0] || binaryValues.b[1]);
        case Operation::BinaryValues::ExtractResult::BadLeft:
          return this->raise("Expected left-hand side of logical-or operator '||' to be a 'bool', but instead got ", describe(lhs));
        case Operation::BinaryValues::ExtractResult::BadRight:
          return this->raise("Expected right-hand side of logical-or operator '||' to be a 'bool', but instead got ", describe(rhs));
        }
        break;
      case ValueBinaryOp::IfTrue:
        // Too late to truly short-circuit here
        switch (binaryValues.extractBools(lhs, rhs)) {
        case Operation::BinaryValues::ExtractResult::Match:
          return this->createHardValueBool(binaryValues.b[0] && binaryValues.b[1]);
        case Operation::BinaryValues::ExtractResult::BadLeft:
          return this->raise("Expected left-hand side of logical-and operator '&&' to be a 'bool', but instead got ", describe(lhs));
        case Operation::BinaryValues::ExtractResult::BadRight:
          return this->raise("Expected right-hand side of logical-and operator '&&' to be a 'bool', but instead got ", describe(rhs));
        }
        break;
      }
      return this->panic("Unknown binary operator: ", op);
    }
    HardValue ternary(ValueTernaryOp op, const HardValue& lhs, const HardValue& mid, const HardValue& rhs) {
      Bool condition;
      switch (op) {
      case ValueTernaryOp::IfThenElse:
        if (lhs->getBool(condition)) {
          return condition ? mid : rhs;
        }
        return this->raise("Expected condition of ternary operator '?:' to be a 'bool', but instead got ", describe(lhs));
      }
      return this->panic("Unknown ternary operator: ", op);
    }
    HardValue precheck(ValueMutationOp op, HardValue& lhs, ValueFlags rhs) {
      // Handle short-circuits (returns 'Continue' if rhs should be evaluated)
      Bool bvalue;
      EGG_WARNING_SUPPRESS_SWITCH_BEGIN
        switch (op) {
        case ValueMutationOp::IfNull:
          // a ??= b
          // If lhs is null, we need to evaluation the rhs
          if (lhs->getPrimitiveFlag() == ValueFlags::Null) {
            // TODO: thread safety by setting lhs to some mutex
            return HardValue::Continue;
          }
          return lhs;
        case ValueMutationOp::IfFalse:
          // a ||= b
          // Iff lhs is false, we need to evaluation the rhs
          if (!lhs->getBool(bvalue)) {
            return this->raise("Expected left-hand target of '||=' to be a 'bool', but instead got ", describe(lhs));
          }
          if (!Bits::hasAnySet(rhs, ValueFlags::Bool)) {
            return this->raise("Expected right-hand value of '||=' to be a 'bool', but instead got ", describe(rhs));
          }
          if (!bvalue) {
            // Inform the caller we need to evaluate the rhs
            // TODO: thread safety by setting lhs to some mutex
            return HardValue::Continue;
          }
          return HardValue::True;
        case ValueMutationOp::IfTrue:
          // a &&= b
          // If lhs is NOT true, we need to evaluation the rhs
          if (!lhs->getBool(bvalue)) {
            return this->raise("Expected left-hand target of '&&=' to be a 'bool', but instead got ", describe(lhs));
          }
          if (!Bits::hasAnySet(rhs, ValueFlags::Bool)) {
            return this->raise("Expected right-hand value of '&&=' to be a 'bool', but instead got ", describe(rhs));
          }
          if (bvalue) {
            // Inform the caller we need to evaluate the rhs
            // TODO: thread safety by setting lhs to some mutex
            return HardValue::Continue;
          }
          return HardValue::False;
        case ValueMutationOp::Noop:
          // Just return the lhs
          assert(rhs == ValueFlags::Void);
          return lhs;
        }
      EGG_WARNING_SUPPRESS_SWITCH_END
        return HardValue::Continue;
    }
    HardValue mutate(ValueMutationOp op, HardValue& lhs, const HardValue& rhs) {
      // Return the value before the mutation
      switch (op) {
      case ValueMutationOp::Decrement:
      case ValueMutationOp::Increment:
      case ValueMutationOp::Noop:
        assert(rhs->getPrimitiveFlag() == ValueFlags::Void);
        return lhs->mutate(op, rhs.get());
      case ValueMutationOp::IfNull:
      case ValueMutationOp::IfFalse:
      case ValueMutationOp::IfTrue:
        // The condition was already tested in 'precheckMutationOp()'
        return rhs;
      case ValueMutationOp::Assign:
      case ValueMutationOp::Add:
      case ValueMutationOp::Subtract:
      case ValueMutationOp::Multiply:
      case ValueMutationOp::Divide:
      case ValueMutationOp::Remainder:
      case ValueMutationOp::BitwiseAnd:
      case ValueMutationOp::BitwiseOr:
      case ValueMutationOp::BitwiseXor:
      case ValueMutationOp::ShiftLeft:
      case ValueMutationOp::ShiftRight:
      case ValueMutationOp::ShiftRightUnsigned:
        return lhs->mutate(op, rhs.get());
      }
      return lhs;
    }
    HardValue predicate(ValuePredicateOp op, const HardValue& lhs, const HardValue& rhs) {
      // TODO: Turn this into a first-class object with 'void()' semantics
      const char* comparison = nullptr;
      HardValue result;
      switch (op) {
      case ValuePredicateOp::LogicalNot:
        assert(rhs->getVoid());
        result = this->unary(ValueUnaryOp::LogicalNot, lhs);
        break;
      case ValuePredicateOp::LessThan:
        comparison = "<";
        result = this->binary(ValueBinaryOp::LessThan, lhs, rhs);
        break;
      case ValuePredicateOp::LessThanOrEqual:
        comparison = "<=";
        result = this->binary(ValueBinaryOp::LessThanOrEqual, lhs, rhs);
        break;
      case ValuePredicateOp::Equal:
        comparison = "==";
        result = this->binary(ValueBinaryOp::Equal, lhs, rhs);
        break;
      case ValuePredicateOp::NotEqual:
        comparison = "!=";
        result = this->binary(ValueBinaryOp::NotEqual, lhs, rhs);
        break;
      case ValuePredicateOp::GreaterThanOrEqual:
        comparison = ">=";
        result = this->binary(ValueBinaryOp::GreaterThanOrEqual, lhs, rhs);
        break;
      case ValuePredicateOp::GreaterThan:
        comparison = ">";
        result = this->binary(ValueBinaryOp::GreaterThan, lhs, rhs);
        break;
      case ValuePredicateOp::None:
      default:
        assert(rhs->getVoid());
        result = lhs;
        break;
      }
      if (result.hasFlowControl()) {
        return result;
      }
      Bool pass;
      if (!result->getBool(pass)) {
        return this->raise("Expected predicate value to be a 'bool', but instead got ", describe(result));
      }
      if (pass) {
        return HardValue::True;
      }
      String message;
      if (comparison != nullptr) {
        message = this->concat("Assertion is untrue: ", lhs, ' ', comparison, ' ', rhs);
      } else {
        message = this->createString("Assertion is untrue");
      }
      auto ob = this->createRuntimeErrorBuilder(message);
      if (comparison != nullptr) {
        if (!lhs->getVoid()) {
          ob->addReadOnlyProperty(this->createHardValue("left"), lhs);
        }
        ob->addReadOnlyProperty(this->createHardValue("operator"), this->createHardValue(comparison));
        if (!rhs->getVoid()) {
          ob->addReadOnlyProperty(this->createHardValue("right"), rhs);
        }
      }
      auto error = this->createHardValueObject(ob->build());
      return this->raiseException(error);
    }
    bool assignPrimitive(IValue& lhs, ValueFlags lflags, const IValue& rhs, ValueFlags rflags, bool promote = true) {
      // Assign with possible int-to-float promotion
      EGG_WARNING_SUPPRESS_SWITCH_BEGIN
      switch (rflags) {
      case ValueFlags::Null:
      case ValueFlags::Bool:
      case ValueFlags::Float:
      case ValueFlags::String:
      case ValueFlags::Object:
        if (Bits::hasAnySet(lflags, rflags)) {
          return lhs.set(rhs);
        }
        break;
      case ValueFlags::Int:
        if (Bits::hasAnySet(lflags, ValueFlags::Int)) {
          return lhs.set(rhs);
        }
        if (promote && Bits::hasAnySet(lflags, ValueFlags::Float)) {
          Int ivalue;
          if (rhs.getInt(ivalue)) {
            auto fvalue = Arithmetic::promote(ivalue);
            auto hvalue = this->vm.createHardValueFloat(fvalue);
            return lhs.set(hvalue.get());
          }
        }
        break;
      }
      EGG_WARNING_SUPPRESS_SWITCH_END
      return false;
    }
    HardPtr<IObjectBuilder> createRuntimeErrorBuilder(const String& message, const SourceRange* source = nullptr);
  };

  class VMTypeDeducer {
    VMTypeDeducer(const VMTypeDeducer&) = delete;
    VMTypeDeducer& operator=(const VMTypeDeducer&) = delete;
  private:
    using TypeLookup = IVMModuleBuilder::TypeLookup;
    using Reporter = IVMModuleBuilder::Reporter;
    using Node = IVMModuleBuilder::Node;
    IAllocator& allocator;
    ITypeForge& forge;
    const TypeLookup& lookup;
    Reporter* reporter;
  public:
    VMTypeDeducer(IAllocator& allocator, ITypeForge& forge, const TypeLookup& lookup, Reporter* reporter)
      : allocator(allocator),
        forge(forge),
        lookup(lookup),
        reporter(reporter) {
    }
    Type deduce(Node& node) {
      switch (node.kind) {
      case Node::Kind::TypeLiteral:
        return getHardTypeOrNone(node.literal);
      case Node::Kind::ExprLiteral:
        return node.literal->getRuntimeType();
      case Node::Kind::ExprFunctionCall:
        assert(node.literal->getVoid());
        assert(node.children.size() >= 1);
        return this->deduceFunctionCall(*node.children.front(), node.range);
      case Node::Kind::ExprVariableGet:
        assert(node.children.empty());
        return this->deduceVariableGet(node.literal, node.range);
      case Node::Kind::ExprPropertyGet:
        assert(node.literal->getVoid());
        assert(node.children.size() == 2);
        return this->deducePropertyGet(*node.children.front(), *node.children.back(), node.range);
      case Node::Kind::ExprIndexGet:
        assert(node.literal->getVoid());
        assert(node.children.size() == 2);
        return this->deduceIndexGet(*node.children.front(), node.range);
      case Node::Kind::ExprPointeeGet:
        assert(node.literal->getVoid());
        assert(node.children.size() == 1);
        return this->deducePointeeGet(*node.children.front(), node.range);
      case Node::Kind::ExprVariableRef:
        assert(node.children.empty());
        return this->deduceReference(this->deduceVariableGet(node.literal, node.range));
      case Node::Kind::ExprPropertyRef:
        assert(node.literal->getVoid());
        assert(node.children.size() == 2);
        return this->deduceReference(this->deducePropertyGet(*node.children.front(), *node.children.back(), node.range));
      case Node::Kind::ExprIndexRef:
        assert(node.literal->getVoid());
        assert(node.children.size() == 2);
        return this->deduceReference(this->deduceIndexGet(*node.children.front(), node.range));
      case Node::Kind::ExprUnaryOp:
        assert(node.literal->getVoid());
        assert(node.children.size() == 1);
        return this->deduceUnaryOp(node.unaryOp, *node.children.front(), node.range);
      case Node::Kind::ExprBinaryOp:
        assert(node.literal->getVoid());
        assert(node.children.size() == 2);
        return this->deduceBinaryOp(node.binaryOp, *node.children.front(), *node.children.back(), node.range);
      case Node::Kind::ExprTernaryOp:
        assert(node.literal->getVoid());
        assert(node.children.size() == 3);
        return this->deduceTernaryOp(node.ternaryOp, *node.children[0], *node.children[1], *node.children[2], node.range);
      case Node::Kind::ExprPredicateOp:
        return Type::Bool;
      case Node::Kind::ExprArray:
        return this->forge.forgeArrayType(Type::AnyQ, Modifiability::All);
      case Node::Kind::ExprObject:
        return Type::Object;
      case Node::Kind::StmtBlock:
      case Node::Kind::StmtFunctionDefine:
      case Node::Kind::StmtFunctionCapture:
      case Node::Kind::StmtFunctionInvoke:
      case Node::Kind::StmtVariableDeclare:
      case Node::Kind::StmtVariableDefine:
      case Node::Kind::StmtVariableSet:
      case Node::Kind::StmtVariableMutate:
      case Node::Kind::StmtVariableUndeclare:
      case Node::Kind::StmtPropertySet:
      case Node::Kind::StmtPropertyMutate:
      case Node::Kind::StmtIndexMutate:
      case Node::Kind::StmtPointerMutate:
      case Node::Kind::StmtIf:
      case Node::Kind::StmtWhile:
      case Node::Kind::StmtDo:
      case Node::Kind::StmtForEach:
      case Node::Kind::StmtForLoop:
      case Node::Kind::StmtSwitch:
      case Node::Kind::StmtCase:
      case Node::Kind::StmtBreak:
      case Node::Kind::StmtContinue:
      case Node::Kind::StmtThrow:
      case Node::Kind::StmtTry:
      case Node::Kind::StmtCatch:
      case Node::Kind::StmtRethrow:
      case Node::Kind::StmtReturn:
      case Node::Kind::StmtYield:
      case Node::Kind::StmtYieldAll:
      case Node::Kind::StmtYieldBreak:
      case Node::Kind::StmtYieldContinue:
        return Type::Void;
      case Node::Kind::Root:
      case Node::Kind::ExprGuard:
      case Node::Kind::ExprNamed:
      case Node::Kind::TypeInfer:
      case Node::Kind::TypeManifestation:
        // Should not be deduced directly
        break;
      }
      return this->fail(node.range, "TODO: Cannot deduce type for unexpected module node kind");
    }
  private:
    Type deduceVariableGet(const HardValue& literal, const SourceRange& range) {
      String symbol;
      if (!literal->getString(symbol)) {
        return this->fail(range, "Invalid variable literal: ", literal);
      }
      auto type = this->lookup.typeLookup(symbol);
      if (type == nullptr) {
        return this->fail(range, "Cannot deduce type for unknown variable: '", symbol, "'");
      }
      return type;
    }
    Type deducePropertyGet(Node& instance, Node& property, const SourceRange& range) {
      // TODO
      if (instance.kind == Node::Kind::TypeLiteral) {
        auto type = getHardTypeOrNone(instance.literal);
        return this->fail(range, "Type '", *type, "' does not support the property '", property.literal.get(), "'");
      }
      return Type::AnyQ;
    }
    Type deduceIndexGet(Node& instance, const SourceRange& range) {
      auto itype = this->deduce(instance);
      if (itype == nullptr) {
        return nullptr;
      }
      Type etype = nullptr;
      auto count = itype->getShapeCount();
      for (size_t index = 0; index < count; ++index) {
        auto* shape = itype->getShape(index);
        if ((shape != nullptr) && (shape->indexable != nullptr)) {
          auto type = shape->indexable->getResultType();
          if (etype == nullptr) {
            etype = type;
          } else {
            etype = this->forge.forgeUnionType(etype, type);
          }
        }
      }
      if (etype == nullptr) {
        return this->fail(range, "Values of type '", *itype, "' do not support index operator '[]'");
      }
      return etype;
    }
    Type deducePointeeGet(Node& instance, const SourceRange& range) {
      auto itype = this->deduce(instance);
      if (itype == nullptr) {
        return nullptr;
      }
      Type ptype = nullptr;
      auto count = itype->getShapeCount();
      for (size_t index = 0; index < count; ++index) {
        auto* shape = itype->getShape(index);
        if ((shape != nullptr) && (shape->pointable != nullptr)) {
          auto type = shape->pointable->getPointeeType();
          if (ptype == nullptr) {
            ptype = type;
          } else {
            ptype = this->forge.forgeUnionType(ptype, type);
          }
        }
      }
      if (ptype == nullptr) {
        return this->fail(range, "Values of type '", *itype, "' do not support pointer operator '*'");
      }
      return ptype;
    }
    Type deduceReference(Type pointee) {
      if (pointee == nullptr) {
        return nullptr;
      }
      return this->forge.forgePointerType(pointee, Modifiability::All);
    }
    Type deduceUnaryOp(ValueUnaryOp op, Node& rhs, const SourceRange& range) {
      switch (op) {
      case ValueUnaryOp::Negate:
        return this->deduce(rhs);
      case ValueUnaryOp::BitwiseNot:
        return Type::Int;
      case ValueUnaryOp::LogicalNot:
        return Type::Bool;
      }
      return this->fail(range, "TODO: Cannot deduce type for unary operator");
    }
    Type deduceBinaryOp(ValueBinaryOp op, Node& lhs, Node& rhs, const SourceRange& range) {
      switch (op) {
      case ValueBinaryOp::Add: // a + b
      case ValueBinaryOp::Subtract: // a - b
      case ValueBinaryOp::Multiply: // a * b
      case ValueBinaryOp::Divide: // a / b
      case ValueBinaryOp::Remainder: // a % b
        if (this->isDeducedAsFloat(lhs) || this->isDeducedAsFloat(rhs)) {
          return Type::Float;
        }
        return Type::Int;
      case ValueBinaryOp::LessThan: // a < b
        break;
      case ValueBinaryOp::LessThanOrEqual: // a <= b
        break;
      case ValueBinaryOp::Equal: // a == b
        break;
      case ValueBinaryOp::NotEqual: // a != b
        break;
      case ValueBinaryOp::GreaterThanOrEqual: // a >= b
        break;
      case ValueBinaryOp::GreaterThan: // a > b
        break;
      case ValueBinaryOp::BitwiseAnd: // a & b
        break;
      case ValueBinaryOp::BitwiseOr: // a | b
        break;
      case ValueBinaryOp::BitwiseXor: // a ^ b
        break;
      case ValueBinaryOp::ShiftLeft: // a << b
        break;
      case ValueBinaryOp::ShiftRight: // a >> b
        break;
      case ValueBinaryOp::ShiftRightUnsigned: // a >>> b
        break;
      case ValueBinaryOp::IfNull: // a ?? b
        break;
      case ValueBinaryOp::IfFalse: // a || b
        break;
      case ValueBinaryOp::IfTrue: // a && b
        break;
      }
      return this->fail(range, "TODO: Cannot deduce type for binary operator");
    }
    Type deduceTernaryOp(ValueTernaryOp op, Node&, Node& lhs, Node& rhs, const SourceRange& range) {
      if (op != ValueTernaryOp::IfThenElse) {
        return this->fail(range, "TODO: Cannot deduce type for ternary operator");
      }
      auto ltype = this->deduce(lhs);
      if (ltype == nullptr) {
        return nullptr;
      }
      auto rtype = this->deduce(rhs);
      if (rtype == nullptr) {
        return nullptr;
      }
      return this->forge.forgeUnionType(ltype, rtype);
    }
    Type deduceFunctionCall(Node& function, const SourceRange& range) {
      if (function.kind == Node::Kind::TypeManifestation) {
        Int flags;
        if (function.literal->getInt(flags)) {
          return this->forge.forgePrimitiveType(ValueFlags(flags));
        }
      }
      auto ftype = this->deduce(function);
      if (ftype == nullptr) {
        return nullptr;
      }
      Type rtype = nullptr;
      auto count = ftype->getShapeCount();
      for (size_t index = 0; index < count; ++index) {
        auto* shape = ftype->getShape(index);
        if ((shape != nullptr) && (shape->callable != nullptr)) {
          // TODO: Match arity of call (number of arguments)
          auto type = shape->callable->getReturnType();
          if (rtype == nullptr) {
            rtype = type;
          } else {
            rtype = this->forge.forgeUnionType(rtype, type);
          }
        }
      }
      if (rtype == nullptr) {
        return this->fail(range, "Values of type '", *ftype,"' do not support function call semantics");
      }
      return rtype;
    }
    bool isDeducedAsFloat(Node& node) {
      auto type = this->deduce(node);
      return (type != nullptr) && Bits::hasAnySet(type->getPrimitiveFlags(), ValueFlags::Float);
    }
    template<typename... ARGS>
    Type fail(const SourceRange& range, ARGS&&... args) {
      if (this->reporter != nullptr) {
        auto problem = StringBuilder::concat(this->allocator, std::forward<ARGS>(args)...);
        this->reporter->report(range, problem);
      }
      return nullptr;
    }
  };

  class VMModuleBuilder : public VMUncollectable<IVMModuleBuilder> {
    VMModuleBuilder(const VMModuleBuilder&) = delete;
    VMModuleBuilder& operator=(const VMModuleBuilder&) = delete;
  private:
    HardPtr<VMProgram> program;
    HardPtr<VMModule> module; // becomes null once built
  public:
    VMModuleBuilder(IVM& vm, VMProgram& program, const String& resource)
      : VMUncollectable(vm),
        program(&program),
        module(vm.getAllocator().makeRaw<VMModule>(vm, resource)) {
      assert(this->module != nullptr);
    }
    virtual Node& getRoot() override {
      assert(this->module != nullptr);
      return this->module->getRoot();
    }
    virtual HardPtr<IVMModule> build() override {
      HardPtr<VMModule> built = this->module;
      if (built != nullptr) {
        this->module = nullptr;
        this->program->addModule(*built);
      }
      return built;
    }
    virtual Node& exprValueUnaryOp(ValueUnaryOp op, Node& arg, const SourceRange& range) override {
      auto& node = this->module->createNode(Node::Kind::ExprUnaryOp, range);
      node.unaryOp = op;
      node.addChild(arg);
      return node;
    }
    virtual Node& exprValueBinaryOp(ValueBinaryOp op, Node& lhs, Node& rhs, const SourceRange& range) override {
      auto& node = this->module->createNode(Node::Kind::ExprBinaryOp, range);
      node.binaryOp = op;
      node.addChild(lhs);
      node.addChild(rhs);
      return node;
    }
    virtual Node& exprValueTernaryOp(ValueTernaryOp op, Node& lhs, Node& mid, Node& rhs, const SourceRange& range) override {
      auto& node = this->module->createNode(Node::Kind::ExprTernaryOp, range);
      node.ternaryOp = op;
      node.addChild(lhs);
      node.addChild(mid);
      node.addChild(rhs);
      return node;
    }
    virtual Node& exprValuePredicateOp(ValuePredicateOp op, const SourceRange& range) override {
      auto& node = this->module->createNode(Node::Kind::ExprPredicateOp, range);
      node.predicateOp = op;
      return node;
    }
    virtual Node& exprLiteral(const HardValue& literal, const SourceRange& range) override {
      auto& node = this->module->createNode(Node::Kind::ExprLiteral, range);
      node.literal = literal;
      return node;
    }
    virtual Node& exprFunctionCall(Node& function, const SourceRange& range) override {
      auto& node = this->module->createNode(Node::Kind::ExprFunctionCall, range);
      node.addChild(function);
      return node;
    }
    virtual Node& exprVariableGet(const String& symbol, const SourceRange& range) override {
      auto& node = this->module->createNode(Node::Kind::ExprVariableGet, range);
      node.literal = this->createHardValueString(symbol);
      return node;
    }
    virtual Node& exprPropertyGet(Node& instance, Node& property, const SourceRange& range) override {
      auto& node = this->module->createNode(Node::Kind::ExprPropertyGet, range);
      node.addChild(instance);
      node.addChild(property);
      return node;
    }
    virtual Node& exprIndexGet(Node& instance, Node& index, const SourceRange& range) override {
      auto& node = this->module->createNode(Node::Kind::ExprIndexGet, range);
      node.addChild(instance);
      node.addChild(index);
      return node;
    }
    virtual Node& exprPointeeGet(Node& pointer, const SourceRange& range) override {
      auto& node = this->module->createNode(Node::Kind::ExprPointeeGet, range);
      node.addChild(pointer);
      return node;
    }
    virtual Node& exprVariableRef(const String& symbol, const SourceRange& range) override {
      auto& node = this->module->createNode(Node::Kind::ExprVariableRef, range);
      node.literal = this->createHardValueString(symbol);
      return node;
    }
    virtual Node& exprPropertyRef(Node& instance, Node& property, const SourceRange& range) override {
      auto& node = this->module->createNode(Node::Kind::ExprPropertyRef, range);
      node.addChild(instance);
      node.addChild(property);
      return node;
    }
    virtual Node& exprIndexRef(Node& instance, Node& index, const SourceRange& range) override {
      auto& node = this->module->createNode(Node::Kind::ExprIndexRef, range);
      node.addChild(instance);
      node.addChild(index);
      return node;
    }
    virtual Node& exprArray(const SourceRange& range) override {
      auto& node = this->module->createNode(Node::Kind::ExprArray, range);
      return node;
    }
    virtual Node& exprObject(const SourceRange& range) override {
      auto& node = this->module->createNode(Node::Kind::ExprObject, range);
      return node;
    }
    virtual Node& exprGuard(const String& symbol, Node& value, const SourceRange& range) override {
      auto& node = this->module->createNode(Node::Kind::ExprGuard, range);
      node.literal = this->createHardValueString(symbol);
      node.addChild(value);
      return node;
    }
    virtual Node& exprNamed(const HardValue& name, Node& value, const SourceRange& range) override {
      auto& node = this->module->createNode(Node::Kind::ExprNamed, range);
      node.literal = name;
      node.addChild(value);
      return node;
    }
    virtual Node& typeLiteral(const Type& type, const SourceRange& range) override {
      auto& node = this->module->createNode(Node::Kind::TypeLiteral, range);
      node.literal = this->createHardValueType(type);
      return node;
    }
    virtual Node& typeManifestation(ValueFlags flags, const SourceRange& range) override {
      auto& node = this->module->createNode(Node::Kind::TypeManifestation, range);
      node.literal = this->createHardValueInt(Int(flags));
      return node;
    }
    virtual Node& stmtBlock(const SourceRange& range) override {
      auto& node = this->module->createNode(Node::Kind::StmtBlock, range);
      return node;
    }
    virtual Node& stmtIf(Node& condition, const SourceRange& range) override {
      auto& node = this->module->createNode(Node::Kind::StmtIf, range);
      node.addChild(condition);
      return node;
    }
    virtual Node& stmtWhile(Node& condition, Node& block, const SourceRange& range) override {
      auto& node = this->module->createNode(Node::Kind::StmtWhile, range);
      node.addChild(condition);
      node.addChild(block);
      return node;
    }
    virtual Node& stmtDo(Node& block, Node& condition, const SourceRange& range) override {
      auto& node = this->module->createNode(Node::Kind::StmtDo, range);
      node.addChild(block);
      node.addChild(condition);
      return node;
    }
    virtual Node& stmtForEach(const String& symbol, Node& type, Node& iteration, Node& block, const SourceRange& range) override {
      auto& node = this->module->createNode(Node::Kind::StmtForEach, range);
      node.literal = this->createHardValueString(symbol);
      node.addChild(type);
      node.addChild(iteration);
      node.addChild(block);
      return node;
    }
    virtual Node& stmtForLoop(Node& initial, Node& condition, Node& advance, Node& block, const SourceRange& range) override {
      // Note the change in order to be execution-based (initial/condition/block/advance)
      auto& node = this->module->createNode(Node::Kind::StmtForLoop, range);
      node.addChild(initial);
      node.addChild(condition);
      node.addChild(block);
      node.addChild(advance);
      return node;
    }
    virtual Node& stmtSwitch(Node& expression, size_t defaultIndex, const SourceRange& range) override {
      auto& node = this->module->createNode(Node::Kind::StmtSwitch, range);
      node.addChild(expression);
      node.defaultIndex = defaultIndex;
      return node;
    }
    virtual Node& stmtCase(Node& block, const SourceRange& range) override {
      auto& node = this->module->createNode(Node::Kind::StmtCase, range);
      node.addChild(block);
      return node;
    }
    virtual Node& stmtBreak(const SourceRange& range) override {
      auto& node = this->module->createNode(Node::Kind::StmtBreak, range);
      return node;
    }
    virtual Node& stmtContinue(const SourceRange& range) override {
      auto& node = this->module->createNode(Node::Kind::StmtContinue, range);
      return node;
    }
    virtual Node& stmtFunctionDefine(const String& symbol, Node& type, size_t captures, const SourceRange& range) override {
      auto& node = this->module->createNode(Node::Kind::StmtFunctionDefine, range);
      node.literal = this->createHardValueString(symbol);
      node.defaultIndex = captures + 1; // child index of invoke node
      node.addChild(type);
      return node;
    }
    virtual Node& stmtFunctionCapture(const String& symbol, const SourceRange& range) override {
      auto& node = this->module->createNode(Node::Kind::StmtFunctionCapture, range);
      node.literal = this->createHardValueString(symbol);
      return node;
    }
    virtual Node& stmtFunctionInvoke(const SourceRange& range) override {
      auto& node = this->module->createNode(Node::Kind::StmtFunctionInvoke, range);
      return node;
    }
    virtual Node& stmtVariableDeclare(const String& symbol, Node& type, const SourceRange& range) override {
      auto& node = this->module->createNode(Node::Kind::StmtVariableDeclare, range);
      node.literal = this->createHardValueString(symbol);
      node.addChild(type);
      return node;
    }
    virtual Node& stmtVariableDefine(const String& symbol, Node& type, Node& value, const SourceRange& range) override {
      auto& node = this->module->createNode(Node::Kind::StmtVariableDefine, range);
      node.literal = this->createHardValueString(symbol);
      node.addChild(type);
      node.addChild(value);
      return node;
    }
    virtual Node& stmtVariableSet(const String& symbol, Node& value, const SourceRange& range) override {
      auto& node = this->module->createNode(Node::Kind::StmtVariableSet, range);
      node.literal = this->createHardValueString(symbol);
      node.addChild(value);
      return node;
    }
    virtual Node& stmtVariableMutate(const String& symbol, ValueMutationOp op, Node& value, const SourceRange& range) override {
      auto& node = this->module->createNode(Node::Kind::StmtVariableMutate, range);
      node.literal = this->createHardValueString(symbol);
      node.mutationOp = op;
      node.addChild(value);
      return node;
    }
    virtual Node& stmtVariableUndeclare(const String& symbol, const SourceRange& range) override {
      auto& node = this->module->createNode(Node::Kind::StmtVariableUndeclare, range);
      node.literal = this->createHardValueString(symbol);
      return node;
    }
    virtual Node& stmtPropertySet(Node& instance, Node& property, Node& value, const SourceRange& range) override {
      auto& node = this->module->createNode(Node::Kind::StmtPropertySet, range);
      node.addChild(instance);
      node.addChild(property);
      node.addChild(value);
      return node;
    }
    virtual Node& stmtPropertyMutate(Node& instance, Node& property, ValueMutationOp op, Node& value, const SourceRange& range) override {
      auto& node = this->module->createNode(Node::Kind::StmtPropertyMutate, range);
      node.mutationOp = op;
      node.addChild(instance);
      node.addChild(property);
      node.addChild(value);
      return node;
    }
    virtual Node& stmtIndexMutate(Node& instance, Node& index, ValueMutationOp op, Node& value, const SourceRange& range) override {
      auto& node = this->module->createNode(Node::Kind::StmtIndexMutate, range);
      node.mutationOp = op;
      node.addChild(instance);
      node.addChild(index);
      node.addChild(value);
      return node;
    }
    virtual Node& stmtPointeeMutate(Node& instance, ValueMutationOp op, Node& value, const SourceRange& range) override {
      auto& node = this->module->createNode(Node::Kind::StmtPointerMutate, range);
      node.mutationOp = op;
      node.addChild(instance);
      node.addChild(value);
      return node;
    }
    virtual Node& stmtThrow(Node& exception, const SourceRange& range) override {
      auto& node = this->module->createNode(Node::Kind::StmtThrow, range);
      node.addChild(exception);
      return node;
    }
    virtual Node& stmtTry(Node& block, const SourceRange& range) override {
      auto& node = this->module->createNode(Node::Kind::StmtTry, range);
      node.addChild(block);
      return node;
    }
    virtual Node& stmtCatch(const String& symbol, Node& type, const SourceRange& range) override {
      auto& node = this->module->createNode(Node::Kind::StmtCatch, range);
      node.literal = this->createHardValueString(symbol);
      node.addChild(type);
      return node;
    }
    virtual Node& stmtRethrow(const SourceRange& range) override {
      auto& node = this->module->createNode(Node::Kind::StmtRethrow, range);
      return node;
    }
    virtual Node& stmtReturn(const SourceRange& range) override {
      auto& node = this->module->createNode(Node::Kind::StmtReturn, range);
      return node;
    }
    virtual Node& stmtYield(Node& value, const SourceRange& range) override {
      auto& node = this->module->createNode(Node::Kind::StmtYield, range);
      node.addChild(value);
      return node;
    }
    virtual Node& stmtYieldAll(Node& value, const SourceRange& range) override {
      auto& node = this->module->createNode(Node::Kind::StmtYieldAll, range);
      node.addChild(value);
      return node;
    }
    virtual Node& stmtYieldBreak(const SourceRange& range) override {
      auto& node = this->module->createNode(Node::Kind::StmtYieldBreak, range);
      return node;
    }
    virtual Node& stmtYieldContinue(const SourceRange& range) override {
      auto& node = this->module->createNode(Node::Kind::StmtYieldContinue, range);
      return node;
    }
    virtual ITypeForge& getTypeForge() const override {
      return this->vm.getTypeForge();
    }
    virtual Type deduce(Node& node, const TypeLookup& lookup, Reporter* reporter) override {
      VMTypeDeducer deducer{ this->getAllocator(), this->getTypeForge(), lookup, reporter };
      return deducer.deduce(node);
    }
    virtual void appendChild(Node& parent, Node& child) override {
      parent.addChild(child);
    }
  };

  class VMProgramBuilder : public VMUncollectable<IVMProgramBuilder> {
    VMProgramBuilder(const VMProgramBuilder&) = delete;
    VMProgramBuilder& operator=(const VMProgramBuilder&) = delete;
  private:
    HardPtr<VMProgram> program; // becomes null once built
  public:
    explicit VMProgramBuilder(IVM& vm)
      : VMUncollectable(vm),
        program(vm.getAllocator().makeRaw<VMProgram>(vm)) {
      assert(this->program != nullptr);
    }
    virtual IVM& getVM() const override {
      return this->vm;
    }
    virtual bool addBuiltin(const String& symbol, const Type& type) override {
      assert(this->program != nullptr);
      return this->program->addBuiltin(symbol, type);
    }
    virtual void visitBuiltins(const std::function<void(const String& symbol, const Type& type)>& visitor) const override {
      assert(this->program != nullptr);
      for (const auto& builtin : this->program->getBuiltins()) {
        visitor(builtin.first, builtin.second);
      }
    }
    virtual HardPtr<IVMModuleBuilder> createModuleBuilder(const String& resource) override {
      assert(this->program != nullptr);
      return HardPtr(this->getAllocator().makeRaw<VMModuleBuilder>(this->vm, *this->program, resource));
    }
    virtual HardPtr<IVMProgram> build() override {
      HardPtr<VMProgram> built = this->program;
      if (built != nullptr) {
        this->program = nullptr;
      }
      return built;
    }
  };

  class VMFunction : public HardReferenceCounted<IVMCallHandler> {
    VMFunction(const VMFunction&) = delete;
    VMFunction& operator=(const VMFunction&) = delete;
  private:
    IVM& vm;
    Type ftype;
    const IVMModule::Node& definition;
  public:
    VMFunction(IVM& vm, const Type& ftype, const IVMModule::Node& definition)
      : vm(vm),
        ftype(ftype),
        definition(definition) {
      assert(this->ftype.validate());
      assert(this->definition.kind == IVMModule::Node::Kind::StmtFunctionDefine);
    }
    virtual HardValue call(IVMExecution& execution, const ICallArguments& arguments, const IVMCallCaptures* captures) override {
      auto* signature = this->ftype.getOnlyFunctionSignature();
      if (signature == nullptr) {
        StringBuilder sb;
        sb << "Missing function signature in '";
        sb.describe(*this->ftype);
        sb << "'";
        return execution.raiseRuntimeError(sb.build(this->vm.getAllocator()), nullptr);
      }
      return execution.initiateTailCall(*signature, arguments, this->definition, captures);
    }
    virtual void hardDestroy() const override {
      this->vm.getAllocator().destroy(this);
    }
  };

  class VMRunner : public VMCollectable<IVMRunner> {
    VMRunner(const VMRunner&) = delete;
    VMRunner& operator=(const VMRunner&) = delete;
  private:
    struct NodeStack {
      const IVMModule::Node* node;
      String scope; // Name of variable declared here
      size_t index; // Node-specific state variable
      std::deque<HardValue> deque; // Results of child nodes computation
      HardValue value; // Used by switch/try etc.
    };
    HardPtr<IVMProgram> program;
    std::stack<NodeStack> stack;
    VMSymbolTable symtable;
    VMExecution execution;
  public:
    VMRunner(IVM& vm, IVMProgram& program, const IVMModule::Node& root)
      : VMCollectable(vm),
        program(&program),
        execution(vm) {
      this->execution.runner = this;
      this->symtable.push();
      this->push(root);
    }
    virtual void softVisit(ICollectable::IVisitor& visitor) const override {
      this->symtable.softVisit(visitor);
    }
    virtual bool validate() const override {
      if (VMCollectable::validate() && !this->stack.empty()) {
        auto& top = this->stack.top();
        if ((top.node != nullptr) && top.node->literal.validate()) {
          // The literal in the module node should not be owned
          return top.node->literal->softGetBasket() == nullptr;
        }
      }
      return false;
    }
    virtual int print(Printer& printer) const override {
      printer << "[VMRunner]";
      return 0;
    }
    virtual void addBuiltin(const String& symbol, const HardValue& value) override {
      this->symtable.builtin(symbol, &this->vm.createSoftValue(value));
    }
    virtual RunOutcome run(HardValue& retval, RunFlags flags) override {
      if (flags == RunFlags::Step) {
        if (this->stepNodeChecked(retval)) {
          return RunOutcome::Stepped;
        }
      } else  if (flags != RunFlags::None) {
        retval = this->raiseRuntimeError(this->createString("TODO: Run flags not yet supported in runner"));
        return RunOutcome::Failed;
      } else {
        while (this->stepNodeChecked(retval)) {
          // Step
        }
      }
      if (retval.hasFlowControl()) {
        return RunOutcome::Failed;
      }
      return RunOutcome::Succeeded;
    }
    HardPtr<IVMCallStack> getCallStack(const SourceRange* source) const {
      // TODO full stack chain
      assert(!this->stack.empty());
      const auto* top = this->stack.top().node;
      assert(top != nullptr);
      auto callstack{ this->vm.getAllocator().makeHard<VMCallStack>() };
      callstack->resource = top->module.getResource();
      if (source != nullptr) {
        callstack->range = *source;
      } else {
        callstack->range = top->range;
      }
      // TODO callstack->function
      return callstack;
    }
    template<typename... ARGS>
    HardValue raiseRuntimeError(ARGS&&... args) {
      auto message = this->execution.concat(std::forward<ARGS>(args)...);
      return this->execution.raiseRuntimeError(message, nullptr);
    }
    HardValue initiateTailCall(const IFunctionSignature& signature, const ICallArguments& arguments, const IVMModule::Node& definition, const IVMCallCaptures* captures) {
      // We need to set the argument/capture symbols and initiate the execution of the block
      assert(definition.kind == IVMModule::Node::Kind::StmtFunctionDefine);
      assert(!this->stack.empty());
      assert(this->stack.top().node->kind == IVMModule::Node::Kind::ExprFunctionCall);
      assert(this->stack.top().scope.empty());
      this->stack.pop();
      this->symtable.push();
      if (captures != nullptr) {
        for (size_t index = 0; index < captures->getCaptureCount(); ++index) {
          auto* capture = captures->getCaptureByIndex(index);
          assert(capture != nullptr);
          assert(capture->soft != nullptr);
          assert(capture->soft->softGetBasket() != nullptr);
          auto* extant = this->symtable.add(capture->kind, capture->name, capture->type, capture->soft);
          if (extant != nullptr) {
            switch (extant->kind) {
            case VMSymbolTable::Kind::Builtin:
              return this->raiseRuntimeError("Captured symbol already declared as a builtin: '", capture->name, "'");
            case VMSymbolTable::Kind::Variable:
              return this->raiseRuntimeError("Captured symbol already declared as a variable: '", capture->name, "'");
            case VMSymbolTable::Kind::Type:
              return this->raiseRuntimeError("Captured symbol already declared as a type: '", capture->name, "'");
            }
          }
        }
      }
      for (size_t index = 0; index < signature.getParameterCount(); ++index) {
        // TODO optional/variadic arguments
        HardValue value;
        if (!arguments.getArgumentValueByIndex(index, value)) {
          return this->raiseRuntimeError("Argument count mismatch"); // TODO
        }
        auto& parameter = signature.getParameter(index);
        auto pname = parameter.getName();
        auto ptype = parameter.getType();
        auto& poly = this->vm.createSoftValue();
        if (!this->execution.assignValue(poly, ptype, value.get())) {
          auto message = this->execution.concat("Type mismatch for parameter '", pname, "': Expected '", ptype, "', but instead got ", describe(value));
          SourceRange source;
          if (arguments.getArgumentSourceByIndex(index, source)) {
            return this->execution.raiseRuntimeError(message, &source);
          }
          return this->execution.raiseRuntimeError(message, nullptr);
        }
        auto* extant = this->symtable.add(VMSymbolTable::Kind::Variable, pname, ptype, &poly);
        if (extant != nullptr) {
          switch (extant->kind) {
          case VMSymbolTable::Kind::Builtin:
            return this->raiseRuntimeError("Parameter symbol already declared as a builtin: '", pname, "'");
          case VMSymbolTable::Kind::Variable:
            return this->raiseRuntimeError("Parameter symbol already declared: '", pname, "'");
          case VMSymbolTable::Kind::Type:
            return this->raiseRuntimeError("Parameter symbol already declared as a type: '", pname, "'");
          }
        }
      }
      // Push the invoke node
      auto* invoke = definition.children[definition.defaultIndex];
      assert(invoke != nullptr);
      this->push(*invoke);
      return HardValue::Continue;
    }
    void printSymtable(Printer& printer) const {
      this->symtable.print(printer);
    }
  private:
    bool stepNode(HardValue& retval);
    bool stepNodeChecked(HardValue& retval);
    bool stepBlock(HardValue& retval, size_t first = 0);
    NodeStack& push(const IVMModule::Node& node, const String& scope = {}, size_t index = 0) {
      return this->stack.emplace(&node, scope, index);
    }
    bool pop(HardValue value) { // sic byval
      assert(!this->stack.empty());
      const auto& symbol = this->stack.top().scope;
      if (!symbol.empty()) {
        (void)this->symtable.remove(symbol);
      }
      this->stack.pop();
      assert(!this->stack.empty());
      this->stack.top().deque.emplace_back(std::move(value));
      return true;
    }
    template<typename... ARGS>
    bool raise(ARGS&&... args) {
      auto error = this->raiseRuntimeError(std::forward<ARGS>(args)...);
      return this->pop(error);
    }
    bool raiseBadPropertyModification(const HardValue& instance, const HardValue& property) {
      if (instance->getPrimitiveFlag() == ValueFlags::String) {
        String pname;
        if (property->getString(pname)) {
          return this->raise("Strings do not support modification of properties such as '", pname, "'");
        }
        return this->raise("Strings do not support modification of properties");
      }
      return this->raise("Expected left-hand side of property operator '.' to be an object, but instead got ", describe(instance));
    }
    bool variableScopeBegin(NodeStack& top, const Type& type) {
      assert(top.scope.empty());
      assert(type != nullptr);
      if (!top.node->literal->getString(top.scope)) {
        // No symbol declared
        return true;
      }
      assert(!top.scope.empty());
      auto& poly = this->vm.createSoftValue();
      auto* extant = this->symtable.add(VMSymbolTable::Kind::Variable, top.scope, type, &poly);
      if (extant != nullptr) {
        switch (extant->kind) {
        case VMSymbolTable::Kind::Builtin:
          this->raise("Variable symbol already declared as a builtin: '", top.scope, "'");
          return false;
        case VMSymbolTable::Kind::Variable:
          this->raise("Variable symbol already declared: '", top.scope, "'");
          return false;
        case VMSymbolTable::Kind::Type:
          this->raise("Variable symbol already declared as a type: '", top.scope, "'");
          return false;
        }
      }
      return true;
    }
    void variableScopeEnd(NodeStack& top) {
      // Prematurely end the scope (e.g. in 'else' clause of guarded 'if' statement)
      if (!top.scope.empty()) {
        (void)this->symtable.remove(top.scope);
        top.scope = {};
      }
    }
    bool symbolSet(const IVMModule::Node* node, const HardValue& value) {
      assert(node != nullptr);
      String name;
      if (!node->literal->getString(name)) {
        this->raise("Invalid program node literal for variable identifier");
        return false;
      }
      if (value.hasFlowControl()) {
        this->pop(value);
        return false;
      }
      if (value->getPrimitiveFlag() == ValueFlags::Void) {
        this->raise("Cannot set variable '", name, "' to an uninitialized value");
        return false;
      }
      auto extant = this->symtable.find(name);
      if (extant == nullptr) {
        this->raise("Unknown variable: '", name, "'");
        return false;
      }
      if (extant->kind == VMSymbolTable::Kind::Builtin) {
        this->raise("Cannot re-assign built-in value: '", name, "'");
        return false;
      }
      if (!this->execution.assignValue(*extant->soft, extant->type, value.get())) {
        this->raise("Type mismatch setting variable '", name, "': expected '", extant->type, "' but instead got ", describe(value));
        return false;
      }
      extant->kind = VMSymbolTable::Kind::Variable;
      return true;
    }
    HardValue symbolGuard(const HardValue& symbol, const HardValue& value) {
      String name;
      if (!symbol->getString(name)) {
        return this->raiseRuntimeError("Invalid program node literal for variable identifier");
      }
      if (value.hasFlowControl()) {
        return value;
      }
      if (value->getPrimitiveFlag() == ValueFlags::Void) {
        return HardValue::False;
      }
      auto extant = this->symtable.find(name);
      if (extant == nullptr) {
        return this->raiseRuntimeError("Unknown variable: '", name, "'");
      }
      if (extant->kind == VMSymbolTable::Kind::Builtin) {
        return this->raiseRuntimeError("Cannot re-assign built-in value: '", name, "'");
      }
      if (!this->execution.assignValue(*extant->soft, extant->type, value.get())) {
        return HardValue::False;
      }
      extant->kind = VMSymbolTable::Kind::Variable;
      return HardValue::True;
    }
    HardValue arrayConstruct(const std::deque<HardValue>& elements, const std::vector<IVMModule::Node*>& mnodes) {
      // TODO: support '...' inclusion
      assert(elements.size() == mnodes.size());
      auto array = ObjectFactory::createVanillaArray(this->vm);
      assert(array != nullptr);
      if (!elements.empty()) {
        auto push = array->vmPropertyGet(this->execution, this->createHardValue("push"));
        HardObject pusher;
        if (!push->getHardObject(pusher)) {
          return this->raiseRuntimeError("Vanilla array does not have 'push()' property");
        }
        auto mnode = mnodes.cbegin();
        for (const auto& element : elements) {
          // OPTIMIZE
          CallArguments arguments;
          arguments.addUnnamed(element, &(*mnode++)->range);
          auto retval = pusher->vmCall(this->execution, arguments);
          if (retval.hasFlowControl()) {
            return retval;
          }
        }
      }
      return this->createHardValueObject(array);
    }
    HardValue objectConstruct(const std::deque<HardValue>& elements) {
      // TODO: support '...' inclusion
      assert((elements.size() % 2) == 0);
      auto object = ObjectFactory::createVanillaObject(this->vm);
      assert(object != nullptr);
      auto element = elements.cbegin();
      while (element != elements.cend()) {
        auto& property = *element++;
        auto& value = *element++;
        auto retval = object->vmPropertySet(this->execution, property, value);
        if (retval.hasFlowControl()) {
          return retval;
        }
      }
      return this->createHardValueObject(object);
    }
    HardValue functionConstruct(const Type& ftype, const IVMModule::Node& definition) {
      // TODO optimize
      assert(ftype.validate());
      auto handler = HardPtr(this->getAllocator().makeRaw<VMFunction>(this->vm, ftype, definition));
      assert(handler != nullptr);
      std::vector<VMCallCapture> captures;
      for (size_t index = 1; index < definition.defaultIndex; ++index) {
        auto* capture = definition.children[index];
        assert(capture != nullptr);
        assert(capture->kind == IVMModule::Node::Kind::StmtFunctionCapture);
        String symbol;
        if (!capture->literal->getString(symbol)) {
          return this->raiseRuntimeError("Failed to fetch captured symbol name");
        }
        auto* found = this->symtable.find(symbol);
        if (found == nullptr) {
          return this->raiseRuntimeError("Cannot find required captured symbol: '", symbol, "'");
        }
        captures.emplace_back(found->kind, found->type, symbol, found->soft);
      }
      auto* signature = ftype.getOnlyFunctionSignature();
      if ((signature != nullptr) && (signature->getGeneratedType() != nullptr)) {
        auto object = ObjectFactory::createVanillaGenerator(this->vm, ftype, *handler, std::move(captures));
        assert(object != nullptr);
        return this->createHardValueObject(object);
      }
      auto object = ObjectFactory::createVanillaFunction(this->vm, ftype, *handler, std::move(captures));
      assert(object != nullptr);
      return this->createHardValueObject(object);
    }
    HardValue stringIndexGet(const String& string, const HardValue& index) {
      Int ivalue;
      if (!index->getInt(ivalue)) {
        return this->raiseRuntimeError("Expected index in string operator '[]' to be an 'int', but instead got ", describe(index));
      }
      auto cp = string.codePointAt(size_t(ivalue));
      if (cp < 0) {
        return this->raiseRuntimeError("String index ", ivalue, " is out of range for a string of length ", string.length());
      }
      return this->createHardValueString(String::fromUTF32(this->getAllocator(), &cp, 1));
    }
    HardValue stringIndexRef(const String& string, const HardValue& index) {
      auto value = this->stringIndexGet(string, index);
      if (value.hasFlowControl()) {
        return value;
      }
      auto pointer = ObjectFactory::createPointerToValue(this->vm, value, Modifiability::Read);
      assert(pointer != nullptr);
      return this->createHardValueObject(pointer);
    }
    HardValue stringPropertyGet(const String& string, const HardValue& property) {
      // TODO reference identity (i.e. 's.hash == s.hash' as opposed to 's.hash() == s.hash()'
      String name;
      if (!property->getString(name)) {
        return this->raiseRuntimeError("Expected right-hand side of string operator '.' to be a 'string', but instead got ", describe(property));
      }
      if (name.equals("length")) {
        return this->createHardValueInt(Int(string.length()));
      }
      using Factory = std::function<HardObject(IVM&, const String&)>;
      static const std::map<std::string, Factory> map = {
        { "compareTo", ObjectFactory::createStringProxyCompareTo },
        { "contains", ObjectFactory::createStringProxyContains },
        { "endsWith", ObjectFactory::createStringProxyEndsWith },
        { "hash", ObjectFactory::createStringProxyHash },
        { "indexOf", ObjectFactory::createStringProxyIndexOf },
        { "join", ObjectFactory::createStringProxyJoin },
        { "lastIndexOf", ObjectFactory::createStringProxyLastIndexOf },
        { "padLeft", ObjectFactory::createStringProxyPadLeft },
        { "padRight", ObjectFactory::createStringProxyPadRight },
        { "repeat", ObjectFactory::createStringProxyRepeat },
        { "replace", ObjectFactory::createStringProxyReplace },
        { "slice", ObjectFactory::createStringProxySlice },
        { "startsWith", ObjectFactory::createStringProxyStartsWith },
        { "toString", ObjectFactory::createStringProxyToString }
      };
      auto found = map.find(name.toUTF8());
      if (found == map.end()) {
        return this->raiseRuntimeError("Unknown string property name: '", name, "'");
      }
      return this->createHardValueObject(found->second(this->vm, string));
    }
    HardValue stringPropertyRef(const String& string, const HardValue& property) {
      auto value = this->stringPropertyGet(string, property);
      if (value.hasFlowControl()) {
        return value;
      }
      auto pointer = ObjectFactory::createPointerToValue(this->vm, value, Modifiability::Read);
      assert(pointer != nullptr);
      return this->createHardValueObject(pointer);
    }
    HardValue typePropertyGet(const Type& type, const HardValue& property) {
      // TODO
      assert(type != nullptr);
      String name;
      if (!property->getString(name)) {
        return this->raiseRuntimeError("Expected right-hand side of type operator '.' to be a property name, but instead got ", describe(property));
      }
      return this->raiseRuntimeError("Type '", *type, "' does not support the property '", name, "'");
    }
    HardValue typePropertyRef(const Type& type, const HardValue& property) {
      auto value = this->typePropertyGet(type, property);
      if (value.hasFlowControl()) {
        return value;
      }
      auto pointer = ObjectFactory::createPointerToValue(this->vm, value, Modifiability::Read);
      assert(pointer != nullptr);
      return this->createHardValueObject(pointer);
    }
  };

  HardValue VMExecution::debugSymtable() {
    // TODO debugging only
    StringBuilder sb;
    this->runner->printSymtable(sb);
    return this->createHardValueString(sb.build(this->vm.getAllocator()));
  }

  class VMDefault : public HardReferenceCountedAllocator<IVM> {
    VMDefault(const VMDefault&) = delete;
    VMDefault& operator=(const VMDefault&) = delete;
  protected:
    HardPtr<IBasket> basket;
    ILogger& logger;
    HardPtr<ITypeForge> forge;
    std::map<ValueFlags, HardObject> manifestations;
  public:
    VMDefault(IAllocator& allocator, ILogger& logger)
      : HardReferenceCountedAllocator<IVM>(allocator),
        basket(BasketFactory::createBasket(allocator)),
        logger(logger) {
      this->forge = TypeForgeFactory::createTypeForge(allocator, *this->basket);
      this->manifestations.emplace(ValueFlags::Type, ObjectFactory::createManifestationType(*this));
      this->manifestations.emplace(ValueFlags::Void, ObjectFactory::createManifestationVoid(*this));
      this->manifestations.emplace(ValueFlags::Bool, ObjectFactory::createManifestationBool(*this));
      this->manifestations.emplace(ValueFlags::Int, ObjectFactory::createManifestationInt(*this));
      this->manifestations.emplace(ValueFlags::Float, ObjectFactory::createManifestationFloat(*this));
      this->manifestations.emplace(ValueFlags::String, ObjectFactory::createManifestationString(*this));
      this->manifestations.emplace(ValueFlags::Object, ObjectFactory::createManifestationObject(*this));
      this->manifestations.emplace(ValueFlags::Any, ObjectFactory::createManifestationAny(*this));
    }
    virtual IAllocator& getAllocator() const override {
      return this->allocator;
    }
    virtual IBasket& getBasket() const override {
      return *this->basket;
    }
    virtual ILogger& getLogger() const override {
      return this->logger;
    }
    virtual ITypeForge& getTypeForge() const override {
      return *this->forge;
    }
    virtual HardObject getManifestation(ValueFlags flags) override {
      auto found = this->manifestations.find(flags);
      if (found != this->manifestations.end()) {
        return found->second;
      }
      return HardObject();
    }
    virtual String createStringUTF8(const void* utf8, size_t bytes, size_t codepoints) override {
      return String::fromUTF8(this->allocator, utf8, bytes, codepoints);
    }
    virtual String createStringUTF32(const void* utf32, size_t codepoints) override {
      return String::fromUTF32(this->allocator, utf32, codepoints);
    }
    virtual HardPtr<IVMProgramBuilder> createProgramBuilder() override {
      return HardPtr(this->getAllocator().makeRaw<VMProgramBuilder>(*this));
    }
    virtual HardValue createHardValueVoid() override {
      return HardValue::Void;
    }
    virtual HardValue createHardValueNull() override {
      return HardValue::Null;
    }
    virtual HardValue createHardValueBool(Bool value) override {
      return value ? HardValue::True : HardValue::False;
    }
    virtual HardValue createHardValueInt(Int value) override {
      return ValueFactory::createInt(this->allocator, value);
    }
    virtual HardValue createHardValueFloat(Float value) override {
      return ValueFactory::createFloat(this->allocator, value);
    }
    virtual HardValue createHardValueString(const String& value) override {
      return ValueFactory::createString(this->allocator, value);
    }
    virtual HardValue createHardValueObject(const HardObject& value) override {
      return ValueFactory::createHardObject(this->allocator, value);
    }
    virtual HardValue createHardValueType(const Type& value) override {
      return ValueFactory::createType(this->allocator, value);
    }
    virtual HardObject createBuiltinAssert() override {
      return ObjectFactory::createBuiltinAssert(*this);
    }
    virtual HardObject createBuiltinPrint() override {
      return ObjectFactory::createBuiltinPrint(*this);
    }
    virtual HardObject createBuiltinExpando() override {
      return ObjectFactory::createBuiltinExpando(*this);
    }
    virtual HardObject createBuiltinCollector() override {
      return ObjectFactory::createBuiltinCollector(*this);
    }
    virtual HardObject createBuiltinSymtable() override {
      return ObjectFactory::createBuiltinSymtable(*this);
    }
  private:
    virtual void softAcquire(ICollectable*& target, const ICollectable* value) override {
      // TODO: thread safety
      assert(target == nullptr);
      if (value != nullptr) {
        target = this->basket->take(*value);
      } else {
        target = nullptr;
      }
    }
    virtual IValue* softHarden(const IValue* soft) override {
      // TODO: thread safety
      return const_cast<IValue*>(soft);
    }
    virtual IValue* softCreateValue(const IValue* init) override {
      // TODO: thread safety
      auto* created = SoftValue::createPoly(this->allocator);
      if (created != nullptr) {
        auto* taken = this->basket->take(*created);
        if (taken == created) {
          if ((init == nullptr) || created->set(*init)) {
            return created;
          }
        }
      }
      return nullptr;
    }
    virtual IValue* softCreateAlias(const IValue& value) override {
      // TODO: thread safety
      auto* taken = this->basket->take(value);
      assert(taken == &value);
      return static_cast<IValue*>(taken);
    }
    virtual IValue* softCreateOwned(const IValue& hard) override {
      // TODO: optimize
      // TODO: thread safety
      // If we would need to transfer to our basket, clone it instead
      if (hard.softGetBasket() == this->basket.get()) {
        return const_cast<IValue*>(&hard);
      }
      auto cloned = this->softCreateValue(&hard);
      assert((cloned == nullptr) || (cloned->softGetBasket() == this->basket.get()));
      return cloned;
    }
    virtual bool softSetValue(IValue*& target, const IValue& value) override {
      // TODO: thread safety
      // Note we do not currently update the target pointer but may do later for optimization reasons
      assert(target != nullptr);
      return target->set(value);
    }
    virtual HardValue softMutateValue(IValue*& target, ValueMutationOp mutation, const IValue& value) override {
      // TODO: thread safety
      // Note we do not currently update the target pointer but may do later for optimization reasons
      assert(target != nullptr);
      return target->mutate(mutation, value);
    }
  };
}

void egg::ovum::IVMModule::Node::printLocation(Printer& printer) const {
  printer << this->module.getResource() << this->range << ": ";
}

void egg::ovum::IVMModule::Node::hardDestroy() const {
  this->module.getAllocator().destroy(this);
}

bool VMRunner::stepNode(HardValue& retval) {
  // Return false iff 'retval' has been set and the block has finished
  auto& top = this->stack.top();
  switch (top.node->kind) {
  case IVMModule::Node::Kind::Root:
    assert(top.node->literal->getVoid());
    assert(top.index <= top.node->children.size());
    return this->stepBlock(retval);
  case IVMModule::Node::Kind::StmtBlock:
    assert(top.node->literal->getVoid());
    assert(top.index <= top.node->children.size());
    if (!this->stepBlock(retval)) {
      return this->pop(retval);
    }
    break;
  case IVMModule::Node::Kind::StmtFunctionDefine:
    assert(top.node->children.size() >= 2);
    assert(top.index <= top.node->children.size());
    if (top.index == 0) {
      // Evaluate the type
      this->push(*top.node->children[top.index++]);
    } else if (top.index == 1) {
      // Set the symbol
      assert(top.deque.size() == 1);
      auto& ftype = top.deque.front();
      if (ftype.hasFlowControl()) {
        return this->pop(ftype);
      }
      Type type;
      if (!ftype->getHardType(type) || (type == nullptr)) {
        return this->raise("Invalid type literal module node for function definition");
      }
      if (!this->variableScopeBegin(top, type)) {
        return true;
      }
      top.deque.clear();
      // Find the statements after the capture symbols
      while ((top.index < (top.node->children.size() - 1)) && (top.node->children[top.index]->kind == IVMModule::Node::Kind::StmtFunctionCapture)) {
        top.index++;
      }
      assert(top.node->children[top.index]->kind == IVMModule::Node::Kind::StmtFunctionInvoke); // WIBBLE
      if (!this->symbolSet(top.node, this->functionConstruct(type, *top.node))) {
        return true;
      }
      // Skip the function invocation block
      top.index++;
      // Perform the first statement of the scope of the function name
      this->push(*top.node->children[top.index++]);
    } else if (!this->stepBlock(retval, 2)) {
      return this->pop(retval);
    }
    break;
  case IVMModule::Node::Kind::StmtFunctionCapture:
   return this->raise("Unexpected function symbol capture module node encountered");
  case IVMModule::Node::Kind::StmtFunctionInvoke:
    // Placeholder actually pushed on to the stack by 'VMRunner::initiateTailCall()'
    assert(top.index <= top.node->children.size());
    if (!this->stepBlock(retval)) {
      if (retval.hasAnyFlags(ValueFlags::Return)) {
        retval->getInner(retval);
      }
      this->symtable.pop();
      return this->pop(retval);
    }
    break;
  case IVMModule::Node::Kind::StmtVariableDeclare:
    assert(top.node->children.size() >= 1);
    assert(top.index <= top.node->children.size());
    if (top.index == 0) {
      // Evaluate the type
      this->push(*top.node->children[top.index++]);
    } else {
      if (top.index == 1) {
        assert(top.deque.size() == 1);
        auto& vtype = top.deque.front();
        if (vtype.hasFlowControl()) {
          return this->pop(vtype);
        }
        Type type;
        if (!vtype->getHardType(type) || (type == nullptr)) {
          return this->raise("Invalid type literal module node for variable declaration");
        }
        if (!this->variableScopeBegin(top, type)) {
          return true;
        }
        top.deque.clear();
      }
      if (!this->stepBlock(retval, 1)) {
        return this->pop(retval);
      }
    }
    break;
  case IVMModule::Node::Kind::StmtVariableDefine:
    assert(top.node->children.size() >= 2);
    assert(top.index <= top.node->children.size());
    if (top.index == 0) {
      // Evaluate the type
      this->push(*top.node->children[top.index++]);
    } else if (top.index == 1) {
      // Evaluate the initial value
      assert(top.deque.size() == 1);
      auto& vtype = top.deque.front();
      if (vtype.hasFlowControl()) {
        return this->pop(vtype);
      }
      Type type;
      if (!vtype->getHardType(type) || (type == nullptr)) {
        return this->raise("Invalid type literal module node for variable definition");
      }
      if (!this->variableScopeBegin(top, type)) {
        return true;
      }
      top.deque.clear();
      this->push(*top.node->children[top.index++]);
    } else {
      if (top.index == 2) {
        assert(top.deque.size() == 1);
        if (!this->symbolSet(top.node, top.deque.front())) {
          return true;
        }
        top.deque.clear();
      }
      if (!this->stepBlock(retval, 2)) {
        return this->pop(retval);
      }
    }
    break;
  case IVMModule::Node::Kind::StmtVariableSet:
    assert(top.node->children.size() == 1);
    assert(top.index <= top.node->children.size());
    if (top.index == 0) {
      // Evaluate the value
      this->push(*top.node->children[top.index++]);
    } else {
      assert(top.deque.size() == 1);
      if (!this->symbolSet(top.node, top.deque.front())) {
        return true;
      }
      return this->pop(HardValue::Void);
    }
    break;
  case IVMModule::Node::Kind::StmtVariableMutate:
    assert(top.node->children.size() == 1);
    assert(top.index <= 1);
    {
      // TODO: thread safety
      String symbol;
      if (!top.node->literal->getString(symbol)) {
        return this->raise("Invalid program node literal for variable identifier");
      }
      auto extant = this->symtable.find(symbol);
      if (extant == nullptr) {
        return this->raise("Unknown identifier: '", symbol, "'");
      }
      if (extant->kind == VMSymbolTable::Kind::Builtin) {
        if (top.node->mutationOp == ValueMutationOp::Assign) {
          return this->raise("Cannot re-assign built-in value: '", symbol, "'");
        }
        return this->raise("Cannot modify built-in value: '", symbol, "'");
      }
      HardValue lhs{ *extant->soft };
      if (top.index == 0) {
        assert(top.deque.empty());
        // TODO: Get correct rhs static type
        auto result = this->execution.precheckValueMutationOp(top.node->mutationOp, lhs, ValueFlags::AnyQ);
        if (!result.hasFlowControl()) {
          // Short-circuit (discard result)
          return this->pop(HardValue::Void);
        } else if (result->getPrimitiveFlag() == ValueFlags::Continue) {
          // Continue with evaluation of rhs
          this->push(*top.node->children[top.index++]);
        } else {
          return this->pop(result);
        }
      } else {
        assert(top.deque.size() == 1);
        // Check the rhs
        auto& rhs = top.deque.front();
        if (rhs.hasFlowControl()) {
          return this->pop(rhs);
        }
        auto before = this->execution.evaluateValueMutationOp(top.node->mutationOp, lhs, rhs);
        if (before.hasFlowControl()) {
          return this->pop(before);
        }
        return this->pop(HardValue::Void);
      }
    }
    break;
  case IVMModule::Node::Kind::StmtVariableUndeclare:
    assert(top.node->children.empty());
    assert(top.index == 0);
    assert(top.deque.empty());
    if (top.node->literal->getString(top.scope)) {
      this->variableScopeEnd(top);
    }
    return this->pop(HardValue::Void);
  case IVMModule::Node::Kind::StmtPropertySet:
    assert(top.node->literal->getVoid());
    assert(top.node->children.size() == 3);
    assert(top.index <= 3);
    if (!top.deque.empty()) {
      // Check the last evaluation
      auto& latest = top.deque.back();
      if (latest.hasFlowControl()) {
        return this->pop(latest);
      }
    }
    if (top.index < 3) {
      // Evaluate the expressions
      this->push(*top.node->children[top.index++]);
    } else {
      // Perform the property fetch (object targets only, not strings)
      assert(top.deque.size() == 3);
      auto& instance = top.deque.front();
      auto& property = top.deque[1];
      HardObject object;
      if (!instance->getHardObject(object)) {
        return this->raiseBadPropertyModification(instance, property);
      }
      auto& value = top.deque.back();
      return this->pop(object->vmPropertySet(this->execution, property, value));
    }
    break;
  case IVMModule::Node::Kind::StmtPropertyMutate:
    assert(top.node->literal->getVoid());
    assert(top.node->children.size() == 3);
    assert(top.index <= 3);
    if (!top.deque.empty()) {
      // Check the last evaluation
      auto& latest = top.deque.back();
      if (latest.hasFlowControl()) {
        return this->pop(latest);
      }
    }
    if (top.index < 3) {
      // Evaluate the expressions
      this->push(*top.node->children[top.index++]);
    } else {
      // Perform the property mutation (object targets only, not strings)
      assert(top.deque.size() == 3);
      auto& instance = top.deque.front();
      auto& property = top.deque[1];
      HardObject object;
      if (!instance->getHardObject(object)) {
        return this->raiseBadPropertyModification(instance, property);
      }
      auto& value = top.deque.back();
      auto result = object->vmPropertyMut(this->execution, property, top.node->mutationOp, value);
      return this->pop(result.hasFlowControl() ? result : HardValue::Void);
    }
    break;
  case IVMModule::Node::Kind::StmtIndexMutate:
    assert(top.node->literal->getVoid());
    assert(top.node->children.size() == 3);
    assert(top.index <= 3);
    if (!top.deque.empty()) {
      // Check the last evaluation
      auto& latest = top.deque.back();
      if (latest.hasFlowControl()) {
        return this->pop(latest);
      }
    }
    if (top.index < 3) {
      // Evaluate the expressions
      this->push(*top.node->children[top.index++]);
    } else {
      // Perform the current mutation (object targets only, not strings)
      assert(top.deque.size() == 3);
      auto& instance = top.deque.front();
      HardObject object;
      if (!instance->getHardObject(object)) {
        return this->raise("Expected left-hand side of index operator '[]' to be an object, but instead got ", describe(instance));
      }
      auto& index = top.deque[1];
      auto& value = top.deque.back();
      auto result = object->vmIndexMut(this->execution, index, top.node->mutationOp, value);
      return this->pop(result.hasFlowControl() ? result : HardValue::Void);
    }
    break;
  case IVMModule::Node::Kind::StmtPointerMutate:
    assert(top.node->literal->getVoid());
    assert(top.node->children.size() == 2);
    assert(top.index <= 2);
    if (!top.deque.empty()) {
      // Check the last evaluation
      auto& latest = top.deque.back();
      if (latest.hasFlowControl()) {
        return this->pop(latest);
      }
    }
    if (top.index < 2) {
      // Evaluate the expressions
      this->push(*top.node->children[top.index++]);
    } else {
      // Perform the current mutation
      assert(top.deque.size() == 2);
      auto& pointer = top.deque.front();
      HardObject object;
      if (!pointer->getHardObject(object)) {
        return this->raise("Expected expression after pointer operator '*' to be an object, but instead got ", describe(pointer));
      }
      auto& value = top.deque.back();
      auto result = object->vmPointeeMut(this->execution, top.node->mutationOp, value);
      return this->pop(result.hasFlowControl() ? result : HardValue::Void);
    }
    break;
  case IVMModule::Node::Kind::StmtIf:
    assert((top.node->children.size() == 2) || (top.node->children.size() == 3));
    assert(top.index <= top.node->children.size());
    if (top.index == 0) {
      // Evaluate the condition
      assert(top.deque.empty());
      this->push(*top.node->children[top.index++]);
    } else {
      assert(top.deque.size() == 1);
      auto& latest = top.deque.back();
      if (latest.hasFlowControl()) {
        return this->pop(latest);
      }
      if (top.index == 1) {
        // Test the condition
        Bool condition;
        if (!latest->getBool(condition)) {
          return this->raise("Expected 'if' condition to be a 'bool', but instead got ", describe(latest));
        }
        top.deque.clear();
        if (condition) {
          // Perform the compulsory "when true" block
          this->push(*top.node->children[top.index++]);
        } else if (top.node->children.size() > 2) {
          // Perform the optional "when false" block
          top.index++;
          this->push(*top.node->children[top.index++]);
        } else {
          // Skip both blocks
          return this->pop(HardValue::Void);
        }
      } else {
        // The controlled block has completed
        assert(latest->getVoid());
        return this->pop(latest);
      }
    }
    break;
  case IVMModule::Node::Kind::StmtWhile:
    assert(top.node->children.size() == 2);
    assert(top.index <= 2);
    if (top.index == 0) {
      // Evaluate the condition
      assert(top.deque.empty());
      this->push(*top.node->children[top.index++]);
    } else {
      assert(top.deque.size() == 1);
      auto& latest = top.deque.back();
      if (latest.hasFlowControl()) {
        // TODO break and continue
        return this->pop(latest);
      }
      if (top.index == 1) {
        // Test the condition
        Bool condition;
        if (!latest->getBool(condition)) {
          return this->raise("Expected 'while' condition to be a 'bool', but instead got ", describe(latest));
        }
        top.deque.clear();
        if (condition) {
          // Perform the controlled block
          this->push(*top.node->children[top.index++]);
        } else {
          // The condition failed
          return this->pop(HardValue::Void);
        }
      } else {
        // The controlled block has completed so re-evaluate the condition
        assert(latest->getVoid());
        top.deque.clear();
        // Evaluate the condition
        this->push(*top.node->children[0]);
        top.index = 1;
      }
    }
    break;
  case IVMModule::Node::Kind::StmtDo:
    assert(top.node->literal->getVoid());
    assert(top.node->children.size() == 2);
    assert(top.index <= 2);
    assert(top.scope.empty());
    if (top.index == 0) {
      // Perform the controlled block
      this->push(*top.node->children[top.index++]);
    } else {
      assert(top.deque.size() == 1);
      auto& latest = top.deque.back();
      if (latest.hasFlowControl()) {
        // TODO break and continue
        return this->pop(latest);
      }
      if (top.index == 1) {
        // The controlled block has completed so evaluate the condition
        assert(latest->getVoid());
        top.deque.clear();
        // Evaluate the condition
        this->push(*top.node->children[top.index++]);
      } else {
        // Test the condition
        Bool condition;
        if (!latest->getBool(condition)) {
          return this->raise("Expected 'do' condition to be a 'bool', but instead got ", describe(latest));
        }
        top.deque.clear();
        if (condition) {
          // Perform the controlled block again
          this->push(*top.node->children[0]);
          top.index = 1;
        } else {
          // The condition failed
          return this->pop(HardValue::Void);
        }
      }
    }
    break;
  case IVMModule::Node::Kind::StmtForEach:
    assert(top.node->children.size() == 3);
    if (top.index == 0) {
      // Evaluate the type
      this->push(*top.node->children[top.index++]);
    } else if (top.index == 1) {
      // Evaluate the iterator value
      assert(top.deque.size() == 1);
      auto& vtype = top.deque.front();
      if (vtype.hasFlowControl()) {
        return this->pop(vtype);
      }
      Type type;
      if (!vtype->getHardType(type) || (type == nullptr)) {
        return this->raise("Invalid type literal module node for 'for' each variable");
      }
      if (!this->variableScopeBegin(top, type)) {
        return true;
      }
      top.deque.clear();
      this->push(*top.node->children[top.index++]);
    } else {
      assert(top.deque.size() >= 1);
      auto& iterator = top.deque.front();
      HardObject object;
      if (top.index == 2) {
        // Check the iterator
        if (iterator.hasFlowControl()) {
          return this->pop(iterator);
        }
        if (iterator->getHardObject(object)) {
          // Make sure this object is iterable
          iterator = object->vmIterate(execution);
          if (iterator.hasFlowControl()) {
            return this->pop(iterator);
          }
        }
        assert(top.deque.size() == 1);
      } else {
        // We're executing the controlled block
        assert(top.deque.size() == 2);
        auto& latest = top.deque.back();
        if (latest.hasFlowControl()) {
          // TODO: handle break and continue
          return this->pop(iterator);
        }
      }
      // Fetch the next iterated value
      HardValue value;
      if (iterator->getHardObject(object)) {
        // Iterate over the values returned from a "<type>()" function
        CallArguments empty{};
        value = object->vmCall(execution, empty);
        if (value.hasFlowControl() || value->getVoid()) {
          // Failed or completed
          return this->pop(value);
        }
        top.index++;
      } else {
        String string;
        if (iterator->getString(string)) {
          // Iterate over the codepoints in the string
          auto cp = string.codePointAt(top.index++ - 2);
          if (cp < 0) {
            // Completed
            return this->pop(HardValue::Void);
          }
          value = this->createHardValueString(String::fromUTF32(this->getAllocator(), &cp, 1));
        } else {
          // Object iteration
          return this->raise("Iteration by 'for' each statement is not supported by ", describe(iterator));
        }
      }
      // Assign to the control variable
      if (!this->symbolSet(top.node, value)) {
        return true;
      }
      // Execute the controlled block
      top.deque.resize(1);
      this->push(*top.node->children[2]);
    }
    break;
  case IVMModule::Node::Kind::StmtForLoop:
    assert(top.node->children.size() == 4);
    assert(top.index <= 4);
    if (top.index == 0) {
      // Perform 'initial'
      assert(top.deque.empty());
      this->push(*top.node->children[top.index++]);
    } else {
      assert(top.deque.size() == 1);
      auto& latest = top.deque.back();
      if (latest.hasFlowControl()) {
        // TODO break and continue
        return this->pop(latest);
      }
      if (top.index == 1) {
        // Evaluate the condition
        assert(latest->getVoid());
        top.deque.clear();
        this->push(*top.node->children[top.index++]);
      } else if (top.index == 2) {
        // Test the condition
        Bool condition;
        if (!latest->getBool(condition)) {
          return this->raise("Expected 'for' condition to be a 'bool', but instead got ", describe(latest));
        }
        top.deque.clear();
        if (condition) {
          // Perform the controlled block
          this->push(*top.node->children[top.index++]);
        } else {
          // The condition failed
          return this->pop(HardValue::Void);
        }
      } else if (top.index == 3) {
        // The controlled block has completed so perform 'advance'
        assert(latest->getVoid());
        top.deque.clear();
        this->push(*top.node->children[top.index++]);
      } else {
        // The third clause has completed so re-evaluate the condition
        assert(latest->getVoid());
        top.deque.clear();
        this->push(*top.node->children[1]);
        top.index = 2;
      }
    }
    break;
  case IVMModule::Node::Kind::StmtSwitch:
    assert(top.node->literal->getVoid());
    assert(top.node->children.size() > 1);
    if (top.index == 0) {
      if (top.deque.empty()) {
        // Evaluate the switch expression
        this->push(*top.node->children[0]);
        assert(top.index == 0);
      } else {
        // Test the switch expression evaluation
        assert(top.deque.size() == 1);
        auto& latest = top.deque.back();
        if (latest.hasFlowControl()) {
          return this->pop(latest);
        }
        // Match the first case/default statement
        top.index = 1;
        auto& added = this->push(*top.node->children[1]);
        assert(added.node->kind == IVMModule::Node::Kind::StmtCase);
        added.value = latest;
      }
    } else if (top.deque.size() == 1) {
      // Just run an unconditional block
      auto& latest = top.deque.back();
      IVMModule::Node* child;
      EGG_WARNING_SUPPRESS_SWITCH_BEGIN
      switch (latest->getPrimitiveFlag()) {
      case ValueFlags::Break:
        // Matched; break out of the switch statement
        return this->pop(HardValue::Void);
      case ValueFlags::Continue:
        // Matched; continue to next block without re-testing the expression
        top.deque.clear();
        if (++top.index >= top.node->children.size()) {
          top.index = 1;
        }
        child = top.node->children[top.index];
        assert(child->kind == IVMModule::Node::Kind::StmtCase);
        assert(!child->children.empty());
        this->push(*child->children.front());
        break;
      default:
        assert(latest.hasFlowControl());
        return this->pop(latest);
      }
      EGG_WARNING_SUPPRESS_SWITCH_END
    } else {
      // Just matched a case/default clause
      auto& latest = top.deque.back();
      IVMModule::Node* child;
      EGG_WARNING_SUPPRESS_SWITCH_BEGIN
      switch (latest->getPrimitiveFlag()) {
      case ValueFlags::Bool:
        // The switch expression does not match this case/default clause
        if (++top.index < top.node->children.size()) {
          // Match the next case/default statement
          top.deque.pop_back();
          assert(top.deque.size() == 1);
          auto& added = this->push(*top.node->children[top.index]);
          assert(added.node->kind == IVMModule::Node::Kind::StmtCase);
          added.value = top.deque.front();
        } else {
          // That was the last clause; we need to use the default clause, if any
          if (top.node->defaultIndex == 0) {
            // No default clause
            return this->pop(HardValue::Void);
          } else {
            // Prepare to run the block associated with the default clause
            top.deque.clear();
            top.index = top.node->defaultIndex;
            child = top.node->children[top.index];
            assert(child->kind == IVMModule::Node::Kind::StmtCase);
            assert(!child->children.empty());
            this->push(*child->children.front());
          }
        }
        break;
      case ValueFlags::Break:
        // Matched; break out of the switch statement
        return this->pop(HardValue::Void);
      case ValueFlags::Continue:
        // Matched; continue to next block without re-testing the expression
        top.deque.clear();
        if (++top.index >= top.node->children.size()) {
          top.index = 1;
        }
        child = top.node->children[top.index];
        assert(child->kind == IVMModule::Node::Kind::StmtCase);
        assert(!child->children.empty());
        this->push(*child->children.front());
        break;
      default:
        assert(latest.hasFlowControl());
        return this->pop(latest);
      }
      EGG_WARNING_SUPPRESS_SWITCH_END
    }
    break;
  case IVMModule::Node::Kind::StmtCase:
    assert(top.node->literal->getVoid());
    assert(!top.node->children.empty());
    if (top.index == 0) {
      assert(top.deque.empty());
      if (top.node->children.size() == 1) {
        // This is a 'default' clause ('case' with no expressions)
        return this->pop(HardValue::False);
      }
      // Evaluate the first expression
      this->push(*top.node->children[1]);
      top.index = 1;
    } else if (top.index < top.node->children.size()) {
      // Test the evaluation
      assert(top.deque.size() == 1);
      auto& latest = top.deque.back();
      if (latest.hasFlowControl()) {
        return this->pop(latest);
      }
      if (Operation::areEqual(top.value, latest, true, false)) { // TODO: promote?
        // Got a match, so execute the block
        top.deque.clear();
        this->push(*top.node->children[0]);
        top.index = top.node->children.size();
      } else {
        // Step to the next case expression
        if (++top.index < top.node->children.size()) {
          // Evaluate the next expression
          top.deque.clear();
          this->push(*top.node->children[top.index]);
        } else {
          // No expressions matched
          return this->pop(HardValue::False);
        }
      }
    } else {
      // Test the block execution
      assert(top.deque.size() == 1);
      auto& latest = top.deque.back();
      EGG_WARNING_SUPPRESS_SWITCH_BEGIN
      switch (latest->getPrimitiveFlag()) {
      case ValueFlags::Void:
        this->log(ILogger::Source::Runtime, ILogger::Severity::Warning, this->createString("Fell off the end of the case/default clause")); // TODO
        return this->pop(HardValue::Continue);
      case ValueFlags::Break:
        // Explicit 'break'
        return this->pop(HardValue::Break);
      case ValueFlags::Continue:
        // Explicit 'continue'
        return this->pop(HardValue::Continue);
      default:
        // Probably an exception
        if (latest.hasFlowControl()) {
          return this->pop(latest);
        }
        this->log(ILogger::Source::Runtime, ILogger::Severity::Warning, this->createString("Discarded value in switch case/default clause")); // TODO
        return this->pop(HardValue::Void);
      }
      EGG_WARNING_SUPPRESS_SWITCH_END
    }
    break;
  case IVMModule::Node::Kind::StmtBreak:
    assert(top.node->literal->getVoid());
    assert(top.node->children.empty());
    return this->pop(HardValue::Break);
  case IVMModule::Node::Kind::StmtContinue:
    assert(top.node->literal->getVoid());
    assert(top.node->children.empty());
    return this->pop(HardValue::Continue);
  case IVMModule::Node::Kind::StmtThrow:
    assert(top.node->literal->getVoid());
    assert(top.node->children.size() == 1);
    assert(top.index <= 1);
    if (top.index == 0) {
      // Evaluate the exception
      assert(top.deque.empty());
      this->push(*top.node->children[top.index++]);
    } else {
      // Check the exception
      assert(top.deque.size() == 1);
      auto& value = top.deque.front();
      if (value.hasFlowControl()) {
        return this->pop(value);
      }
      auto result = this->execution.raiseException(value);
      return this->pop(result);
    }
    break;
  case IVMModule::Node::Kind::StmtTry:
    assert(top.node->literal->getVoid());
    assert(top.node->children.size() > 1);
    if (top.index == 0) {
      // Execute the try block
      assert(top.deque.empty());
      this->push(*top.node->children[top.index++]);
    } else {
      // Check any exception
      assert(!top.deque.empty());
      auto& exception = top.deque.front();
      if (top.index == 1) {
        // Just executed the try block
        assert(top.deque.size() == 1);
        if (exception->getPrimitiveFlag() == ValueFlags::Throw) {
          // Erroneous rethrow within try block
          return this->raise("Unexpected exception rethrow within 'try' block");
        }
        top.value = exception;
      } else {
        // Just executed a catch/finally block
        assert(top.deque.size() == 2);
        auto* previous = top.node->children[top.index - 1];
        auto& latest = top.deque.back();
        if (latest->getPrimitiveFlag() == ValueFlags::Throw) {
          // Rethrow the original exception
          if (previous->kind != IVMModule::Node::Kind::StmtCatch) {
            return this->raise("Unexpected exception rethrow within 'finally' block");
          }
          assert(exception.hasAnyFlags(ValueFlags::Throw));
        } else if (latest.hasFlowControl()) {
          // The catch/finally block raised a different exception
          top.value = latest;
          exception = HardValue::Void;
        } else if (previous->kind == IVMModule::Node::Kind::StmtCatch) {
          // We've successfully handled the exception
          top.value = HardValue::Void;
          exception = HardValue::Void;
        } else {
          // We've completed a finally block
          assert(latest->getPrimitiveFlag() == ValueFlags::Void);
        }
        top.deque.pop_back();
      }
      assert(top.deque.size() == 1);
      while (top.index < top.node->children.size()) {
        // Check the next clause
        auto* child = top.node->children[top.index++];
        if (child->kind != IVMModule::Node::Kind::StmtCatch) {
          // Always run finally clauses
          this->push(*child);
          return true;
        } else if (exception.hasAnyFlags(ValueFlags::Throw)) {
          // Only run a catch clause if an exception is still outstanding
          auto& added = this->push(*child);
          added.value = exception;
          return true;
        }
      }
      return this->pop(top.value);
    }
    break;
  case IVMModule::Node::Kind::StmtCatch:
    assert(top.index <= top.node->children.size());
    if (top.index == 0) {
      // Evaluate the type
      this->push(*top.node->children[top.index++]);
    } else {
      if (top.index == 1) {
        // Evaluate the initial value
        assert(top.deque.size() == 1);
        auto& ctype = top.deque.front();
        if (ctype.hasFlowControl()) {
          return this->pop(ctype);
        }
        Type type;
        if (!ctype->getHardType(type) || (type == nullptr)) {
          return this->raise("Invalid type literal module node for 'catch' clause");
        }
        if (!this->variableScopeBegin(top, type)) {
          return true;
        }
        HardValue inner;
        if (!top.value->getInner(inner)) {
          return this->raise("Invalid 'catch' clause expression");
        }
        if (!this->symbolSet(top.node, inner)) {
          return true;
        }
        top.deque.clear();
      }
      if (!this->stepBlock(retval, 1)) {
        return this->pop(retval);
      }
    }
    break;
  case IVMModule::Node::Kind::StmtRethrow:
    assert(top.node->literal->getVoid());
    assert(top.node->children.empty());
    return this->pop(HardValue::Rethrow);
  case IVMModule::Node::Kind::StmtReturn:
    assert(top.node->literal->getVoid());
    assert(top.node->children.size() <= 1);
    if (top.node->children.empty()) {
      // No value expression
      return this->pop(ValueFactory::createHardReturn(this->getAllocator(), HardValue::Void));
    }
    if (top.index == 0) {
      // Evaluate the expression
      this->push(*top.node->children[top.index++]);
    } else {
      // Return the value
      assert(top.index == 1);
      assert(top.deque.size() == 1);
      auto& result = top.deque.back();
      if (result.hasFlowControl()) {
        return this->pop(result);
      }
      return this->pop(ValueFactory::createHardReturn(this->getAllocator(), result));
    }
    break;
  case IVMModule::Node::Kind::StmtYield:
  case IVMModule::Node::Kind::StmtYieldAll:
  case IVMModule::Node::Kind::StmtYieldBreak:
  case IVMModule::Node::Kind::StmtYieldContinue:
    return this->raise("'yield' statements are not yet supported");
  case IVMModule::Node::Kind::ExprFunctionCall:
    assert(top.node->literal->getVoid());
    assert(top.index <= top.node->children.size());
    if (!top.deque.empty()) {
      // Check the last evaluation
      auto& latest = top.deque.back();
      if (latest.hasFlowControl()) {
        return this->pop(latest);
      }
    }
    if (top.index < top.node->children.size()) {
      // Assemble the arguments
      this->push(*top.node->children[top.index++]);
    } else {
      // Perform the function call
      assert(top.deque.size() >= 1);
      HardObject function;
      auto& head = top.deque.front();
      if (!head->getHardObject(function)) {
        return this->raise("Function calls are not supported by ", describe(head));
      }
      top.deque.pop_front();
      auto mnode = top.node->children.cbegin();
      CallArguments arguments;
      for (auto& argument : top.deque) {
        // TODO support named arguments
        arguments.addUnnamed(argument, &(*++mnode)->range);
      }
      auto result = function->vmCall(this->execution, arguments);
      if (result->getPrimitiveFlag() == ValueFlags::Continue) {
        // The invocation resulted in a tail call
        return true;
      }
      return this->pop(result);
    }
    break;
  case IVMModule::Node::Kind::ExprUnaryOp:
    assert(top.node->children.size() == 1);
    assert(top.index <= 1);
    if (top.index == 0) {
      // Evaluate the operand
      assert(top.deque.empty());
      this->push(*top.node->children[top.index++]);
    } else {
      // Check the operand
      assert(top.deque.size() == 1);
      auto& latest = top.deque.back();
      if (latest.hasFlowControl()) {
        return this->pop(latest);
      }
      auto result = this->execution.evaluateValueUnaryOp(top.node->unaryOp, top.deque.front());
      return this->pop(result);
    }
    break;
  case IVMModule::Node::Kind::ExprBinaryOp:
    assert(top.node->children.size() == 2);
    assert(top.index <= 2);
    if (top.index == 0) {
      // Evaluate the left-hand-side
      this->push(*top.node->children[top.index++]);
    } else {
      // Check the last evaluation
      auto& latest = top.deque.back();
      if (latest.hasFlowControl()) {
        return this->pop(latest);
      }
      if (top.index == 1) {
        assert(top.deque.size() == 1);
        if (top.node->binaryOp == ValueBinaryOp::IfNull) {
          // Short-circuit '??'
          if (!latest->getNull()) {
            // lhs is not null; no need to evaluate rhs
            return this->pop(latest);
          }
        } else if (top.node->binaryOp == ValueBinaryOp::IfFalse) {
          // Short-circuit '||'
          Bool lhs;
          if (latest->getBool(lhs) && lhs) {
            // lhs is true; no need to evaluate rhs
            return this->pop(HardValue::True);
          }
        } else if (top.node->binaryOp == ValueBinaryOp::IfTrue) {
          // Short-circuit '&&'
          Bool lhs;
          if (latest->getBool(lhs) && !lhs) {
            // lhs is false; no need to evaluate rhs
            return this->pop(HardValue::False);
          }
        }
        this->push(*top.node->children[top.index++]);
      } else {
        assert(top.deque.size() == 2);
        auto result = this->execution.evaluateValueBinaryOp(top.node->binaryOp, top.deque.front(), top.deque.back());
        return this->pop(result);
      }
    }
    break;
  case IVMModule::Node::Kind::ExprTernaryOp:
    assert(top.node->ternaryOp == ValueTernaryOp::IfThenElse);
    assert(top.node->children.size() == 3);
    assert(top.index <= 3);
    if (top.index == 0) {
      // Evaluate the condition
      this->push(*top.node->children[top.index++]);
    } else {
      // Check the last evaluation
      auto& latest = top.deque.back();
      if (latest.hasFlowControl()) {
        return this->pop(latest);
      }
      if (top.index == 1) {
        // Short-circuit '?:'
        assert(top.deque.size() == 1);
        Bool condition;
        if (!latest->getBool(condition)) {
          // The second and third operands are irrelevant; we just want the error message
          auto fail = this->execution.evaluateValueTernaryOp(top.node->ternaryOp, latest, HardValue::Void, HardValue::Void);
          return this->pop(fail);
        }
        this->push(*top.node->children[condition ? 1u : 2u]);
        top.index++;
      } else {
        return this->pop(latest);
      }
    }
    break;
  case IVMModule::Node::Kind::ExprPredicateOp:
    assert(!top.node->children.empty());
    assert(top.index <= top.node->children.size());
    if (!top.deque.empty()) {
      // Check the last evaluation
      auto& latest = top.deque.back();
      if (latest.hasFlowControl()) {
        return this->pop(latest);
      }
    }
    if (top.index < top.node->children.size()) {
      // Assemble the operands
      this->push(*top.node->children[top.index++]);
    } else {
      // Execute the predicate
      switch (top.deque.size()) {
      case 1:
        return this->pop(this->execution.evaluateValuePredicateOp(top.node->predicateOp, top.deque.front(), HardValue::Void));
      case 2:
        return this->pop(this->execution.evaluateValuePredicateOp(top.node->predicateOp, top.deque.front(), top.deque.back()));
      }
      return this->raise("Invalid predicate values");
    }
    break;
  case IVMModule::Node::Kind::ExprLiteral:
    assert(top.node->children.empty());
    assert(top.index == 0);
    assert(top.deque.empty());
    return this->pop(top.node->literal);
  case IVMModule::Node::Kind::ExprVariableGet:
    assert(top.node->children.empty());
    assert(top.index == 0);
    assert(top.deque.empty());
    {
      String symbol;
      if (!top.node->literal->getString(symbol)) {
        return this->raise("Invalid program node literal for identifier");
      }
      auto extant = this->symtable.find(symbol);
      if (extant == nullptr) {
        return this->raise("Unknown identifier: '", symbol, "'");
      }
      HardValue result{ *extant->soft };
      if (result->getVoid()) {
        return this->raise("Variable uninitialized: '", symbol, "'");
      }
      return this->pop(result);
    }
    break;
  case IVMModule::Node::Kind::ExprVariableRef:
    assert(top.node->children.empty());
    assert(top.index == 0);
    assert(top.deque.empty());
    {
      String symbol;
      if (!top.node->literal->getString(symbol)) {
        return this->raise("Invalid program node literal for identifier");
      }
      auto extant = this->symtable.find(symbol);
      if (extant == nullptr) {
        return this->raise("Unknown identifier: '", symbol, "'");
      }
      auto modifiability = (extant->kind == VMSymbolTable::Kind::Builtin) ? Modifiability::Read : Modifiability::ReadWriteMutate;
      HardValue pointee{ *extant->soft };
      auto pointer = ObjectFactory::createPointerToValue(this->vm, pointee, modifiability);
      assert(pointer != nullptr);
      return this->pop(this->createHardValueObject(pointer));
    }
    break;
  case IVMModule::Node::Kind::ExprIndexGet:
    assert(top.node->literal->getVoid());
    assert(top.node->children.size() == 2);
    if (!top.deque.empty()) {
      // Check the last evaluation
      auto& latest = top.deque.back();
      if (latest.hasFlowControl()) {
        return this->pop(latest);
      }
    }
    if (top.index < 2) {
      // Assemble the arguments
      this->push(*top.node->children[top.index++]);
    } else {
      // Perform the current fetch
      assert(top.deque.size() == 2);
      auto& lhs = top.deque.front();
      auto& rhs = top.deque.back();
      HardObject object;
      if (lhs->getHardObject(object)) {
        return this->pop(object->vmIndexGet(this->execution, rhs));
      }
      String string;
      if (lhs->getString(string)) {
        return this->pop(this->stringIndexGet(string, rhs));
      }
      return this->raise("Expected left-hand side of index operator '[]' to support indexing, but instead got ", describe(lhs));
    }
    break;
  case IVMModule::Node::Kind::ExprIndexRef:
    assert(top.node->literal->getVoid());
    assert(top.node->children.size() == 2);
    if (!top.deque.empty()) {
      // Check the last evaluation
      auto& latest = top.deque.back();
      if (latest.hasFlowControl()) {
        return this->pop(latest);
      }
    }
    if (top.index < 2) {
      // Assemble the arguments
      this->push(*top.node->children[top.index++]);
    } else {
      // Perform the current fetch
      assert(top.deque.size() == 2);
      auto& lhs = top.deque.front();
      auto& rhs = top.deque.back();
      HardObject object;
      if (lhs->getHardObject(object)) {
        return this->pop(object->vmIndexRef(this->execution, rhs));
      }
      String string;
      if (lhs->getString(string)) {
        return this->pop(this->stringIndexRef(string, rhs));
      }
      return this->raise("Expected left-hand side of index operator '[]' to support indexing, but instead got ", describe(lhs));
    }
    break;
  case IVMModule::Node::Kind::ExprPropertyGet:
    assert(top.node->literal->getVoid());
    assert(top.node->children.size() == 2);
    if (!top.deque.empty()) {
      // Check the last evaluation
      auto& latest = top.deque.back();
      if (latest.hasFlowControl()) {
        return this->pop(latest);
      }
    }
    if (top.index < 2) {
      // Assemble the arguments
      this->push(*top.node->children[top.index++]);
    } else {
      // Perform the property fetch
      assert(top.deque.size() == 2);
      auto& lhs = top.deque.front();
      auto& rhs = top.deque.back();
      HardObject object;
      if (lhs->getHardObject(object)) {
        return this->pop(object->vmPropertyGet(this->execution, rhs));
      }
      String string;
      if (lhs->getString(string)) {
        return this->pop(this->stringPropertyGet(string, rhs));
      }
      Type type;
      if (lhs->getHardType(type)) {
        return this->pop(this->typePropertyGet(type, rhs));
      }
      return this->raise("Expected left-hand side of property operator '.' to support properties, but instead got ", describe(lhs));
    }
    break;
  case IVMModule::Node::Kind::ExprPropertyRef:
    assert(top.node->literal->getVoid());
    assert(top.node->children.size() == 2);
    if (!top.deque.empty()) {
      // Check the last evaluation
      auto& latest = top.deque.back();
      if (latest.hasFlowControl()) {
        return this->pop(latest);
      }
    }
    if (top.index < 2) {
      // Assemble the arguments
      this->push(*top.node->children[top.index++]);
    } else {
      // Perform the property fetch
      assert(top.deque.size() == 2);
      auto& lhs = top.deque.front();
      auto& rhs = top.deque.back();
      HardObject object;
      if (lhs->getHardObject(object)) {
        return this->pop(object->vmPropertyRef(this->execution, rhs));
      }
      String string;
      if (lhs->getString(string)) {
        return this->pop(this->stringPropertyRef(string, rhs));
      }
      Type type;
      if (lhs->getHardType(type)) {
        return this->pop(this->typePropertyRef(type, rhs));
      }
      return this->raise("Expected left-hand side of property operator '.' to support properties, but instead got ", describe(lhs));
    }
    break;
  case IVMModule::Node::Kind::ExprPointeeGet:
    assert(top.node->literal->getVoid());
    assert(top.node->children.size() == 1);
    if (!top.deque.empty()) {
      // Check the last evaluation
      auto& latest = top.deque.back();
      if (latest.hasFlowControl()) {
        return this->pop(latest);
      }
    }
    if (top.index < 1) {
      // Assemble the arguments
      this->push(*top.node->children[top.index++]);
    } else {
      // Perform the current fetch
      assert(top.deque.size() == 1);
      auto& pointer = top.deque.front();
      HardObject object;
      if (pointer->getHardObject(object)) {
        return this->pop(object->vmPointeeGet(this->execution));
      }
      return this->raise("Expected expression after pointer operator '*' to be a pointer, but instead got ", describe(pointer));
    }
    break;
  case IVMModule::Node::Kind::ExprArray:
    assert(top.node->literal->getVoid());
    if (!top.deque.empty()) {
      // Check the last evaluation
      auto& latest = top.deque.back();
      if (latest.hasFlowControl()) {
        return this->pop(latest);
      }
    }
    if (top.index < top.node->children.size()) {
      // Assemble the elements
      this->push(*top.node->children[top.index++]);
    } else {
      // Construct the array
      return this->pop(this->arrayConstruct(top.deque, top.node->children));
    }
    break;
  case IVMModule::Node::Kind::ExprObject:
    if (!top.deque.empty()) {
      // Check the last evaluation
      auto& latest = top.deque.back();
      if (latest.hasFlowControl()) {
        return this->pop(latest);
      }
    }
    if (top.index < top.node->children.size()) {
      // Assemble the elements
      auto& child = *top.node->children[top.index++];
      assert(child.kind == IVMModule::Node::Kind::ExprNamed);
      assert(child.children.size() == 1);
      top.deque.push_back(child.literal);
      this->push(*child.children.front());
    } else {
      // Construct the object
      return this->pop(this->objectConstruct(top.deque));
    }
    break;
  case IVMModule::Node::Kind::ExprGuard:
    assert(top.node->children.size() == 1);
    if (top.index == 0) {
      // Evaluate the expression
      this->push(*top.node->children[top.index++]);
    } else {
      // Check the last evaluation
      assert(top.deque.size() == 1);
      auto& expr = top.deque.front();
      if (expr.hasFlowControl()) {
        return this->pop(expr);
      }
      return this->pop(this->symbolGuard(top.node->literal, expr));
    }
    break;
  case IVMModule::Node::Kind::ExprNamed:
    return this->raise("TODO: Named expression unexpected at this time");
  case IVMModule::Node::Kind::TypeInfer:
    assert(top.node->children.empty());
    assert(top.index == 0);
    assert(top.deque.empty());
    return this->raise("TODO: Type inferrence not yet implemented");
  case IVMModule::Node::Kind::TypeLiteral:
    assert(top.node->children.empty());
    assert(top.index == 0);
    assert(top.deque.empty());
    return this->pop(top.node->literal);
  case IVMModule::Node::Kind::TypeManifestation:
    assert(top.node->children.empty());
    assert(top.index == 0);
    assert(top.deque.empty());
    {
      Int flags;
      if (!top.node->literal->getInt(flags)) {
        return this->raise("Invalid program node literal for type manifestation");
      }
      auto manifestation = this->vm.getManifestation(ValueFlags(flags));
      return this->pop(this->createHardValueObject(manifestation));
    }
  default:
    return this->raise("Invalid program node kind in program runner");
  }
  // Keep going
  return true;
}

bool VMRunner::stepNodeChecked(HardValue& retval) {
  // Return false iff 'retval' has been set and the block has finished
  assert(this->validate());
  auto finished = this->stepNode(retval);
  assert(this->validate());
  return finished;
}

bool VMRunner::stepBlock(HardValue& retval, size_t first) {
  // Return false iff 'retval' has been set and the block has finished
  auto& top = this->stack.top();
  assert(top.index >= first);
  assert(top.index <= top.node->children.size());
  if (top.index > first) {
    // Check the result of the previous child statement
    assert(top.deque.size() == 1);
    auto& result = top.deque.back();
    if (result.hasFlowControl()) {
      retval = result;
      return false;
    }
    if (result->getPrimitiveFlag() != ValueFlags::Void) {
      this->log(ILogger::Source::Runtime, ILogger::Severity::Warning, this->createString("Discarded value in statement")); // TODO
    }
    top.deque.pop_back();
  }
  assert(top.deque.empty());
  if (top.index < top.node->children.size()) {
    // Execute all the statements
    this->push(*top.node->children[top.index++]);
  } else {
    // Reached the end of the list of statements
    retval = HardValue::Void;
    return false;
  }
  return true;
}

HardPtr<IVMRunner> VMModule::createRunner(IVMProgram& program) {
  auto runner = HardPtr(this->getAllocator().makeRaw<VMRunner>(this->vm, program, *this->root));
  auto taken = this->vm.getBasket().take(*runner);
  assert(taken == runner.get());
  (void)taken;
  return runner;
}

HardPtr<IVMRunner> VMProgram::createRunner() {
  if (this->modules.empty()) {
    return nullptr;
  }
  return this->modules.front()->createRunner(*this);
}

HardPtr<IObjectBuilder> VMExecution::createRuntimeErrorBuilder(const String& message, const SourceRange* source) {
  assert(this->runner != nullptr);
  return ObjectFactory::createRuntimeErrorBuilder(this->vm, message, this->runner->getCallStack(source));
}

HardValue VMExecution::raiseRuntimeError(const String& message, const SourceRange* source) {
  auto builder = this->createRuntimeErrorBuilder(message, source);
  assert(builder != nullptr);
  return this->raiseException(this->createHardValueObject(builder->build()));
}

HardValue VMExecution::initiateTailCall(const IFunctionSignature& signature, const ICallArguments& arguments, const IVMModule::Node& definition, const IVMCallCaptures* captures) {
  assert(this->runner != nullptr);
  return this->runner->initiateTailCall(signature, arguments, definition, captures);
}

egg::ovum::HardPtr<IVM> egg::ovum::VMFactory::createDefault(IAllocator& allocator, ILogger& logger) {
  return allocator.makeHard<VMDefault>(logger);
}
