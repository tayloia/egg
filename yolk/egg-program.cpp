#include "yolk/yolk.h"
#include "yolk/lexers.h"
#include "yolk/egg-tokenizer.h"
#include "yolk/egg-syntax.h"
#include "yolk/egg-parser.h"
#include "yolk/egg-engine.h"
#include "yolk/egg-program.h"

#include <cmath>

void egg::yolk::EggProgramSymbol::setInferredType(const egg::ovum::Type& inferred) {
  // We only allow inferred type updates
  assert(this->type == nullptr);
  this->type = inferred;
}

#if defined(WIBBLE)
egg::ovum::Value egg::yolk::EggProgramSymbol::assign(EggProgramContext& context, const egg::ovum::Value& rhs) {
  // Ask the type to assign the value so that type promotion can occur
  switch (this->kind) {
  case Kind::Builtin:
    return context.raiseFormat("Cannot re-assign built-in value: '", this->name, "'");
  case Kind::Readonly:
    return context.raiseFormat("Cannot modify read-only variable: '", this->name, "'");
  case Kind::ReadWrite:
  default:
    break;
  }
  assert(!rhs.hasFlowControl());
  if (rhs.isVoid()) {
    return context.raiseFormat("Cannot assign 'void' to '", this->name, "'");
  }
  auto retval = this->type->tryAssign(context, this->value, rhs);
  if (retval.hasFlowControl()) {
    // The assignment failed
    if (retval.hasString()) {
      // Convert the error message to a full-blown exception
      return context.raise(retval.getString());
    }
    return retval;
  }
  auto* basket = context.softGetBasket();
  assert(basket != nullptr);
  this->value.soften(*basket);
  return egg::ovum::Value::Void;
}
#endif

void egg::yolk::EggProgramSymbolTable::softVisitLinks(const Visitor& visitor) const {
  // Visit all our soft links
  this->parent.visit(visitor);
  /* WIBBLE
  for (auto& kv : this->map) {
    auto& symbol = *kv.second;
    symbol.getValue().softVisitLink(visitor);
  }
  */
}

void egg::yolk::EggProgramSymbolTable::addBuiltins() {
  // TODO add built-in symbol to symbol table here
  this->addBuiltin("string", egg::ovum::BuiltinFactory::createStringInstance(this->allocator));
  this->addBuiltin("type", egg::ovum::BuiltinFactory::createTypeInstance(this->allocator));
  this->addBuiltin("assert", egg::ovum::BuiltinFactory::createAssertInstance(this->allocator));
  this->addBuiltin("print", egg::ovum::BuiltinFactory::createPrintInstance(this->allocator));
}

void egg::yolk::EggProgramSymbolTable::addBuiltin(const std::string& name, const egg::ovum::Value& value) {
  (void)this->addSymbol(EggProgramSymbol::Kind::Builtin, name, value->getRuntimeType(), value);
}

std::shared_ptr<egg::yolk::EggProgramSymbol> egg::yolk::EggProgramSymbolTable::addSymbol(EggProgramSymbol::Kind kind, const egg::ovum::String& name, const egg::ovum::Type& type, const egg::ovum::Value& value) {
  auto inserted = this->map.emplace(name, std::make_shared<EggProgramSymbol>(kind, name, type, value));
  assert(inserted.second);
  return inserted.first->second;
}

std::shared_ptr<egg::yolk::EggProgramSymbol> egg::yolk::EggProgramSymbolTable::findSymbol(const egg::ovum::String& name, bool includeParents) const {
  auto found = this->map.find(name);
  if (found != this->map.end()) {
    return found->second;
  }
  if (includeParents && (this->parent != nullptr)) {
    return this->parent->findSymbol(name);
  }
  return nullptr;
}

#define EGG_PROGRAM_OPERATOR_STRING(name, text) text,

std::string egg::yolk::EggProgram::unaryToString(egg::yolk::EggProgramUnary op) {
  static const char* const table[] = {
    EGG_PROGRAM_UNARY_OPERATORS(EGG_PROGRAM_OPERATOR_STRING)
    "" // Stops GCC 7.3 complaining with error: array subscript is above array bounds [-Werror=array-bounds]
  };
  auto index = static_cast<size_t>(op);
  assert(index < EGG_NELEMS(table));
  return table[index];
}

std::string egg::yolk::EggProgram::binaryToString(egg::yolk::EggProgramBinary op) {
  static const char* const table[] = {
    EGG_PROGRAM_BINARY_OPERATORS(EGG_PROGRAM_OPERATOR_STRING)
    "" // Stops GCC 7.3 complaining with error: array subscript is above array bounds [-Werror=array-bounds]
  };
  auto index = static_cast<size_t>(op);
  assert(index < EGG_NELEMS(table));
  return table[index];
}

std::string egg::yolk::EggProgram::assignToString(egg::yolk::EggProgramAssign op) {
  static const char* const table[] = {
    EGG_PROGRAM_ASSIGN_OPERATORS(EGG_PROGRAM_OPERATOR_STRING)
    "" // Stops GCC 7.3 complaining with error: array subscript is above array bounds [-Werror=array-bounds]
  };
  auto index = static_cast<size_t>(op);
  assert(index < EGG_NELEMS(table));
  return table[index];
}

std::string egg::yolk::EggProgram::mutateToString(egg::yolk::EggProgramMutate op) {
  static const char* const table[] = {
    EGG_PROGRAM_MUTATE_OPERATORS(EGG_PROGRAM_OPERATOR_STRING)
    "" // Stops GCC 7.3 complaining with error: array subscript is above array bounds [-Werror=array-bounds]
  };
  auto index = static_cast<size_t>(op);
  assert(index < EGG_NELEMS(table));
  return table[index];
}

egg::ovum::ILogger::Severity egg::yolk::EggProgram::execute(IEggEngineContext& context, const egg::ovum::Module& module) {
  // WIBBLE rationalise
  auto& allocator = context.getAllocator();
  auto program = egg::ovum::ProgramFactory::createProgram(allocator, context);
  auto severity = egg::ovum::ILogger::Severity::None;;
  auto result = program->run(*module, &severity);
  if (egg::ovum::Bits::hasAnySet(result->getFlags(), egg::ovum::ValueFlags::Throw)) {
    egg::ovum::Value exception;
    if (result->getInner(exception)) {
      // We've made sure we don't re-print a rethrown exception
      context.log(egg::ovum::ILogger::Source::Runtime, egg::ovum::ILogger::Severity::Error, exception.readable());
    }
    return egg::ovum::ILogger::Severity::Error;
  }
  if (!result->getVoid()) {
    // We expect 'void' here
    auto message = "Internal runtime error: Expected statement to return 'void', but got '" + result->getRuntimeType().toString().toUTF8() + "' instead";
    context.log(egg::ovum::ILogger::Source::Runtime, egg::ovum::ILogger::Severity::Error, message);
    return egg::ovum::ILogger::Severity::Error;
  }
  return severity;
}

egg::ovum::HardPtr<egg::yolk::EggProgramContext> egg::yolk::EggProgram::createRootContext(egg::ovum::IAllocator& allocator, egg::ovum::ILogger& logger, EggProgramSymbolTable& symtable, egg::ovum::ILogger::Severity& maximumSeverity) {
  egg::ovum::LocationRuntime location(this->root->location(), "<module>");
  return allocator.make<EggProgramContext>(location, logger, symtable, maximumSeverity);
}

void egg::yolk::EggProgramContext::softVisitLinks(const Visitor& visitor) const {
  this->symtable.visit(visitor);
}

egg::ovum::HardPtr<egg::yolk::EggProgramContext> egg::yolk::EggProgramContext::createNestedContext(EggProgramSymbolTable& parent, ScopeFunction* prepareFunction) {
  return this->allocator.make<EggProgramContext>(*this, parent, prepareFunction);
}

void egg::yolk::EggProgramContext::log(egg::ovum::ILogger::Source source, egg::ovum::ILogger::Severity severity, const std::string& message) {
  if (severity > *this->maximumSeverity) {
    *this->maximumSeverity = severity;
  }
  this->logger->log(source, severity, message);
}

bool egg::yolk::EggProgramContext::findDuplicateSymbols(const std::vector<std::shared_ptr<IEggProgramNode>>& statements) {
  // Check for duplicate symbols
  bool error = false;
  egg::ovum::String name;
  auto type = egg::ovum::Type::Void;
  std::map<egg::ovum::String, egg::ovum::LocationSource> seen;
  for (auto& statement : statements) {
    if (statement->symbol(name, type)) {
      auto here = statement->location();
      auto already = seen.emplace(std::make_pair(name, here));
      if (!already.second) {
        // Already seen at this level
        this->compiler(egg::ovum::ILogger::Severity::Error, here, "Duplicate symbol declared at module level: '", name, "'");
        this->compiler(egg::ovum::ILogger::Severity::Information, already.first->second, "Previous declaration was here");
        error = true;
      } else if (this->symtable->findSymbol(name) != nullptr) {
        // Seen at an enclosing level
        this->compilerWarning(statement->location(), "Symbol name hides previously declared symbol in enclosing level: '", name, "'");
      }
    }
  }
  return error;
}

void egg::yolk::EggProgramContext::statement(const IEggProgramNode& node) {
  // TODO use runtime location, not source location
  egg::ovum::LocationSource& source = this->location;
  source = node.location();
}

egg::ovum::LocationRuntime egg::yolk::EggProgramContext::swapLocation(const egg::ovum::LocationRuntime& loc) {
  egg::ovum::LocationRuntime before = this->location;
  this->location = loc;
  return before;
}

egg::ovum::Value egg::yolk::EggProgramContext::get(const egg::ovum::String& name) {
  auto symbol = this->symtable->findSymbol(name);
  if (symbol == nullptr) {
    return this->raiseFormat("Unknown identifier: '", name, "'");
  }
  auto& value = symbol->getValue();
  if (value->getVoid()) {
    return this->raiseFormat("Uninitialized identifier: '", name.toUTF8(), "'");
  }
  return value;
}

egg::ovum::Value egg::yolk::EggProgramContext::unexpected(const std::string& expectation, const egg::ovum::Value& value) {
  return this->raiseFormat(expectation, ", but got '", value->getRuntimeType().toString(), "' instead");
}

egg::ovum::Value egg::yolk::EggProgramContext::assertion(const egg::ovum::Value& predicate) {
  egg::ovum::Bool value;
  if (!predicate->getBool(value)) {
    return this->unexpected("Expected assertion predicate to be 'bool'", predicate);
  }
  if (!value) {
    return this->raiseFormat("Assertion is untrue");
  }
  return egg::ovum::Value::Void;
}

void egg::yolk::EggProgramContext::print(const std::string& utf8) {
  return this->log(egg::ovum::ILogger::Source::User, egg::ovum::ILogger::Severity::Information, utf8);
}

egg::ovum::Value egg::yolk::EggProgramContext::raise(const egg::ovum::String& message) {
  return egg::ovum::ValueFactory::createThrowError(this->allocator, this->location, message);
}

egg::ovum::IAllocator& egg::yolk::EggProgramContext::getAllocator() const {
  return this->allocator;
}

egg::ovum::IBasket& egg::yolk::EggProgramContext::getBasket() const {
  assert(this->basket != nullptr);
  return *this->basket;
}
