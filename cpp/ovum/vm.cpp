#include "ovum/ovum.h"
#include "ovum/operation.h"

#include <stack>

namespace {
  using namespace egg::ovum;

  class VMProgramRunner;

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

  class VMProgram : public VMCollectable<IVMProgram> {
    VMProgram(const VMProgram&) = delete;
    VMProgram& operator=(const VMProgram&) = delete;
  private:
    HardPtr<Node> root;
  public:
    VMProgram(IVM& vm, Node& root)
      : VMCollectable(vm), root(&root) {
    }
    virtual void softVisit(ICollectable::IVisitor&) const override {
      // TODO any soft links?
    }
    virtual void print(Printer& printer) const override {
      printer << "[VMProgram]";
    }
    virtual HardPtr<IVMProgramRunner> createRunner() override {
      return VMProgram::makeHardVM<VMProgramRunner>(this->vm, *this);
    }
    const Node& getRunnableRoot() const {
      return *this->root;
    }
    template<typename TYPE, typename... ARGS>
    static HardPtr<TYPE> makeHardVM(IVM& vm, ARGS&&... args) {
      // Use perfect forwarding
      return HardPtr<TYPE>(vm.getAllocator().makeRaw<TYPE>(vm, std::forward<ARGS>(args)...));
    }
  };

  // Only instantiated by composition within 'VMProgramRunner' etc.
  struct VMSymbolTable {
  public:
    enum class Kind {
      Unknown,
      Unset,
      Builtin,
      Variable
    };
  private:
    struct Entry {
      Kind kind;
      HardValue value;
    };
    std::map<String, Entry> entries;
  public:
    void builtin(const String& name, const HardValue& value) {
      auto result = this->entries.emplace(name, Entry(Kind::Builtin, value));
      assert(result.second);
      assert(result.first->second.kind != Kind::Unknown);
      (void)result;
    }
    Kind add(Kind kind, const String& name, const HardValue& value) {
      // Returns the old kind before this request
      assert(kind != Kind::Unknown);
      auto result = this->entries.emplace(name, Entry(kind, value));
      if (result.second) {
        return Kind::Unknown;
      }
      assert(result.first->second.kind != Kind::Unknown);
      return result.first->second.kind;
    }
    Kind set(const String& name, const HardValue& value) {
      // Returns the new kind but only updates if a variable (or unset)
      auto result = this->entries.find(name);
      if (result == this->entries.end()) {
        return Kind::Unknown;
      }
      auto kind = result->second.kind;
      switch (kind) {
      case Kind::Unknown:
      case Kind::Builtin:
        break;
      case Kind::Unset:
        if (result->second.value->set(value.get())) {
          result->second.kind = Kind::Variable;
          return Kind::Variable;
        }
        break;
      case Kind::Variable:
        if (!result->second.value->set(value.get())) {
          return Kind::Unset;
        }
        break;
      }
      return kind;
    }
    Kind remove(const String& name) {
      // Returns the old kind but only removes if variable or unset
      auto result = this->entries.find(name);
      if (result == this->entries.end()) {
        return Kind::Unknown;
      }
      auto kind = result->second.kind;
      if (kind != Kind::Builtin) {
        this->entries.erase(result);
      }
      return kind;
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
  };

  // Only instantiated by composition within 'VMProgramRunner' etc.
  class VMExecution : public VMCommon<IVMExecution> {
    VMExecution(const VMExecution&) = delete;
    VMExecution& operator=(const VMExecution&) = delete;
  public:
    explicit VMExecution(IVM& vm)
      : VMCommon<IVMExecution>(vm) {
    }
    virtual HardValue raiseException(const String& message) override {
      // TODO augment with runtime metadata
      auto& allocator = this->vm.getAllocator();
      auto inner = ValueFactory::createString(allocator, message);
      return ValueFactory::createHardFlowControl(allocator, ValueFlags::Throw, inner);
    }
    virtual HardValue evaluateUnaryOp(UnaryOp op, const HardValue& arg) override {
      Operation::UnaryValue unaryValue;
      switch (op) {
      case UnaryOp::Negate:
        switch (unaryValue.extractArithmetic(arg)) {
        case Operation::UnaryValue::ArithmeticResult::Int:
          return this->createHardValueInt(-unaryValue.i);
        case Operation::UnaryValue::ArithmeticResult::Float:
          return this->createHardValueFloat(-unaryValue.f);
        case Operation::UnaryValue::ArithmeticResult::Mismatch:
          return this->raise("TODO: Invalid value for '-' negation operator");
        }
        break;
      case UnaryOp::BitwiseNot:
        switch (unaryValue.extractInt(arg)) {
        case Operation::UnaryValue::ExtractResult::Match:
          return this->createHardValueInt(~unaryValue.i);
        case Operation::UnaryValue::ExtractResult::Mismatch:
          return this->raise("TODO: Invalid value for '~' bitwise-not operator");
        }
        break;
      case UnaryOp::LogicalNot:
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
    virtual HardValue evaluateBinaryOp(BinaryOp op, const HardValue& lhs, const HardValue& rhs) override {
      Operation::BinaryValues binaryValues;
      switch (op) {
      case BinaryOp::Add:
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
      case BinaryOp::Subtract:
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
      case BinaryOp::Multiply:
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
      case BinaryOp::Divide:
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
      case BinaryOp::Remainder:
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
      case BinaryOp::LessThan:
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
      case BinaryOp::LessThanOrEqual:
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
      case BinaryOp::Equal:
        // Promote ints to floats but ignore IEEE NaN semantics
        return this->createHardValueBool(Operation::areEqual(lhs, rhs, true, false));
      case BinaryOp::NotEqual:
        // Promote ints to floats but ignore IEEE NaN semantics
        return this->createHardValueBool(!Operation::areEqual(lhs, rhs, true, false));
      case BinaryOp::GreaterThanOrEqual:
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
      case BinaryOp::GreaterThan:
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
      case BinaryOp::BitwiseAnd:
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
      case BinaryOp::BitwiseOr:
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
      case BinaryOp::BitwiseXor:
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
      case BinaryOp::ShiftLeft:
        switch (binaryValues.extractInts(lhs, rhs)) {
        case Operation::BinaryValues::ExtractResult::Match:
          return this->createHardValueInt(binaryValues.shiftInts(Arithmetic::Shift::ShiftLeft));
        case Operation::BinaryValues::ExtractResult::BadLeft:
          return this->raise("TODO: Invalid left-hand value for '<<' left-shift operator");
        case Operation::BinaryValues::ExtractResult::BadRight:
          return this->raise("TODO: Invalid right-hand value for '<<' left-shift operator");
        }
        break;
      case BinaryOp::ShiftRight:
        switch (binaryValues.extractInts(lhs, rhs)) {
        case Operation::BinaryValues::ExtractResult::Match:
          return this->createHardValueInt(binaryValues.shiftInts(Arithmetic::Shift::ShiftRight));
        case Operation::BinaryValues::ExtractResult::BadLeft:
          return this->raise("TODO: Invalid left-hand value for '>>' right-shift operator");
        case Operation::BinaryValues::ExtractResult::BadRight:
          return this->raise("TODO: Invalid right-hand value for '>>' right-shift operator");
        }
        break;
      case BinaryOp::ShiftRightUnsigned:
        switch (binaryValues.extractInts(lhs, rhs)) {
        case Operation::BinaryValues::ExtractResult::Match:
          return this->createHardValueInt(binaryValues.shiftInts(Arithmetic::Shift::ShiftRightUnsigned));
        case Operation::BinaryValues::ExtractResult::BadLeft:
          return this->raise("TODO: Invalid left-hand value for '<<<' unsigned-shift operator");
        case Operation::BinaryValues::ExtractResult::BadRight:
          return this->raise("TODO: Invalid right-hand value for '<<<' unsigned-shift operator");
        }
        break;
      case BinaryOp::IfNull:
        // Too late to truly short-circuit here
        return lhs->getNull() ? rhs : lhs;
      case BinaryOp::IfFalse:
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
      case BinaryOp::IfTrue:
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
    virtual HardValue precheckMutationOp(MutationOp op, HardValue& lhs, ValueFlags rhs) override {
      // Handle short-circuits (returns 'Continue' if rhs should be evaluated)
      Bool bvalue;
      EGG_WARNING_SUPPRESS_SWITCH_BEGIN
      switch (op) {
      case MutationOp::IfNull:
        // a ??= b
        // If lhs is null, we need to evaluation the rhs
        if (lhs->getFlags() == ValueFlags::Null) {
          // TODO: thread safety by setting lhs to some mutex
          return HardValue::Continue;
        }
        return lhs;
      case MutationOp::IfFalse:
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
      case MutationOp::IfTrue:
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
      case MutationOp::Noop:
        // Just return the lhs
        assert(rhs == ValueFlags::Void);
        return lhs;
      }
      EGG_WARNING_SUPPRESS_SWITCH_END
      return HardValue::Continue;
    }
    virtual HardValue evaluateMutationOp(MutationOp op, HardValue& lhs, const HardValue& rhs) override {
      // Return the value before the mutation
      HardValue before;
      switch (op) {
      case MutationOp::Assign:
        return lhs->mutate(Mutation::Assign, rhs.get());
      case MutationOp::Decrement:
        assert(rhs->getFlags() == ValueFlags::Void);
        return lhs->mutate(Mutation::Decrement, rhs.get());
      case MutationOp::Increment:
        assert(rhs->getFlags() == ValueFlags::Void);
        return lhs->mutate(Mutation::Increment, rhs.get());
      case MutationOp::Add:
        return lhs->mutate(Mutation::Add, rhs.get());
      case MutationOp::Subtract:
        return lhs->mutate(Mutation::Subtract, rhs.get());
      case MutationOp::Multiply:
        return lhs->mutate(Mutation::Multiply, rhs.get());
      case MutationOp::Divide:
        return lhs->mutate(Mutation::Divide, rhs.get());
      case MutationOp::Remainder:
        return lhs->mutate(Mutation::Remainder, rhs.get());
      case MutationOp::BitwiseAnd:
        return lhs->mutate(Mutation::BitwiseAnd, rhs.get());
      case MutationOp::BitwiseOr:
        return lhs->mutate(Mutation::BitwiseOr, rhs.get());
      case MutationOp::BitwiseXor:
        return lhs->mutate(Mutation::BitwiseXor, rhs.get());
      case MutationOp::ShiftLeft:
        return lhs->mutate(Mutation::ShiftLeft, rhs.get());
      case MutationOp::ShiftRight:
        return lhs->mutate(Mutation::ShiftRight, rhs.get());
      case MutationOp::ShiftRightUnsigned:
        return lhs->mutate(Mutation::ShiftRightUnsigned, rhs.get());
      case MutationOp::IfNull:
      case MutationOp::IfFalse:
      case MutationOp::IfTrue:
        // The condition was already tested in 'precheckMutationOp()'
        return rhs;
      case MutationOp::Noop:
        assert(rhs->getFlags() == ValueFlags::Void);
        return lhs->mutate(Mutation::Noop, rhs.get());
      }
      return lhs;
    }
  };
}

class egg::ovum::IVMProgram::Node : public HardReferenceCounted<IHardAcquireRelease> {
  Node(const Node&) = delete;
  Node& operator=(const Node&) = delete;
private:
  IVM& vm;
  HardPtr<Node> chain; // A linked list of all known nodes in this program
public:
  enum class Kind {
    Root,
    ExprUnaryOp,
    ExprBinaryOp,
    ExprVariable,
    ExprLiteral,
    ExprPropertyGet,
    ExprFunctionCall,
    StmtBlock,
    StmtVariableDeclare,
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
    StmtContinue
  };
  Kind kind;
  HardValue literal; // Only stores simple literals
  union {
    IVMExecution::UnaryOp unaryOp;
    IVMExecution::BinaryOp binaryOp;
    IVMExecution::MutationOp mutationOp;
    size_t defaultIndex;
  };
  std::vector<Node*> children; // Reference-counting hard pointers are stored in the chain
  Node(IVM& vm, Node* parent, Kind kind)
    : HardReferenceCounted<IHardAcquireRelease>(),
      vm(vm),
      chain(nullptr),
      kind(kind) {
    if (parent != nullptr) {
      this->chain = parent->chain;
      parent->chain.set(this);
    }
  }
  void addChild(Node& child) {
    this->children.push_back(&child);
  }
  void addChildren() {
    // Do nothing
  }
  template<typename... ARGS>
  void addChildren(Node& head, ARGS&&... tail) {
    this->addChild(head);
    this->addChildren(std::forward<ARGS>(tail)...);
  }
protected:
  virtual void hardDestroy() const override {
    this->vm.getAllocator().destroy(this);
  }
};

namespace {
  using namespace egg::ovum;

  class VMProgramBuilder : public VMCollectable<IVMProgramBuilder> {
    VMProgramBuilder(const VMProgramBuilder&) = delete;
    VMProgramBuilder& operator=(const VMProgramBuilder&) = delete;
  private:
    HardPtr<Node> root;
  public:
    explicit VMProgramBuilder(IVM& vm)
      : VMCollectable<IVMProgramBuilder>(vm) {
      this->root = VMProgram::makeHardVM<Node>(this->vm, nullptr, Node::Kind::Root);
    }
    virtual void softVisit(ICollectable::IVisitor&) const override {
      // TODO any soft links?
    }
    virtual void print(Printer& printer) const override {
      printer << "[VMProgramBuilder]";
    }
    virtual void addStatement(Node& statement) override {
      assert(this->root != nullptr);
      this->root->addChild(statement);
    }
    virtual HardPtr<IVMProgram> build() override {
      assert(this->root != nullptr);
      auto program = VMProgram::makeHardVM<VMProgram>(this->vm, *this->root);
      this->root = nullptr;
      return program;
    }
    virtual Node& exprUnaryOp(IVMExecution::UnaryOp op, Node& arg) override {
      auto& node = this->makeNode(Node::Kind::ExprUnaryOp);
      node.unaryOp = op;
      node.addChild(arg);
      return node;
    }
    virtual Node& exprBinaryOp(IVMExecution::BinaryOp op, Node& lhs, Node& rhs) override {
      auto& node = this->makeNode(Node::Kind::ExprBinaryOp);
      node.binaryOp = op;
      node.addChild(lhs);
      node.addChild(rhs);
      return node;
    }
    virtual Node& exprVariable(const String& name) override {
      auto& node = this->makeNode(Node::Kind::ExprVariable);
      node.literal = this->createHardValueString(name);
      return node;
    }
    virtual Node& exprLiteral(const HardValue& literal) override {
      auto& node = this->makeNode(Node::Kind::ExprLiteral);
      node.literal = literal;
      return node;
    }
    virtual Node& exprPropertyGet(Node& instance, Node& property) override {
      auto& node = this->makeNode(Node::Kind::ExprPropertyGet);
      node.addChild(instance);
      node.addChild(property);
      return node;
    }
    virtual Node& exprFunctionCall(Node& function) override {
      auto& node = this->makeNode(Node::Kind::ExprFunctionCall);
      node.addChild(function);
      return node;
    }
    virtual Node& stmtBlock() override {
      auto& node = this->makeNode(Node::Kind::StmtBlock);
      return node;
    }
    virtual Node& stmtIf(Node& condition) override {
      auto& node = this->makeNode(Node::Kind::StmtIf);
      node.addChild(condition);
      return node;
    }
    virtual Node& stmtWhile(Node& condition, Node& block) override {
      auto& node = this->makeNode(Node::Kind::StmtWhile);
      node.addChild(condition);
      node.addChild(block);
      return node;
    }
    virtual Node& stmtDo(Node& block, Node& condition) override {
      auto& node = this->makeNode(Node::Kind::StmtDo);
      node.addChild(block);
      node.addChild(condition);
      return node;
    }
    virtual Node& stmtFor(Node& initial, Node& condition, Node& advance, Node& block) override {
      // Note the change in order to be execution-based (initial/condition/block/advance)
      auto& node = this->makeNode(Node::Kind::StmtFor);
      node.addChild(initial);
      node.addChild(condition);
      node.addChild(block);
      node.addChild(advance);
      return node;
    }
    virtual Node& stmtSwitch(Node& expression, size_t defaultIndex) override {
      auto& node = this->makeNode(Node::Kind::StmtSwitch);
      node.addChild(expression);
      node.defaultIndex = defaultIndex;
      return node;
    }
    virtual Node& stmtCase(Node& block) override {
      auto& node = this->makeNode(Node::Kind::StmtCase);
      node.addChild(block);
      return node;
    }
    virtual Node& stmtBreak() override {
      auto& node = this->makeNode(Node::Kind::StmtBreak);
      return node;
    }
    virtual Node& stmtContinue() override {
      auto& node = this->makeNode(Node::Kind::StmtContinue);
      return node;
    }
    virtual Node& stmtVariableDeclare(const String& name) override {
      auto& node = this->makeNode(Node::Kind::StmtVariableDeclare);
      node.literal = this->createHardValueString(name);
      return node;
    }
    virtual Node& stmtVariableDefine(const String& name, Node& value) override {
      auto& node = this->makeNode(Node::Kind::StmtVariableDeclare);
      node.literal = this->createHardValueString(name);
      node.addChild(this->stmtVariableSet(name, value));
      return node;
    }
    virtual Node& stmtVariableSet(const String& name, Node& value) override {
      auto& node = this->makeNode(Node::Kind::StmtVariableSet);
      node.literal = this->createHardValueString(name);
      node.addChild(value);
      return node;
    }
    virtual Node& stmtVariableMutate(const String& name, IVMExecution::MutationOp op, Node& value) override {
      auto& node = this->makeNode(Node::Kind::StmtVariableMutate);
      node.literal = this->createHardValueString(name);
      node.mutationOp = op;
      node.addChild(value);
      return node;
    }
    virtual Node& stmtPropertySet(Node& instance, Node& property, Node& value) override {
      auto& node = this->makeNode(Node::Kind::StmtPropertySet);
      node.addChild(instance);
      node.addChild(property);
      node.addChild(value);
      return node;
    }
    virtual Node& stmtFunctionCall(Node& function) override {
      auto& node = this->makeNode(Node::Kind::StmtFunctionCall);
      node.addChild(function);
      return node;
    }
  private:
    virtual void appendChild(Node& parent, Node& child) override {
      parent.addChild(child);
    }
    Node& makeNode(Node::Kind kind) {
      assert(this->root != nullptr);
      auto node = VMProgram::makeHardVM<Node>(this->vm, this->root.get(), kind);
      return *node;
    }
  };

  class VMProgramRunner : public VMCollectable<IVMProgramRunner> {
    VMProgramRunner(const VMProgramRunner&) = delete;
    VMProgramRunner& operator=(const VMProgramRunner&) = delete;
  private:
    struct NodeStack {
      const IVMProgram::Node* node;
      String scope;
      size_t index;
      std::deque<HardValue> deque;
    };
    HardPtr<VMProgram> program;
    std::stack<NodeStack> stack;
    VMSymbolTable symtable;
    VMExecution execution;
  public:
    VMProgramRunner(IVM& vm, VMProgram& program)
      : VMCollectable<IVMProgramRunner>(vm),
        program(&program),
        execution(vm) {
      this->push(program.getRunnableRoot());
    }
    virtual void softVisit(ICollectable::IVisitor&) const override {
      // TODO any soft links?
    }
    virtual void print(Printer& printer) const override {
      printer << "[VMProgramRunner]";
    }
    virtual void addBuiltin(const String& name, const HardValue& value) override {
      this->symtable.builtin(name, value);
    }
    virtual RunOutcome run(HardValue& retval, RunFlags flags) override {
      if (flags == RunFlags::Step) {
        return this->stepNode(retval);
      }
      if (flags != RunFlags::None) {
        return this->createException(retval, "TODO: Run flags not yet supported in program runner");
      }
      for (;;) {
        auto outcome = this->stepNode(retval);
        if (outcome != RunOutcome::Stepped) {
          return outcome;
        }
      }
    }
  private:
    RunOutcome stepNode(HardValue& retval);
    RunOutcome stepBlock(HardValue& retval);
    NodeStack& push(const IVMProgram::Node& node, const String& scope = {}, size_t index = 0) {
      return this->stack.emplace(&node, scope, index);
    }
    RunOutcome bubble(const HardValue& value) {
      assert(!this->stack.empty());
      this->stack.pop();
      assert(!this->stack.empty());
      this->stack.top().deque.push_back(value);
      return RunOutcome::Stepped;
    }
    RunOutcome failed(HardValue& retval, const HardValue& value) {
      assert(value.hasFlowControl());
      retval = value;
      assert(!this->stack.empty());
      this->stack.pop();
      assert(!this->stack.empty());
      this->stack.top().deque.push_back(retval);
      const auto& symbol = this->stack.top().scope;
      if (!symbol.empty()) {
        (void)this->symtable.remove(symbol);
      }
      return RunOutcome::Failed;
    }
    template<typename... ARGS>
    HardValue createThrow(ARGS&&... args) {
      auto reason = ValueFactory::createString(this->getAllocator(), this->createStringConcat(std::forward<ARGS>(args)...));
      return ValueFactory::createHardFlowControl(this->getAllocator(), ValueFlags::Throw, reason);
    }
    template<typename... ARGS>
    RunOutcome createException(HardValue& retval, ARGS&&... args) {
      return this->failed(retval, this->createThrow(std::forward<ARGS>(args)...));
    }
    template<typename... ARGS>
    String createStringConcat(ARGS&&... args) {
      StringBuilder sb(&this->getAllocator());
      return sb.add(std::forward<ARGS>(args)...).build();
    }
    bool variableScopeBegin(HardValue& retval, NodeStack& top) {
      assert(top.scope.empty());
      if (!top.node->literal->getString(top.scope)) {
        // No symbol declared
        return true;
      }
      assert(!top.scope.empty());
      HardValue poly{ *this->vm.createSoftValue() };
      switch (this->symtable.add(VMSymbolTable::Kind::Unset, top.scope, poly)) {
      case VMSymbolTable::Kind::Unknown:
        break;
      case VMSymbolTable::Kind::Unset:
      case VMSymbolTable::Kind::Variable:
        retval = this->createThrow("Variable symbol already declared: '", top.scope, "'");
        return false;
      case VMSymbolTable::Kind::Builtin:
        retval = this->createThrow("Variable symbol already declared as a builtin: '", top.scope, "'");
        return false;
      }
      return true;
    }
    void variableScopeDrop(String& symbol) {
      if (!symbol.empty()) {
        (void)this->symtable.remove(symbol);
        symbol = {};
      }
    }
    bool variableScopeEnd(HardValue& retval, NodeStack& top) {
      if (!top.scope.empty()) {
        switch (this->symtable.remove(top.scope)) {
        case VMSymbolTable::Kind::Unknown:
          retval = this->createThrow("Unknown variable symbol: '", top.scope, "'"); // WIBBLE
          return false;
        case VMSymbolTable::Kind::Unset:
        case VMSymbolTable::Kind::Variable:
          break;
        case VMSymbolTable::Kind::Builtin:
          retval = this->createThrow(retval, "Cannot undeclare builtin symbol: '", top.scope, "'"); // WIBBLE
          return false;
        }
      }
      return true;
    }
  };

  class VMDefault : public HardReferenceCountedAllocator<IVM> {
    VMDefault(const VMDefault&) = delete;
    VMDefault& operator=(const VMDefault&) = delete;
  protected:
    HardPtr<IBasket> basket;
    ILogger& logger;
  public:
    VMDefault(IAllocator& allocator, ILogger& logger)
      : HardReferenceCountedAllocator<IVM>(allocator),
        basket(BasketFactory::createBasket(allocator)),
        logger(logger) {
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
    virtual String createStringUTF8(const void* utf8, size_t bytes, size_t codepoints) override {
      return String::fromUTF8(&this->allocator, utf8, bytes, codepoints);
    }
    virtual String createStringUTF32(const void* utf32, size_t codepoints) override {
      return String::fromUTF32(&this->allocator, utf32, codepoints);
    }
    virtual HardPtr<IVMProgramBuilder> createProgramBuilder() override {
      return HardPtr<IVMProgramBuilder>(this->allocator.makeRaw<VMProgramBuilder>(*this));
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

egg::ovum::IVMProgramRunner::RunOutcome VMProgramRunner::stepNode(HardValue& retval) {
  auto& top = this->stack.top();
  RunOutcome outcome;
  switch (top.node->kind) {
  case IVMProgram::Node::Kind::Root:
    assert(top.node->literal->getVoid());
    assert(top.index <= top.node->children.size());
    outcome = this->stepBlock(retval);
    if (outcome != RunOutcome::Stepped) {
      return outcome;
    }
    break;
  case IVMProgram::Node::Kind::StmtBlock:
    assert(top.node->literal->getVoid());
    assert(top.index <= top.node->children.size());
    outcome = this->stepBlock(retval);
    if (outcome != RunOutcome::Stepped) {
      return this->bubble(retval); // WIBBLE
    }
    break;
  case IVMProgram::Node::Kind::StmtVariableDeclare:
    assert(top.index <= top.node->children.size());
    if (top.index == 0) {
      if (!this->variableScopeBegin(retval, top)) {
        return RunOutcome::Failed;
      }
    }
    outcome = this->stepBlock(retval);
    if (outcome != RunOutcome::Stepped) {
      if (!this->variableScopeEnd(retval, top)) { // WIBBLE
        return RunOutcome::Failed;
      }
      return this->bubble(retval);
    }
    break;
  case IVMProgram::Node::Kind::StmtVariableSet:
    assert(top.node->children.size() == 1);
    assert(top.index <= top.node->children.size());
    if (top.index == 0) {
      // Evaluate the value
      this->push(*top.node->children[top.index++]);
    } else {
      assert(top.deque.size() == 1);
      String symbol;
      if (!top.node->literal->getString(symbol)) {
        return this->createException(retval, "Invalid program node literal for variable symbol");
      }
      // Check the value
      auto& value = top.deque.front();
      if (value.hasFlowControl()) {
        return this->failed(retval, value);
      }
      switch (this->symtable.set(symbol, value)) {
      case VMSymbolTable::Kind::Unknown:
        return this->createException(retval, "Unknown variable symbol: '", symbol, "'");
      case VMSymbolTable::Kind::Unset:
        return this->createException(retval, "Cannot set variable: '", symbol, "'");
      case VMSymbolTable::Kind::Builtin:
        return this->createException(retval, "Cannot modify builtin symbol: '", symbol, "'");
      case VMSymbolTable::Kind::Variable:
        break;
      }
      return this->bubble(HardValue::Void);
    }
    break;
  case IVMProgram::Node::Kind::StmtVariableMutate:
    assert(top.node->children.size() == 1);
    assert(top.index <= 1);
    {
      // TODO: thread safety
      String symbol;
      if (!top.node->literal->getString(symbol)) {
        return this->createException(retval, "Invalid program node literal for variable symbol");
      }
      HardValue lhs;
      switch (this->symtable.lookup(symbol, lhs)) {
      case VMSymbolTable::Kind::Unknown:
        return this->createException(retval, "Unknown variable symbol: '", symbol, "'");
      case VMSymbolTable::Kind::Unset:
      case VMSymbolTable::Kind::Variable:
        break;
      case VMSymbolTable::Kind::Builtin:
        return this->createException(retval, "Cannot modify builtin symbol: '", symbol, "'");
      }
      if (top.index == 0) {
        assert(top.deque.empty());
        // TODO: Get correct rhs static type
        auto result = this->execution.precheckMutationOp(top.node->mutationOp, lhs, ValueFlags::AnyQ);
        if (!result.hasFlowControl()) {
          // Short-circuit (discard result)
          return this->bubble(HardValue::Void);
        } else if (result->getFlags() == ValueFlags::Continue) {
          // Continue with evaluation of rhs
          this->push(*top.node->children[top.index++]);
        } else {
          return this->failed(retval, result);
        }
      } else {
        assert(top.deque.size() == 1);
        // Check the rhs
        auto& rhs = top.deque.front();
        if (rhs.hasFlowControl()) {
          return this->failed(retval, rhs);
        }
        auto before = this->execution.evaluateMutationOp(top.node->mutationOp, lhs, rhs);
        if (before.hasFlowControl()) {
          return this->failed(retval, before);
        }
        return this->bubble(HardValue::Void);
      }
    }
    break;
  case IVMProgram::Node::Kind::StmtPropertySet:
    assert(top.node->literal->getVoid());
    assert(top.node->children.size() == 3);
    assert(top.index <= 3);
    if (!top.deque.empty()) {
      // Check the last evaluation
      auto& latest = top.deque.back();
      if (latest.hasFlowControl()) {
        return this->failed(retval, latest);
      }
    }
    if (top.index < 3) {
      // Evaluate the expressions
      this->push(*top.node->children[top.index++]);
    } else {
      assert(top.deque.size() == 3);
      HardObject instance;
      if (!top.deque.front()->getHardObject(instance)) {
        return this->createException(retval, "Invalid left hand side for '.' operator");
      }
      auto& property = top.deque[1];
      auto& value = top.deque.back();
      return this->bubble(instance->vmPropertySet(this->execution, property, value));
    }
    break;
  case IVMProgram::Node::Kind::StmtIf:
    assert((top.node->children.size() == 2) || (top.node->children.size() == 3));
    assert(top.index <= top.node->children.size());
    if (top.index == 0) {
      // Evaluate the condition
      if (!this->variableScopeBegin(retval, top)) {
        // TODO: if (var v = a) {}
        return RunOutcome::Failed;
      }
      assert(top.deque.empty());
      this->push(*top.node->children[top.index++]);
    } else {
      assert(top.deque.size() == 1);
      auto& latest = top.deque.back();
      if (latest.hasFlowControl()) {
        return this->failed(retval, latest);
      }
      if (top.index == 1) {
        // Test the condition
        Bool condition;
        if (!latest->getBool(condition)) {
          return this->createException(retval, "Statement 'if' condition expected to be a value of type 'bool'");
        }
        top.deque.clear();
        if (condition) {
          // Perform the compulsory "when true" block
          this->push(*top.node->children[top.index++]);
        } else if (top.node->children.size() > 2) {
          // Perform the optional "when false" block
          this->variableScopeDrop(top.scope);
          top.index++;
          this->push(*top.node->children[top.index++]);
        } else {
          // Skip both blocks
          if (!this->variableScopeEnd(retval, top)) {
            return RunOutcome::Failed;
          }
          return this->bubble(HardValue::Void);
        }
      } else {
        // The controlled block has completed
        assert(latest->getVoid());
        retval = latest;
        if (!this->variableScopeEnd(retval, top)) {
          return RunOutcome::Failed;
        }
        return this->bubble(retval);
      }
    }
    break;
  case IVMProgram::Node::Kind::StmtWhile:
    assert(top.node->children.size() == 2);
    assert(top.index <= 2);
    if (top.index == 0) {
      // Scope any declared variable
      if (!this->variableScopeBegin(retval, top)) {
        // TODO: while (var v = a) {}
        return RunOutcome::Failed;
      }
      // Evaluate the condition
      assert(top.deque.empty());
      this->push(*top.node->children[top.index++]);
    } else {
      assert(top.deque.size() == 1);
      auto& latest = top.deque.back();
      if (latest.hasFlowControl()) {
        // TODO break and continue
        return this->failed(retval, latest);
      }
      if (top.index == 1) {
        // Test the condition
        Bool condition;
        if (!latest->getBool(condition)) {
          return this->createException(retval, "Statement 'while' condition expected to be a value of type 'bool'");
        }
        top.deque.clear();
        if (condition) {
          // Perform the controlled block
          this->push(*top.node->children[top.index++]);
        } else {
          // The condition failed
          if (!this->variableScopeEnd(retval, top)) {
            return RunOutcome::Failed;
          }
          return this->bubble(HardValue::Void);
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
  case IVMProgram::Node::Kind::StmtDo:
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
        return this->failed(retval, latest);
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
          return this->createException(retval, "Statement 'do' condition expected to be a value of type 'bool'");
        }
        top.deque.clear();
        if (condition) {
          // Perform the controlled block again
          this->push(*top.node->children[0]);
          top.index = 1;
        } else {
          // The condition failed
          return this->bubble(HardValue::Void);
        }
      }
    }
    break;
  case IVMProgram::Node::Kind::StmtFor:
    assert(top.node->children.size() == 4);
    assert(top.index <= 4);
    if (top.index == 0) {
      // Scope any declared variable
      if (!this->variableScopeBegin(retval, top)) {
        // TODO: for (var v = a; ...) {}
        return RunOutcome::Failed;
      }
      // Perform 'initial'
      assert(top.deque.empty());
      this->push(*top.node->children[top.index++]);
    } else {
      assert(top.deque.size() == 1);
      auto& latest = top.deque.back();
      if (latest.hasFlowControl()) {
        // TODO break and continue
        return this->failed(retval, latest);
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
          return this->createException(retval, "Statement 'for' condition expected to be a value of type 'bool'");
        }
        top.deque.clear();
        if (condition) {
          // Perform the controlled block
          this->push(*top.node->children[top.index++]);
        } else {
          // The condition failed
          if (!this->variableScopeEnd(retval, top)) {
            return RunOutcome::Failed;
          }
          return this->bubble(HardValue::Void);
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
  case IVMProgram::Node::Kind::StmtSwitch:
    assert(top.node->literal->getVoid());
    assert(top.node->children.size() > 1);
    if (top.index == 0) {
      if (top.deque.empty()) {
        // Scope any declared variable
        if (!this->variableScopeBegin(retval, top)) {
          // TODO: switch (var v = a) {}
          return RunOutcome::Failed;
        }
        // Evaluate the switch expression
        this->push(*top.node->children[0]);
        assert(top.index == 0);
      } else {
        // Test the switch expression evaluation
        assert(top.deque.size() == 1);
        auto& latest = top.deque.back();
        if (latest.hasFlowControl()) {
          return this->failed(retval, latest);
        }
        // Match the first case/default statement
        top.index = 1;
        auto& added = this->push(*top.node->children[1]);
        assert(added.node->kind == IVMProgram::Node::Kind::StmtCase);
        added.deque.push_back(latest);
      }
    } else if (top.deque.size() == 1) {
      // Just run an unconditional block
      auto& latest = top.deque.back();
      IVMProgram::Node* child;
      EGG_WARNING_SUPPRESS_SWITCH_BEGIN
      switch (latest->getFlags()) {
      case ValueFlags::Break:
        // Matched; break out of the switch statement
        if (!this->variableScopeEnd(retval, top)) {
          return RunOutcome::Failed;
        }
        return this->bubble(HardValue::Void);
      case ValueFlags::Continue:
        // Matched; continue to next block without re-testing the expression
        top.deque.clear();
        if (++top.index >= top.node->children.size()) {
          top.index = 1;
        }
        child = top.node->children[top.index];
        assert(child->kind == IVMProgram::Node::Kind::StmtCase);
        assert(!child->children.empty());
        this->push(*child->children.front());
        break;
      default:
        assert(latest.hasFlowControl());
        return this->failed(retval, latest);
      }
      EGG_WARNING_SUPPRESS_SWITCH_END
    } else {
      // Just matched a case/default clause
      auto& latest = top.deque.back();
      IVMProgram::Node* child;
      EGG_WARNING_SUPPRESS_SWITCH_BEGIN
      switch (latest->getFlags()) {
      case ValueFlags::Bool:
        // The switch expression does not match this case/default clause
        if (++top.index < top.node->children.size()) {
          // Match the next case/default statement
          top.deque.pop_back();
          assert(top.deque.size() == 1);
          auto& added = this->push(*top.node->children[top.index]);
          assert(added.node->kind == IVMProgram::Node::Kind::StmtCase);
          added.deque.push_back(top.deque.front());
        } else {
          // That was the last clause; we need to use the default clause, if any
          if (top.node->defaultIndex == 0) {
            // No default clause
            return this->bubble(HardValue::Void);
          } else {
            // Prepare to run the block associated with the default clause
            top.deque.clear();
            top.index = top.node->defaultIndex;
            child = top.node->children[top.index];
            assert(child->kind == IVMProgram::Node::Kind::StmtCase);
            assert(!child->children.empty());
            this->push(*child->children.front());
          }
        }
        break;
      case ValueFlags::Break:
        // Matched; break out of the switch statement
        if (!this->variableScopeEnd(retval, top)) {
          return RunOutcome::Failed;
        }
        return this->bubble(HardValue::Void);
      case ValueFlags::Continue:
        // Matched; continue to next block without re-testing the expression
        top.deque.clear();
        if (++top.index >= top.node->children.size()) {
          top.index = 1;
        }
        child = top.node->children[top.index];
        assert(child->kind == IVMProgram::Node::Kind::StmtCase);
        assert(!child->children.empty());
        this->push(*child->children.front());
        break;
      default:
        assert(latest.hasFlowControl());
        return this->failed(retval, latest);
      }
      EGG_WARNING_SUPPRESS_SWITCH_END
    }
    break;
  case IVMProgram::Node::Kind::StmtCase:
    assert(top.node->literal->getVoid());
    assert(!top.node->children.empty());
    if (top.index == 0) {
      assert(top.deque.size() == 1);
      if (top.node->children.size() == 1) {
        // This is a 'default' clause ('case' with no expressions)
        return this->bubble(HardValue::False);
      } else {
        // Evaluate the first expression
        this->push(*top.node->children[1]);
        top.index = 1;
      }
    } else if (top.index < top.node->children.size()) {
      // Test the evaluation
      assert(top.deque.size() == 2);
      auto& latest = top.deque.back();
      if (latest.hasFlowControl()) {
        return this->failed(retval, latest);
      }
      if (Operation::areEqual(top.deque.front(), latest, true, false)) { // TODO: promote?
        // Got a match, so execute the block
        top.deque.clear();
        this->push(*top.node->children[0]);
        top.index = top.node->children.size();
      } else {
        // Step to the next case expression
        if (++top.index < top.node->children.size()) {
          // Evaluate the next expression
          top.deque.pop_back();
          this->push(*top.node->children[top.index]);
        } else {
          // No expressions matched
          return this->bubble(HardValue::False);
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
        return this->bubble(HardValue::Continue);
      case ValueFlags::Break:
        // Explicit 'break'
        return this->bubble(HardValue::Break);
      case ValueFlags::Continue:
        // Explicit 'continue'
        return this->bubble(HardValue::Continue);
      default:
        // Probably an exception
        if (latest.hasFlowControl()) {
          return this->failed(retval, latest);
        }
        this->log(ILogger::Source::Runtime, ILogger::Severity::Warning, this->createString("Discarded value in switch case/default clause"));
        return this->bubble(HardValue::Void);
      }
      EGG_WARNING_SUPPRESS_SWITCH_END
    }
    break;
  case IVMProgram::Node::Kind::StmtBreak:
    assert(top.node->literal->getVoid());
    assert(top.node->children.size() == 0);
    return this->bubble(HardValue::Break);
  case IVMProgram::Node::Kind::StmtContinue:
    assert(top.node->literal->getVoid());
    assert(top.node->children.size() == 0);
    return this->bubble(HardValue::Continue);
  case IVMProgram::Node::Kind::StmtFunctionCall:
  case IVMProgram::Node::Kind::ExprFunctionCall:
    assert(top.node->literal->getVoid());
    assert(top.index <= top.node->children.size());
    if (!top.deque.empty()) {
      // Check the last evaluation
      auto& latest = top.deque.back();
      if (latest.hasFlowControl()) {
        return this->failed(retval, latest);
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
        return this->createException(retval, "Invalid initial program node value for function call");
      }
      top.deque.pop_front();
      CallArguments arguments;
      for (auto& argument : top.deque) {
        // TODO support named arguments
        arguments.addUnnamed(argument);
      }
      return this->bubble(function->vmCall(this->execution, arguments));
    }
    break;
  case IVMProgram::Node::Kind::ExprUnaryOp:
    assert(top.node->children.size() == 1);
    assert(top.index <= 1);
    if (top.index < 1) {
      // Evaluate the operand
      this->push(*top.node->children[top.index++]);
    } else {
      // Check the operand
      auto& latest = top.deque.back();
      if (latest.hasFlowControl()) {
        return this->failed(retval, latest);
      }
      assert(top.deque.size() == 1);
      auto result = this->execution.evaluateUnaryOp(top.node->unaryOp, top.deque.front());
      return this->bubble(result);
    }
    break;
  case IVMProgram::Node::Kind::ExprBinaryOp:
    assert(top.node->children.size() == 2);
    assert(top.index <= 2);
    if (top.index == 0) {
      // Evaluate the left-hand-side
      this->push(*top.node->children[top.index++]);
    } else {
      // Check the last evaluation
      auto& latest = top.deque.back();
      if (latest.hasFlowControl()) {
        return this->failed(retval, latest);
      }
      if (top.index == 1) {
        assert(top.deque.size() == 1);
        if (top.node->binaryOp == IVMExecution::BinaryOp::IfNull) {
          // Short-circuit '??'
          if (!latest->getNull()) {
            // lhs is not null; no need to evaluate rhs
            return this->bubble(HardValue(latest)); // make a shallow copy
          }
        } else if (top.node->binaryOp == IVMExecution::BinaryOp::IfFalse) {
          // Short-circuit '||'
          Bool lhs;
          if (latest->getBool(lhs) && lhs) {
            // lhs is true; no need to evaluate rhs
            return this->bubble(HardValue::True);
          }
        } else if (top.node->binaryOp == IVMExecution::BinaryOp::IfTrue) {
          // Short-circuit '&&'
          Bool lhs;
          if (latest->getBool(lhs) && !lhs) {
            // lhs is false; no need to evaluate rhs
            return this->bubble(HardValue::False);
          }
        }
        this->push(*top.node->children[top.index++]);
      } else {
        assert(top.deque.size() == 2);
        auto result = this->execution.evaluateBinaryOp(top.node->binaryOp, top.deque.front(), top.deque.back());
        return this->bubble(result);
      }
    }
    break;
  case IVMProgram::Node::Kind::ExprVariable:
    assert(top.node->children.empty());
    assert(top.index == 0);
    assert(top.deque.empty());
    {
      String symbol;
      if (!top.node->literal->getString(symbol)) {
        return this->createException(retval, "Invalid program node literal for variable symbol");
      }
      HardValue value;
      switch (this->symtable.lookup(symbol, value)) {
      case VMSymbolTable::Kind::Unknown:
        return this->createException(retval, "Unknown variable symbol: '", symbol, "'");
      case VMSymbolTable::Kind::Unset:
        return this->createException(retval, "Variable uninitialized: '", symbol, "'");
      case VMSymbolTable::Kind::Builtin:
      case VMSymbolTable::Kind::Variable:
        break;
      }
      return this->bubble(value);
    }
    break;
  case IVMProgram::Node::Kind::ExprLiteral:
    assert(top.node->children.empty());
    assert(top.index == 0);
    assert(top.deque.empty());
    return this->bubble(top.node->literal);
  case IVMProgram::Node::Kind::ExprPropertyGet:
    assert(top.node->literal->getVoid());
    assert(top.node->children.size() == 2);
    if (!top.deque.empty()) {
      // Check the last evaluation
      auto& latest = top.deque.back();
      if (latest.hasFlowControl()) {
        return this->failed(retval, latest);
      }
    }
    if (top.index < 2) {
      // Assemble the arguments
      this->push(*top.node->children[top.index++]);
    } else {
      // Perform the property fetch
      assert(top.deque.size() == 2);
      HardObject instance;
      if (!top.deque.front()->getHardObject(instance)) {
        return this->createException(retval, "Invalid left hand side for '.' operator");
      }
      auto& property = top.deque.back();
      return this->bubble(instance->vmPropertyGet(this->execution, property));
    }
    break;
  default:
    return this->createException(retval, "Invalid program node kind in program runner");
  }
  return RunOutcome::Stepped;
}

egg::ovum::IVMProgramRunner::RunOutcome VMProgramRunner::stepBlock(HardValue& retval) {
  auto& top = this->stack.top();
  assert(top.index <= top.node->children.size());
  if (top.index > 0) {
    // Check the result of the previous child statement
    assert(top.deque.size() == 1);
    auto& result = top.deque.back();
    if (result.hasFlowControl()) {
      // TODO: return/yield is actually success
      retval = result;
      return RunOutcome::Failed;
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
    return RunOutcome::Succeeded;
  }
  return RunOutcome::Stepped;
}

egg::ovum::HardPtr<IVM> egg::ovum::VMFactory::createDefault(IAllocator& allocator, ILogger& logger) {
  return allocator.makeHard<VMDefault>(logger);
}
