#include "yolk/test.h"

namespace {
  class Monitor {
    EGG_NO_COPY(Monitor)
  private:
    std::string data;
  public:
    Monitor() {}
    std::string read() {
      // Fetch the current contents of the monitor and reset it
      auto result = this->data;
      this->data.clear();
      return result;
    }
    void write(char action, const std::string& name) {
      // Write to the monitor
      data += action;
      data += name;
    }
  };

  class Instance : public egg::ovum::IHardAcquireRelease {
  private:
    Monitor* monitor;
    std::string name;
  public:
    Instance(Monitor& monitor, const std::string& name) : monitor(&monitor), name(name) {
      // Log our construction
      this->monitor->write('*', this->name);
    }
    const Instance* getInstanceAddress() const {
      return this;
    }
    virtual ~Instance() {
      // Log our destruction
      this->monitor->write('~', this->name);
    }
    virtual Instance* hardAcquire() const override {
      // Log our increment
      this->monitor->write('+', this->name);
      return const_cast<Instance*>(this);
    }
    virtual void hardRelease() const override {
      // Log our decrement
      this->monitor->write('-', this->name);
    }
  };

  typedef egg::ovum::HardReferenceCounted<Instance> InstanceRC;
}

TEST(TestGC, Atomic8) {
  egg::ovum::Atomic<int8_t> a8{ 100 };
  ASSERT_EQ(100, a8.get());
  ASSERT_EQ(100, a8.add(20));
  ASSERT_EQ(120, a8.get());
  ASSERT_EQ(120, a8.add(20));
  ASSERT_EQ(-116, a8.get()); // Wraps
  ASSERT_EQ(-116, a8.add(-4));
  ASSERT_EQ(-120, a8.get());
}

TEST(TestGC, Atomic64) {
  egg::ovum::Atomic<int64_t> a64{ 100 };
  ASSERT_EQ(100, a64.get());
  ASSERT_EQ(100, a64.add(20));
  ASSERT_EQ(120, a64.get());
  ASSERT_EQ(120, a64.add(20));
  ASSERT_EQ(140, a64.get());
  ASSERT_EQ(140, a64.add(-240));
  ASSERT_EQ(-100, a64.get());
}

TEST(TestGC, Monitor) {
  Monitor monitor;
  ASSERT_EQ("", monitor.read());
  {
    Instance instance(monitor, "stack");
    ASSERT_EQ("*stack", monitor.read());
    ASSERT_EQ(&instance, instance.hardAcquire());
    ASSERT_EQ("+stack", monitor.read());
    instance.hardRelease();
    ASSERT_EQ("-stack", monitor.read());
  }
  ASSERT_EQ("~stack", monitor.read());
}

TEST(TestGC, NotHardReferenceCounted) {
  Monitor monitor;
  ASSERT_EQ("", monitor.read());
  {
    egg::ovum::NotHardReferenceCounted<Instance> instance(monitor, "nhrc");
    ASSERT_EQ("*nhrc", monitor.read());
    ASSERT_EQ(&instance, instance.hardAcquire());
    ASSERT_EQ("", monitor.read());
    instance.hardRelease();
    ASSERT_EQ("", monitor.read());
  }
  ASSERT_EQ("~nhrc", monitor.read());
}

TEST(TestGC, HardPtr) {
  egg::test::Allocator allocator;
  Monitor monitor;
  ASSERT_EQ("", monitor.read());
  {
    egg::ovum::HardPtr<Instance> ref1{ allocator.makeHard<InstanceRC>(1, monitor, "hrc") }; // rc=2
    ASSERT_EQ("*hrc", monitor.read());
    const Instance* raw = ref1->getInstanceAddress();
    ASSERT_EQ(raw, ref1.get());
    {
      egg::ovum::HardPtr<Instance> ref2{ ref1 }; // rc=3
      ASSERT_EQ(raw, ref2.get());
      {
        egg::ovum::HardPtr<Instance> ref3{ raw }; // rc=4
        ASSERT_EQ(raw, ref3.get());
        {
          egg::ovum::NotHardReferenceCounted<Instance> stack(monitor, "nhrc");
          ASSERT_EQ("*nhrc", monitor.read());
          ref3.set(&stack); // rc=3
          ASSERT_EQ(&stack, ref3.get());
          ref3 = ref2; // rc=4
          ASSERT_EQ(raw, ref3.get());
        }
        ASSERT_EQ("~nhrc", monitor.read());
      } // rc=3
    } // rc=2
    ASSERT_EQ(raw, ref1.hardAcquire()); // rc=3
    ref1->hardRelease(); // rc=2
    ref1->hardRelease(); // rc=1
    ASSERT_EQ("", monitor.read());
  } // rc=0
  ASSERT_EQ("~hrc", monitor.read());
}
