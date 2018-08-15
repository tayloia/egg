#include "ovum/ovum.h"
#include "ovum/ast.h"
#include "ovum/utf8.h"

#include <algorithm>

namespace {
  using namespace egg::ovum;

  class MemoryBuf : public std::streambuf {
  public:
    MemoryBuf(const char* begin, const char* end) {
      assert(begin != nullptr);
      assert(end >= begin);
      auto p = const_cast<char*>(begin);
      auto q = const_cast<char*>(end);
      this->setg(p, p, q);
    }
  };

  class MemoryStream : public std::istream {
    MemoryStream(const MemoryStream&) = delete;
    MemoryStream& operator=(const MemoryStream&) = delete;
  private:
    MemoryBuf membuf;
  public:
    MemoryStream(const uint8_t* p, const uint8_t* q)
      : std::istream(&membuf),
        membuf(reinterpret_cast<const char*>(p), reinterpret_cast<const char*>(q)) {
    }
  };

  class ModuleReader {
    ModuleReader(const ModuleReader&) = delete;
    ModuleReader& operator=(const ModuleReader&) = delete;
  private:
    IAllocator& allocator;
    std::istream& stream;
    std::vector<Int> ivalue;
    std::vector<Float> fvalue;
    std::vector<String> svalue;
  public:
    ModuleReader(IAllocator& allocator, std::istream& stream)
      : allocator(allocator), stream(stream) {
    }
    Node read() {
      Node root;
      if (!this->readMagic()) {
        throw std::runtime_error("Invalid magic signature in binary module");
      }
      char ch;
      while (this->stream.get(ch)) {
        switch (Section(uint8_t(ch))) {
        case SECTION_MAGIC:
          throw std::runtime_error("Duplicated magic section in binary module");
        case SECTION_POSINTS:
          this->readInts(false);
          break;
        case SECTION_NEGINTS:
          this->readInts(true);
          break;
        case SECTION_FLOATS:
          this->readFloats();
          break;
        case SECTION_STRINGS:
          this->readStrings();
          break;
        case SECTION_CODE:
          // Read the abstract syntax tree
          root = this->readNode();
          if (!this->stream.get(ch)) {
            // No source section
            return root;
          }
          if (Section(uint8_t(ch)) != SECTION_SOURCE) {
            throw std::runtime_error("Only source sections can follow code sections in binary module");
          }
          return root;
        case SECTION_SOURCE:
          throw std::runtime_error("Source section without preceding code section in binary module");
        default:
          throw std::runtime_error("Unrecognized section in binary module");
        }
      }
      throw std::runtime_error("Missing code section in binary module");
    }
  private:
    bool readMagic() const {
#define EGG_VM_MAGIC_BYTE(byte) if (this->readByte() != byte) { return false; }
      EGG_VM_MAGIC(EGG_VM_MAGIC_BYTE)
#undef EGG_VM_MAGIC_BYTE
      return true;
    }
    void readInts(bool negative) {
      // Read a sequence of 64-bit signed values
      auto count = this->readUnsigned();
      this->ivalue.reserve(this->ivalue.size() + size_t(count));
      while (count > 0) {
        this->ivalue.push_back(this->readInt(negative));
        count--;
      }
    }
    Int readInt(bool negative) const {
      // Read a single 64-bit signed value
      auto i = Int(this->readUnsigned());
      if (negative) {
        i = ~i;
      }
      return i;
    }
    void readFloats() {
      // Read a sequence of 64-bit floating-point values
      auto count = this->readUnsigned();
      this->fvalue.reserve(this->fvalue.size() + size_t(count));
      while (count > 0) {
        this->fvalue.push_back(this->readFloat());
        count--;
      }
    }
    Float readFloat() const {
      // Read a single 64-bit floating-point value
      MantissaExponent me;
      me.mantissa = indexInt(this->readUnsigned());
      me.exponent = indexInt(this->readUnsigned());
      return me.toFloat();
    }
    void readStrings() {
      // Read a sequence of string values
      auto count = this->readUnsigned();
      this->svalue.reserve(this->svalue.size() + size_t(count));
      while (count > 0) {
        this->svalue.push_back(this->readString());
        count--;
      }
    }
    String readString() const {
      size_t codepoints = 0;
      std::stringstream ss;
      while (this->readCodePoint(ss)) {
        codepoints++;
      }
      return String(ss.str(), codepoints);
    }
    bool readCodePoint(std::ostream& out) const {
      char ch;
      if (!this->stream.get(ch)) {
        throw std::runtime_error("Missing UTF-8 string constant in binary module");
      }
      auto byte = uint8_t(ch);
      if (byte == 0xFF) {
        // String terminal
        return false;
      }
      out.put(ch);
      if (byte < 0x80) {
        // Fast code path for ASCII
        return true;
      }
      auto length = utf8::sizeFromLeadByte(byte);
      if (length == SIZE_MAX) {
        throw std::runtime_error("Corrupt UTF-8 string constant in binary module");
      }
      assert(length > 1);
      for (size_t i = 1; i < length; ++i) {
        if (!this->stream.get(ch)) {
          throw std::runtime_error("Truncated UTF-8 string constant in binary module");
        }
        out.put(ch);
        byte = uint8_t(ch);
        if ((byte & 0xC0) != 0x80) {
          // Bad continuation byte
          throw std::runtime_error("Malformed UTF-8 string constant in binary module");
        }
      }
      return true;
    }
    Node readNode() const {
      auto byte = this->readByte();
      auto opcode = opcodeFromMachineByte(byte);
      if (opcode == OPCODE_reserved) {
        throw std::runtime_error("Invalid opcode in code section of binary module");
      }
      auto operand = std::numeric_limits<uint64_t>::max();
      auto& properties = opcodeProperties(opcode);
      if (properties.operand) {
        operand = this->readUnsigned();
      }
      std::vector<Node> attributes;
      while (this->stream.peek() == OPCODE_ATTRIBUTE) {
        attributes.push_back(this->readNode());
      }
      std::vector<Node> children;
      auto count = childrenFromMachineByte(byte);
      if (count == SIZE_MAX) {
        // This is a list terminated with an OPCODE_END sentinel
        while (this->stream.peek() != OPCODE_END) {
          children.push_back(this->readNode());
        }
        this->stream.get(); // skip the sentinel
      } else {
        for (size_t child = 0; child < count; ++child) {
          children.push_back(this->readNode());
        }
      }
      if (!properties.operand) {
        // No operand
        return NodeFactory::create(this->allocator, opcode, &children, &attributes);
      }
      EGG_WARNING_SUPPRESS_SWITCH_BEGIN
      switch (opcode) {
      case OPCODE_IVALUE:
        // Operand is an index into the int table
        return NodeFactory::create(this->allocator, opcode, &children, &attributes, this->indexInt(operand));
      case OPCODE_FVALUE:
        // Operand is an index into the float table
        return NodeFactory::create(this->allocator, opcode, &children, &attributes, this->indexFloat(operand));
      case OPCODE_SVALUE:
        // Operand is an index into the string table
        return NodeFactory::create(this->allocator, opcode, &children, &attributes, this->indexString(operand));
      }
      EGG_WARNING_SUPPRESS_SWITCH_END
      // Operand is probably an operator index
      return NodeFactory::create(this->allocator, opcode, &children, &attributes, Int(operand));
    }
    Int indexInt(uint64_t index) const {
      if (index >= this->ivalue.size()) {
        throw std::runtime_error("Invalid integer constant index in binary module");
      }
      return this->ivalue[size_t(index)];
    }
    Float indexFloat(uint64_t index) const {
      if (index >= this->fvalue.size()) {
        throw std::runtime_error("Invalid floating-point constant index in binary module");
      }
      return this->fvalue[size_t(index)];
    }
    String indexString(uint64_t index) const {
      if (index >= this->svalue.size()) {
        throw std::runtime_error("Invalid string constant index in binary module");
      }
      return this->svalue[size_t(index)];
    }
    uint64_t readUnsigned() const {
      // Read up to 63 bits as an unsigned integer
      auto byte = this->readByte();
      if (byte <= 0x80) {
        // Fast return for small values
        return byte;
      }
      uint64_t result = byte;
      size_t bits = 0;
      while (byte >= 0x80) {
        byte = this->readByte();
        bits += 7;
        if (bits > 63) {
          throw std::runtime_error("Unsigned integer overflow in binary module");
        }
        result = ((result - 0x80) << 7) + byte;
      }
      assert(result < 0x8000000000000000);
      return result;
    }
    uint8_t readByte() const {
      // Read a single 8-bit unsigned integer
      char ch;
      if (!this->stream.get(ch)) {
        throw std::runtime_error("Truncated section in binary module");
      }
      return uint8_t(ch);
    }
  };

  class ModuleWriter {
    ModuleWriter(const ModuleWriter&) = delete;
    ModuleWriter& operator=(const ModuleWriter&) = delete;
  private:
    std::ostream& stream;
    std::map<Int, size_t> ivalues;
    std::map<std::pair<Int, Int>, size_t> fvalues;
    StringMap<size_t> svalues;
    size_t positives;
  public:
    explicit ModuleWriter(std::ostream& stream)
      : stream(stream), positives(0) {
    }
    void write(const INode& root) {
      this->findConstants(root);
      this->writeMagic();
      this->writeInts();
      this->writeFloats();
      this->writeStrings();
      this->writeCode(root);
    }
  private:
    void findConstants(const INode& node) {
      switch (node.getOperand()) {
      case INode::Operand::Int:
        // Keep track of the integers
        this->foundInt(node.getInt());
        break;
      case INode::Operand::Float:
        // Keep track of the mantissa/exponent integers
        this->foundFloat(node.getFloat());
        break;
      case INode::Operand::String:
        // Keep track of the strings
        this->foundString(node.getString());
        break;
      case INode::Operand::None:
        // Nothing to keep track of
        break;
      }
      auto n = node.getAttributes();
      for (size_t i = 0; i < n; ++i) {
        this->findConstants(node.getAttribute(i));
      }
      n = node.getChildren();
      for (size_t i = 0; i < n; ++i) {
        this->findConstants(node.getChild(i));
      }
    }
    void foundInt(Int value) {
      auto inserted = this->ivalues.emplace(value, SIZE_MAX);
      if (inserted.second && (value >= 0)) {
        this->positives++;
      }
    }
    void foundFloat(Float value) {
      MantissaExponent me;
      me.fromFloat(value);
      foundInt(me.mantissa);
      foundInt(me.exponent);
      auto key = std::make_pair(me.mantissa, me.exponent);
      this->fvalues.emplace(key, SIZE_MAX);
    }
    void foundString(String value) {
      this->svalues.emplace(value, SIZE_MAX);
    }
    void writeMagic() const {
#define EGG_VM_MAGIC_BYTE(byte) this->writeByte(byte);
      EGG_VM_MAGIC(EGG_VM_MAGIC_BYTE)
#undef EGG_VM_MAGIC_BYTE
    }
    void writeInts() {
      size_t index = 0;
      for (auto& i : this->ivalues) {
        if (i.first >= 0) {
          assert(i.second == SIZE_MAX);
          if (index == 0) {
            this->writeByte(SECTION_POSINTS);
            this->writeUnsigned(this->positives);
          }
          i.second = index++;
          this->writeUnsigned(uint64_t(i.first));
        }
      }
      assert(index == this->positives);
      for (auto& i : this->ivalues) {
        if (i.first < 0) {
          assert(i.second == SIZE_MAX);
          if (index == this->positives) {
            this->writeByte(SECTION_NEGINTS);
            this->writeUnsigned(this->ivalues.size() - this->positives);
          }
          i.second = index++;
          this->writeUnsigned(~uint64_t(i.first));
        }
      }
      assert(index == this->ivalues.size());
    }
    void writeFloats() {
      size_t index = 0;
      for (auto& i : this->fvalues) {
        assert(i.second == SIZE_MAX);
        if (index == 0) {
          this->writeByte(SECTION_FLOATS);
          this->writeUnsigned(this->fvalues.size());
        }
        i.second = index++;
        this->writeUnsigned(this->ivalues[i.first.first]); // mantissa
        this->writeUnsigned(this->ivalues[i.first.second]); // exponent
      }
      assert(index == this->fvalues.size());
    }
    void writeStrings() {
      size_t index = 0;
      for (auto& i : this->svalues) {
        assert(i.second == SIZE_MAX);
        if (index == 0) {
          this->writeByte(SECTION_STRINGS);
          this->writeUnsigned(this->svalues.size());
        }
        i.second = index++;
        this->writeString(i.first);
      }
      assert(index == this->svalues.size());
    }
    void writeString(const String& str) const {
      auto length = str.length();
      if (length > 0) {
        this->stream.write(reinterpret_cast<const char*>(str->begin()), std::streamsize(length));
      }
      this->writeByte(0xFF);
    }
    void writeCode(const INode& node) {
      this->writeByte(SECTION_CODE);
      this->writeNode(node);
    }
    void writeNode(const INode& node) {
      auto opcode = node.getOpcode();
      auto& properties = opcodeProperties(opcode);
      auto n = node.getChildren();
      if ((n < properties.minargs) || (n > properties.maxargs)) {
        throw std::runtime_error("Invalid number of opcode arguments in binary module");
      }
      auto byte = opcode - properties.minargs + std::min(n, size_t(EGG_VM_NARGS));
      assert(byte <= 0xFF);
      this->writeByte(uint8_t(byte));
      if (properties.operand) {
        EGG_WARNING_SUPPRESS_SWITCH_BEGIN
        switch (opcode) {
        case OPCODE_IVALUE:
          this->writeUnsigned(this->ivalues[node.getInt()]);
          break;
        case OPCODE_FVALUE:
          MantissaExponent me;
          me.fromFloat(node.getFloat());
          this->writeUnsigned(this->fvalues[std::make_pair(me.mantissa, me.exponent)]);
          break;
        case OPCODE_SVALUE:
          this->writeUnsigned(this->svalues[node.getString()]);
          break;
        default:
          this->writeUnsigned(uint64_t(node.getInt()));
          break;
        }
        EGG_WARNING_SUPPRESS_SWITCH_END
      }
      auto a = node.getAttributes();
      for (size_t i = 0; i < a; ++i) {
        this->writeNode(node.getAttribute(i));
      }
      for (size_t i = 0; i < n; ++i) {
        this->writeNode(node.getChild(i));
      }
      if (n >= EGG_VM_NARGS) {
        this->writeByte(OPCODE_END);
      }
    }
    void writeUnsigned(uint64_t value) const {
      if (value <= 0x80) {
        // Fast route for small values
        writeByte(uint8_t(value));
        return;
      }
      char buffer[10];
      auto* p = std::end(buffer);
      *(--p) = char(value & 0x7F);
      value >>= 7;
      do {
        assert(p > std::begin(buffer));
        *(--p) = char(value | 0x80);
        value >>= 7;
      } while (value > 0);
      this->stream.write(p, std::end(buffer) - p);
    }
    void writeByte(uint8_t byte) const {
      this->stream.put(char(byte));
    }
  };

  class ModuleDefault : public HardReferenceCounted<IModule> {
    ModuleDefault(const ModuleDefault&) = delete;
    ModuleDefault& operator=(const ModuleDefault&) = delete;
  private:
    Node root;
  public:
    explicit ModuleDefault(IAllocator& allocator)
      : HardReferenceCounted(allocator) {
    }
    ModuleDefault(IAllocator& allocator, INode& root)
      : HardReferenceCounted(allocator),
        root(&root) {
    }
    virtual INode& getRootNode() const override {
      assert(this->root != nullptr);
      return *this->root;
    }
    void readFromStream(std::istream& stream) {
      // Read the abstract syntax tree
      assert(this->root == nullptr);
      ModuleReader reader(this->allocator, stream);
      this->root = reader.read();
      assert(this->root != nullptr);
    }
  };
}

egg::ovum::Module egg::ovum::ModuleFactory::fromBinaryStream(IAllocator& allocator, std::istream& stream) {
  HardPtr<ModuleDefault> module(allocator.create<ModuleDefault>(0, allocator));
  module->readFromStream(stream);
  return module;
}

egg::ovum::Module egg::ovum::ModuleFactory::fromMemory(IAllocator& allocator, const uint8_t* begin, const uint8_t* end) {
  MemoryStream stream(begin, end);
  return ModuleFactory::fromBinaryStream(allocator, stream);
}

egg::ovum::Module egg::ovum::ModuleFactory::fromRootNode(IAllocator& allocator, INode& root) {
  return Module(allocator.create<ModuleDefault>(0, allocator, root));
}

void egg::ovum::ModuleFactory::toBinaryStream(const IModule& module, std::ostream& stream) {
  ModuleWriter writer(stream);
  writer.write(module.getRootNode());
}

egg::ovum::ModuleBuilder::ModuleBuilder(IAllocator& allocator)
  : allocator(allocator) {
}

egg::ovum::Node egg::ovum::ModuleBuilder::createModule(const Node& block) {
  return this->createNode(OPCODE_MODULE, block);
}

egg::ovum::Node egg::ovum::ModuleBuilder::createValueInt(Int value) {
  Nodes attrs;
  std::swap(this->attributes, attrs);
  return NodeFactory::create(this->allocator, OPCODE_IVALUE, nullptr, &attrs, value);
}

egg::ovum::Node egg::ovum::ModuleBuilder::createValueFloat(Float value) {
  Nodes attrs;
  std::swap(this->attributes, attrs);
  return NodeFactory::create(this->allocator, OPCODE_FVALUE, nullptr, &attrs, value);
}

egg::ovum::Node egg::ovum::ModuleBuilder::createValueString(const String& value) {
  Nodes attrs;
  std::swap(this->attributes, attrs);
  return NodeFactory::create(this->allocator, OPCODE_SVALUE, nullptr, &attrs, value);
}

egg::ovum::Node egg::ovum::ModuleBuilder::createValueArray(const Nodes& elements) {
  return this->createNode(OPCODE_AVALUE, elements);
}

egg::ovum::Node egg::ovum::ModuleBuilder::createValueObject(const Nodes& fields) {
  return this->createNode(OPCODE_OVALUE, fields);
}

egg::ovum::Node egg::ovum::ModuleBuilder::createOperator(Opcode opcode, Int op, const Nodes& children) {
  Nodes attrs;
  std::swap(this->attributes, attrs);
  return NodeFactory::create(this->allocator, opcode, &children, &attrs, op);
}

egg::ovum::Node egg::ovum::ModuleBuilder::createNode(Opcode opcode) {
  if (this->attributes.empty()) {
    return NodeFactory::create(this->allocator, opcode);
  }
  Nodes attrs;
  std::swap(this->attributes, attrs);
  return NodeFactory::create(this->allocator, opcode, nullptr, &attrs);
}

egg::ovum::Node egg::ovum::ModuleBuilder::createNode(Opcode opcode, const Node& child0) {
  if (this->attributes.empty()) {
    return NodeFactory::create(this->allocator, opcode, child0);
  }
  Nodes nodes{ child0 };
  Nodes attrs;
  std::swap(this->attributes, attrs);
  return NodeFactory::create(this->allocator, opcode, &nodes, &attrs);
}

egg::ovum::Node egg::ovum::ModuleBuilder::createNode(Opcode opcode, const Node& child0, const Node& child1) {
  if (this->attributes.empty()) {
    return NodeFactory::create(this->allocator, opcode, child0, child1);
  }
  Nodes nodes{ child0, child1 };
  Nodes attrs;
  std::swap(this->attributes, attrs);
  return NodeFactory::create(this->allocator, opcode, &nodes, &attrs);
}

egg::ovum::Node egg::ovum::ModuleBuilder::createNode(Opcode opcode, const Node& child0, const Node& child1, const Node& child2) {
  if (this->attributes.empty()) {
    return NodeFactory::create(this->allocator, opcode, child0, child1, child2);
  }
  Nodes nodes{ child0, child1, child2 };
  Nodes attrs;
  std::swap(this->attributes, attrs);
  return NodeFactory::create(this->allocator, opcode, &nodes, &attrs);
}

egg::ovum::Node egg::ovum::ModuleBuilder::createNode(Opcode opcode, const Node& child0, const Node& child1, const Node& child2, const Node& child3) {
  if (this->attributes.empty()) {
    return NodeFactory::create(this->allocator, opcode, child0, child1, child2, child3);
  }
  Nodes nodes{ child0, child1, child2, child3 };
  Nodes attrs;
  std::swap(this->attributes, attrs);
  return NodeFactory::create(this->allocator, opcode, &nodes, &attrs);
}

egg::ovum::Node egg::ovum::ModuleBuilder::createNode(Opcode opcode, const Nodes& children) {
  if (this->attributes.empty()) {
    return NodeFactory::create(this->allocator, opcode, children);
  }
  Nodes attrs;
  std::swap(this->attributes, attrs);
  return NodeFactory::create(this->allocator, opcode, &children, &attrs);
}
