#include "yolk.h"

#include <set>

class egg::gc::Basket::Head : public egg::gc::Collectable {
  Head(const Head&) = delete;
  Head& operator=(const Head&) = delete;
public:
  Head() : Collectable(), collectables(0) {}
  size_t collectables;
  void remove(Collectable& collectable) {
    // Remove this collectable from the list of known collectables in this basket
    assert(collectable.basket != nullptr);
    assert(collectable.basket->head == this);
    assert(&collectable != this);
    Head::resetLinks(collectable);
    auto* prev = collectable.prevInBasket;
    auto* next = collectable.nextInBasket;
    prev->nextInBasket = next;
    next->prevInBasket = prev;
    collectable.prevInBasket = nullptr;
    collectable.nextInBasket = nullptr;
    collectable.basket = nullptr;
    this->collectables--;
  }
  static void markRecursive(std::set<egg::gc::Collectable*>& unmarked, egg::gc::Collectable& collectable) {
    // Recursively follow known links
    auto removed = unmarked.erase(&collectable);
    if (removed) {
      for (auto* p = collectable.ownedLinks; p != nullptr; p = p->next) {
        if (p->to != nullptr) {
          assert(p->from == &collectable);
          markRecursive(unmarked, *p->to);
        }
      }
    }
  }
  static void resetLinks(egg::gc::Collectable& collectable) {
    // Reset all the links owned by a collectable
    for (auto* p = collectable.ownedLinks; p != nullptr; p = p->next) {
      p->to = nullptr; // mark as reset
    }
    collectable.ownedLinks = nullptr;
  }
};

egg::gc::Basket::Basket()
  : head(new Head) {
  this->head->basket = this;
  this->head->prevInBasket = this->head;
  this->head->nextInBasket = this->head;
}

egg::gc::Basket::~Basket() {
  assert(this->head->collectables == 0);
  delete this->head;
}

void egg::gc::Basket::add(Collectable& collectable) {
  // Add this collectable to the list of known collectables in this basket
  assert(this->head != nullptr);
  assert(collectable.basket == nullptr);
  assert(collectable.prevInBasket == nullptr);
  assert(collectable.nextInBasket == nullptr);
  // Take a hard reference owned by the basket
  auto acquired = collectable.hard.acquire(); 
  assert(acquired > 1); // Expect an extenal hard reference too
  EGG_UNUSED(acquired);
  collectable.basket = this;
  auto* next = this->head->nextInBasket;
  this->head->nextInBasket = &collectable;
  collectable.prevInBasket = this->head;
  collectable.nextInBasket = next;
  if (next != nullptr) {
    next->prevInBasket = &collectable;
  }
  this->head->collectables++;
}

egg::gc::Basket::Link::Link(Basket& basket, Collectable& from, Collectable& to)
  : from(&from), to(&to), next(from.ownedLinks) {
  // Make sure 'from' and 'to' are in the basket and add a reference from 'from' to 'to'
  if (from.basket == nullptr) {
    // Add the parent as a non-root member of this basket
    basket.add(to);
  }
  assert(from.basket == &basket);
  if (to.basket == nullptr) {
    // Add the pointee as a non-root member of this basket
    basket.add(to);
  }
  assert(to.basket == &basket);
  from.ownedLinks = this;
}

egg::gc::Basket::Link::~Link() {
  // Make sure we're not an active link
  if (this->to != nullptr) {
    // We're pointing at something (not already reset)
    auto origin = this->findOrigin();
    assert(origin != nullptr);
    assert(*origin == this);
    *origin = this->next;
    this->to = nullptr; // mark as reset
  }
}

egg::gc::Basket::Link** egg::gc::Basket::Link::findOrigin() const {
  // Find the pointer that points to this link in the chain of links owned by the collectable
  assert(this->from != nullptr);
  assert(this->to != nullptr);
  for (auto** p = &this->from->ownedLinks; *p != nullptr; p = &(*p)->next) {
    if (*p == this) {
      return p;
    }
    assert((*p)->from != nullptr);
    assert((*p)->to != nullptr);
    assert((*p)->from->basket == (*p)->to->basket);
  }
  return nullptr;
}

egg::gc::Collectable* egg::gc::Basket::Link::get() const {
  // TODO gc/locking semantics
  return this->to;
}

void egg::gc::Basket::Link::set(egg::gc::Collectable& pointee) {
  // TODO gc/locking semantics
  this->to = &pointee;
}

void egg::gc::Basket::visitCollectables(IVisitor& visitor) {
  // Visit all the collectables in this basket (excluding the head)
  for (auto* p = this->head->nextInBasket; p != this->head; p = p->nextInBasket) {
    visitor.visit(*p);
  }
}

void egg::gc::Basket::visitRoots(IVisitor& visitor) {
  // Visit all the roots in this basket
  for (auto* p = this->head->nextInBasket; p != this->head; p = p->nextInBasket) {
    if (p->hard.get() > 1) {
      visitor.visit(*p);
    }
  }
}

void egg::gc::Basket::visitGarbage(IVisitor& visitor) {
  // Construct a list of all known collectables
  std::set<Collectable*> unmarked;
  for (auto* p = this->head->nextInBasket; p != this->head; p = p->nextInBasket) {
    auto inserted = unmarked.emplace(p).second;
    assert(inserted);
    EGG_UNUSED(inserted);
  }
  // Follow the hierarchy of collectable references from the roots
  for (auto* p = this->head->nextInBasket; p != this->head; p = p->nextInBasket) {
    if (p->hard.get() > 1) {
      Head::markRecursive(unmarked, *p);
    }
  }
  // Now collect and visit the remaining unmarked garbage
  for (auto p = unmarked.begin(), q = unmarked.end(); p != q; ++p) {
    auto& dead = **p;
    this->head->remove(dead);
    visitor.visit(dead);
    dead.releaseHard();
  }
}

void egg::gc::Basket::visitPurge(IVisitor& visitor) {
  // Visit all the roots in this basket after purging them
  auto* p = this->head->nextInBasket;
  // Reset the head to 'empty'
  this->head->prevInBasket = this->head;
  this->head->nextInBasket = this->head;
  this->head->collectables = 0;
  while (p != this->head) {
    // Quickly unlink everything before calling the visitor
    auto& dead = *p;
    p = p->nextInBasket;
    Head::resetLinks(dead);
    visitor.visit(dead);
    dead.releaseHard();
  }
}

std::shared_ptr<egg::gc::Basket> egg::gc::BasketFactory::createBasket() {
  return std::make_shared<Basket>();
}
