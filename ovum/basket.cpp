#include "ovum/ovum.h"

#include <map>
#include <stack>

void egg::ovum::BasketDefault::take(ICollectable& collectable) {
  collectable.hardAcquire();
  auto* previous = collectable.softSetBasket(this);
  assert(previous == nullptr);
  if (previous != nullptr) {
    // Edge cases: ownership directly transferred
    collectable.hardRelease();
  }
  if (previous != this) {
    // Add to our list of owned collectables
    this->owned.insert(&collectable);
  }
}

void egg::ovum::BasketDefault::drop(ICollectable& collectable) {
  auto* previous = collectable.softSetBasket(nullptr);
  assert(previous == this);
  if (previous != nullptr) {
    collectable.hardRelease();
  }
  if (previous == this) {
    // Remove from our list of owned collectables
    this->owned.erase(&collectable);
  }
}

size_t egg::ovum::BasketDefault::collect() {
  // TODO thread safety
  std::stack<ICollectable*> pending;
  std::set<ICollectable*> unreachable;
  for (auto collectable : this->owned) {
    if (collectable->softIsRoot()) {
      // Construct a list of roots to start the search from
      pending.push(collectable);
    } else {
      // Assume all non-roots are unreachable
      unreachable.insert(collectable);
    }
  }
  while (!pending.empty()) {
    auto collectable = pending.top();
    pending.pop();
    assert(unreachable.count(collectable) == 0);
    collectable->softVisitLinks([&](ICollectable& target) {
      auto entry = unreachable.find(&target);
      if (entry != unreachable.end()) {
        // It's a node that just been deemed reachable
        unreachable.erase(entry);
        pending.push(&target);
      }
    });
  }
  for (auto collectable : unreachable) {
    this->drop(*collectable);
  }
  return unreachable.size();
}

size_t egg::ovum::BasketDefault::purge() {
  size_t purged = 0;
  for (auto front = this->owned.begin(); front != this->owned.end();  front = this->owned.begin()) {
    this->drop(**front);
    purged++;
  }
  return purged;
}
