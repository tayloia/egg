#include "yolk/yolk.h"
#include "yolk/lexers.h"
#include "yolk/egg-tokenizer.h"
#include "yolk/egg-syntax.h"
#include "yolk/egg-parser.h"
#include "yolk/egg-engine.h"
#include "yolk/egg-program.h"

namespace {
  class EggProgramParameters : public egg::ovum::IParameters {
  private:
    struct Pair {
      Pair() = delete;
      egg::ovum::Variant value;
      egg::ovum::LocationSource location;
    };
    std::vector<Pair> positional;
    std::map<egg::ovum::String, Pair> named;
  public:
    explicit EggProgramParameters(size_t count) {
      this->positional.reserve(count);
    }
    void addPositional(const egg::ovum::Variant& value, const egg::ovum::LocationSource& location) {
      Pair pair{ value, location };
      this->positional.emplace_back(std::move(pair));
    }
    void addNamed(const egg::ovum::String& name, const egg::ovum::Variant& value, const egg::ovum::LocationSource& location) {
      Pair pair{ value, location };
      this->named.emplace(name, std::move(pair));
    }
    virtual size_t getPositionalCount() const override {
      return this->positional.size();
    }
    virtual egg::ovum::Variant getPositional(size_t index) const override {
      return this->positional.at(index).value;
    }
    virtual const egg::ovum::LocationSource* getPositionalLocation(size_t index) const override {
      return &this->positional.at(index).location;
    }
    virtual size_t getNamedCount() const override {
      return this->named.size();
    }
    virtual egg::ovum::String getName(size_t index) const override {
      auto iter = this->named.begin();
      std::advance(iter, index);
      return iter->first;
    }
    virtual egg::ovum::Variant getNamed(const egg::ovum::String& name) const override {
      return this->named.at(name).value;
    }
    virtual const egg::ovum::LocationSource* getNamedLocation(const egg::ovum::String& name) const override {
      return &this->named.at(name).location;
    }
  };
}

egg::yolk::EggProgramExpression::EggProgramExpression(egg::yolk::EggProgramContext& context, const egg::yolk::IEggProgramNode& node)
  : context(&context),
    before(context.swapLocation(egg::ovum::LocationRuntime(node.location(), "TODO()"))) {
  // TODO use runtime location, not source location
}

egg::yolk::EggProgramExpression::~EggProgramExpression() {
  (void)this->context->swapLocation(this->before);
}

egg::ovum::Variant egg::yolk::EggProgramContext::executeScope(const IEggProgramNode* node, ScopeAction action) {
  egg::ovum::String name;
  egg::ovum::Type type{ egg::ovum::Type::Void };
  if ((node != nullptr) && node->symbol(name, type)) {
    // Perform the action with a new scope containing our symbol
    auto nested = this->getAllocator().make<EggProgramSymbolTable>(this->symtable.get());
    nested->addSymbol(EggProgramSymbol::ReadWrite, name, type);
    auto context = this->createNestedContext(*nested);
    return action(*context);
  }
  // Just perform the action in the current scope
  return action(*this);
}

egg::ovum::Variant egg::yolk::EggProgramContext::executeStatements(const std::vector<std::shared_ptr<IEggProgramNode>>& statements) {
  // Execute all the statements one after another
  egg::ovum::String name;
  auto type = egg::ovum::Type::Void;
  for (auto& statement : statements) {
    if (statement->symbol(name, type)) {
      // We've checked for duplicate symbols already
      this->symtable->addSymbol(EggProgramSymbol::ReadWrite, name, type);
    }
    auto retval = statement->execute(*this);
    if (retval.hasFlowControl()) {
      return retval;
    }
  }
  return egg::ovum::Variant::Void;
}

egg::ovum::Variant egg::yolk::EggProgramContext::executeModule(const IEggProgramNode& self, const std::vector<std::shared_ptr<IEggProgramNode>>& statements) {
  this->statement(self);
  return this->executeStatements(statements);
}

egg::ovum::Variant egg::yolk::EggProgramContext::executeBlock(const IEggProgramNode& self, const std::vector<std::shared_ptr<IEggProgramNode>>& statements) {
  this->statement(self);
  auto nested = this->getAllocator().make<EggProgramSymbolTable>(this->symtable.get());
  auto context = this->createNestedContext(*nested);
  return context->executeStatements(statements);
}

egg::ovum::Variant egg::yolk::EggProgramContext::executeDeclare(const IEggProgramNode& self, const egg::ovum::String& name, const egg::ovum::Type& type, const IEggProgramNode* rvalue) {
  // The type information has already been used in the symbol declaration phase
  EGG_UNUSED(type); // Used in assertion only
  this->statement(self);
  if (rvalue != nullptr) {
    // The declaration contains an initial value
    return this->set(name, rvalue->execute(*this)); // not .direct()
  }
  return egg::ovum::Variant::Void;
}

egg::ovum::Variant egg::yolk::EggProgramContext::executeGuard(const IEggProgramNode& self, const egg::ovum::String& name, const egg::ovum::Type& type, const IEggProgramNode& rvalue) {
  // The type information has already been used in the symbol declaration phase
  EGG_UNUSED(type); // Used in assertion only
  this->statement(self);
  assert(type != nullptr);
  return this->guard(name, rvalue.execute(*this)); // not .direct()
}

egg::ovum::Variant egg::yolk::EggProgramContext::executeAssign(const IEggProgramNode& self, EggProgramAssign op, const IEggProgramNode& lvalue, const IEggProgramNode& rvalue) {
  this->statement(self);
  return this->assign(op, lvalue, rvalue);
}

egg::ovum::Variant egg::yolk::EggProgramContext::executeMutate(const IEggProgramNode& self, EggProgramMutate op, const IEggProgramNode& lvalue) {
  this->statement(self);
  return this->mutate(op, lvalue);
}

egg::ovum::Variant egg::yolk::EggProgramContext::executeBreak(const IEggProgramNode& self) {
  this->statement(self);
  return egg::ovum::Variant::Break;
}

egg::ovum::Variant egg::yolk::EggProgramContext::executeContinue(const IEggProgramNode& self) {
  this->statement(self);
  return egg::ovum::Variant::Continue;
}

egg::ovum::Variant egg::yolk::EggProgramContext::executeDo(const IEggProgramNode& self, const IEggProgramNode& cond, const IEggProgramNode& block) {
  this->statement(self);
  egg::ovum::Variant retval;
  do {
    retval = block.execute(*this);
    if (retval.hasFlowControl()) {
      if (retval.is(egg::ovum::VariantBits::Break)) {
        // Just leave the loop
        return egg::ovum::Variant::Void;
      }
      if (!retval.is(egg::ovum::VariantBits::Continue)) {
        // Probably an exception
        return retval;
      }
    }
    retval = this->condition(cond);
    if (!retval.isBool()) {
      // Condition evaluation failed
      return retval;
    }
  } while (retval.getBool());
  return egg::ovum::Variant::Void;
}

egg::ovum::Variant egg::yolk::EggProgramContext::executeIf(const IEggProgramNode& self, const IEggProgramNode& cond, const IEggProgramNode& trueBlock, const IEggProgramNode* falseBlock) {
  this->statement(self);
  return this->executeScope(&cond, [&](EggProgramContext& scope) {
    auto retval = scope.condition(cond);
    if (!retval.isBool()) {
      return retval;
    }
    if (retval.getBool()) {
      return trueBlock.execute(scope);
    }
    if (falseBlock != nullptr) {
      // We run the 'else' block in the original scope (with no guarded identifiers)
      return falseBlock->execute(*this);
    }
    return egg::ovum::Variant::Void;
  });
}

egg::ovum::Variant egg::yolk::EggProgramContext::executeFor(const IEggProgramNode& self, const IEggProgramNode* pre, const IEggProgramNode* cond, const IEggProgramNode* post, const IEggProgramNode& block) {
  this->statement(self);
  return this->executeScope(pre, [&](EggProgramContext& scope) {
    egg::ovum::Variant retval;
    if (pre != nullptr) {
      retval = pre->execute(scope);
      if (retval.hasFlowControl()) {
        // Probably an exception in the pre-loop statement
        return retval;
      }
    }
    if (cond == nullptr) {
      // There's no explicit condition
      for (;;) {
        retval = block.execute(scope);
        if (retval.hasFlowControl()) {
          if (retval.is(egg::ovum::VariantBits::Break)) {
            // Just leave the loop
            return egg::ovum::Variant::Void;
          }
          if (!retval.is(egg::ovum::VariantBits::Continue)) {
            // Probably an exception in the block
            return retval;
          }
        }
        if (post != nullptr) {
          retval = post->execute(scope);
          if (retval.hasFlowControl()) {
            // Probably an exception in the post-loop statement
            return retval;
          }
        }
      }
    }
    retval = scope.condition(*cond);
    while (retval.isBool()) {
      if (!retval.getBool()) {
        // The condition was false
        return egg::ovum::Variant::Void;
      }
      retval = block.execute(scope);
      if (retval.hasFlowControl()) {
        if (retval.is(egg::ovum::VariantBits::Break)) {
          // Just leave the loop
          return egg::ovum::Variant::Void;
        }
        if (!retval.is(egg::ovum::VariantBits::Continue)) {
          // Probably an exception in the block
          return retval;
        }
      }
      if (post != nullptr) {
        retval = post->execute(scope);
        if (retval.hasFlowControl()) {
          // Probably an exception in the post-loop statement
          return retval;
        }
      }
      retval = scope.condition(*cond);
    }
    return retval;
  });
}

egg::ovum::Variant egg::yolk::EggProgramContext::executeForeach(const IEggProgramNode& self, const IEggProgramNode& lvalue, const IEggProgramNode& rvalue, const IEggProgramNode& block) {
  this->statement(self);
  return this->executeScope(&lvalue, [&](EggProgramContext& scope) {
    auto dst = lvalue.assignee(scope);
    if (dst == nullptr) {
      return scope.raiseFormat("Iteration target in 'for' statement is not valid");
    }
    auto src = rvalue.execute(scope).direct();
    if (src.hasFlowControl()) {
      return src;
    }
    if (src.isString()) {
      // Optimization for string codepoint iteration
      return scope.executeForeachString(*dst, src.getString(), block);
    }
    if (src.hasObject()) {
      auto object = src.getObject();
      return scope.executeForeachIterate(*dst, *object, block);
    }
    return scope.raiseFormat("Cannot iterate '", src.getRuntimeType().toString(), "'");
  });
}

egg::ovum::Variant egg::yolk::EggProgramContext::executeForeachString(IEggProgramAssignee& target, const egg::ovum::String& source, const IEggProgramNode& block) {
  size_t index = 0;
  for (auto codepoint = source.codePointAt(0); codepoint >= 0; codepoint = source.codePointAt(++index)) {
    auto retval = target.set(egg::ovum::Variant{ egg::ovum::StringFactory::fromCodePoint(this->allocator, char32_t(codepoint)) });
    if (retval.hasFlowControl()) {
      // The assignment failed
      return retval;
    }
    retval = block.execute(*this);
    if (retval.hasFlowControl()) {
      if (retval.is(egg::ovum::VariantBits::Break)) {
        // Just leave the loop
        return egg::ovum::Variant::Void;
      }
      if (!retval.is(egg::ovum::VariantBits::Continue)) {
        // Probably an exception in the block
        return retval;
      }
    }
  }
  if (index != source.length()) {
    return this->raiseFormat("Cannot iterate through a malformed string");
  }
  return egg::ovum::Variant::Void;
}

egg::ovum::Variant egg::yolk::EggProgramContext::executeForeachIterate(IEggProgramAssignee& target, egg::ovum::IObject& source, const IEggProgramNode& block) {
  auto iterate = source.iterate(*this);
  if (iterate.hasFlowControl()) {
    // The iterator could not be created
    return iterate;
  }
  if (!iterate.hasObject()) {
    return this->unexpected("The 'for' statement expected an iterator", iterate); // TODO
  }
  auto iteration = iterate.getObject();
  for (;;) {
    auto next = iteration->iterate(*this);
    if (next.hasFlowControl()) {
      // An error occurred in the iterator
      return next;
    }
    if (next.isVoid()) {
      // The iterator concluded
      break;
    }
    auto retval = target.set(next);
    if (retval.hasFlowControl()) {
      // The assignment failed
      return retval;
    }
    retval = block.execute(*this);
    if (retval.hasFlowControl()) {
      if (retval.is(egg::ovum::VariantBits::Break)) {
        // Just leave the loop
        break;
      }
      if (!retval.is(egg::ovum::VariantBits::Continue)) {
        // Probably an exception in the block
        return retval;
      }
    }
  }
  return egg::ovum::Variant::Void;
}

egg::ovum::Variant egg::yolk::EggProgramContext::executeFunctionDefinition(const IEggProgramNode& self, const egg::ovum::String& name, const egg::ovum::Type& type, const std::shared_ptr<IEggProgramNode>& block) {
  // This defines a function, it doesn't call it
  this->statement(self);
  auto symbol = this->symtable->findSymbol(name);
  assert(symbol != nullptr);
  assert(symbol->getValue().isVoid());
  return symbol->assign(*this, this->createVanillaFunction(type, block));
}

egg::ovum::Variant egg::yolk::EggProgramContext::executeFunctionCall(const egg::ovum::Type& type, const egg::ovum::IParameters& parameters, const std::shared_ptr<IEggProgramNode>& block) {
  // This actually calls a function
  assert(block != nullptr);
  auto callable = type->callable();
  if (callable == nullptr) {
    return this->raiseFormat("Expected function-like expression to be callable, but got '", type.toString(), "' instead");
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
  auto nested = this->getAllocator().make<EggProgramSymbolTable>(this->symtable.get());
  for (size_t i = 0; i < given; ++i) {
    auto& parameter = callable->getParameter(i);
    auto pname = parameter.getName();
    assert(!pname.empty());
    auto ptype = parameter.getType();
    auto pvalue = parameters.getPositional(i);
    assert(!pvalue.hasFlowControl());
    // Use 'assign' to perform promotion, etc.
    auto result = nested->addSymbol(EggProgramSymbol::ReadWrite, pname, ptype)->assign(*this, pvalue);
    if (result.hasFlowControl()) {
      // Re-create the exception with the parameter name included
      auto* plocation = parameters.getPositionalLocation(i);
      if (plocation != nullptr) {
        // Update our current source location (it will be restored when executeFunctionCall returns)
        egg::ovum::LocationSource& source = this->location;
        source = *plocation;
      }
      return this->raiseFormat("Type mismatch for parameter '", pname, "': Expected '", ptype.toString(), "', but got '", pvalue.getRuntimeType().toString(), "' instead");
    }
  }
  auto context = this->createNestedContext(*nested);
  auto retval = block->execute(*context);
  if (retval.stripFlowControl(egg::ovum::VariantBits::Return)) {
    // Explicit return
    return retval;
  }
  return retval;
}

egg::ovum::Variant egg::yolk::EggProgramContext::executeGeneratorDefinition(const IEggProgramNode& self, const egg::ovum::Type& gentype, const egg::ovum::Type& rettype, const std::shared_ptr<IEggProgramNode>& block) {
  // This defines a generator, it doesn't call it
  // A generator is a function that simple returns an iterator function
  this->statement(self);
  assert(gentype->callable() != nullptr);
  auto itertype = gentype->callable()->getReturnType();
  auto retval = this->createVanillaGenerator(itertype, rettype, block);
  retval.addFlowControl(egg::ovum::VariantBits::Return);
  return retval;
}

egg::ovum::Variant egg::yolk::EggProgramContext::executeReturn(const IEggProgramNode& self, const IEggProgramNode* value) {
  this->statement(self);
  if (value == nullptr) {
    // This is a void return
    return egg::ovum::Variant::ReturnVoid;
  }
  auto result = value->execute(*this).direct();
  if (!result.hasFlowControl()) {
    // Need to convert the result to a return flow control
    result.addFlowControl(egg::ovum::VariantBits::Return);
  }
  return result;
}

egg::ovum::Variant egg::yolk::EggProgramContext::executeSwitch(const IEggProgramNode& self, const IEggProgramNode& value, int64_t defaultIndex, const std::vector<std::shared_ptr<IEggProgramNode>>& cases) {
  this->statement(self);
  // This is a two-phase process:
  // Phase 1 evaluates the case values
  // Phase 2 executes the block(s) as appropriate
  return this->executeScope(&value, [&](EggProgramContext& scope) {
    auto expr = value.execute(scope).direct();
    if (expr.hasFlowControl()) {
      return expr;
    }
    auto matched = size_t(defaultIndex);
    for (size_t index = 0; index < cases.size(); ++index) {
      auto retval = scope.executeWithValue(*cases[index], expr).direct();
      if (!retval.isBool()) {
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
      if (retval.is(egg::ovum::VariantBits::Break)) {
        // Explicit end of case clause
        break;
      }
      if (!retval.is(egg::ovum::VariantBits::Continue)) {
        // Probably some other flow control such as a return or exception
        return retval;
      }
      matched++;
    }
    return egg::ovum::Variant::Void;
  });
}

egg::ovum::Variant egg::yolk::EggProgramContext::executeCase(const IEggProgramNode& self, const std::vector<std::shared_ptr<IEggProgramNode>>& values, const IEggProgramNode& block) {
  auto against = this->scopeValue;
  if (against != nullptr) {
    // We're matching against values
    for (auto& i : values) {
      auto value = i->execute(*this).direct();
      if (value.hasFlowControl()) {
        return value;
      }
      if (value == *against) {
        // Found a match, so return 'true'
        return egg::ovum::Variant::True;
      }
    }
    // No match; the switch may have a 'default' clause however
    return egg::ovum::Variant::False;
  }
  this->statement(self);
  return block.execute(*this);
}

egg::ovum::Variant egg::yolk::EggProgramContext::executeThrow(const IEggProgramNode& self, const IEggProgramNode* exception) {
  this->statement(self);
  if (exception == nullptr) {
    // This is a rethrow
    return egg::ovum::Variant::Rethrow;
  }
  auto value = exception->execute(*this).direct();
  if (value.hasFlowControl()) {
    return value;
  }
  if (!value.hasOne(egg::ovum::VariantBits::Any)) {
    return this->raiseFormat("Cannot 'throw' a value of type '", value.getRuntimeType().toString(), "'");
  }
  return this->raise(value.getString());
}

egg::ovum::Variant egg::yolk::EggProgramContext::executeTry(const IEggProgramNode& self, const IEggProgramNode& block, const std::vector<std::shared_ptr<IEggProgramNode>>& catches, const IEggProgramNode* final) {
  this->statement(self);
  auto retval = block.execute(*this);
  if (retval.stripFlowControl(egg::ovum::VariantBits::Throw)) {
    // An exception has indeed been thrown
    for (auto& i : catches) {
      auto match = this->executeWithValue(*i, retval).direct();
      if (!match.isBool()) {
        // Failed to evaluate the catch condition
        return this->executeFinally(match, final);
      }
      if (match.getBool()) {
        // This catch clause has been successfully executed
        return this->executeFinally(egg::ovum::Variant::Void, final);
      }
    }
  }
  return this->executeFinally(retval, final);
}

egg::ovum::Variant egg::yolk::EggProgramContext::executeCatch(const IEggProgramNode& self, const egg::ovum::String& name, const IEggProgramNode& type, const IEggProgramNode& block) {
  this->statement(self);
  auto exception = this->scopeValue;
  assert(exception != nullptr);
  assert(!exception->hasFlowControl());
  // TODO return false if typeof(exception) != type
  auto nested = this->getAllocator().make<EggProgramSymbolTable>(this->symtable.get());
  nested->addSymbol(EggProgramSymbol::ReadWrite, name, type.getType(), *exception);
  auto context = this->createNestedContext(*nested);
  auto retval = block.execute(*context);
  if (retval.hasFlowControl()) {
    // Check for a rethrow
    if (retval.is(egg::ovum::VariantBits::Throw | egg::ovum::VariantBits::Void)) {
      return *exception;
    }
    return retval;
  }
  if (retval.isVoid()) {
    // Return 'true' to indicate to the 'try' statement that we ran this 'catch' block
    return egg::ovum::Variant::True;
  }
  return retval;
}

egg::ovum::Variant egg::yolk::EggProgramContext::executeFinally(const egg::ovum::Variant& retval, const IEggProgramNode* final) {
  if (final != nullptr) {
    auto secondary = final->execute(*this);
    if (!secondary.isVoid()) {
      return secondary;
    }
  }
  return retval;
}

egg::ovum::Variant egg::yolk::EggProgramContext::executeWhile(const IEggProgramNode& self, const IEggProgramNode& cond, const IEggProgramNode& block) {
  this->statement(self);
  return this->executeScope(&cond, [&](EggProgramContext& scope) {
    auto retval = scope.condition(cond);
    while (retval.isBool()) {
      if (!retval.getBool()) {
        // Condition failed, leave the loop
        return egg::ovum::Variant::Void;
      }
      retval = block.execute(scope);
      if (retval.hasFlowControl()) {
        if (retval.is(egg::ovum::VariantBits::Break)) {
          // Just leave the loop
          return egg::ovum::Variant::Void;
        }
        if (!retval.is(egg::ovum::VariantBits::Continue)) {
          // Probably an exception
          break;
        }
      }
      retval = scope.condition(cond);
    }
    return retval;
  });
}

egg::ovum::Variant egg::yolk::EggProgramContext::executeYield(const IEggProgramNode& self, const IEggProgramNode&) {
  // We can only yield from a stackless coroutine via 'coexecute()'
  this->statement(self);
  return this->raiseFormat("Internal runtime error: Attempt to execute 'yield' in stackful context");
}

egg::ovum::Variant egg::yolk::EggProgramContext::executeArray(const IEggProgramNode& self, const std::vector<std::shared_ptr<IEggProgramNode>>& values) {
  // OPTIMIZE
  EggProgramExpression expression(*this, self);
  auto result = this->createVanillaArray();
  if (!result.hasFlowControl() && result.hasObject()) {
    auto object = result.getObject();
    int64_t index = 0;
    for (auto& value : values) {
      auto entry = value->execute(*this).direct();
      if (entry.hasFlowControl()) {
        return entry;
      }
      entry = object->setIndex(*this, egg::ovum::Variant{ index }, entry);
      if (entry.hasFlowControl()) {
        return entry;
      }
      ++index;
    }
  }
  return result;
}

egg::ovum::Variant egg::yolk::EggProgramContext::executeObject(const IEggProgramNode& self, const std::vector<std::shared_ptr<IEggProgramNode>>& values) {
  // OPTIMIZE
  EggProgramExpression expression(*this, self);
  auto result = this->createVanillaObject();
  if (!result.hasFlowControl() && result.hasObject()) {
    auto object = result.getObject();
    egg::ovum::String name;
    auto type = egg::ovum::Type::Void;
    for (auto& value : values) {
      if (!value->symbol(name, type)) {
        return this->raiseFormat("Internal runtime error: Failed to fetch name of object property");
      }
      auto entry = value->execute(*this).direct();
      if (entry.hasFlowControl()) {
        return entry;
      }
      entry = object->setProperty(*this, name, entry);
      if (entry.hasFlowControl()) {
        return entry;
      }
    }
  }
  return result;
}

egg::ovum::Variant egg::yolk::EggProgramContext::executeCall(const IEggProgramNode& self, const IEggProgramNode& callee, const std::vector<std::shared_ptr<IEggProgramNode>>& parameters) {
  EggProgramExpression expression(*this, self);
  auto func = callee.execute(*this).direct();
  if (func.hasFlowControl()) {
    return func;
  }
  EggProgramParameters params(parameters.size());
  egg::ovum::String name;
  auto type = egg::ovum::Type::Void;
  for (auto& parameter : parameters) {
    auto value = parameter->execute(*this).direct();
    if (value.hasFlowControl()) {
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

egg::ovum::Variant egg::yolk::EggProgramContext::executeIdentifier(const IEggProgramNode& self, const egg::ovum::String& name, bool byref) {
  EggProgramExpression expression(*this, self);
  return this->get(name, byref);
}

egg::ovum::Variant egg::yolk::EggProgramContext::executeLiteral(const IEggProgramNode& self, const egg::ovum::Variant& value) {
  EggProgramExpression expression(*this, self);
  return value;
}

egg::ovum::Variant egg::yolk::EggProgramContext::executeBrackets(const IEggProgramNode& self, const IEggProgramNode& instance, const IEggProgramNode& index) {
  EggProgramExpression expression(*this, self);
  // Override our location with the index value
  this->location.column++; // TODO a better way of doing this?
  auto lhs = instance.execute(*this).direct();
  if (lhs.hasFlowControl()) {
    return lhs;
  }
  auto rhs = index.execute(*this).direct();
  if (rhs.hasFlowControl()) {
    return rhs;
  }
  return this->bracketsGet(lhs, rhs);
}

egg::ovum::Variant egg::yolk::EggProgramContext::executeDot(const IEggProgramNode& self, const IEggProgramNode& instance, const egg::ovum::String& property) {
  EggProgramExpression expression(*this, self);
  auto lhs = instance.execute(*this).direct();
  if (lhs.hasFlowControl()) {
    return lhs;
  }
  return this->dotGet(lhs, property);
}

egg::ovum::Variant egg::yolk::EggProgramContext::executeUnary(const IEggProgramNode& self, EggProgramUnary op, const IEggProgramNode& expr) {
  EggProgramExpression expression(*this, self);
  egg::ovum::Variant value{};
  return this->unary(op, expr, value);
}

egg::ovum::Variant egg::yolk::EggProgramContext::executeBinary(const IEggProgramNode& self, EggProgramBinary op, const IEggProgramNode& lhs, const IEggProgramNode& rhs) {
  EggProgramExpression expression(*this, self);
  egg::ovum::Variant left{};
  egg::ovum::Variant right{};
  return this->binary(op, lhs, rhs, left, right);
}

egg::ovum::Variant egg::yolk::EggProgramContext::executeTernary(const IEggProgramNode& self, const IEggProgramNode& cond, const IEggProgramNode& whenTrue, const IEggProgramNode& whenFalse) {
  EggProgramExpression expression(*this, self);
  auto retval = this->condition(cond).direct();
  if (retval.isBool()) {
    return retval.getBool() ? whenTrue.execute(*this).direct() : whenFalse.execute(*this).direct();
  }
  return retval;
}

egg::ovum::Variant egg::yolk::EggProgramContext::executePredicate(const IEggProgramNode& self, EggProgramBinary op, const IEggProgramNode& lhs, const IEggProgramNode& rhs) {
  EggProgramExpression expression(*this, self);
  egg::ovum::Variant left{};
  egg::ovum::Variant right{};
  auto result = this->binary(op, lhs, rhs, left, right);
  if (!result.isBool() || result.getBool()) {
    // It wasn't a predicate failure, i.e. didn't return bool:false
    return result;
  }
  auto operation = EggProgram::binaryToString(op);
  auto raised = this->raiseFormat("Assertion is untrue: ", left.toString(), " ", operation, " ", right.toString());
  if (raised.hasAll(egg::ovum::VariantBits::Throw | egg::ovum::VariantBits::Object)) {
    // Augment the exception with extra information
    auto exception = raised.getObject();
    exception->setProperty(*this, "left", left);
    exception->setProperty(*this, "operator", egg::ovum::Variant(egg::ovum::String(operation)));
    exception->setProperty(*this, "right", right);
  }
  return raised;
}

egg::ovum::Variant egg::yolk::EggProgramContext::executeWithValue(const IEggProgramNode& node, const egg::ovum::Variant& value) {
  // Run an execute call with a scope value set
  assert(this->scopeValue == nullptr);
  try {
    this->scopeValue = &value;
    auto result = node.execute(*this); // not .direct()
    this->scopeValue = nullptr;
    return result;
  } catch (...) {
    this->scopeValue = nullptr;
    throw;
  }
}

egg::ovum::ILogger::Severity egg::yolk::EggProgram::execute(IEggEngineExecutionContext& execution) {
  // Place the symbol table in our basket
  auto& allocator = execution.allocator();
  auto symtable = allocator.make<EggProgramSymbolTable>();
  this->basket->take(*symtable);
  symtable->addBuiltins();
  egg::ovum::ILogger::Severity severity = egg::ovum::ILogger::Severity::None;
  auto context = this->createRootContext(allocator, execution, *symtable, severity);
  auto retval = this->root->execute(*context);
  if (retval.stripFlowControl(egg::ovum::VariantBits::Throw)) {
    // TODO exception location
    execution.log(egg::ovum::ILogger::Source::Runtime, egg::ovum::ILogger::Severity::Error, retval.toString().toUTF8());
  } else if (retval.hasFlowControl()) {
    auto message = "Internal runtime error: Expected statement to return 'void', but got '" + retval.getRuntimeType().toString().toUTF8() + "' instead";
    execution.log(egg::ovum::ILogger::Source::Runtime, egg::ovum::ILogger::Severity::Error, message);
  }
  return severity;
}
