#include "ovum/ovum.h"
#include "ovum/operation.h"

#include <stack>

namespace {
  class VMModule;
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
    ExprVariable,
    ExprLiteral,
    ExprPropertyGet,
    ExprStringCall,
    ExprFunctionCall,
    TypeInfer,
    TypeLiteral,
    StmtBlock,
    StmtVariableDeclare,
    StmtVariableDefine,
    StmtVariableSet,
    StmtVariableMutate,
    StmtPropertySet,
    StmtFunctionCall,
    StmtIf,
    StmtWhile,
    StmtDo,
    StmtFor,
    StmtSwitch,
    StmtCase,
    StmtBreak,
    StmtContinue,
    StmtThrow,
    StmtTry,
    StmtCatch,
    StmtRethrow
  };
  VMModule& module;
  Kind kind;
  size_t line; // May be zero
  size_t column; // May be zero
  HardValue literal; // Only stores simple literals
  union {
    ValueUnaryOp unaryOp;
    ValueBinaryOp binaryOp;
    ValueTernaryOp ternaryOp;
    ValueMutationOp mutationOp;
    size_t defaultIndex;
  };
  std::vector<Node*> children; // Reference-counting hard pointers are stored in the chain
  Node(VMModule& module, Kind kind, size_t line, size_t column, Node* chain)
    : HardReferenceCounted<IHardAcquireRelease>(),
      chain(chain),
      module(module),
      kind(kind),
      line(line),
      column(column) {
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

  class VMRunner;

  class VMCallStack : public HardReferenceCountedAllocator<IVMCallStack> {
    VMCallStack(const VMCallStack&) = delete;
    VMCallStack& operator=(const VMCallStack&) = delete;
  public:
    String resource;
    size_t line;
    size_t column;
    String function;
  public:
    explicit VMCallStack(IAllocator& allocator)
      : HardReferenceCountedAllocator<IVMCallStack>(allocator),
        line(0),
        column(0) {
    }
    virtual void print(Printer& printer) const override {
      if (!this->resource.empty() || (this->line > 0) || (this->column > 0)) {
        printer << this->resource;
        if ((this->line > 0) || (this->column > 0)) {
          printer << '(' << this->line;
          if (this->column > 0) {
            printer << ',' << this->column;
          }
          printer << ')';
        }
        printer << ": ";
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
      this->root = this->getAllocator().makeRaw<Node>(*this, Node::Kind::Root, 0u, 0u, nullptr);
      this->chain.set(this->root);
    }
    virtual HardPtr<IVMRunner> createRunner(IVMProgram& program) override {
      return HardPtr<VMRunner>(this->getAllocator().makeRaw<VMRunner>(this->vm, program, *this->root));
    }
    const String& getResource() const {
      return this->resource;
    }
    Node& getRoot() const {
      return *this->root;
    }
    Node& createNode(Node::Kind kind, size_t line, size_t column) {
      // Make sure we add the node to the internal linked list
      auto* node = this->getAllocator().makeRaw<Node>(*this, kind, line, column, this->chain.get());
      assert(node != nullptr);
      this->chain.set(node);
      return *node;
    }
  };

  // Only instantiated by composition within 'VMRunner' etc.
  struct VMSymbolTable {
  public:
    enum class Kind {
      Unknown,
      Unset,
      Builtin,
      Variable
    };
    enum class SetResult {
      Unknown,
      Builtin,
      Mismatch,
      Success
    };
    enum class RemoveResult {
      Unknown,
      Builtin,
      Success
    };
  private:
    struct Entry {
      Kind kind;
      Type type;
      HardValue value;
    };
    std::map<String, Entry> entries;
  public:
    void builtin(const String& name, const Type& type, const HardValue& value) {
      auto result = this->entries.emplace(name, Entry{ Kind::Builtin, type, value });
      assert(result.second);
      assert(result.first->second.kind != Kind::Unknown);
      (void)result;
    }
    Kind add(Kind kind, const String& name, const Type& type, const HardValue& value) {
      // Returns the old kind before this request
      assert(kind != Kind::Unknown);
      auto result = this->entries.emplace(name, Entry{ kind, type, value });
      if (result.second) {
        return Kind::Unknown;
      }
      assert(result.first->second.kind != Kind::Unknown);
      return result.first->second.kind;
    }
    SetResult set(IVMExecution& execution, const String& name, const HardValue& value) {
      // Returns the result (only updates with 'Success')
      auto result = this->entries.find(name);
      if (result == this->entries.end()) {
        return SetResult::Unknown;
      }
      auto kind = result->second.kind;
      switch (kind) {
      case Kind::Unknown:
        return SetResult::Unknown;
      case Kind::Builtin:
        return SetResult::Builtin;
      case Kind::Unset:
        if (execution.assignValue(result->second.value, result->second.type, value)) {
          result->second.kind = Kind::Variable;
          return SetResult::Success;
        }
        break;
      case Kind::Variable:
        if (execution.assignValue(result->second.value, result->second.type, value)) {
          return SetResult::Success;
        }
        break;
      }
      return SetResult::Mismatch;
    }
    RemoveResult remove(const String& name) {
      // Returns the result (only removes with 'Success')
      auto result = this->entries.find(name);
      if (result == this->entries.end()) {
        return RemoveResult::Unknown;
      }
      auto kind = result->second.kind;
      if (kind == Kind::Builtin) {
        return RemoveResult::Builtin;
      }
      this->entries.erase(result);
      return RemoveResult::Success;
    }
    Kind lookup(const String& name, HardValue& value) {
      // Returns the current kind and current value
      auto result = this->entries.find(name);
      if (result == this->entries.end()) {
        return Kind::Unknown;
      }
      value = result->second.value;
      return result->second.kind;
    }
    Type type(const String& name) {
      return this->entries[name].type;
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
    virtual HardValue raiseRuntimeError(const String& message) override;
    virtual bool assignValue(HardValue& dst, const Type& type, const HardValue& src) override {
      // Assign with int-to-float promotion
      return Operation::assign(this->vm.getAllocator(), dst, type, src, true);
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
  private:
    template<typename... ARGS>
    HardValue raise(ARGS&&... args) {
      // TODO: Non-string exception?
      auto message = StringBuilder::concat(this->vm.getAllocator(), std::forward<ARGS>(args)...);
      auto exception = this->createHardValueString(message);
      return this->raiseException(exception);
    }
    HardValue augment(const HardValue& value) {
      // TODO what if the user program throws a raw string within an expression?
      if (value->getFlags() == (ValueFlags::Throw | ValueFlags::String)) {
        HardValue inner;
        if (value->getInner(inner)) {
          String message;
          if (inner->getString(message)) {
            return this->raiseRuntimeError(message);
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
          return this->raise("TODO: Invalid value for '-' negation operator");
        }
        break;
      case ValueUnaryOp::BitwiseNot:
        switch (unaryValue.extractInt(arg)) {
        case Operation::UnaryValue::ExtractResult::Match:
          return this->createHardValueInt(~unaryValue.i);
        case Operation::UnaryValue::ExtractResult::Mismatch:
          return this->raise("TODO: Invalid value for '~' bitwise-not operator");
        }
        break;
      case ValueUnaryOp::LogicalNot:
        switch (unaryValue.extractBool(arg)) {
        case Operation::UnaryValue::ExtractResult::Match:
          return this->createHardValueBool(!unaryValue.b);
        case Operation::UnaryValue::ExtractResult::Mismatch:
          return this->raise("TODO: Invalid value for '!' logical-not operator");
        }
        break;
      }
      return this->raise("TODO: Unknown unary operator");
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
          return this->raise("TODO: Invalid left-hand value for '+' addition operator");
        case Operation::BinaryValues::PromotionResult::BadRight:
          return this->raise("TODO: Invalid right-hand value for '+' addition operator");
        }
        break;
      case ValueBinaryOp::Subtract:
        switch (binaryValues.promote(lhs, rhs)) {
        case Operation::BinaryValues::PromotionResult::Ints:
          return this->createHardValueInt(binaryValues.i[0] - binaryValues.i[1]);
        case Operation::BinaryValues::PromotionResult::Floats:
          return this->createHardValueFloat(binaryValues.f[0] - binaryValues.f[1]);
        case Operation::BinaryValues::PromotionResult::BadLeft:
          return this->raise("TODO: Invalid left-hand value for '-' subtraction operator");
        case Operation::BinaryValues::PromotionResult::BadRight:
          return this->raise("TODO: Invalid right-hand value for '-' subtraction operator");
        }
        break;
      case ValueBinaryOp::Multiply:
        switch (binaryValues.promote(lhs, rhs)) {
        case Operation::BinaryValues::PromotionResult::Ints:
          return this->createHardValueInt(binaryValues.i[0] * binaryValues.i[1]);
        case Operation::BinaryValues::PromotionResult::Floats:
          return this->createHardValueFloat(binaryValues.f[0] * binaryValues.f[1]);
        case Operation::BinaryValues::PromotionResult::BadLeft:
          return this->raise("TODO: Invalid left-hand value for '*' multiplication operator");
        case Operation::BinaryValues::PromotionResult::BadRight:
          return this->raise("TODO: Invalid right-hand value for '*' multiplication operator");
        }
        break;
      case ValueBinaryOp::Divide:
        switch (binaryValues.promote(lhs, rhs)) {
        case Operation::BinaryValues::PromotionResult::Ints:
          if (binaryValues.i[1] == 0) {
            return this->raise("TODO: Integer division by zero in '/' division operator");
          }
          return this->createHardValueInt(binaryValues.i[0] / binaryValues.i[1]);
        case Operation::BinaryValues::PromotionResult::Floats:
          return this->createHardValueFloat(binaryValues.f[0] / binaryValues.f[1]);
        case Operation::BinaryValues::PromotionResult::BadLeft:
          return this->raise("TODO: Invalid left-hand value for '/' division operator");
        case Operation::BinaryValues::PromotionResult::BadRight:
          return this->raise("TODO: Invalid right-hand value for '/' division operator");
        }
        break;
      case ValueBinaryOp::Remainder:
        switch (binaryValues.promote(lhs, rhs)) {
        case Operation::BinaryValues::PromotionResult::Ints:
          if (binaryValues.i[1] == 0) {
            return this->raise("TODO: Integer division by zero in '%' remainder operator");
          }
          return this->createHardValueInt(binaryValues.i[0] % binaryValues.i[1]);
        case Operation::BinaryValues::PromotionResult::Floats:
          return this->createHardValueFloat(std::fmod(binaryValues.f[0], binaryValues.f[1]));
        case Operation::BinaryValues::PromotionResult::BadLeft:
          return this->raise("TODO: Invalid left-hand value for '%' remainder operator");
        case Operation::BinaryValues::PromotionResult::BadRight:
          return this->raise("TODO: Invalid right-hand value for '%' remainder operator");
        }
        break;
      case ValueBinaryOp::LessThan:
        switch (binaryValues.promote(lhs, rhs)) {
        case Operation::BinaryValues::PromotionResult::Ints:
          return this->createHardValueBool(binaryValues.compareInts(Arithmetic::Compare::LessThan));
        case Operation::BinaryValues::PromotionResult::Floats:
          return this->createHardValueBool(binaryValues.compareFloats(Arithmetic::Compare::LessThan, false));
        case Operation::BinaryValues::PromotionResult::BadLeft:
          return this->raise("TODO: Invalid left-hand value for '<' comparison operator");
        case Operation::BinaryValues::PromotionResult::BadRight:
          return this->raise("TODO: Invalid right-hand value for '<' comparison operator");
        }
        break;
      case ValueBinaryOp::LessThanOrEqual:
        switch (binaryValues.promote(lhs, rhs)) {
        case Operation::BinaryValues::PromotionResult::Ints:
          return this->createHardValueBool(binaryValues.compareInts(Arithmetic::Compare::LessThanOrEqual));
        case Operation::BinaryValues::PromotionResult::Floats:
          return this->createHardValueBool(binaryValues.compareFloats(Arithmetic::Compare::LessThanOrEqual, false));
        case Operation::BinaryValues::PromotionResult::BadLeft:
          return this->raise("TODO: Invalid left-hand value for '<=' comparison operator");
        case Operation::BinaryValues::PromotionResult::BadRight:
          return this->raise("TODO: Invalid right-hand value for '<=' comparison operator");
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
          return this->raise("TODO: Invalid left-hand value for '>=' comparison operator");
        case Operation::BinaryValues::PromotionResult::BadRight:
          return this->raise("TODO: Invalid right-hand value for '>=' comparison operator");
        }
        break;
      case ValueBinaryOp::GreaterThan:
        switch (binaryValues.promote(lhs, rhs)) {
        case Operation::BinaryValues::PromotionResult::Ints:
          return this->createHardValueBool(binaryValues.compareInts(Arithmetic::Compare::GreaterThan));
        case Operation::BinaryValues::PromotionResult::Floats:
          return this->createHardValueBool(binaryValues.compareFloats(Arithmetic::Compare::GreaterThan, false));
        case Operation::BinaryValues::PromotionResult::BadLeft:
          return this->raise("TODO: Invalid left-hand value for '>' comparison operator");
        case Operation::BinaryValues::PromotionResult::BadRight:
          return this->raise("TODO: Invalid right-hand value for '>' comparison operator");
        }
        break;
      case ValueBinaryOp::BitwiseAnd:
        switch (binaryValues.extractBitwise(lhs, rhs)) {
        case Operation::BinaryValues::BitwiseResult::Bools:
          return this->createHardValueBool(bool(binaryValues.b[0] & binaryValues.b[1]));
        case Operation::BinaryValues::BitwiseResult::Ints:
          return this->createHardValueInt(binaryValues.i[0] & binaryValues.i[1]);
        case Operation::BinaryValues::BitwiseResult::BadLeft:
          return this->raise("TODO: Invalid left-hand value for '&' bitwise-and operator");
        case Operation::BinaryValues::BitwiseResult::BadRight:
          return this->raise("TODO: Invalid right-hand value for '&' bitwise-and operator");
        case Operation::BinaryValues::BitwiseResult::Mismatch:
          return this->raise("TODO: Type mismatch in values for '&' bitwise-and operator");
        }
        break;
      case ValueBinaryOp::BitwiseOr:
        switch (binaryValues.extractBitwise(lhs, rhs)) {
        case Operation::BinaryValues::BitwiseResult::Bools:
          return this->createHardValueBool(bool(binaryValues.b[0] | binaryValues.b[1]));
        case Operation::BinaryValues::BitwiseResult::Ints:
          return this->createHardValueInt(binaryValues.i[0] | binaryValues.i[1]);
        case Operation::BinaryValues::BitwiseResult::BadLeft:
          return this->raise("TODO: Invalid left-hand value for '|' bitwise-or operator");
        case Operation::BinaryValues::BitwiseResult::BadRight:
          return this->raise("TODO: Invalid right-hand value for '|' bitwise-or operator");
        case Operation::BinaryValues::BitwiseResult::Mismatch:
          return this->raise("TODO: Type mismatch in values for '|' bitwise-or operator");
        }
        break;
      case ValueBinaryOp::BitwiseXor:
        switch (binaryValues.extractBitwise(lhs, rhs)) {
        case Operation::BinaryValues::BitwiseResult::Bools:
          return this->createHardValueBool(bool(binaryValues.b[0] ^ binaryValues.b[1]));
        case Operation::BinaryValues::BitwiseResult::Ints:
          return this->createHardValueInt(binaryValues.i[0] ^ binaryValues.i[1]);
        case Operation::BinaryValues::BitwiseResult::BadLeft:
          return this->raise("TODO: Invalid left-hand value for '^' bitwise-xor operator");
        case Operation::BinaryValues::BitwiseResult::BadRight:
          return this->raise("TODO: Invalid right-hand value for '^' bitwise-xor operator");
        case Operation::BinaryValues::BitwiseResult::Mismatch:
          return this->raise("TODO: Type mismatch in values for '^' bitwise-xor operator");
        }
        break;
      case ValueBinaryOp::ShiftLeft:
        switch (binaryValues.extractInts(lhs, rhs)) {
        case Operation::BinaryValues::ExtractResult::Match:
          return this->createHardValueInt(binaryValues.shiftInts(Arithmetic::Shift::ShiftLeft));
        case Operation::BinaryValues::ExtractResult::BadLeft:
          return this->raise("TODO: Invalid left-hand value for '<<' left-shift operator");
        case Operation::BinaryValues::ExtractResult::BadRight:
          return this->raise("TODO: Invalid right-hand value for '<<' left-shift operator");
        }
        break;
      case ValueBinaryOp::ShiftRight:
        switch (binaryValues.extractInts(lhs, rhs)) {
        case Operation::BinaryValues::ExtractResult::Match:
          return this->createHardValueInt(binaryValues.shiftInts(Arithmetic::Shift::ShiftRight));
        case Operation::BinaryValues::ExtractResult::BadLeft:
          return this->raise("TODO: Invalid left-hand value for '>>' right-shift operator");
        case Operation::BinaryValues::ExtractResult::BadRight:
          return this->raise("TODO: Invalid right-hand value for '>>' right-shift operator");
        }
        break;
      case ValueBinaryOp::ShiftRightUnsigned:
        switch (binaryValues.extractInts(lhs, rhs)) {
        case Operation::BinaryValues::ExtractResult::Match:
          return this->createHardValueInt(binaryValues.shiftInts(Arithmetic::Shift::ShiftRightUnsigned));
        case Operation::BinaryValues::ExtractResult::BadLeft:
          return this->raise("TODO: Invalid left-hand value for '<<<' unsigned-shift operator");
        case Operation::BinaryValues::ExtractResult::BadRight:
          return this->raise("TODO: Invalid right-hand value for '<<<' unsigned-shift operator");
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
          return this->raise("TODO: Invalid left-hand value for '||' logical-or operator");
        case Operation::BinaryValues::ExtractResult::BadRight:
          return this->raise("TODO: Invalid right-hand value for '||' logical-or operator");
        }
        break;
      case ValueBinaryOp::IfTrue:
        // Too late to truly short-circuit here
        switch (binaryValues.extractBools(lhs, rhs)) {
        case Operation::BinaryValues::ExtractResult::Match:
          return this->createHardValueBool(binaryValues.b[0] && binaryValues.b[1]);
        case Operation::BinaryValues::ExtractResult::BadLeft:
          return this->raise("TODO: Invalid left-hand value for '&&' logical-and operator");
        case Operation::BinaryValues::ExtractResult::BadRight:
          return this->raise("TODO: Invalid right-hand value for '&&' logical-and operator");
        }
        break;
      }
      return this->raise("TODO: Unknown binary operator");
    }
    HardValue ternary(ValueTernaryOp op, const HardValue& lhs, const HardValue& mid, const HardValue& rhs) {
      Bool condition;
      switch (op) {
      case ValueTernaryOp::IfThenElse:
        if (lhs->getBool(condition)) {
          return condition ? mid : rhs;
        }
        return this->raise("TODO: Invalid condition value for '?:' ternary operator");
      }
      return this->raise("TODO: Unknown ternary operator");
    }
    HardValue precheck(ValueMutationOp op, HardValue& lhs, ValueFlags rhs) {
      // Handle short-circuits (returns 'Continue' if rhs should be evaluated)
      Bool bvalue;
      EGG_WARNING_SUPPRESS_SWITCH_BEGIN
        switch (op) {
        case ValueMutationOp::IfNull:
          // a ??= b
          // If lhs is null, we need to evaluation the rhs
          if (lhs->getFlags() == ValueFlags::Null) {
            // TODO: thread safety by setting lhs to some mutex
            return HardValue::Continue;
          }
          return lhs;
        case ValueMutationOp::IfFalse:
          // a ||= b
          // Iff lhs is false, we need to evaluation the rhs
          if ((lhs->getFlags() != ValueFlags::Bool) || lhs->getBool(bvalue)) {
            return this->raise("TODO: Invalid left-hand target for '||=' operator");
          }
          if (!Bits::hasAnySet(rhs, ValueFlags::Bool)) {
            return this->raise("TODO: Invalid right-hand type for '||=' operator");
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
          if ((lhs->getFlags() != ValueFlags::Bool) || lhs->getBool(bvalue)) {
            return this->raise("TODO: Invalid left-hand target for '&&=' operator");
          }
          if (!Bits::hasAnySet(rhs, ValueFlags::Bool)) {
            return this->raise("TODO: Invalid right-hand type for '&&=' operator");
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
      case ValueMutationOp::Assign:
        return lhs->mutate(Mutation::Assign, rhs.get());
      case ValueMutationOp::Decrement:
        assert(rhs->getFlags() == ValueFlags::Void);
        return lhs->mutate(Mutation::Decrement, rhs.get());
      case ValueMutationOp::Increment:
        assert(rhs->getFlags() == ValueFlags::Void);
        return lhs->mutate(Mutation::Increment, rhs.get());
      case ValueMutationOp::Add:
        return lhs->mutate(Mutation::Add, rhs.get());
      case ValueMutationOp::Subtract:
        return lhs->mutate(Mutation::Subtract, rhs.get());
      case ValueMutationOp::Multiply:
        return lhs->mutate(Mutation::Multiply, rhs.get());
      case ValueMutationOp::Divide:
        return lhs->mutate(Mutation::Divide, rhs.get());
      case ValueMutationOp::Remainder:
        return lhs->mutate(Mutation::Remainder, rhs.get());
      case ValueMutationOp::BitwiseAnd:
        return lhs->mutate(Mutation::BitwiseAnd, rhs.get());
      case ValueMutationOp::BitwiseOr:
        return lhs->mutate(Mutation::BitwiseOr, rhs.get());
      case ValueMutationOp::BitwiseXor:
        return lhs->mutate(Mutation::BitwiseXor, rhs.get());
      case ValueMutationOp::ShiftLeft:
        return lhs->mutate(Mutation::ShiftLeft, rhs.get());
      case ValueMutationOp::ShiftRight:
        return lhs->mutate(Mutation::ShiftRight, rhs.get());
      case ValueMutationOp::ShiftRightUnsigned:
        return lhs->mutate(Mutation::ShiftRightUnsigned, rhs.get());
      case ValueMutationOp::IfNull:
      case ValueMutationOp::IfFalse:
      case ValueMutationOp::IfTrue:
        // The condition was already tested in 'precheckMutationOp()'
        return rhs;
      case ValueMutationOp::Noop:
        assert(rhs->getFlags() == ValueFlags::Void);
        return lhs->mutate(Mutation::Noop, rhs.get());
      }
      return lhs;
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
      : VMUncollectable<IVMModuleBuilder>(vm),
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
    virtual Node& exprValueUnaryOp(ValueUnaryOp op, Node& arg, size_t line, size_t column) override {
      auto& node = this->module->createNode(Node::Kind::ExprUnaryOp, line, column);
      node.unaryOp = op;
      node.addChild(arg);
      return node;
    }
    virtual Node& exprValueBinaryOp(ValueBinaryOp op, Node& lhs, Node& rhs, size_t line, size_t column) override {
      auto& node = this->module->createNode(Node::Kind::ExprBinaryOp, line, column);
      node.binaryOp = op;
      node.addChild(lhs);
      node.addChild(rhs);
      return node;
    }
    virtual Node& exprValueTernaryOp(ValueTernaryOp op, Node& lhs, Node& mid, Node& rhs, size_t line, size_t column) override {
      auto& node = this->module->createNode(Node::Kind::ExprTernaryOp, line, column);
      node.ternaryOp = op;
      node.addChild(lhs);
      node.addChild(mid);
      node.addChild(rhs);
      return node;
    }
    virtual Node& exprVariable(const String& symbol, size_t line, size_t column) override {
      auto& node = this->module->createNode(Node::Kind::ExprVariable, line, column);
      node.literal = this->createHardValueString(symbol);
      return node;
    }
    virtual Node& exprStringCall(size_t line, size_t column) override {
      auto& node = this->module->createNode(Node::Kind::ExprStringCall, line, column);
      return node;
    }
    virtual Node& exprLiteral(const HardValue& literal, size_t line, size_t column) override {
      auto& node = this->module->createNode(Node::Kind::ExprLiteral, line, column);
      node.literal = literal;
      return node;
    }
    virtual Node& exprPropertyGet(Node& instance, Node& property, size_t line, size_t column) override {
      auto& node = this->module->createNode(Node::Kind::ExprPropertyGet, line, column);
      node.addChild(instance);
      node.addChild(property);
      return node;
    }
    virtual Node& exprFunctionCall(Node& function, size_t line, size_t column) override {
      if (function.kind == Node::Kind::ExprStringCall) {
        // Hoist the call
        return function;
      }
      auto& node = this->module->createNode(Node::Kind::ExprFunctionCall, line, column);
      node.addChild(function);
      return node;
    }
    virtual Node& typeLiteral(const Type& type, size_t line, size_t column) override {
      auto& node = this->module->createNode(Node::Kind::TypeLiteral, line, column);
      node.literal = this->createHardValueType(type);
      return node;
    }
    virtual Node& stmtBlock(size_t line, size_t column) override {
      auto& node = this->module->createNode(Node::Kind::StmtBlock, line, column);
      return node;
    }
    virtual Node& stmtIf(Node& condition, size_t line, size_t column) override {
      auto& node = this->module->createNode(Node::Kind::StmtIf, line, column);
      node.addChild(condition);
      return node;
    }
    virtual Node& stmtWhile(Node& condition, Node& block, size_t line, size_t column) override {
      auto& node = this->module->createNode(Node::Kind::StmtWhile, line, column);
      node.addChild(condition);
      node.addChild(block);
      return node;
    }
    virtual Node& stmtDo(Node& block, Node& condition, size_t line, size_t column) override {
      auto& node = this->module->createNode(Node::Kind::StmtDo, line, column);
      node.addChild(block);
      node.addChild(condition);
      return node;
    }
    virtual Node& stmtFor(Node& initial, Node& condition, Node& advance, Node& block, size_t line, size_t column) override {
      // Note the change in order to be execution-based (initial/condition/block/advance)
      auto& node = this->module->createNode(Node::Kind::StmtFor, line, column);
      node.addChild(initial);
      node.addChild(condition);
      node.addChild(block);
      node.addChild(advance);
      return node;
    }
    virtual Node& stmtSwitch(Node& expression, size_t defaultIndex, size_t line, size_t column) override {
      auto& node = this->module->createNode(Node::Kind::StmtSwitch, line, column);
      node.addChild(expression);
      node.defaultIndex = defaultIndex;
      return node;
    }
    virtual Node& stmtCase(Node& block, size_t line, size_t column) override {
      auto& node = this->module->createNode(Node::Kind::StmtCase, line, column);
      node.addChild(block);
      return node;
    }
    virtual Node& stmtBreak(size_t line, size_t column) override {
      auto& node = this->module->createNode(Node::Kind::StmtBreak, line, column);
      return node;
    }
    virtual Node& stmtContinue(size_t line, size_t column) override {
      auto& node = this->module->createNode(Node::Kind::StmtContinue, line, column);
      return node;
    }
    virtual Node& stmtVariableDeclare(const String& symbol, Node& type, size_t line, size_t column) override {
      auto& node = this->module->createNode(Node::Kind::StmtVariableDeclare, line, column);
      node.literal = this->createHardValueString(symbol);
      node.addChild(type);
      return node;
    }
    virtual Node& stmtVariableDefine(const String& symbol, Node& type, Node& value, size_t line, size_t column) override {
      auto& node = this->module->createNode(Node::Kind::StmtVariableDefine, line, column);
      node.literal = this->createHardValueString(symbol);
      node.addChild(type);
      node.addChild(value);
      return node;
    }
    virtual Node& stmtVariableSet(const String& symbol, Node& value, size_t line, size_t column) override {
      auto& node = this->module->createNode(Node::Kind::StmtVariableSet, line, column);
      node.literal = this->createHardValueString(symbol);
      node.addChild(value);
      return node;
    }
    virtual Node& stmtVariableMutate(const String& symbol, ValueMutationOp op, Node& value, size_t line, size_t column) override {
      auto& node = this->module->createNode(Node::Kind::StmtVariableMutate, line, column);
      node.literal = this->createHardValueString(symbol);
      node.mutationOp = op;
      node.addChild(value);
      return node;
    }
    virtual Node& stmtPropertySet(Node& instance, Node& property, Node& value, size_t line, size_t column) override {
      auto& node = this->module->createNode(Node::Kind::StmtPropertySet, line, column);
      node.addChild(instance);
      node.addChild(property);
      node.addChild(value);
      return node;
    }
    virtual Node& stmtFunctionCall(Node& function, size_t line, size_t column) override {
      auto& node = this->module->createNode(Node::Kind::StmtFunctionCall, line, column);
      node.addChild(function);
      return node;
    }
    virtual Node& stmtThrow(Node& exception, size_t line, size_t column) override {
      auto& node = this->module->createNode(Node::Kind::StmtThrow, line, column);
      node.addChild(exception);
      return node;
    }
    virtual Node& stmtTry(Node& block, size_t line, size_t column) override {
      auto& node = this->module->createNode(Node::Kind::StmtTry, line, column);
      node.addChild(block);
      return node;
    }
    virtual Node& stmtCatch(const String& symbol, Node& type, size_t line, size_t column) override {
      auto& node = this->module->createNode(Node::Kind::StmtCatch, line, column);
      node.literal = this->createHardValueString(symbol);
      node.addChild(type);
      return node;
    }
    virtual Node& stmtRethrow(size_t line, size_t column) override {
      auto& node = this->module->createNode(Node::Kind::StmtRethrow, line, column);
      return node;
    }
    virtual Type deduceType(Node& node) override {
      switch (node.kind) {
      case Node::Kind::ExprLiteral:
        return node.literal->getType();
      case Node::Kind::TypeLiteral:
        return node.literal->getType();
      case Node::Kind::ExprStringCall:
        return Type::String;
      case Node::Kind::Root:
      case Node::Kind::ExprUnaryOp:
      case Node::Kind::ExprBinaryOp:
      case Node::Kind::ExprTernaryOp:
      case Node::Kind::ExprVariable:
      case Node::Kind::ExprPropertyGet:
      case Node::Kind::ExprFunctionCall:
      case Node::Kind::TypeInfer:
      case Node::Kind::StmtBlock:
      case Node::Kind::StmtVariableDeclare:
      case Node::Kind::StmtVariableDefine:
      case Node::Kind::StmtVariableSet:
      case Node::Kind::StmtVariableMutate:
      case Node::Kind::StmtPropertySet:
      case Node::Kind::StmtFunctionCall:
      case Node::Kind::StmtIf:
      case Node::Kind::StmtWhile:
      case Node::Kind::StmtDo:
      case Node::Kind::StmtFor:
      case Node::Kind::StmtSwitch:
      case Node::Kind::StmtCase:
      case Node::Kind::StmtBreak:
      case Node::Kind::StmtContinue:
      case Node::Kind::StmtThrow:
      case Node::Kind::StmtTry:
      case Node::Kind::StmtCatch:
      case Node::Kind::StmtRethrow:
        break;
      }
      assert(false);
      return nullptr;
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
      : VMUncollectable<IVMProgramBuilder>(vm),
        program(vm.getAllocator().makeRaw<VMProgram>(vm)) {
      assert(this->program != nullptr);
    }
    virtual IVM& getVM() const override {
      return this->vm;
    }
    virtual HardPtr<IVMModuleBuilder> createModuleBuilder(const String& resource) override {
      assert(this->program != nullptr);
      return HardPtr<IVMModuleBuilder>(this->getAllocator().makeRaw<VMModuleBuilder>(this->vm, *this->program, resource));
    }
    virtual HardPtr<IVMProgram> build() override {
      HardPtr<VMProgram> built = this->program;
      if (built != nullptr) {
        this->program = nullptr;
      }
      return built;
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
      HardValue value;
    };
    HardPtr<IVMProgram> program;
    std::stack<NodeStack> stack;
    VMSymbolTable symtable;
    VMExecution execution;
  public:
    VMRunner(IVM& vm, IVMProgram& program, const IVMModule::Node& root)
      : VMCollectable<IVMRunner>(vm),
        program(&program),
        execution(vm) {
      this->execution.runner = this;
      this->push(root);
    }
    virtual void softVisit(ICollectable::IVisitor&) const override {
      // TODO any soft links?
    }
    virtual void print(Printer& printer) const override {
      printer << "[VMRunner]";
    }
    virtual void addBuiltin(const String& symbol, const HardValue& value) override {
      this->symtable.builtin(symbol, value->getType(), value);
    }
    virtual RunOutcome run(HardValue& retval, RunFlags flags) override {
      if (flags == RunFlags::Step) {
        if (this->stepNode(retval)) {
          return RunOutcome::Stepped;
        }
      } else  if (flags != RunFlags::None) {
        retval = this->createRuntimeError(this->createString("TODO: Run flags not yet supported in runner"));
        return RunOutcome::Failed;
      } else {
        while (this->stepNode(retval)) {
          // Step
        }
      }
      if (retval.hasAnyFlags(ValueFlags::Break | ValueFlags::Continue | ValueFlags::Throw)) {
        return RunOutcome::Failed;
      }
      return RunOutcome::Succeeded;
    }
    HardPtr<IVMCallStack> getCallStack() const {
      // TODO full stack chain
      HardPtr<VMCallStack> callstack{ this->vm.getAllocator().makeHard<VMCallStack>() };
      assert(!this->stack.empty());
      const auto* top = this->stack.top().node;
      assert(top != nullptr);
      callstack->resource = top->module.getResource();
      callstack->line = top->line;
      callstack->column = top->column;
      // TODO callstack->function
      return callstack;
    }
    template<typename... ARGS>
    HardValue createRuntimeError(ARGS&&... args) {
      auto& allocator = this->vm.getAllocator();
      auto message = StringBuilder::concat(allocator, std::forward<ARGS>(args)...);
      auto callstack = this->getCallStack();
      auto exception = ObjectFactory::createRuntimeError(this->vm, message, callstack);
      auto inner = ValueFactory::createHardObject(allocator, exception);
      return ValueFactory::createHardThrow(allocator, inner);
    }
  private:
    bool stepNode(HardValue& retval);
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
      auto error = this->createRuntimeError(std::forward<ARGS>(args)...);
      return this->pop(error);
    }
    bool variableScopeBegin(NodeStack& top, const Type& type) {
      assert(top.scope.empty());
      if (!top.node->literal->getString(top.scope)) {
        // No symbol declared
        return true;
      }
      assert(!top.scope.empty());
      HardValue poly{ *this->vm.createSoftValue() };
      switch (this->symtable.add(VMSymbolTable::Kind::Unset, top.scope, type, poly)) {
      case VMSymbolTable::Kind::Unknown:
        break;
      case VMSymbolTable::Kind::Unset:
      case VMSymbolTable::Kind::Variable:
        this->raise("Variable symbol already declared: '", top.scope, "'");
        return false;
      case VMSymbolTable::Kind::Builtin:
        this->raise("Variable symbol already declared as a builtin: '", top.scope, "'");
        return false;
      }
      return true;
    }
    void variableScopeEnd(NodeStack& top) {
      if (!top.scope.empty()) {
        (void)this->symtable.remove(top.scope);
        top.scope = {};
      }
    }
    bool symbolSet(const HardValue& symbol, const HardValue& value) {
      String name;
      if (!symbol->getString(name)) {
        this->raise("Invalid program node literal for variable symbol");
        return false;
      }
      if (value.hasFlowControl()) {
        this->pop(value);
        return false;
      }
      if (value->getFlags() == ValueFlags::Void) {
        this->raise("Cannot set variable '", name, "' to an uninitialized value");
        return false;
      }
      switch (this->symtable.set(this->execution, name, value)) {
      case VMSymbolTable::SetResult::Unknown:
        this->raise("Unknown variable symbol: '", name, "'");
        return false;
      case VMSymbolTable::SetResult::Builtin:
        this->raise("Cannot modify builtin symbol: '", name, "'");
        return false;
      case VMSymbolTable::SetResult::Mismatch:
        this->raise("Type mismatch setting variable '", name, "': expected '", this->symtable.type(name), "' but got '", value->getType(), "'");
        return false;
      case VMSymbolTable::SetResult::Success:
        break;
      }
      return true;
    }
    HardValue stringPropertyGet(const String& string, const HardValue& property) {
      String pname;
      if (!property->getString(pname)) {
        return this->createRuntimeError("Invalid right hand side for string '.' operator");
      }
      if (pname.equals("length")) {
        return this->createHardValueInt(Int(string.length()));
      }
      auto message = this->createString("Invalid right hand side for string '.' operator");
      return this->createRuntimeError("Unknown string property name: '", pname, "'");
    }
  };

  class VMDefault : public HardReferenceCountedAllocator<IVM> {
    VMDefault(const VMDefault&) = delete;
    VMDefault& operator=(const VMDefault&) = delete;
  protected:
    HardPtr<IBasket> basket;
    ILogger& logger;
    HardPtr<ITypeForge> forge;
  public:
    VMDefault(IAllocator& allocator, ILogger& logger)
      : HardReferenceCountedAllocator<IVM>(allocator),
        basket(BasketFactory::createBasket(allocator)),
        logger(logger) {
      this->forge = TypeForgeFactory::createTypeForge(allocator, *this->basket);
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
    virtual String createStringUTF8(const void* utf8, size_t bytes, size_t codepoints) override {
      return String::fromUTF8(this->allocator, utf8, bytes, codepoints);
    }
    virtual String createStringUTF32(const void* utf32, size_t codepoints) override {
      return String::fromUTF32(this->allocator, utf32, codepoints);
    }
    virtual HardPtr<IVMProgramBuilder> createProgramBuilder() override {
      return HardPtr<VMProgramBuilder>(this->getAllocator().makeRaw<VMProgramBuilder>(*this));
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
    virtual IValue* softCreateValue() override {
      // TODO: thread safety
      auto* created = SoftValue::createPoly(this->allocator);
      assert(created != nullptr);
      auto* taken = this->basket->take(*created);
      assert(taken == created);
      return static_cast<IValue*>(taken);
    }
    virtual IValue* softCreateAlias(const IValue& value) override {
      // TODO: thread safety
      auto* taken = this->basket->take(value);
      assert(taken == &value);
      return static_cast<IValue*>(taken);
    }
    virtual bool softSetValue(IValue*& target, const IValue& value) override {
      // TODO: thread safety
      // Note we do not currently update the target pointer but may do later for optimization reasons
      assert(target != nullptr);
      return target->set(value);
    }
  };
}

void egg::ovum::IVMModule::Node::printLocation(Printer& printer) const {
  printer << this->module.getResource();
  if ((this->line > 0) || (this->column > 0)) {
    printer << '(' << this->line;
    if (this->column > 0) {
      printer << ',' << this->column;
    }
    printer << ')';
  }
  printer << ": ";
}

void egg::ovum::IVMModule::Node::hardDestroy() const {
  this->module.getAllocator().destroy(this);
}

bool VMRunner::stepNode(HardValue& retval) {
  // Return true iff 'retval' has been set and the block has finished
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
        auto type = vtype->getType();
        assert(type != nullptr);
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
      auto type = vtype->getType();
      assert(type != nullptr);
      if (!this->variableScopeBegin(top, type)) {
        return true;
      }
      top.deque.clear();
      this->push(*top.node->children[top.index++]);
    } else {
      if (top.index == 2) {
        assert(top.deque.size() == 1);
        if (!this->symbolSet(top.node->literal, top.deque.front())) {
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
      if (!this->symbolSet(top.node->literal, top.deque.front())) {
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
        return this->raise("Invalid program node literal for variable symbol");
      }
      HardValue lhs;
      switch (this->symtable.lookup(symbol, lhs)) {
      case VMSymbolTable::Kind::Unknown:
        return this->raise("Unknown variable symbol: '", symbol, "'");
      case VMSymbolTable::Kind::Unset:
      case VMSymbolTable::Kind::Variable:
        break;
      case VMSymbolTable::Kind::Builtin:
        return this->raise("Cannot modify builtin symbol: '", symbol, "'");
      }
      if (top.index == 0) {
        assert(top.deque.empty());
        // TODO: Get correct rhs static type
        auto result = this->execution.precheckValueMutationOp(top.node->mutationOp, lhs, ValueFlags::AnyQ);
        if (!result.hasFlowControl()) {
          // Short-circuit (discard result)
          return this->pop(HardValue::Void);
        } else if (result->getFlags() == ValueFlags::Continue) {
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
      HardObject object;
      if (!top.deque.front()->getHardObject(object)) {
        return this->raise("Invalid left hand side for '.' operator");
      }
      auto& property = top.deque[1];
      auto& value = top.deque.back();
      return this->pop(object->vmPropertySet(this->execution, property, value));
    }
    break;
  case IVMModule::Node::Kind::StmtIf:
    assert((top.node->children.size() == 2) || (top.node->children.size() == 3));
    assert(top.index <= top.node->children.size());
    if (top.index == 0) {
      // Evaluate the condition
      if (!this->variableScopeBegin(top, nullptr)) {
        // TODO: if (var v = a) {}
        return true;
      }
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
          return this->raise("Statement 'if' condition expected to be a value of type 'bool'");
        }
        top.deque.clear();
        if (condition) {
          // Perform the compulsory "when true" block
          this->push(*top.node->children[top.index++]);
        } else if (top.node->children.size() > 2) {
          // Perform the optional "when false" block
          this->variableScopeEnd(top);
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
      // Scope any declared variable
      if (!this->variableScopeBegin(top, nullptr)) {
        // TODO: while (var v = a) {}
        return true;
      }
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
          return this->raise("Statement 'while' condition expected to be a value of type 'bool'");
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
          return this->raise("Statement 'do' condition expected to be a value of type 'bool'");
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
  case IVMModule::Node::Kind::StmtFor:
    assert(top.node->children.size() == 4);
    assert(top.index <= 4);
    if (top.index == 0) {
      // Scope any declared variable
      if (!this->variableScopeBegin(top, nullptr)) {
        // TODO: for (var v = a; ...) {}
        return true;
      }
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
          return this->raise("Statement 'for' condition expected to be a value of type 'bool'");
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
        // Scope any declared variable
        if (!this->variableScopeBegin(top, nullptr)) {
          // TODO: switch (var v = a) {}
          return true;
        }
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
      switch (latest->getFlags()) {
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
      switch (latest->getFlags()) {
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
      switch (latest->getFlags()) {
      case ValueFlags::Void:
        this->log(ILogger::Source::Runtime, ILogger::Severity::Warning, this->createString("Fell off the end of the case/default clause"));
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
        this->log(ILogger::Source::Runtime, ILogger::Severity::Warning, this->createString("Discarded value in switch case/default clause"));
        return this->pop(HardValue::Void);
      }
      EGG_WARNING_SUPPRESS_SWITCH_END
    }
    break;
  case IVMModule::Node::Kind::StmtBreak:
    assert(top.node->literal->getVoid());
    assert(top.node->children.size() == 0);
    return this->pop(HardValue::Break);
  case IVMModule::Node::Kind::StmtContinue:
    assert(top.node->literal->getVoid());
    assert(top.node->children.size() == 0);
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
        if (exception->getFlags() == ValueFlags::Throw) {
          // Erroneous rethrow within try block
          return this->raise("Unexpected exception rethrow within 'try' block");
        }
        top.value = exception;
      } else {
        // Just executed a catch/finally block
        assert(top.deque.size() == 2);
        auto* previous = top.node->children[top.index - 1];
        auto& latest = top.deque.back();
        if (latest->getFlags() == ValueFlags::Throw) {
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
          assert(latest->getFlags() == ValueFlags::Void);
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
        auto& vtype = top.deque.front();
        if (vtype.hasFlowControl()) {
          return this->pop(vtype);
        }
        auto type = vtype->getType();
        assert(type != nullptr);
        if (!this->variableScopeBegin(top, type)) {
          return true;
        }
        HardValue inner;
        if (!top.value->getInner(inner)) {
          return this->raise("Invalid 'catch' clause expression");
        }
        if (!this->symbolSet(top.node->literal, inner)) {
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
    assert(top.node->children.size() == 0);
    return this->pop(HardValue::Rethrow);
  case IVMModule::Node::Kind::StmtFunctionCall:
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
      if (!top.deque.front()->getHardObject(function)) {
        return this->raise("Invalid initial program node value for function call");
      }
      top.deque.pop_front();
      CallArguments arguments;
      for (auto& argument : top.deque) {
        // TODO support named arguments
        arguments.addUnnamed(argument);
      }
      return this->pop(function->vmCall(this->execution, arguments));
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
  case IVMModule::Node::Kind::ExprVariable:
    assert(top.node->children.empty());
    assert(top.index == 0);
    assert(top.deque.empty());
    {
      String symbol;
      if (!top.node->literal->getString(symbol)) {
        return this->raise("Invalid program node literal for variable symbol");
      }
      HardValue value;
      switch (this->symtable.lookup(symbol, value)) {
      case VMSymbolTable::Kind::Unknown:
        return this->raise("Unknown variable symbol: '", symbol, "'");
      case VMSymbolTable::Kind::Unset:
        return this->raise("Variable uninitialized: '", symbol, "'");
      case VMSymbolTable::Kind::Builtin:
      case VMSymbolTable::Kind::Variable:
        break;
      }
      return this->pop(value);
    }
    break;
  case IVMModule::Node::Kind::ExprLiteral:
    assert(top.node->children.empty());
    assert(top.index == 0);
    assert(top.deque.empty());
    return this->pop(top.node->literal);
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
      return this->raise("Invalid left hand side for '.' operator");
    }
    break;
  case IVMModule::Node::Kind::ExprStringCall:
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
      // Perform the concatenation
      StringBuilder sb;
      for (auto& argument : top.deque) {
        sb.add(argument);
      }
      auto result = sb.build(this->getAllocator());
      return this->pop(this->createHardValueString(result));
    }
    break;
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
  default:
    return this->raise("Invalid program node kind in program runner");
  }
  return true;
}

bool VMRunner::stepBlock(HardValue& retval, size_t first) {
  // Return true iff 'retval' has been set and the block has finished
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
    if (result->getFlags() != ValueFlags::Void) {
      this->log(ILogger::Source::Runtime, ILogger::Severity::Warning, this->createString("Discarded value in statement"));
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

HardPtr<IVMRunner> VMProgram::createRunner() {
  if (this->modules.empty()) {
    return nullptr;
  }
  return this->modules.front()->createRunner(*this);
}

HardValue VMExecution::raiseRuntimeError(const String& message) {
  assert(this->runner != nullptr);
  return this->runner->createRuntimeError(message);
}

egg::ovum::HardPtr<IVM> egg::ovum::VMFactory::createDefault(IAllocator& allocator, ILogger& logger) {
  return allocator.makeHard<VMDefault>(logger);
}
