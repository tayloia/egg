#include "ovum/ovum.h"

#include <stack>

namespace {
  using namespace egg::ovum;

  class BasketDefault : public HardReferenceCounted<IBasket> {
    BasketDefault(const BasketDefault&) = delete;
    BasketDefault& operator=(const BasketDefault&) = delete;
  private:
    std::set<ICollectable*> owned;
    uint64_t bytes;
  public:
    explicit BasketDefault(IAllocator& allocator)
      : HardReferenceCounted(allocator, 0), bytes(0) {
    }
    virtual ~BasketDefault() {
      // Make sure we no longer own any collectables
      assert(this->owned.empty());
    }
    virtual void take(ICollectable& collectable) override {
      // Take ownership of the collectable (acquire a reference count)
      auto* acquired = collectable.hardAcquire();
      // We don't support tear-offs
      assert(acquired == &collectable);
      auto* previous = collectable.softSetBasket(this);
      assert(previous == nullptr);
      if (previous != nullptr) {
        // Edge cases: ownership directly transferred (shouldn't happen, but clean up anyway)
        acquired->hardRelease();
      }
      if (previous != this) {
        // Add to our list of owned collectables
        this->owned.insert(&collectable);
      }
    }
    virtual void drop(ICollectable& collectable) override {
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
    virtual size_t collect() override {
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
            // It's a node that has just been deemed reachable
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
    virtual size_t purge() override {
      size_t purged = 0;
      for (auto front = this->owned.begin(); front != this->owned.end(); front = this->owned.begin()) {
        this->drop(**front);
        purged++;
      }
      return purged;
    }
    virtual bool statistics(Statistics& out) const override {
      out.currentBlocksOwned = this->owned.size();
      out.currentBytesOwned = this->bytes;
      return true;
    }
  };
}

egg::ovum::HardPtr<egg::ovum::IBasket> egg::ovum::BasketFactory::createBasket(egg::ovum::IAllocator& allocator) {
  return allocator.make<BasketDefault>();
}
