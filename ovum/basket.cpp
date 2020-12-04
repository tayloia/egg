#include "ovum/ovum.h"

#include <set>
#include <stack>

// WUBBLE
#include <iostream>

namespace {
  using namespace egg::ovum;

  class BasketDefault : public HardReferenceCounted<IBasket> {
    BasketDefault(const BasketDefault&) = delete;
    BasketDefault& operator=(const BasketDefault&) = delete;
  private:
    std::set<const ICollectable*> owned; // TODO unordered_set?
    uint64_t bytes;
  public:
    explicit BasketDefault(IAllocator& allocator)
      : HardReferenceCounted(allocator, 0),
        bytes(0) {
    }
    virtual ~BasketDefault() {
      // Make sure we no longer own any collectables
      this->dump();
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
      // WIBBLE std::cout << "&&& COLLECT: BEGIN" << std::endl;
      std::stack<const ICollectable*> pending;
      std::set<const ICollectable*> unreachable;
      for (auto* collectable : this->owned) {
        assert(collectable->softGetBasket() == this);
        if (collectable->softIsRoot()) {
          // Construct a list of roots to start the search from
          // WIBBLE std::cout << "&&& COLLECT: ROOT " << collectable << " " << typeid(*collectable).name() << std::endl;
          pending.push(collectable);
        } else {
          // Assume all non-roots are unreachable
          // WIBBLE std::cout << "&&& COLLECT: UNREACHABLE " << collectable << " " << typeid(*collectable).name() << std::endl;
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
            // WIBBLE std::cout << "&&& COLLECT: REACHABLE " << collectable << " " << typeid(*collectable).name() << std::endl;
            pending.push(&target);
          }
          });
      }
      for (auto collectable : unreachable) {
        // WIBBLE std::cout << "&&& COLLECT: DROP " << collectable << " " << typeid(*collectable).name() << std::endl;
        this->drop(*collectable);
      }
      // WIBBLE std::cout << "&&& COLLECT: END" << std::endl;
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
    void dump(std::ostream* out = nullptr) const override {
      // WIBBLE
      std::ostream& os = (out == nullptr) ? std::cout : *out;
      os << "^^^ BASKET DUMP BEGIN (blocks=" << this->owned.size() << ", bytes=" << this->bytes << ")" << std::endl;
      for (auto entry : this->owned) {
        os << "^^^ OWNED: " << reinterpret_cast<const void*>(entry) << " " << typeid(*entry).name();
        if (entry->softIsRoot()) {
          os << " <ROOT>";
        }
        os << std::endl;
        entry->softVisit([&](const ICollectable& target) {
          os << "^^^  LINK: " << reinterpret_cast<const void*>(&target) << " " << typeid(target).name() << std::endl;
          });
      }
      os << "^^^ BASKET DUMP END" << std::endl;
    }
  };
}

egg::ovum::Basket egg::ovum::BasketFactory::createBasket(egg::ovum::IAllocator& allocator) {
  return allocator.makeHard<BasketDefault>();
}
