#include "ovum/ovum.h"
#include "ovum/ast.h"
#include "ovum/utf8.h"

#include <map>

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
    ast::Node read() {
      ast::Node root;
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
      ast::MantissaExponent me;
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
      out.put(ch);
      auto byte = uint8_t(ch);
      if (byte < 0x80) {
        // Fast code path for ASCII
        return true;
      }
      if (byte == 0xFF) {
        // String terminal
        return false;
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
    ast::Node readNode() const {
      auto byte = this->readByte();
      auto opcode = ast::opcodeFromMachineByte(byte);
      if (opcode == OPCODE_reserved) {
        throw std::runtime_error("Invalid opcode in code section of binary module");
      }
      auto operand = std::numeric_limits<uint64_t>::max();
      auto& properties = ast::opcodeProperties(opcode);
      if (properties.operand) {
        operand = this->readUnsigned();
      }
      std::vector<ast::Node> attributes;
      while (this->stream.peek() == OPCODE_ATTRIBUTE) {
        attributes.push_back(this->readNode());
      }
      std::vector<ast::Node> children;
      auto count = ast::childrenFromMachineByte(byte);
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
        return ast::NodeFactory::create(this->allocator, opcode, std::move(children), std::move(attributes));
      }
      EGG_WARNING_SUPPRESS_SWITCH_BEGIN
      switch (opcode) {
      case OPCODE_IVALUE:
        // Operand is an index into the int table
        return ast::NodeFactory::create(this->allocator, opcode, std::move(children), std::move(attributes), this->indexInt(operand));
      case OPCODE_FVALUE:
        // Operand is an index into the float table
        return ast::NodeFactory::create(this->allocator, opcode, std::move(children), std::move(attributes), this->indexFloat(operand));
      case OPCODE_SVALUE:
        // Operand is an index into the string table
        return ast::NodeFactory::create(this->allocator, opcode, std::move(children), std::move(attributes), this->indexString(operand));
      }
      EGG_WARNING_SUPPRESS_SWITCH_END
      // Operand is probably an operator index
      return ast::NodeFactory::create(this->allocator, opcode, std::move(children), std::move(attributes), Int(operand));
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
    std::map<String, size_t> svalues;
    size_t positives;
  public:
    explicit ModuleWriter(std::ostream& stream)
      : stream(stream), positives(0) {
    }
    void write(const ast::INode& root) {
      this->findConstants(root);
      this->writeMagic();
    }
  private:
    void findConstants(const ast::INode& node) {
      switch (node.getOperand()) {
      case ast::INode::Operand::Int:
        // Keep track of the integers
        this->foundInt(node.getInt());
        break;
      case ast::INode::Operand::Float:
        // Keep track of the mantissa/exponent integers
        this->foundFloat(node.getFloat());
        break;
      case ast::INode::Operand::String:
        // Keep track of the strings
        this->foundString(node.getString());
        break;
      case ast::INode::Operand::None:
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
      auto inserted = this->ivalues.emplace(value, size_t(0));
      if (inserted.second && (value >= 0)) {
        this->positives++;
      }
    }
    void foundFloat(Float value) {
      ast::MantissaExponent me;
      me.fromFloat(value);
      foundInt(me.mantissa);
      foundInt(me.exponent);
      auto key = std::make_pair(me.mantissa, me.exponent);
      this->fvalues.emplace(key, this->fvalues.size());
    }
    void foundString(String value) {
      this->svalues.emplace(value, this->svalues.size());
    }
    void writeMagic() const {
#define EGG_VM_MAGIC_BYTE(byte) this->writeByte(byte);
      EGG_VM_MAGIC(EGG_VM_MAGIC_BYTE)
#undef EGG_VM_MAGIC_BYTE
    }
    void writeByte(uint8_t byte) const {
      this->stream.put(char(byte));
    }
  };

  class ModuleDefault : public HardReferenceCounted<IModule> {
    ModuleDefault(const ModuleDefault&) = delete;
    ModuleDefault& operator=(const ModuleDefault&) = delete;
  private:
    ast::Node root;
  public:
    explicit ModuleDefault(IAllocator& allocator)
      : HardReferenceCounted(allocator) {
    }
    ModuleDefault(IAllocator& allocator, ast::INode& root)
      : HardReferenceCounted(allocator),
        root(&root) {
    }
    virtual ast::INode& getRootNode() const override {
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

egg::ovum::Module egg::ovum::ModuleFactory::fromRootNode(IAllocator& allocator, ast::INode& root) {
  return Module(allocator.create<ModuleDefault>(0, allocator, root));
}

void egg::ovum::ModuleFactory::toBinaryStream(const IModule& module, std::ostream& stream) {
  ModuleWriter writer(stream);
  writer.write(module.getRootNode());
}

egg::ovum::ast::ModuleBuilder::ModuleBuilder(IAllocator& allocator)
  : allocator(allocator) {
}

egg::ovum::ast::Node egg::ovum::ast::ModuleBuilder::createModule(Node&& block) {
  return this->createNode(OPCODE_MODULE, { std::move(block) });
}

egg::ovum::ast::Node egg::ovum::ast::ModuleBuilder::createBlock(Nodes&& statements) {
  return this->createNode(OPCODE_BLOCK, std::move(statements));
}

egg::ovum::ast::Node egg::ovum::ast::ModuleBuilder::createNoop() {
  return this->createNode(OPCODE_NOOP, {});
}

egg::ovum::ast::Node egg::ovum::ast::ModuleBuilder::createValueArray(Nodes&& elements) {
  return this->createNode(OPCODE_AVALUE, std::move(elements));
}

egg::ovum::ast::Node egg::ovum::ast::ModuleBuilder::createValueInt(Int value) {
  Nodes attrs;
  std::swap(this->attributes, attrs);
  return NodeFactory::create(this->allocator, OPCODE_IVALUE, {}, std::move(attrs), value);
}

egg::ovum::ast::Node egg::ovum::ast::ModuleBuilder::createValueFloat(Float value) {
  Nodes attrs;
  std::swap(this->attributes, attrs);
  return NodeFactory::create(this->allocator, OPCODE_FVALUE, {}, std::move(attrs), value);
}

egg::ovum::ast::Node egg::ovum::ast::ModuleBuilder::createValueString(String value) {
  Nodes attrs;
  std::swap(this->attributes, attrs);
  return NodeFactory::create(this->allocator, OPCODE_SVALUE, {}, std::move(attrs), value);
}

egg::ovum::ast::Node egg::ovum::ast::ModuleBuilder::createNode(Opcode opcode, Nodes&& children) {
  if (this->attributes.empty()) {
    return NodeFactory::create(this->allocator, opcode, std::move(children));
  }
  Nodes attrs;
  std::swap(this->attributes, attrs);
  return NodeFactory::create(this->allocator, opcode, std::move(children), std::move(attrs));
}
