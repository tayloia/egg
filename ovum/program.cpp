#include "ovum/ovum.h"
#include "ovum/node.h"
#include "ovum/module.h"
#include "ovum/program.h"

namespace {
  using namespace egg::ovum;

  class ProgramDefault : public HardReferenceCounted<IProgram> {
    ProgramDefault(const ProgramDefault&) = delete;
    ProgramDefault& operator=(const ProgramDefault&) = delete;
  private:
    ILogger& logger;
  public:
    explicit ProgramDefault(IAllocator& allocator, ILogger& logger)
      : HardReferenceCounted(allocator, 0), logger(logger) {
    }
    virtual Variant run(const IModule&) override {
      // WIBBLE
      return Variant::Void;
    }
  };
}

egg::ovum::Program egg::ovum::ProgramFactory::create(IAllocator& allocator, ILogger& logger) {
  return allocator.make<ProgramDefault>(logger);
}
