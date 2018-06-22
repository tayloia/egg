#include "test.h"

namespace {
  class Monitor {
    EGG_NO_COPY(Monitor);
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

  class Instance {
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
    virtual Instance* acquireHard() const {
      // Log our increment
      this->monitor->write('+', this->name);
      return const_cast<Instance*>(this);
    }
    virtual void releaseHard() const {
      // Log our decrement
      this->monitor->write('-', this->name);
    }
  };

  typedef egg::gc::HardReferenceCounted<Instance> InstanceRC;
}

TEST(TestGCHard, Atomic8) {
  egg::gc::Atomic<int8_t> a8{ 100 };
  ASSERT_EQ(100, a8.get());
  ASSERT_EQ(100, a8.add(20));
  ASSERT_EQ(120, a8.get());
  ASSERT_EQ(120, a8.add(20));
  ASSERT_EQ(-116, a8.get()); // Wraps
  ASSERT_EQ(-116, a8.add(-4));
  ASSERT_EQ(-120, a8.get());
}

TEST(TestGCHard, Atomic64) {
  egg::gc::Atomic<int64_t> a64{ 100 };
  ASSERT_EQ(100, a64.get());
  ASSERT_EQ(100, a64.add(20));
  ASSERT_EQ(120, a64.get());
  ASSERT_EQ(120, a64.add(20));
  ASSERT_EQ(140, a64.get());
  ASSERT_EQ(140, a64.add(-240));
  ASSERT_EQ(-100, a64.get());
}

TEST(TestGCHard, ReferenceCount) {
  egg::gc::ReferenceCount rc{ 1 };
  ASSERT_EQ(2u, rc.acquire());
  ASSERT_EQ(3u, rc.acquire());
  ASSERT_EQ(2u, rc.release());
  ASSERT_EQ(1u, rc.release());
  ASSERT_EQ(0u, rc.release());
}

TEST(TestGCHard, Monitor) {
  Monitor monitor;
  ASSERT_EQ("", monitor.read());
  {
    Instance instance(monitor, "stack");
    ASSERT_EQ("*stack", monitor.read());
    ASSERT_EQ(&instance, instance.acquireHard());
    ASSERT_EQ("+stack", monitor.read());
    instance.releaseHard();
    ASSERT_EQ("-stack", monitor.read());
  }
  ASSERT_EQ("~stack", monitor.read());
}

TEST(TestGCHard, NotReferenceCounted) {
  Monitor monitor;
  ASSERT_EQ("", monitor.read());
  {
    egg::gc::NotReferenceCounted<Instance> instance(monitor, "nrc");
    ASSERT_EQ("*nrc", monitor.read());
    ASSERT_EQ(&instance, instance.acquireHard());
    ASSERT_EQ("", monitor.read());
    instance.releaseHard();
    ASSERT_EQ("", monitor.read());
  }
  ASSERT_EQ("~nrc", monitor.read());
}

TEST(TestGCHard, HardReferenceCounted) {
  Monitor monitor;
  ASSERT_EQ("", monitor.read());
  {
    egg::gc::HardReferenceCounted<Instance> instance(1, monitor, "hrc"); // rc=1
    ASSERT_EQ("*hrc", monitor.read());
    ASSERT_EQ(&instance, instance.acquireHard()); // rc=2
    ASSERT_EQ("", monitor.read());
    instance.releaseHard(); // rc=1
    ASSERT_EQ("", monitor.read());
  }
  ASSERT_EQ("~hrc", monitor.read());
}

TEST(TestGCHard, HardRef) {
  Monitor monitor;
  ASSERT_EQ("", monitor.read());
  {
    egg::gc::HardRef<Instance> ref1{ egg::gc::HardRef<Instance>::make<InstanceRC>(1, monitor, "hrc") }; // rc=2
    ASSERT_EQ("*hrc", monitor.read());
    const Instance* raw = ref1->getInstanceAddress();
    ASSERT_EQ(raw, ref1.get());
    {
      egg::gc::HardRef<Instance> ref2{ ref1 }; // rc=3
      ASSERT_EQ(raw, ref2.get());
      {
        egg::gc::HardRef<Instance> ref3{ raw }; // rc=4
        ASSERT_EQ(raw, ref3.get());
        {
          egg::gc::NotReferenceCounted<Instance> stack(monitor, "nrc");
          ASSERT_EQ("*nrc", monitor.read());
          ref3.set(&stack); // rc=3
          ASSERT_EQ(&stack, ref3.get());
          ref3 = ref2; // rc=4
          ASSERT_EQ(raw, ref3.get());
        }
        ASSERT_EQ("~nrc", monitor.read());
      } // rc=3
    } // rc=2
    ASSERT_EQ(raw, ref1.acquireHard()); // rc=3
    ref1->releaseHard(); // rc=2
    ref1->releaseHard(); // rc=1
    ASSERT_EQ("", monitor.read());
  } // rc=0
  ASSERT_EQ("~hrc", monitor.read());
}
