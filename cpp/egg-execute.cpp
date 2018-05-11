#include "yolk.h"
#include "lexers.h"
#include "egg-tokenizer.h"
#include "egg-syntax.h"
#include "egg-parser.h"
#include "egg-engine.h"
#include "egg-program.h"

namespace {
  class EggProgramExpression {
  private:
    egg::yolk::EggProgramContext* context;
    egg::lang::LocationRuntime before;
  public:
    EggProgramExpression(egg::yolk::EggProgramContext& context, const egg::yolk::IEggProgramNode& node)
      : context(&context) {
      // TODO use runtime location, not source location
      egg::lang::LocationRuntime after(node.location(), egg::lang::String::fromUTF8("TODO()"));
      this->before = this->context->swapLocation(after);
    }
    ~EggProgramExpression() {
      (void)this->context->swapLocation(this->before);
    }
  };

  class EggProgramParametersEmpty : public egg::lang::IParameters {
  public:
    virtual size_t getPositionalCount() const override {
      return 0;
    }
    virtual egg::lang::Value getPositional(size_t) const override {
      return egg::lang::Value::Void;
    }
    virtual size_t getNamedCount() const override {
      return 0;
    }
    virtual egg::lang::String getName(size_t) const override {
      return egg::lang::String::Empty;
    }
    virtual egg::lang::Value getNamed(const egg::lang::String&) const override {
      return egg::lang::Value::Void;
    }
  };
  const EggProgramParametersEmpty parametersEmpty;

  class EggProgramParameters : public egg::lang::IParameters {
  private:
    std::vector<egg::lang::Value> positional;
    std::map<egg::lang::String, egg::lang::Value> named;
  public:
    explicit EggProgramParameters(size_t count) {
      this->positional.reserve(count);
    }
    void addPositional(const egg::lang::Value& value) {
      this->positional.push_back(value);
    }
    void addNamed(const egg::lang::String& name, const egg::lang::Value& value) {
      this->named.emplace(name, value);
    }
    virtual size_t getPositionalCount() const override {
      return this->positional.size();
    }
    virtual egg::lang::Value getPositional(size_t index) const override {
      return this->positional.at(index);
    }
    virtual size_t getNamedCount() const override {
      return this->named.size();
    }
    virtual egg::lang::String getName(size_t index) const override {
      auto iter = this->named.begin();
      std::advance(iter, index);
      return iter->first;
    }
    virtual egg::lang::Value getNamed(const egg::lang::String& name) const override {
      return this->named.at(name);
    }
  };

  class EggProgramFunction : public egg::gc::HardReferenceCounted<egg::lang::IObject> {
    EGG_NO_COPY(EggProgramFunction);
  private:
    egg::yolk::EggProgramContext& program;
    egg::lang::ITypeRef type;
    std::shared_ptr<egg::yolk::IEggProgramNode> block;
  public:
    EggProgramFunction(egg::yolk::EggProgramContext& program, const egg::lang::IType& type, const std::shared_ptr<egg::yolk::IEggProgramNode>& block)
      : HardReferenceCounted(0), program(program), type(&type), block(block) {
      assert(block != nullptr);
    }
    virtual bool dispose() override {
      return false;
    }
    virtual egg::lang::Value toString() const override {
      return egg::lang::Value(egg::lang::String::concat("<", this->type->toString(), ">"));
    }
    virtual egg::lang::Value getRuntimeType() const override {
      return egg::lang::Value(*this->type);
    }
    virtual egg::lang::Value call(egg::lang::IExecution&, const egg::lang::IParameters& parameters) override {
      return this->program.executeFunctionCall(*this->type, parameters, *this->block);
    }
    virtual egg::lang::Value getProperty(egg::lang::IExecution& execution, const egg::lang::String& property) override {
      return execution.raiseFormat(this->type->toString(), " does not support properties such as '.", property, "'");
    }
    virtual egg::lang::Value setProperty(egg::lang::IExecution& execution, const egg::lang::String& property, const egg::lang::Value&) override {
      return execution.raiseFormat(this->type->toString(), " does not support properties such as '.", property, "'");
    }
    virtual egg::lang::Value getIndex(egg::lang::IExecution& execution, const egg::lang::Value&) override {
      return execution.raiseFormat(this->type->toString(), " does not support indexing with '[]'");
    }
    virtual egg::lang::Value setIndex(egg::lang::IExecution& execution, const egg::lang::Value&, const egg::lang::Value&) override {
      return execution.raiseFormat(this->type->toString(), " does not support indexing with '[]'");
    }
    virtual egg::lang::Value iterate(egg::lang::IExecution& execution) override {
      return execution.raiseFormat(this->type->toString(), " does not support iteration");
    }
  };
}

egg::lang::Value egg::yolk::EggProgramContext::executeStatements(const std::vector<std::shared_ptr<IEggProgramNode>>& statements) {
  // Execute all the statements one after another
  egg::lang::String name;
  auto type = egg::lang::Type::Void;
  for (auto& statement : statements) {
    if (statement->symbol(name, type)) {
      // We've checked for duplicate symbols already
      this->symtable->addSymbol(name, *type);
    }
    auto retval = statement->execute(*this);
    if (!retval.is(egg::lang::Discriminator::Void)) {
      return retval;
    }
  }
  return egg::lang::Value::Void;
}

egg::lang::Value egg::yolk::EggProgramContext::executeModule(const IEggProgramNode& self, const std::vector<std::shared_ptr<IEggProgramNode>>& statements) {
  this->statement(self);
  return this->executeStatements(statements);
}

egg::lang::Value egg::yolk::EggProgramContext::executeBlock(const IEggProgramNode& self, const std::vector<std::shared_ptr<IEggProgramNode>>& statements) {
  this->statement(self);
  EggProgramSymbolTable nested(this->symtable);
  EggProgramContext context(*this, nested);
  return context.executeStatements(statements);
}

egg::lang::Value egg::yolk::EggProgramContext::executeDeclare(const IEggProgramNode& self, const egg::lang::String& name, const egg::lang::IType&, const IEggProgramNode* rvalue) {
  // The type information has already been used in the symbol declaration phase
  this->statement(self);
  if (rvalue != nullptr) {
    // The declaration contains an initial value
    return this->set(name, rvalue->execute(*this));
  }
  return egg::lang::Value::Void;
}

egg::lang::Value egg::yolk::EggProgramContext::executeAssign(const IEggProgramNode& self, EggProgramAssign op, const IEggProgramNode& lvalue, const IEggProgramNode& rvalue) {
  this->statement(self);
  return this->assign(op, lvalue, rvalue);
}

egg::lang::Value egg::yolk::EggProgramContext::executeMutate(const IEggProgramNode& self, EggProgramMutate op, const IEggProgramNode& lvalue) {
  this->statement(self);
  return this->mutate(op, lvalue);
}

egg::lang::Value egg::yolk::EggProgramContext::executeBreak(const IEggProgramNode& self) {
  this->statement(self);
  return egg::lang::Value::Break;
}

egg::lang::Value egg::yolk::EggProgramContext::executeContinue(const IEggProgramNode& self) {
  this->statement(self);
  return egg::lang::Value::Continue;
}

egg::lang::Value egg::yolk::EggProgramContext::executeDo(const IEggProgramNode& self, const IEggProgramNode& cond, const IEggProgramNode& block) {
  this->statement(self);
  egg::lang::Value retval;
  do {
    retval = block.execute(*this);
    if (!retval.is(egg::lang::Discriminator::Void)) {
      if (retval.is(egg::lang::Discriminator::Break)) {
        // Just leave the loop
        return egg::lang::Value::Void;
      }
      if (!retval.is(egg::lang::Discriminator::Continue)) {
        // Probably an exception
        return retval;
      }
    }
    retval = this->condition(cond);
    if (!retval.is(egg::lang::Discriminator::Bool)) {
      // Condition evaluation failed
      return retval;
    }
  } while (retval.getBool());
  return egg::lang::Value::Void;
}

egg::lang::Value egg::yolk::EggProgramContext::executeIf(const IEggProgramNode& self, const IEggProgramNode& cond, const IEggProgramNode& trueBlock, const IEggProgramNode* falseBlock) {
  this->statement(self);
  egg::lang::Value retval = this->condition(cond);
  if (!retval.is(egg::lang::Discriminator::Bool)) {
    return retval;
  }
  if (retval.getBool()) {
    return trueBlock.execute(*this);
  }
  if (falseBlock != nullptr) {
    return falseBlock ->execute(*this);
  }
  return egg::lang::Value::Void;
}

egg::lang::Value egg::yolk::EggProgramContext::executeFor(const IEggProgramNode& self, const IEggProgramNode* pre, const IEggProgramNode* cond, const IEggProgramNode* post, const IEggProgramNode& block) {
  this->statement(self);
  egg::lang::Value retval;
  if (pre != nullptr) {
    retval = pre->execute(*this);
    if (!retval.is(egg::lang::Discriminator::Void)) {
      // Probably an exception in the pre-loop statement
      return retval;
    }
  }
  if (cond == nullptr) {
    // There's no explicit condition
    for (;;) {
      retval = block.execute(*this);
      if (!retval.is(egg::lang::Discriminator::Void)) {
        if (retval.is(egg::lang::Discriminator::Break)) {
          // Just leave the loop
          return egg::lang::Value::Void;
        }
        if (!retval.is(egg::lang::Discriminator::Continue)) {
          // Probably an exception in the condition expression
          return retval;
        }
      }
      if (post != nullptr) {
        retval = post->execute(*this);
        if (!retval.is(egg::lang::Discriminator::Void)) {
          // Probably an exception in the post-loop statement
          return retval;
        }
      }
    }
  }
  retval = this->condition(*cond);
  while (retval.is(egg::lang::Discriminator::Bool)) {
    if (!retval.getBool()) {
      // The condition was false
      return egg::lang::Value::Void;
    }
    retval = block.execute(*this);
    if (!retval.is(egg::lang::Discriminator::Void)) {
      if (retval.is(egg::lang::Discriminator::Break)) {
        // Just leave the loop
        return egg::lang::Value::Void;
      }
      if (!retval.is(egg::lang::Discriminator::Continue)) {
        // Probably an exception in the block
        return retval;
      }
    }
    if (post != nullptr) {
      retval = post->execute(*this);
      if (!retval.is(egg::lang::Discriminator::Void)) {
        // Probably an exception in the post-loop statement
        return retval;
      }
    }
    retval = this->condition(*cond);
  }
  return retval;
}

egg::lang::Value egg::yolk::EggProgramContext::executeForeach(const IEggProgramNode& self, const IEggProgramNode& lvalue, const IEggProgramNode& rvalue, const IEggProgramNode& block) {
  this->statement(self);
  auto dst = lvalue.assignee(*this);
  if (dst == nullptr) {
    return this->raiseFormat("Iteration target in 'for' statement is not valid");
  }
  auto src = rvalue.execute(*this);
  if (src.has(egg::lang::Discriminator::FlowControl)) {
    return src;
  }
  if (src.is(egg::lang::Discriminator::String)) {
    return this->executeForeachString(*dst, src.getString(), block);
  }
  if (src.is(egg::lang::Discriminator::Object)) {
    return this->executeForeachIterate(*dst, src.getObject(), block);
  }
  return this->raiseFormat("Cannot iterate '", src.getRuntimeType().toString(), "'");
}

egg::lang::Value egg::yolk::EggProgramContext::executeForeachString(IEggProgramAssignee& target, const egg::lang::String& source, const IEggProgramNode& block) {
  size_t index = 0;
  for (auto codepoint = source.codePointAt(0); codepoint >= 0; codepoint = source.codePointAt(++index)) {
    auto retval = target.set(egg::lang::Value{ egg::lang::String::fromCodePoint(char32_t(codepoint)) });
    if (retval.has(egg::lang::Discriminator::FlowControl)) {
      // The assignment failed
      return retval;
    }
    retval = block.execute(*this);
    if (!retval.is(egg::lang::Discriminator::Void)) {
      if (retval.is(egg::lang::Discriminator::Break)) {
        // Just leave the loop
        return egg::lang::Value::Void;
      }
      if (!retval.is(egg::lang::Discriminator::Continue)) {
        // Probably an exception in the block
        return retval;
      }
    }
  }
  if (index != source.length()) {
    return this->raiseFormat("Cannot iterate through a malformed string");
  }
  return egg::lang::Value::Void;
}

egg::lang::Value egg::yolk::EggProgramContext::executeForeachIterate(IEggProgramAssignee& target, egg::lang::IObject& source, const IEggProgramNode& block) {
  auto iterator = source.iterate(*this);
  if (iterator.has(egg::lang::Discriminator::FlowControl)) {
    // The iterator could not be created
    return iterator;
  }
  if (!iterator.is(egg::lang::Discriminator::Object)) {
    return this->unexpected("The 'for' statement expected an iterator", iterator); // TODO
  }
  for (;;) {
    auto next = iterator.getObject().call(*this, parametersEmpty);
    if (next.has(egg::lang::Discriminator::FlowControl)) {
      // An error occurred in the iterator
      return next;
    }
    if (next.is(egg::lang::Discriminator::Void)) {
      // The iterator concluded
      break;
    }
    auto retval = target.set(next);
    if (retval.has(egg::lang::Discriminator::FlowControl)) {
      // The assignment failed
      return retval;
    }
    retval = block.execute(*this);
    if (!retval.is(egg::lang::Discriminator::Void)) {
      if (retval.is(egg::lang::Discriminator::Break)) {
        // Just leave the loop
        break;
      }
      if (!retval.is(egg::lang::Discriminator::Continue)) {
        // Probably an exception in the block
        return retval;
      }
    }
  }
  return egg::lang::Value::Void;
}

egg::lang::Value egg::yolk::EggProgramContext::executeFunctionDefinition(const IEggProgramNode& self, const egg::lang::String& name, const egg::lang::IType& type, const std::shared_ptr<IEggProgramNode>& block) {
  // This defines a function, it doesn't call it
  this->statement(self);
  auto symbol = this->symtable->findSymbol(name);
  assert(symbol != nullptr);
  // We can store this directly in the symbol table without going through the type system
  // otherwise we get issues with function assignment
  assert(symbol->value.is(egg::lang::Discriminator::Void));
  symbol->value = egg::lang::Value(*new EggProgramFunction(*this, type, block));
  return egg::lang::Value::Void;
}

egg::lang::Value egg::yolk::EggProgramContext::executeFunctionCall(const egg::lang::IType& type, const egg::lang::IParameters& parameters, const IEggProgramNode& block) {
  // This actually calls a function
  EggProgramSymbolTable nested(this->symtable);
  egg::lang::IType::ExecuteParametersSetter setter = [&](const egg::lang::String& k, const egg::lang::IType& t, const egg::lang::Value& v) {
    auto retval = nested.addSymbol(k, t)->assign(*this, v);
    assert(!retval.has(egg::lang::Discriminator::FlowControl));
  };
  auto retval = type.executeParameters(*this, parameters, setter);
  if (!retval.has(egg::lang::Discriminator::FlowControl)) {
    EggProgramContext context(*this, nested);
    retval = block.execute(context);
    if (retval.stripFlowControl(egg::lang::Discriminator::Return)) {
      return retval;
    }
  }
  return retval;
}

egg::lang::Value egg::yolk::EggProgramContext::executeReturn(const IEggProgramNode& self, const IEggProgramNode* value) {
  this->statement(self);
  if (value == nullptr) {
    // This is a void return
    return egg::lang::Value::ReturnVoid;
  }
  auto result = value->execute(*this);
  if (!result.has(egg::lang::Discriminator::FlowControl)) {
    // Need to convert the result to a return flow control
    result.addFlowControl(egg::lang::Discriminator::Return);
  }
  return result;
}

egg::lang::Value egg::yolk::EggProgramContext::executeSwitch(const IEggProgramNode& self, const IEggProgramNode& value, int64_t defaultIndex, const std::vector<std::shared_ptr<IEggProgramNode>>& cases) {
  this->statement(self);
  // This is a two-phase process:
  // Phase 1 evaluates the case values
  // Phase 2 executes the block(s) as appropriate
  auto expr = value.execute(*this);
  if (expr.has(egg::lang::Discriminator::FlowControl)) {
    return expr;
  }
  auto matched = size_t(defaultIndex);
  for (size_t index = 0; index < cases.size(); ++index) {
    auto retval = cases[index]->executeWithExpression(*this, expr);
    if (!retval.is(egg::lang::Discriminator::Bool)) {
      // Failed to evaluate a case label
      return retval;
    }
    if (retval.getBool()) {
      // This was a match
      matched = index;
      break;
    }
  }
  while (matched < cases.size()) {
    auto retval = cases[matched]->execute(*this);
    if (retval.is(egg::lang::Discriminator::Break)) {
      // Explicit end of case clause
      break;
    }
    if (!retval.is(egg::lang::Discriminator::Continue)) {
      // Probably some other flow control such as a return or exception
      return retval;
    }
    matched++;
  }
  return egg::lang::Value::Void;
}

egg::lang::Value egg::yolk::EggProgramContext::executeCase(const IEggProgramNode& self, const std::vector<std::shared_ptr<IEggProgramNode>>& values, const IEggProgramNode& block, const egg::lang::Value* against) {
  if (against != nullptr) {
    // We're matching against values
    for (auto& i : values) {
      auto value = i->execute(*this);
      if (value.has(egg::lang::Discriminator::FlowControl)) {
        return value;
      }
      if (value == *against) {
        // Found a match, so return 'true'
        return egg::lang::Value::True;
      }
    }
    // No match; the switch may have a 'default' clause however
    return egg::lang::Value::False;
  }
  this->statement(self);
  return block.execute(*this);
}

egg::lang::Value egg::yolk::EggProgramContext::executeThrow(const IEggProgramNode& self, const IEggProgramNode* exception) {
  this->statement(self);
  if (exception == nullptr) {
    // This is a rethrow
    return egg::lang::Value::Rethrow;
  }
  auto value = exception->execute(*this);
  if (value.has(egg::lang::Discriminator::FlowControl)) {
    return value;
  }
  if (!value.has(egg::lang::Discriminator::Any)) {
    return this->raiseFormat("Cannot 'throw' a value of type '", value.getTagString(), "'");
  }
  return this->raise(value.getString());
}

egg::lang::Value egg::yolk::EggProgramContext::executeTry(const IEggProgramNode& self, const IEggProgramNode& block, const std::vector<std::shared_ptr<IEggProgramNode>>& catches, const IEggProgramNode* final) {
  this->statement(self);
  auto retval = block.execute(*this);
  if (retval.stripFlowControl(egg::lang::Discriminator::Exception)) {
    // An exception has indeed been thrown
    for (auto& i : catches) {
      auto match = i->executeWithExpression(*this, retval);
      if (!match.is(egg::lang::Discriminator::Bool)) {
        // Failed to evaluate the catch condition
        return this->executeFinally(match, final);
      }
      if (match.getBool()) {
        // This catch clause has been successfully executed
        return this->executeFinally(egg::lang::Value::Void, final);
      }
    }
  }
  return this->executeFinally(retval, final);
}

egg::lang::Value egg::yolk::EggProgramContext::executeCatch(const IEggProgramNode& self, const egg::lang::String& name, const IEggProgramNode& type, const IEggProgramNode& block, const egg::lang::Value& exception) {
  this->statement(self);
  assert(!exception.has(egg::lang::Discriminator::FlowControl));
  // TODO return false if typeof(exception) != type
  EggProgramSymbolTable nested(this->symtable);
  nested.addSymbol(name, *type.getType())->value = exception;
  EggProgramContext context(*this, nested);
  auto retval = block.execute(context);
  if (retval.has(egg::lang::Discriminator::FlowControl)) {
    // Check for a rethrow
    if (retval.is(egg::lang::Discriminator::Exception | egg::lang::Discriminator::Void)) {
      return exception;
    }
    return retval;
  }
  if (retval.is(egg::lang::Discriminator::Void)) {
    // Return 'true' to indicate to the 'try' statement that we ran this 'catch' block
    return egg::lang::Value::True;
  }
  return retval;
}

egg::lang::Value egg::yolk::EggProgramContext::executeFinally(const egg::lang::Value& retval, const IEggProgramNode* final) {
  if (final != nullptr) {
    auto secondary = final->execute(*this);
    if (!secondary.is(egg::lang::Discriminator::Void)) {
      return secondary;
    }
  }
  return retval;
}

egg::lang::Value egg::yolk::EggProgramContext::executeUsing(const IEggProgramNode& self, const IEggProgramNode& value, const IEggProgramNode& block) {
  this->statement(self);
  auto expr = value.execute(*this);
  if (expr.has(egg::lang::Discriminator::FlowControl)) {
    return expr;
  }
  if (!expr.is(egg::lang::Discriminator::Null)) {
    // No need to clean up afterwards
    return block.execute(*this);
  }
  if (!expr.is(egg::lang::Discriminator::Object)) {
    return this->unexpected("Expected expression in 'using' statement to be 'null' or an object", expr);
  }
  auto retval = block.execute(*this);
  auto& object = expr.getObject();
  if (!object.dispose()) {
    return this->raiseFormat("Failed to 'dispose' object instance at end of 'using' statement");
  }
  return retval;
}

egg::lang::Value egg::yolk::EggProgramContext::executeWhile(const IEggProgramNode& self, const IEggProgramNode& cond, const IEggProgramNode& block) {
  this->statement(self);
  auto retval = this->condition(cond);
  while (retval.is(egg::lang::Discriminator::Bool)) {
    retval = block.execute(*this);
    if (!retval.is(egg::lang::Discriminator::Void)) {
      if (retval.is(egg::lang::Discriminator::Break)) {
        // Just leave the loop
        return egg::lang::Value::Void;
      }
      if (!retval.is(egg::lang::Discriminator::Continue)) {
        // Probably an exception
        break;
      }
    }
    retval = this->condition(cond);
  }
  return retval;
}

egg::lang::Value egg::yolk::EggProgramContext::executeYield(const IEggProgramNode& self, const IEggProgramNode& value) {
  this->statement(self);
  auto result = value.execute(*this);
  if (!result.has(egg::lang::Discriminator::FlowControl)) {
    // Need to convert the result to a return flow control
    result.addFlowControl(egg::lang::Discriminator::Yield);
  }
  return result;
}

egg::lang::Value egg::yolk::EggProgramContext::executeArray(const IEggProgramNode& self, const std::vector<std::shared_ptr<IEggProgramNode>>& values) {
  // OPTIMIZE
  EggProgramExpression expression(*this, self);
  auto result = this->createVanillaArray();
  if (result.is(egg::lang::Discriminator::Object)) {
    auto& object = result.getObject();
    int64_t index = 0;
    for (auto& value : values) {
      auto entry = value->execute(*this);
      if (entry.has(egg::lang::Discriminator::FlowControl)) {
        return entry;
      }
      entry = object.setIndex(*this, egg::lang::Value{ index }, entry);
      if (entry.has(egg::lang::Discriminator::FlowControl)) {
        return entry;
      }
      ++index;
    }
  }
  return result;
}

egg::lang::Value egg::yolk::EggProgramContext::executeObject(const IEggProgramNode& self, const std::vector<std::shared_ptr<IEggProgramNode>>& values) {
  // OPTIMIZE
  EggProgramExpression expression(*this, self);
  auto result = this->createVanillaObject();
  if (result.is(egg::lang::Discriminator::Object)) {
    auto& object = result.getObject();
    egg::lang::String name;
    auto type = egg::lang::Type::Void;
    for (auto& value : values) {
      if (!value->symbol(name, type)) {
        return this->raiseFormat("Internal runtime error: Failed to fetch name of object property");
      }
      auto entry = value->execute(*this);
      if (entry.has(egg::lang::Discriminator::FlowControl)) {
        return entry;
      }
      entry = object.setProperty(*this, name, entry);
      if (entry.has(egg::lang::Discriminator::FlowControl)) {
        return entry;
      }
    }
  }
  return result;
}

egg::lang::Value egg::yolk::EggProgramContext::executeCall(const IEggProgramNode& self, const IEggProgramNode& callee, const std::vector<std::shared_ptr<IEggProgramNode>>& parameters) {
  EggProgramExpression expression(*this, self);
  auto func = callee.execute(*this);
  if (func.has(egg::lang::Discriminator::FlowControl)) {
    return func;
  }
  EggProgramParameters params(parameters.size());
  egg::lang::String name;
  auto type = egg::lang::Type::Void;
  for (auto& parameter : parameters) {
    auto value = parameter->execute(*this);
    if (value.has(egg::lang::Discriminator::FlowControl)) {
      return value;
    }
    if (parameter->symbol(name, type)) {
      params.addNamed(name, value);
    } else {
      params.addPositional(value);
    }
  }
  return this->call(func, params);
}

egg::lang::Value egg::yolk::EggProgramContext::executeCast(const IEggProgramNode& self, egg::lang::Discriminator tag, const std::vector<std::shared_ptr<IEggProgramNode>>& parameters) {
  EggProgramExpression expression(*this, self);
  EggProgramParameters params(parameters.size());
  egg::lang::String name;
  auto type = egg::lang::Type::Void;
  for (auto& parameter : parameters) {
    auto value = parameter->execute(*this);
    if (value.has(egg::lang::Discriminator::FlowControl)) {
      return value;
    }
    if (parameter->symbol(name, type)) {
      params.addNamed(name, value);
    } else {
      params.addPositional(value);
    }
  }
  return this->cast(tag, params);
}

egg::lang::Value egg::yolk::EggProgramContext::executeIdentifier(const IEggProgramNode& self, const egg::lang::String& name) {
  EggProgramExpression expression(*this, self);
  return this->get(name);
}

egg::lang::Value egg::yolk::EggProgramContext::executeLiteral(const IEggProgramNode& self, const egg::lang::Value& value) {
  EggProgramExpression expression(*this, self);
  return value;
}

egg::lang::Value egg::yolk::EggProgramContext::executeUnary(const IEggProgramNode& self, EggProgramUnary op, const IEggProgramNode& value) {
  EggProgramExpression expression(*this, self);
  return this->unary(op, value);
}

egg::lang::Value egg::yolk::EggProgramContext::executeBinary(const IEggProgramNode& self, EggProgramBinary op, const IEggProgramNode& lhs, const IEggProgramNode& rhs) {
  EggProgramExpression expression(*this, self);
  return this->binary(op, lhs, rhs);
}

egg::lang::Value egg::yolk::EggProgramContext::executeTernary(const IEggProgramNode& self, const IEggProgramNode& cond, const IEggProgramNode& whenTrue, const IEggProgramNode& whenFalse) {
  EggProgramExpression expression(*this, self);
  auto retval = this->condition(cond);
  if (retval.is(egg::lang::Discriminator::Bool)) {
    return retval.getBool() ? whenTrue.execute(*this) : whenFalse.execute(*this);
  }
  return retval;
}

egg::lang::LogSeverity egg::yolk::EggProgram::execute(IEggEngineExecutionContext& execution) {
  EggProgramSymbolTable symtable(nullptr);
  symtable.addBuiltins();
  egg::lang::LogSeverity severity = egg::lang::LogSeverity::None;
  EggProgramContext context(execution, symtable, severity);
  auto retval = this->root->execute(context);
  if (!retval.is(egg::lang::Discriminator::Void)) {
    std::string message;
    if (retval.stripFlowControl(egg::lang::Discriminator::Exception)) {
      // TODO exception location
      message = retval.toUTF8();
    } else {
      message = "Expected statement to return 'void', but got '" + retval.getTagString() + "' instead";
    }
    context.log(egg::lang::LogSource::Runtime, egg::lang::LogSeverity::Error, message);
  }
  return severity;
}
