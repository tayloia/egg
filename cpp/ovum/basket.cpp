#include "ovum/ovum.h"

#include <set>
#include <stack>

namespace {
  using namespace egg::ovum;

  class BasketDefault : public HardReferenceCountedAllocator<IBasket> {
    BasketDefault(const BasketDefault&) = delete;
    BasketDefault& operator=(const BasketDefault&) = delete;
  private:
    std::set<const ICollectable*> owned; // TODO unordered_set?
    uint64_t bytes;
  public:
    explicit BasketDefault(IAllocator& allocator)
      : HardReferenceCountedAllocator<IBasket>(allocator),
        bytes(0) {
    }
    virtual ~BasketDefault() {
      // Make sure we no longer own any collectables
      assert(this->owned.empty());
    }
    virtual ICollectable* take(const ICollectable& collectable) override {
      // Add to our list of owned collectables
      switch (collectable.softSetBasket(this)) {
      case ICollectable::SetBasketResult::Exempt:
        // The collectable isn't reference-counted
        assert(collectable.softGetBasket() == nullptr);
        return const_cast<ICollectable*>(&collectable);
      case ICollectable::SetBasketResult::Unaltered:
        // No change of basket
        assert(collectable.softGetBasket() == this);
        return const_cast<ICollectable*>(&collectable);
      case ICollectable::SetBasketResult::Altered:
        // We been transferred to us
        break;
      case ICollectable::SetBasketResult::Failed:
        // Transferred between baskets
        throw std::logic_error("Soft pointer basket transfer violation (take)");
      }
      auto* acquired = static_cast<ICollectable*>(collectable.hardAcquire());
      assert(acquired != nullptr);
      assert(acquired->softGetBasket() == this);
      if (!this->owned.insert(acquired).second) {
        // We were already known about, but shouldn't have been!
        throw std::logic_error("Soft pointer basket ownership violation (take)");
      }
      return acquired;
    }
    virtual void drop(const ICollectable& collectable) override {
      // Remove from our list of owned collectables
      assert(collectable.softGetBasket() == this);
      if (this->owned.erase(&collectable) == 0) {
        throw std::logic_error("Soft pointer basket ownership violation (drop)");
      }
      if (collectable.softSetBasket(nullptr) != ICollectable::SetBasketResult::Altered) {
        throw std::logic_error("Soft pointer basket transfer violation (drop)");
      }
      collectable.hardRelease();
    }
    virtual size_t collect() override {
      // TODO thread safety
      std::stack<const ICollectable*> pending;
      std::set<const ICollectable*> unreachable;
      for (auto* collectable : this->owned) {
        assert(collectable->softGetBasket() == this);
        if (collectable->softIsRoot()) {
          // Construct a list of roots to start the search from
          pending.push(collectable);
        } else {
          // Assume all non-roots are unreachable
          unreachable.insert(collectable);
        }
      }
      while (!pending.empty()) {
        auto* collectable = pending.top();
        pending.pop();
        assert(unreachable.count(collectable) == 0);
        collectable->softVisit([&](const ICollectable& target) {
          assert(target.softGetBasket() == this);
          assert(this->owned.find(&target) != this->owned.end());
          if (unreachable.erase(&target) > 0) {
            // It's a node that has just been deemed reachable
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
    virtual void print(Printer& printer) const override {
      size_t index = 0;
      for (const auto* entry : this->owned) {
        printer << "    [" << index++ << "] ";
        entry->print(printer);
        printer.stream() << std::endl;
      }
    }
  };
}

bool egg::ovum::IBasket::verify(std::ostream& os, size_t minimum, size_t maximum) {
  Statistics stats;
  auto collected = this->collect();
  if (collected > 0) {
    os << "$$$ Collected " << collected << " from basket" << std::endl;
  }
  if (this->statistics(stats)) {
    if ((minimum == 0) && (maximum == SIZE_MAX)) {
      // The default is to check for an empty basket
      if (stats.currentBlocksOwned == 0) {
        return true;
      }
    } else if ((stats.currentBlocksOwned >= minimum) && (stats.currentBlocksOwned <= maximum)) {
      // Otherwise use the explicit bounds
      return true;
    }
    os << "$$$ Basket still owns " << stats.currentBytesOwned << " bytes in " << stats.currentBlocksOwned << " blocks" << std::endl;
  } else {
    os << "$$$ Unable to determine number of remaining basket blocks" << std::endl;
  }
  Printer printer(os, Print::Options::DEFAULT);
  this->print(printer);
  return false;
}

egg::ovum::HardPtr<IBasket> egg::ovum::BasketFactory::createBasket(egg::ovum::IAllocator& allocator) {
  return allocator.makeHard<BasketDefault>();
}
