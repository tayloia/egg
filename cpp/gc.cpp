#include "yolk.h"

#include <set>

class egg::gc::Basket::Head : public egg::gc::Collectable {
  Head(const Head&) = delete;
  Head& operator=(const Head&) = delete;
public:
  explicit Head(Basket& basket) : Collectable(basket), roots(), collectables(0) {}
  std::set<Collectable*> roots;
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
    (void)this->roots.erase(&collectable);
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
  : head(nullptr) {
  this->head = new Head(*this);
  this->head->prevInBasket = this->head;
  this->head->nextInBasket = this->head;
}

egg::gc::Basket::~Basket() {
  assert(this->head->collectables == 0);
  delete this->head;
}

void egg::gc::Basket::add(Collectable& collectable, bool root) {
  // Add this collectable to the list of known collectables in this basket
  assert(this->head != nullptr);
  assert(collectable.prevInBasket == nullptr);
  assert(collectable.nextInBasket == nullptr);
  assert(collectable.basket == this);
  auto* next = this->head->nextInBasket;
  this->head->nextInBasket = &collectable;
  collectable.prevInBasket = this->head;
  collectable.nextInBasket = next;
  if (next != nullptr) {
    next->prevInBasket = &collectable;
  }
  this->head->collectables++;
  if (root) {
    auto inserted = this->head->roots.emplace(&collectable).second;
    assert(inserted);
    EGG_UNUSED(inserted);
  }
}

void egg::gc::Basket::setRoot(Collectable& collectable, bool root) {
  // Set the flag saying whether this collectable is a root or not
  assert(this == collectable.basket);
  assert(this->head != nullptr);
  if (root) {
    (void)this->head->roots.emplace(&collectable);
  } else {
    (void)this->head->roots.erase(&collectable);
  }
}

egg::gc::Basket::Link::Link(Basket& basket, Collectable& from, Collectable& to)
  : from(&from), to(&to), next(from.ownedLinks) {
  // Make sure 'from' and 'to' are in the basket and add a reference from 'from' to 'to'
  if (from.basket == nullptr) {
    // Add the parent as a non-root member of this basket
    basket.add(to, false);
  }
  assert(from.basket == &basket);
  if (to.basket == nullptr) {
    // Add the pointee as a non-root member of this basket
    basket.add(to, false);
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
  auto p = this->head->roots.begin();
  auto q = this->head->roots.end();
  while (p != q) {
    visitor.visit(**p);
    ++p;
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
  for (auto p = this->head->roots.begin(), q = this->head->roots.end(); p != q; ++p) {
    Head::markRecursive(unmarked, **p);
  }
  // Now collect and visit the remaining unmarked garbage
  for (auto p = unmarked.begin(), q = unmarked.end(); p != q; ++p) {
    this->head->remove(**p);
    visitor.visit(**p);
  }
}

void egg::gc::Basket::visitPurge(IVisitor& visitor) {
  // Visit all the roots in this basket after purging them
  auto* p = this->head->nextInBasket;
  // Reset the head to 'empty'
  this->head->prevInBasket = this->head;
  this->head->nextInBasket = this->head;
  this->head->collectables = 0;
  this->head->roots.clear();
  while (p != this->head) {
    // Quickly unlink everything before calling the visitor
    Head::resetLinks(*p);
    auto* q = p->nextInBasket;
    visitor.visit(*p);
    p = q;
  }
}

std::shared_ptr<egg::gc::Basket> egg::gc::BasketFactory::createBasket() {
  return std::make_shared<Basket>();
}
