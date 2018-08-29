#include "yolk/test.h"
#include "yolk/lexers.h"
#include "yolk/egg-tokenizer.h"
#include "yolk/egg-syntax.h"
#include "yolk/egg-parser.h"
#include "yolk/egg-engine.h"
#include "yolk/egg-program.h"

using namespace egg::yolk;

namespace {
  void hexdump(std::ostream& stream, const egg::ovum::Memory& memory) {
    static const char hex[] = "0123456789ABCDEF";
    if (memory != nullptr) {
      auto* p = memory->begin();
      auto* q = memory->end();
      stream << '[';
      if (p < q) {
        stream << hex[*p >> 4] << hex[*p & 0xF];
        p++;
      }
      while (p < q) {
        stream << ' ' << hex[*p >> 4] << hex[*p & 0xF];
        p++;
      }
      stream << ']';
    }
  }

  std::string compile(TextStream& stream) {
    egg::test::Allocator allocator;
    auto root = EggParserFactory::parseModule(allocator, stream);
    auto engine = EggEngineFactory::createEngineFromParsed(allocator, root);
    auto logger = std::make_shared<egg::test::Logger>();
    auto preparation = EggEngineFactory::createPreparationContext(allocator, logger);
    if (engine->prepare(*preparation) != egg::ovum::ILogger::Severity::Error) {
      auto compilation = EggEngineFactory::createCompilationContext(allocator, logger);
      egg::ovum::Module module;
      if (engine->compile(*compilation, module) != egg::ovum::ILogger::Severity::Error) {
        hexdump(logger->logged, egg::ovum::ModuleFactory::toMemory(allocator, *module));
      }
    }
    return logger->logged.str();
  }
}

TEST(TestModules, Minimal) {
  StringTextStream stream("");
  auto retval = compile(stream);
  ASSERT_EQ("[A3 67 67 56 4D 00 FE FD 9D 42]", retval);
}

TEST(TestModules, HelloWorld) {
  StringTextStream stream("print(\"hello world\");");
  auto retval = compile(stream);
  ASSERT_EQ("[A3 67 67 56 4D 00 04 02 68 65 6C 6C 6F 20 77 6F 72 6C 64 FF 70 72 69 6E 74 FF FE FD 9D A4 2B 12 01 12 00]", retval);
}

TEST(TestModules, Coverage) {
  FileTextStream stream("~/yolk/test/data/coverage.egg");
  auto retval = compile(stream);
  ASSERT_STARTSWITH(retval, "[A3 67 67 56 4D 00 ");
}

TEST(TestModules, Compiler) {
  egg::test::Allocator allocator;
  egg::test::Logger logger;
  ASSERT_VARIANT(egg::ovum::VariantBits::Void, egg::test::Compiler::run(allocator, logger, "~/yolk/test/data/coverage.egg"));
}
