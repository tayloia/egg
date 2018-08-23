#include "yolk/yolk.h"
#include "yolk/lexers.h"
#include "yolk/egg-tokenizer.h"
#include "yolk/egg-syntax.h"
#include "yolk/egg-parser.h"
#include "yolk/egg-engine.h"
#include "yolk/egg-program.h"

#include "ovum/node.h"
#include "ovum/module.h"

egg::ovum::ILogger::Severity egg::yolk::EggProgram::compile(IEggEngineCompilationContext& compilation, egg::ovum::Module&) {
  compilation.log(egg::ovum::ILogger::Source::Compiler, egg::ovum::ILogger::Severity::Warning, "WIBBLE");
  return egg::ovum::ILogger::Severity::Warning;
}
