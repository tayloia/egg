#include "yolk/yolk.h"
#include "yolk/lexers.h"
#include "yolk/egg-tokenizer.h"
#include "yolk/egg-syntax.h"
#include "yolk/egg-parser.h"
#include "yolk/egg-engine.h"
#include "yolk/egg-program.h"

#include <cmath>

void egg::yolk::EggProgramSymbolTable::addBuiltins(egg::ovum::ITypeFactory& factory, egg::ovum::IBasket& basket) {
  // TODO add built-in symbol to symbol table here
  this->addBuiltin("string", egg::ovum::BuiltinFactory::createStringInstance(factory, basket));
  this->addBuiltin("type", egg::ovum::BuiltinFactory::createTypeInstance(factory, basket));
  this->addBuiltin("assert", egg::ovum::BuiltinFactory::createAssertInstance(factory, basket));
  this->addBuiltin("print", egg::ovum::BuiltinFactory::createPrintInstance(factory, basket));
}

void egg::yolk::EggProgramSymbolTable::addBuiltin(const std::string& name, const egg::ovum::Value& value) {
  (void)this->addSymbol(EggProgramSymbol::Kind::Builtin, name, value->getRuntimeType());
}

std::shared_ptr<egg::yolk::EggProgramSymbol> egg::yolk::EggProgramSymbolTable::addSymbol(EggProgramSymbol::Kind kind, const egg::ovum::String& name, const egg::ovum::Type& type) {
  auto inserted = this->map.emplace(name, std::make_shared<EggProgramSymbol>(kind, name, type));
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
  // TODO rationalise
  auto program = egg::ovum::ProgramFactory::createProgram(context.getAllocator(), context);
  auto severity = egg::ovum::ILogger::Severity::None;
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

egg::ovum::HardPtr<egg::yolk::EggProgramContext> egg::yolk::EggProgram::createRootContext(egg::ovum::ITypeFactory& factory, egg::ovum::ILogger& logger, EggProgramSymbolTable& symtable, egg::ovum::ILogger::Severity& maximumSeverity) {
  egg::ovum::LocationRuntime location(this->root->location(), "<module>");
  return egg::ovum::HardPtr<EggProgramContext>(factory.getAllocator().makeRaw<EggProgramContext>(factory, location, logger, symtable, maximumSeverity));
}

egg::ovum::HardPtr<egg::yolk::EggProgramContext> egg::yolk::EggProgramContext::createNestedContext(EggProgramSymbolTable& parent, ScopeFunction* prepareFunction) {
  return egg::ovum::HardPtr<EggProgramContext>(factory.getAllocator().makeRaw<EggProgramContext>(*this, parent, prepareFunction));
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
  EggProgramSymbol symbol;
  std::map<egg::ovum::String, egg::ovum::LocationSource> seen;
  for (auto& statement : statements) {
    if (statement->symbol(symbol)) {
      auto here = statement->location();
      auto already = seen.emplace(std::make_pair(symbol.name, here));
      if (!already.second) {
        // Already seen at this level
        this->compilerProblem(egg::ovum::ILogger::Severity::Error, here, "Duplicate symbol declared at module level: '", symbol.name, "'");
        this->compilerProblem(egg::ovum::ILogger::Severity::Information, already.first->second, "Previous declaration was here");
        error = true;
      } else if (this->symtable->findSymbol(symbol.name) != nullptr) {
        // Seen at an enclosing level
        this->compilerWarning(statement->location(), "Symbol name hides previously declared symbol in enclosing level: '", symbol.name, "'");
      }
    }
  }
  return error;
}
