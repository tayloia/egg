#include "test.h"

TEST(TestDictionaries, Empty) {
  egg::yolk::Dictionary<std::string, int> births;
  ASSERT_EQ(0u, births.length());
  ASSERT_TRUE(births.isEmpty());
}

TEST(TestDictionaries, TryAdd) {
  egg::yolk::Dictionary<std::string, int> births;
  ASSERT_TRUE(births.tryAdd("Isaac Newton", 1643));
  ASSERT_EQ(1u, births.length());
  ASSERT_FALSE(births.isEmpty());
  ASSERT_FALSE(births.tryAdd("Isaac Newton", 1643));
  ASSERT_EQ(1u, births.length());
}

TEST(TestDictionaries, TryGet) {
  egg::yolk::Dictionary<std::string, int> births;
  ASSERT_TRUE(births.tryAdd("Isaac Newton", 1643));
  ASSERT_EQ(1u, births.length());
  int got = -1;
  ASSERT_TRUE(births.tryGet("Isaac Newton", got));
  ASSERT_EQ(1643, got);
  ASSERT_FALSE(births.tryGet("Albert Einstein", got));
  ASSERT_EQ(1643, got);
}

TEST(TestDictionaries, TryRemove) {
  egg::yolk::Dictionary<std::string, int> births;
  ASSERT_FALSE(births.tryRemove("Isaac Newton"));
  ASSERT_TRUE(births.tryAdd("Isaac Newton", 1643));
  ASSERT_EQ(1u, births.length());
  ASSERT_TRUE(births.tryRemove("Isaac Newton"));
  ASSERT_EQ(0u, births.length());
  ASSERT_FALSE(births.tryRemove("Isaac Newton"));
}

TEST(TestDictionaries, Contains) {
  egg::yolk::Dictionary<std::string, int> births;
  ASSERT_TRUE(births.tryAdd("Isaac Newton", 1643));
  ASSERT_TRUE(births.contains("Isaac Newton"));
  ASSERT_FALSE(births.contains("Albert Einstein"));
}

TEST(TestDictionaries, GetOrDefault) {
  egg::yolk::Dictionary<std::string, int> births;
  ASSERT_TRUE(births.tryAdd("Isaac Newton", 1643));
  ASSERT_EQ(1643, births.getOrDefault("Isaac Newton", -1));
  ASSERT_EQ(-1, births.getOrDefault("Albert Einstein", -1));
}

TEST(TestDictionaries, AddOrUpdate) {
  egg::yolk::Dictionary<std::string, int> births;
  ASSERT_TRUE(births.addOrUpdate("Isaac Newton", -1));
  ASSERT_EQ(1u, births.length());
  ASSERT_FALSE(births.addOrUpdate("Isaac Newton", 1643));
  ASSERT_EQ(1u, births.length());
}

TEST(TestDictionaries, GetKeys) {
  egg::yolk::Dictionary<std::string, int> births;
  std::vector<std::string> keys;
  ASSERT_EQ(0u, births.getKeys(keys));
  ASSERT_EQ(0u, keys.size());
  ASSERT_TRUE(births.tryAdd("Isaac Newton", 1643));
  ASSERT_TRUE(births.tryAdd("Albert Einstein", 1879));
  ASSERT_EQ(2u, births.getKeys(keys));
  ASSERT_EQ(2u, keys.size());
  ASSERT_EQ("Isaac Newton", keys[0]);
  ASSERT_EQ("Albert Einstein", keys[1]);
}

TEST(TestDictionaries, GetValues) {
  egg::yolk::Dictionary<std::string, int> births;
  std::vector<int> values;
  ASSERT_EQ(0u, births.getValues(values));
  ASSERT_EQ(0u, values.size());
  ASSERT_TRUE(births.tryAdd("Isaac Newton", 1643));
  ASSERT_TRUE(births.tryAdd("Albert Einstein", 1879));
  ASSERT_EQ(2u, births.getValues(values));
  ASSERT_EQ(2u, values.size());
  ASSERT_EQ(1643, values[0]);
  ASSERT_EQ(1879, values[1]);
}

TEST(TestDictionaries, GetKeyValues) {
  egg::yolk::Dictionary<std::string, int> births;
  std::vector<std::pair<std::string, int>> keyvalues;
  ASSERT_EQ(0u, births.getKeyValues(keyvalues));
  ASSERT_EQ(0u, keyvalues.size());
  ASSERT_TRUE(births.tryAdd("Isaac Newton", 1643));
  ASSERT_TRUE(births.tryAdd("Albert Einstein", 1879));
  ASSERT_EQ(2u, births.getKeyValues(keyvalues));
  ASSERT_EQ(2u, keyvalues.size());
  ASSERT_EQ("Isaac Newton", keyvalues[0].first);
  ASSERT_EQ(1643, keyvalues[0].second);
  ASSERT_EQ("Albert Einstein", keyvalues[1].first);
  ASSERT_EQ(1879, keyvalues[1].second);
}
