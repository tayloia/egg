#include "yolk/test.h"
#include "yolk/engine.h"

TEST(TestEngine, CreateDefault) {
  auto engine = egg::yolk::EngineFactory::createDefault();
  ASSERT_NE(nullptr, engine);
  ASSERT_TRUE(engine->getOptions().includeStandardBuiltins);
}

TEST(TestEngine, CreateRunWithOptions) {
  egg::yolk::IEngine::Options options{ .includeStandardBuiltins = false };
  auto engine = egg::yolk::EngineFactory::createWithOptions(options);
  ASSERT_NE(nullptr, engine);
  ASSERT_FALSE(engine->getOptions().includeStandardBuiltins);
}

TEST(TestEngine, RunEmpty) {
  auto engine = egg::yolk::EngineFactory::createDefault();
  ASSERT_NE(nullptr, engine);
  auto script = engine->loadScriptFromString(engine->createString(""));
  ASSERT_NE(nullptr, script);
  auto retval = script->run();
  ASSERT_VALUE(egg::ovum::HardValue::Void, retval);
}

TEST(TestEngine, RunAssertSuccess) {
  auto engine = egg::yolk::EngineFactory::createDefault();
  auto script = engine->loadScriptFromString(engine->createString("assert(6 * 7 == 42);"));
  auto retval = script->run();
  ASSERT_VALUE(egg::ovum::HardValue::Void, retval);
}

TEST(TestEngine, RunAssertFailure) {
  auto engine = egg::yolk::EngineFactory::createDefault();
  auto script = engine->loadScriptFromString(engine->createString("assert(6 * 7 == 41);"));
  auto retval = script->run();
  ASSERT_TRUE(retval.hasAnyFlags(egg::ovum::ValueFlags::Throw));
  egg::ovum::HardValue inner;
  ASSERT_TRUE(retval->getInner(inner));
  ASSERT_PRINT("(1,8-18): Assertion is untrue: 42 == 41", inner);
}

TEST(TestEngine, RunPrint) {
  egg::test::Logger logger;
  auto engine = egg::yolk::EngineFactory::createDefault();
  engine->withLogger(logger);
  auto script = engine->loadScriptFromString(engine->createString("print(\"Hello, world!\");"));
  ASSERT_VALUE(egg::ovum::HardValue::Void, script->run());
  ASSERT_EQ("Hello, world!\n", logger.logged.str());
}
