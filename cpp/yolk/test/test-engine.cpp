#include "yolk/test.h"
#include "yolk/engine.h"

TEST(TestEngine, RunEmpty) {
  auto engine = egg::yolk::EngineFactory::createDefault();
  ASSERT_NE(nullptr, engine);
  auto script = engine->loadScriptFromString(engine->createString(""));
  ASSERT_NE(nullptr, script);
//  auto retval = script->run();
//  ASSERT_VALUE(egg::ovum::HardValue::Void, retval);
}
