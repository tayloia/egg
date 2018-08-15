#include "yolk/yolk.h"

#include <set>

namespace {
  class VisitorCounter : public egg::gc::Basket::IVisitor {
  public:
    size_t count = 0;
    virtual void visit(egg::gc::Collectable&) override {
      this->count++;
    }
  };
}

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

egg::gc::Basket::Link::Link(Collectable& from, Collectable* to)
  : from(&from), to(to), next(from.ownedLinks) {
  // Make sure 'from' and 'to' are in the same basket and add a reference from 'from' to 'to'
  from.ownedLinks = this;
  if (to == nullptr) {
    assert(from.basket != nullptr);
  } else {
    if (from.basket == nullptr) {
      assert(to->basket != nullptr);
      to->basket->add(from);
      assert(from.basket != nullptr);
    } else if (to->basket == nullptr) {
      assert(from.basket != nullptr);
      from.basket->add(*to);
      assert(to->basket != nullptr);
    }
    assert(to->basket != nullptr);
    assert(from.basket == to->basket);
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

void egg::gc::Basket::Link::set(Collectable& owner, Collectable& pointee) {
  // TODO gc/locking semantics
  if (this->from == nullptr) {
    this->from = &owner;
    this->next = owner.ownedLinks;
    owner.ownedLinks = this;
  }
  assert(this->from == &owner);
  if (owner.basket == nullptr) {
    assert(pointee.basket != nullptr);
    pointee.basket->add(owner);
    assert(owner.basket != nullptr);
  } else if (pointee.basket == nullptr) {
    assert(owner.basket != nullptr);
    owner.basket->add(pointee);
    assert(pointee.basket != nullptr);
  }
  this->to = &pointee;
  assert(this->from != nullptr);
  assert(this->from->basket != nullptr);
  assert(this->to != nullptr);
  assert(this->to->basket == this->from->basket);
}

void egg::gc::Basket::Link::reset() {
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

egg::gc::Basket::Basket()
  : head(new Head) {
  this->head->basket = this;
  this->head->prevInBasket = this->head;
  this->head->nextInBasket = this->head;
}

egg::gc::Basket::~Basket() {
  assert(this->validate());
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
  assert(acquired > 1);
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

void egg::gc::Basket::visitCollectables(IVisitor& visitor) {
  // Visit all the collectables in this basket (excluding the head)
  assert(this->validate());
  for (auto* p = this->head->nextInBasket; p != this->head; p = p->nextInBasket) {
    visitor.visit(*p);
  }
}

void egg::gc::Basket::visitRoots(IVisitor& visitor) {
  // Visit all the roots in this basket
  assert(this->validate());
  for (auto* p = this->head->nextInBasket; p != this->head; p = p->nextInBasket) {
    if (p->hard.get() > 1) {
      visitor.visit(*p);
    }
  }
}

void egg::gc::Basket::visitGarbage(IVisitor& visitor) {
  // Construct a list of all known collectables
  assert(this->validate());
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
    dead.hardRelease();
  }
  assert(this->validate());
}

void egg::gc::Basket::visitPurge(IVisitor& visitor) {
  // Visit all the roots in this basket after purging them
  assert(this->validate());
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
    dead.hardRelease();
  }
  assert(this->validate());
}

size_t egg::gc::Basket::collectGarbage() {
  assert(this->validate());
  VisitorCounter visitor;
  this->visitGarbage(visitor);
  return visitor.count;
}

size_t egg::gc::Basket::purgeAll() {
  assert(this->validate());
  VisitorCounter visitor;
  this->visitPurge(visitor);
  return visitor.count;
}

bool egg::gc::Basket::validate() const {
  // Make sure the entire basket is valid
  // Usually on for debugging, e.g. "assert(basket.validate());"
  assert(this->head != nullptr);
  assert(this->head->prevInBasket != nullptr);
  assert(this->head->nextInBasket != nullptr);
  assert(this->head->hard.get() == 0);
  size_t count = 0;
  for (auto* p = this->head->nextInBasket; p != this->head; p = p->nextInBasket) {
    assert(p != nullptr);
    assert(p->basket == this);
    assert(p->hard.get() > 0);
    assert(p->prevInBasket != nullptr);
    assert(p->nextInBasket != nullptr);
    assert(p->prevInBasket->nextInBasket == p);
    assert(p->nextInBasket->prevInBasket == p);
    for (auto* q = p->ownedLinks; q != nullptr; q = q->next) {
      assert(q->from == p);
      if (q->to != nullptr) {
        assert(q->to->basket == this);
      }
    }
    count++;
  }
  assert(this->head->collectables == count);
  return true;
}

std::shared_ptr<egg::gc::Basket> egg::gc::BasketFactory::createBasket() {
  return std::make_shared<Basket>();
}
