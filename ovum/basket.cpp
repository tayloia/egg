#include "ovum/ovum.h"

#include <set>
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
        fprintf(stderr, "Basket transfer for collectable %p: %p to %p\n", acquired, previous, this);
        acquired->hardRelease();
      }
      if (previous != this) {
        // Add to our list of owned collectables
        fprintf(stderr, "Basket %p takes collectable %p\n", this, acquired);
        this->owned.insert(&collectable);
      }
    }
    virtual void drop(ICollectable& collectable) override {
      fprintf(stderr, "Basket %p dropping collectable %p\n", this, &collectable);
      auto* previous = collectable.softSetBasket(nullptr);
      assert(previous == this);
      fprintf(stderr, "Basket %p drops collectable %p\n", this, &collectable);
      if (previous != nullptr) {
        collectable.hardRelease();
      }
      if (previous == this) {
        // Remove from our list of owned collectables
        this->owned.erase(&collectable);
      }
      fprintf(stderr, "Basket %p dropped collectable %p\n", this, &collectable);
    }
    virtual size_t collect() override {
      assert(this->validate());
      // TODO thread safety
      std::stack<ICollectable*> pending;
      std::set<ICollectable*> unreachable;
      for (auto* collectable : this->owned) {
        if (collectable->softIsRoot()) {
          // Construct a list of roots to start the search from
          fprintf(stderr, "Basket %p has root collectable %p\n", this, collectable);
          pending.push(collectable);
        } else {
          // Assume all non-roots are unreachable
          fprintf(stderr, "Basket %p has unreachable collectable %p\n", this, collectable);
          unreachable.insert(collectable);
        }
      }
      while (!pending.empty()) {
        auto* collectable = pending.top();
        pending.pop();
        assert(unreachable.count(collectable) == 0);
        collectable->softVisitLinks([&](ICollectable& target) {
          auto entry = unreachable.find(&target);
          if (entry != unreachable.end()) {
            // It's a node that has just been deemed reachable
            fprintf(stderr, "Basket %p has reached collectable %p\n", this, &target);
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
  private:
    bool validate() const {
      fprintf(stderr, "--- BEGIN validate basket %p ---\n", this);
      for (auto* collectable : this->owned) {
        auto size = AllocatorDefaultPolicy::memsize(collectable, alignof(ICollectable));
        if (collectable->softIsRoot()) {
          fprintf(stderr, "Owns root collectable %p (size %u)\n", collectable, unsigned(size));
        } else {
          fprintf(stderr, "Owns unreachable collectable %p (size %u)\n", collectable, unsigned(size));
        }
        assert(size > 0);
      }
      fprintf(stderr, "--- END validate basket %p ---\n", this);
      return true;
    }
  };
}

egg::ovum::Basket egg::ovum::BasketFactory::createBasket(egg::ovum::IAllocator& allocator) {
  return allocator.make<BasketDefault>();
}
