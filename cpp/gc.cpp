#include "yolk.h"

#include <set>

class egg::gc::Basket::Head : public egg::gc::Collectable {
  Head(const Head&) = delete;
  Head& operator=(const Head&) = delete;
public:
  Head() {}
  std::set<Collectable*> roots;
};

egg::gc::Basket::Basket()
  : head(new Head) {
  this->head->basket = this;
  this->head->prevInBasket = this->head;
  this->head->nextInBasket = this->head;
}

egg::gc::Basket::~Basket() {
  delete this->head;
}

void egg::gc::Basket::add(Collectable& collectable, bool root) {
  // Add this collectable to the list of known collectables in this basket
  assert(this->head != nullptr);
  assert(collectable.prevInBasket == nullptr);
  assert(collectable.nextInBasket == nullptr);
  assert(collectable.basket == nullptr);
  collectable.basket = this;
  auto* next = this->head->nextInBasket;
  this->head->nextInBasket = &collectable;
  collectable.prevInBasket = this->head;
  collectable.nextInBasket = next;
  if (next != nullptr) {
    next->prevInBasket = &collectable;
  }
  if (root) {
    auto inserted = this->head->roots.emplace(&collectable).second;
    assert(inserted);
    EGG_UNUSED(inserted);
  }
}

void egg::gc::Basket::remove(Collectable& collectable) {
  // Remove this collectable from the list of known collectables in this basket
  assert(collectable.basket == this);
  assert(&collectable != this->head);
  auto* prev = collectable.prevInBasket;
  auto* next = collectable.nextInBasket;
  prev->nextInBasket = next;
  next->prevInBasket = prev;
  collectable.prevInBasket = nullptr;
  collectable.nextInBasket = nullptr;
  collectable.basket = nullptr;
  (void)this->head->roots.erase(&collectable);
}

void egg::gc::Basket::visitCollectables(IVisitor& visitor) {
  // Visit all the collectables in this basket (excluding the head)
  for (auto* p = this->head->nextInBasket; p != this->head; p = p->nextInBasket) {
    visitor.visit(*p);
  }
}

std::shared_ptr<egg::gc::Basket> egg::gc::BasketFactory::createBasket() {
  return std::make_shared<Basket>();
}
