#include "ovum/ovum.h"

#include <set>
#include <stack>

namespace {
  using namespace egg::ovum;

  class BasketCollector : public ICollectable::IVisitor {
    BasketCollector(const BasketCollector&) = delete;
    BasketCollector& operator=(const BasketCollector&) = delete;
  public:
    BasketCollector(IBasket& basket, const std::set<const ICollectable*>& owned)
      : basket(basket),
        owned(owned) {
    }
    IBasket& basket;
    const std::set<const ICollectable*>& owned;
    std::stack<const ICollectable*> pending;
    std::set<const ICollectable*> unreachable;
    void collect() {
      // TODO thread safety
      for (const auto* collectable : this->owned) {
        assert(collectable->softGetBasket() == &basket);
        if (collectable->softIsRoot()) {
          // Construct a list of roots to start the search from
          this->pending.push(collectable);
        } else {
          // Assume all non-roots are unreachable
          this->unreachable.insert(collectable);
        }
      }
      while (!this->pending.empty()) {
        auto* collectable = pending.top();
        this->pending.pop();
        assert(this->unreachable.count(collectable) == 0);
        collectable->softVisit(*this);
      }
    }
    virtual void visit(const ICollectable& target) override {
      assert(target.softGetBasket() == &this->basket);
      assert(this->owned.find(&target) != this->owned.end());
      if (this->unreachable.erase(&target) > 0) {
        // It's a node that has just been deemed reachable
        this->pending.push(&target);
      }
    }
  };

  class BasketDefault : public HardReferenceCountedAllocator<IBasket> {
    BasketDefault(const BasketDefault&) = delete;
    BasketDefault& operator=(const BasketDefault&) = delete;
  private:
    std::set<const ICollectable*> owned; // TODO unordered_set?
  public:
    explicit BasketDefault(IAllocator& allocator)
      : HardReferenceCountedAllocator<IBasket>(allocator) {
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
        throw InternalException("Soft pointer basket transfer violation (take)");
      }
      auto* acquired = static_cast<ICollectable*>(collectable.hardAcquire());
      assert(acquired != nullptr);
      assert(acquired->softGetBasket() == this);
      if (!this->owned.insert(acquired).second) {
        // We were already known about, but shouldn't have been!
        throw InternalException("Soft pointer basket ownership violation (take)");
      }
      return acquired;
    }
    virtual void drop(const ICollectable& collectable) override {
      // Remove from our list of owned collectables
      assert(collectable.softGetBasket() == this);
      if (this->owned.erase(&collectable) == 0) {
        throw InternalException("Soft pointer basket ownership violation (drop)");
      }
      if (collectable.softSetBasket(nullptr) != ICollectable::SetBasketResult::Altered) {
        throw InternalException("Soft pointer basket transfer violation (drop)");
      }
      collectable.hardRelease();
    }
    virtual size_t collect() override {
      // TODO thread safety
      BasketCollector collector(*this, this->owned);
      collector.collect();
      for (auto collectable : collector.unreachable) {
        this->drop(*collectable);
      }
      return collector.unreachable.size();
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
      (void)this->allocator.statistics(out);
      out.currentBlocksOwned = this->owned.size();
      return true;
    }
    virtual void print(Printer& printer) const override {
      for (const auto* collectable : this->owned) {
        printer.stream << "    [" << collectable << "] " << typeid(*collectable).name() << " ";
        collectable->print(printer);
        printer.stream << std::endl;
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
    os << "$$$ Basket still owns " << stats.currentBlocksOwned << " blocks" << std::endl;
  } else {
    os << "$$$ Unable to determine number of remaining basket blocks" << std::endl;
  }
  Print::Options options{};
  options.quote = '"';
  Printer printer{ os, options };
  this->print(printer);
  return false;
}

egg::ovum::HardPtr<IBasket> egg::ovum::BasketFactory::createBasket(egg::ovum::IAllocator& allocator) {
  return allocator.makeHard<BasketDefault>();
}
