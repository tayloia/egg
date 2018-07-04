#include "yolk.h"
#include "lexers.h"
#include "egg-tokenizer.h"
#include "egg-syntax.h"
#include "egg-parser.h"
#include "egg-engine.h"
#include "egg-program.h"

namespace {
  class EggProgramParameters : public egg::lang::IParameters {
  private:
    struct Pair {
      Pair() = delete;
      egg::lang::Value value;
      egg::lang::LocationSource location;
    };
    std::vector<Pair> positional;
    std::map<egg::lang::String, Pair> named;
  public:
    explicit EggProgramParameters(size_t count) {
      this->positional.reserve(count);
    }
    void addPositional(const egg::lang::Value& value, const egg::lang::LocationSource& location) {
      Pair pair{ value, location };
      this->positional.emplace_back(std::move(pair));
    }
    void addNamed(const egg::lang::String& name, const egg::lang::Value& value, const egg::lang::LocationSource& location) {
      Pair pair{ value, location };
      this->named.emplace(name, std::move(pair));
    }
    virtual size_t getPositionalCount() const override {
      return this->positional.size();
    }
    virtual egg::lang::Value getPositional(size_t index) const override {
      return this->positional.at(index).value;
    }
    virtual const egg::lang::LocationSource* getPositionalLocation(size_t index) const override {
      return &this->positional.at(index).location;
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
      return this->named.at(name).value;
    }
    virtual const egg::lang::LocationSource* getNamedLocation(const egg::lang::String& name) const override {
      return &this->named.at(name).location;
    }
  };

  class EggProgramFunctionObject : public egg::lang::IObject {
    EGG_NO_COPY(EggProgramFunctionObject);
  private:
    egg::gc::SoftRef<egg::yolk::EggProgramContext> program;
    egg::lang::ITypeRef type;
    std::shared_ptr<egg::yolk::IEggProgramNode> block;
  public:
    EggProgramFunctionObject(egg::yolk::EggProgramContext& program, const egg::lang::ITypeRef& type, const std::shared_ptr<egg::yolk::IEggProgramNode>& block)
      : program(),
        type(type),
        block(block) {
      assert(block != nullptr);
      this->softLink(this->program, &program);
    }
    virtual egg::lang::Value toString() const override {
      return egg::lang::Value(egg::lang::String::concat("<", this->type->toString(), ">"));
    }
    virtual egg::lang::ITypeRef getRuntimeType() const override {
      return this->type;
    }
    virtual egg::lang::Value call(egg::lang::IExecution&, const egg::lang::IParameters& parameters) override {
      return this->program->executeFunctionCall(this->type, parameters, *this->block);
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

egg::yolk::EggProgramExpression::EggProgramExpression(egg::yolk::EggProgramContext& context, const egg::yolk::IEggProgramNode& node)
  : context(&context),
    before(context.swapLocation(egg::lang::LocationRuntime(node.location(), egg::lang::String::fromUTF8("TODO()")))) {
  // TODO use runtime location, not source location
}

egg::yolk::EggProgramExpression::~EggProgramExpression() {
  (void)this->context->swapLocation(this->before);
}

egg::lang::Value egg::yolk::EggProgramContext::executeScope(const IEggProgramNode* node, ScopeAction action) {
  egg::lang::String name;
  egg::lang::ITypeRef type{ egg::lang::Type::Void };
  if ((node != nullptr) && node->symbol(name, type)) {
    // Perform the action with a new scope containing our symbol
    auto nested = egg::gc::HardRef<EggProgramSymbolTable>::make(this->symtable.get());
    nested->addSymbol(EggProgramSymbol::ReadWrite, name, type);
    auto context = this->createNestedContext(*nested);
    return action(*context);
  }
  // Just perform the action in the current scope
  return action(*this);
}

egg::lang::Value egg::yolk::EggProgramContext::executeStatements(const std::vector<std::shared_ptr<IEggProgramNode>>& statements) {
  // Execute all the statements one after another
  egg::lang::String name;
  auto type = egg::lang::Type::Void;
  for (auto& statement : statements) {
    if (statement->symbol(name, type)) {
      // We've checked for duplicate symbols already
      this->symtable->addSymbol(EggProgramSymbol::ReadWrite, name, type);
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
  auto nested = egg::gc::HardRef<EggProgramSymbolTable>::make(this->symtable.get());
  auto context = this->createNestedContext(*nested);
  return context->executeStatements(statements);
}

egg::lang::Value egg::yolk::EggProgramContext::executeDeclare(const IEggProgramNode& self, const egg::lang::String& name, const egg::lang::ITypeRef& type, const IEggProgramNode* rvalue) {
  // The type information has already been used in the symbol declaration phase
  EGG_UNUSED(type);
  this->statement(self);
  assert(type->getSimpleTypes() != egg::lang::Discriminator::Inferred);
  if (rvalue != nullptr) {
    // The declaration contains an initial value
    return this->set(name, rvalue->execute(*this)); // not .direct()
  }
  return egg::lang::Value::Void;
}

egg::lang::Value egg::yolk::EggProgramContext::executeGuard(const IEggProgramNode& self, const egg::lang::String& name, const egg::lang::ITypeRef& type, const IEggProgramNode& rvalue) {
  // The type information has already been used in the symbol declaration phase
  EGG_UNUSED(type);
  this->statement(self);
  assert(type->getSimpleTypes() != egg::lang::Discriminator::Inferred);
  return this->guard(name, rvalue.execute(*this)); // not .direct()
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
  return this->executeScope(&cond, [&](EggProgramContext& scope) {
    auto retval = scope.condition(cond);
    if (!retval.is(egg::lang::Discriminator::Bool)) {
      return retval;
    }
    if (retval.getBool()) {
      return trueBlock.execute(scope);
    }
    if (falseBlock != nullptr) {
      // We run the 'else' block in the original scope (with no guarded identifiers)
      return falseBlock->execute(*this);
    }
    return egg::lang::Value::Void;
  });
}

egg::lang::Value egg::yolk::EggProgramContext::executeFor(const IEggProgramNode& self, const IEggProgramNode* pre, const IEggProgramNode* cond, const IEggProgramNode* post, const IEggProgramNode& block) {
  this->statement(self);
  return this->executeScope(pre, [&](EggProgramContext& scope) {
    egg::lang::Value retval;
    if (pre != nullptr) {
      retval = pre->execute(scope);
      if (!retval.is(egg::lang::Discriminator::Void)) {
        // Probably an exception in the pre-loop statement
        return retval;
      }
    }
    if (cond == nullptr) {
      // There's no explicit condition
      for (;;) {
        retval = block.execute(scope);
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
          retval = post->execute(scope);
          if (!retval.is(egg::lang::Discriminator::Void)) {
            // Probably an exception in the post-loop statement
            return retval;
          }
        }
      }
    }
    retval = scope.condition(*cond);
    while (retval.is(egg::lang::Discriminator::Bool)) {
      if (!retval.getBool()) {
        // The condition was false
        return egg::lang::Value::Void;
      }
      retval = block.execute(scope);
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
        retval = post->execute(scope);
        if (!retval.is(egg::lang::Discriminator::Void)) {
          // Probably an exception in the post-loop statement
          return retval;
        }
      }
      retval = scope.condition(*cond);
    }
    return retval;
  });
}

egg::lang::Value egg::yolk::EggProgramContext::executeForeach(const IEggProgramNode& self, const IEggProgramNode& lvalue, const IEggProgramNode& rvalue, const IEggProgramNode& block) {
  this->statement(self);
  return this->executeScope(&lvalue, [&](EggProgramContext& scope) {
    auto dst = lvalue.assignee(scope);
    if (dst == nullptr) {
      return scope.raiseFormat("Iteration target in 'for' statement is not valid");
    }
    auto src = rvalue.execute(scope).direct();
    if (src.has(egg::lang::Discriminator::FlowControl)) {
      return src;
    }
    if (src.is(egg::lang::Discriminator::String)) {
      // Optimization for string codepoint iteration
      return scope.executeForeachString(*dst, src.getString(), block);
    }
    if (src.has(egg::lang::Discriminator::Object)) {
      auto object = src.getObject();
      return scope.executeForeachIterate(*dst, *object, block);
    }
    return scope.raiseFormat("Cannot iterate '", src.getRuntimeType()->toString(), "'");
  });
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
  auto iterate = source.iterate(*this);
  if (iterate.has(egg::lang::Discriminator::FlowControl)) {
    // The iterator could not be created
    return iterate;
  }
  if (!iterate.has(egg::lang::Discriminator::Object)) {
    return this->unexpected("The 'for' statement expected an iterator", iterate); // TODO
  }
  auto iteration = iterate.getObject();
  for (;;) {
    auto next = iteration->iterate(*this);
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

egg::lang::Value egg::yolk::EggProgramContext::executeFunctionDefinition(const IEggProgramNode& self, const egg::lang::String& name, const egg::lang::ITypeRef& type, const std::shared_ptr<IEggProgramNode>& block) {
  // This defines a function, it doesn't call it
  this->statement(self);
  auto symbol = this->symtable->findSymbol(name);
  assert(symbol != nullptr);
  assert(symbol->getValue().is(egg::lang::Discriminator::Void));
  return symbol->assign(*this->symtable, *this, egg::lang::Value::make<EggProgramFunctionObject>(*this, type, block));
}

egg::lang::Value egg::yolk::EggProgramContext::executeFunctionCall(const egg::lang::ITypeRef& type, const egg::lang::IParameters& parameters, const IEggProgramNode& block) {
  // This actually calls a function
  auto callable = type->callable();
  if (callable == nullptr) {
    return this->raiseFormat("Expected function-like expression to be callable, but got '", type->toString(), "' instead");
  }
  if (parameters.getNamedCount() > 0) {
    return this->raiseFormat("Named parameters in function calls are not yet supported"); // TODO
  }
  auto given = parameters.getPositionalCount();
  auto expected = callable->getParameterCount();
  if (given < expected) {
    return this->raiseFormat("Too few parameters in function call: Expected ", expected, ", but got ", given);
  }
  if (given > expected) {
    return this->raiseFormat("Too many parameters in function call: Expected ", expected, ", but got ", given);
  }
  // TODO: Better type checking
  auto nested = egg::gc::HardRef<EggProgramSymbolTable>::make(this->symtable.get());
  for (size_t i = 0; i < given; ++i) {
    auto& parameter = callable->getParameter(i);
    auto pname = parameter.getName();
    assert(!pname.empty());
    auto ptype = parameter.getType();
    auto pvalue = parameters.getPositional(i);
    assert(!pvalue.has(egg::lang::Discriminator::FlowControl));
    // Use 'assign' to perform promotion, etc.
    auto result = nested->addSymbol(EggProgramSymbol::ReadWrite, pname, ptype)->assign(*this->symtable, *this, pvalue);
    if (result.has(egg::lang::Discriminator::FlowControl)) {
      // Re-create the exception with the parameter name included
      auto* plocation = parameters.getPositionalLocation(i);
      if (plocation != nullptr) {
        // Update our current source location (it will be restored when executeFunctionCall returns)
        egg::lang::LocationSource& source = this->location;
        source = *plocation;
      }
      return this->raiseFormat("Type mismatch for parameter '", pname, "': Expected '", ptype->toString(), "', but got '", pvalue.getRuntimeType()->toString(), "' instead");
    }
  }
  auto context = this->createNestedContext(*nested);
  auto retval = block.execute(*context);
  if (retval.stripFlowControl(egg::lang::Discriminator::Return)) {
    // Explicit return
    return retval;
  }
  return retval;
}

egg::lang::Value egg::yolk::EggProgramContext::executeReturn(const IEggProgramNode& self, const IEggProgramNode* value) {
  this->statement(self);
  if (value == nullptr) {
    // This is a void return
    return egg::lang::Value::ReturnVoid;
  }
  auto result = value->execute(*this).direct();
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
  return this->executeScope(&value, [&](EggProgramContext& scope) {
    auto expr = value.execute(scope).direct();
    if (expr.has(egg::lang::Discriminator::FlowControl)) {
      return expr;
    }
    auto matched = size_t(defaultIndex);
    for (size_t index = 0; index < cases.size(); ++index) {
      auto retval = scope.executeWithValue(*cases[index], expr).direct();
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
      auto retval = cases[matched]->execute(scope);
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
  });
}

egg::lang::Value egg::yolk::EggProgramContext::executeCase(const IEggProgramNode& self, const std::vector<std::shared_ptr<IEggProgramNode>>& values, const IEggProgramNode& block) {
  auto against = this->scopeValue;
  if (against != nullptr) {
    // We're matching against values
    for (auto& i : values) {
      auto value = i->execute(*this).direct();
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
  auto value = exception->execute(*this).direct();
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
      auto match = this->executeWithValue(*i, retval).direct();
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

egg::lang::Value egg::yolk::EggProgramContext::executeCatch(const IEggProgramNode& self, const egg::lang::String& name, const IEggProgramNode& type, const IEggProgramNode& block) {
  this->statement(self);
  auto exception = this->scopeValue;
  assert(exception != nullptr);
  assert(!exception->has(egg::lang::Discriminator::FlowControl));
  // TODO return false if typeof(exception) != type
  auto nested = egg::gc::HardRef<EggProgramSymbolTable>::make(this->symtable.get());
  nested->addSymbol(EggProgramSymbol::ReadWrite, name, type.getType(), *exception);
  auto context = this->createNestedContext(*nested);
  auto retval = block.execute(*context);
  if (retval.has(egg::lang::Discriminator::FlowControl)) {
    // Check for a rethrow
    if (retval.is(egg::lang::Discriminator::Exception | egg::lang::Discriminator::Void)) {
      return *exception;
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

egg::lang::Value egg::yolk::EggProgramContext::executeWhile(const IEggProgramNode& self, const IEggProgramNode& cond, const IEggProgramNode& block) {
  this->statement(self);
  return this->executeScope(&cond, [&](EggProgramContext& scope) {
    auto retval = scope.condition(cond);
    while (retval.is(egg::lang::Discriminator::Bool)) {
      if (!retval.getBool()) {
        // Condition failed, leave the loop
        return egg::lang::Value::Void;
      }
      retval = block.execute(scope);
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
      retval = scope.condition(cond);
    }
    return retval;
  });
}

egg::lang::Value egg::yolk::EggProgramContext::executeYield(const IEggProgramNode& self, const IEggProgramNode& value) {
  this->statement(self);
  auto result = value.execute(*this).direct();
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
  if (!result.has(egg::lang::Discriminator::FlowControl) && result.has(egg::lang::Discriminator::Object)) {
    auto object = result.getObject();
    int64_t index = 0;
    for (auto& value : values) {
      auto entry = value->execute(*this).direct();
      if (entry.has(egg::lang::Discriminator::FlowControl)) {
        return entry;
      }
      entry = object->setIndex(*this, egg::lang::Value{ index }, entry);
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
  if (!result.has(egg::lang::Discriminator::FlowControl) && result.has(egg::lang::Discriminator::Object)) {
    auto object = result.getObject();
    egg::lang::String name;
    auto type = egg::lang::Type::Void;
    for (auto& value : values) {
      if (!value->symbol(name, type)) {
        return this->raiseFormat("Internal runtime error: Failed to fetch name of object property");
      }
      auto entry = value->execute(*this).direct();
      if (entry.has(egg::lang::Discriminator::FlowControl)) {
        return entry;
      }
      entry = object->setProperty(*this, name, entry);
      if (entry.has(egg::lang::Discriminator::FlowControl)) {
        return entry;
      }
    }
  }
  return result;
}

egg::lang::Value egg::yolk::EggProgramContext::executeCall(const IEggProgramNode& self, const IEggProgramNode& callee, const std::vector<std::shared_ptr<IEggProgramNode>>& parameters) {
  EggProgramExpression expression(*this, self);
  auto func = callee.execute(*this).direct();
  if (func.has(egg::lang::Discriminator::FlowControl)) {
    return func;
  }
  EggProgramParameters params(parameters.size());
  egg::lang::String name;
  auto type = egg::lang::Type::Void;
  for (auto& parameter : parameters) {
    auto value = parameter->execute(*this).direct();
    if (value.has(egg::lang::Discriminator::FlowControl)) {
      return value;
    }
    if (parameter->symbol(name, type)) {
      params.addNamed(name, value, parameter->location());
    } else {
      params.addPositional(value, parameter->location());
    }
  }
  return this->call(func, params);
}

egg::lang::Value egg::yolk::EggProgramContext::executeIdentifier(const IEggProgramNode& self, const egg::lang::String& name, bool byref) {
  EggProgramExpression expression(*this, self);
  return this->get(name, byref);
}

egg::lang::Value egg::yolk::EggProgramContext::executeLiteral(const IEggProgramNode& self, const egg::lang::Value& value) {
  EggProgramExpression expression(*this, self);
  return value;
}

egg::lang::Value egg::yolk::EggProgramContext::executeBrackets(const IEggProgramNode& self, const IEggProgramNode& instance, const IEggProgramNode& index) {
  EggProgramExpression expression(*this, self);
  // Override our location with the index value
  this->location.column++; // TODO a better way of doing this?
  auto lhs = instance.execute(*this).direct();
  if (lhs.has(egg::lang::Discriminator::FlowControl)) {
    return lhs;
  }
  auto rhs = index.execute(*this).direct();
  if (rhs.has(egg::lang::Discriminator::FlowControl)) {
    return rhs;
  }
  return lhs.getRuntimeType()->bracketsGet(*this, lhs, rhs);
}

egg::lang::Value egg::yolk::EggProgramContext::executeDot(const IEggProgramNode& self, const IEggProgramNode& instance, const egg::lang::String& property) {
  EggProgramExpression expression(*this, self);
  auto lhs = instance.execute(*this).direct();
  if (lhs.has(egg::lang::Discriminator::FlowControl)) {
    return lhs;
  }
  return lhs.getRuntimeType()->dotGet(*this, lhs, property);
}

egg::lang::Value egg::yolk::EggProgramContext::executeUnary(const IEggProgramNode& self, EggProgramUnary op, const IEggProgramNode& expr) {
  EggProgramExpression expression(*this, self);
  egg::lang::Value value{};
  return this->unary(op, expr, value);
}

egg::lang::Value egg::yolk::EggProgramContext::executeBinary(const IEggProgramNode& self, EggProgramBinary op, const IEggProgramNode& lhs, const IEggProgramNode& rhs) {
  EggProgramExpression expression(*this, self);
  egg::lang::Value left{};
  egg::lang::Value right{};
  return this->binary(op, lhs, rhs, left, right);
}

egg::lang::Value egg::yolk::EggProgramContext::executeTernary(const IEggProgramNode& self, const IEggProgramNode& cond, const IEggProgramNode& whenTrue, const IEggProgramNode& whenFalse) {
  EggProgramExpression expression(*this, self);
  auto retval = this->condition(cond).direct();
  if (retval.is(egg::lang::Discriminator::Bool)) {
    return retval.getBool() ? whenTrue.execute(*this).direct() : whenFalse.execute(*this).direct();
  }
  return retval;
}

egg::lang::Value egg::yolk::EggProgramContext::executePredicate(const IEggProgramNode& self, EggProgramBinary op, const IEggProgramNode& lhs, const IEggProgramNode& rhs) {
  EggProgramExpression expression(*this, self);
  egg::lang::Value left{};
  egg::lang::Value right{};
  auto result = this->binary(op, lhs, rhs, left, right);
  if (!result.is(egg::lang::Discriminator::Bool) || result.getBool()) {
    // It wasn't a predicate failure, i.e. didn't return bool:false
    return result;
  }
  auto operation = EggProgram::binaryToString(op);
  auto raised = this->raiseFormat("Assertion is untrue: ", left.toString(), " ", operation, " ", right.toString());
  if (raised.has(egg::lang::Discriminator::Exception) && raised.has(egg::lang::Discriminator::Object)) {
    // Augment the exception with extra information
    auto exception = raised.getObject();
    exception->setProperty(*this, egg::lang::String::fromUTF8("left"), left);
    exception->setProperty(*this, egg::lang::String::fromUTF8("operator"), egg::lang::Value(egg::lang::String::fromUTF8(operation)));
    exception->setProperty(*this, egg::lang::String::fromUTF8("right"), right);
  }
  return raised;
}

egg::lang::LogSeverity egg::yolk::EggProgram::execute(IEggEngineExecutionContext& execution) {
  // Place the symbol table in our basket
  auto symtable = this->basket.make<EggProgramSymbolTable>();
  symtable->addBuiltins();
  egg::lang::LogSeverity severity = egg::lang::LogSeverity::None;
  auto context = this->createRootContext(execution, *symtable, severity);
  auto retval = this->root->execute(*context);
  if (!retval.is(egg::lang::Discriminator::Void)) {
    std::string message;
    if (retval.stripFlowControl(egg::lang::Discriminator::Exception)) {
      // TODO exception location
      message = retval.toUTF8();
    } else {
      message = "Expected statement to return 'void', but got '" + retval.getTagString() + "' instead";
    }
    execution.log(egg::lang::LogSource::Runtime, egg::lang::LogSeverity::Error, message);
  }
  return severity;
}
