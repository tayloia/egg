namespace egg::ovum {
  using VM = HardPtr<IVM>;

  class VMFactory {
  public:
    static VM createDefault(IAllocator& allocator);
    static VM createTest(IAllocator& allocator);
  };
}
