#include "yolk.h"
#include "lexers.h"
#include "egg-tokenizer.h"
#include "egg-syntax.h"
#include "egg-parser.h"
#include "egg-engine.h"
#include "egg-program.h"

egg::yolk::EggProgramNodeFlags egg::yolk::EggProgramContext::unimplemented(const std::string& function) {
  this->log(egg::lang::LogSource::Compiler, egg::lang::LogSeverity::Error, "UNIMPLEMENTED " + function);
  return EggProgramNodeFlags::Error; // WIBBLE
}

egg::lang::LogSeverity egg::yolk::EggProgram::prepare(IEggEnginePreparationContext& preparation) {
  EggProgramSymbolTable symtable(nullptr);
  symtable.addBuiltins();
  egg::lang::LogSeverity severity = egg::lang::LogSeverity::None;
  EggProgramContext context(preparation, symtable, severity);
  auto error = egg::lang::Bits::hasAnySet(this->root->prepare(context), EggProgramNodeFlags::Error);
  if (error) {
    context.log(egg::lang::LogSource::Compiler, egg::lang::LogSeverity::Error, "WIBBLE");
    return egg::lang::LogSeverity::Error;
  }
  return severity;
}
