#include "ovum/ovum.h"
#include "ovum/node.h"
#include "ovum/module.h"
#include "ovum/utf.h"

#include <algorithm>

namespace {
  using namespace egg::ovum;

  struct OpcodeTable {
    static const OpcodeTable instance;
    Opcode opcode[256];
    OpcodeProperties properties[256];
  private:
    OpcodeTable() {
      std::memset(this->opcode, -1, sizeof(this->opcode));
      std::memset(this->properties, 0, sizeof(this->properties));
      // cppcheck-suppress unreadVariable
      const size_t N = EGG_VM_NARGS; // used in the macro below
#define EGG_VM_OPCODES_TABLE(opcode, minbyte, minargs, maxargs, text) this->fill(opcode, minargs, maxargs, text);
      EGG_VM_OPCODES(EGG_VM_OPCODES_TABLE)
#undef EGG_VM_OPCODES_TABLE
    }
    // cppcheck-suppress unusedPrivateFunction
    void fill(Opcode code, size_t minargs, size_t maxargs, const char* text) {
      assert(code != OPCODE_reserved);
      assert(text != nullptr);
      assert(minargs <= maxargs);
      assert(maxargs <= EGG_VM_NARGS);
      assert((code >= 0x00) && (code <= 0xFF));
      auto& prop = this->properties[code];
      assert(prop.name == 0);
      prop.name = text;
      prop.minargs = minargs;
      prop.maxargs = (maxargs < EGG_VM_NARGS) ? maxargs : SIZE_MAX;
      prop.minbyte = uint8_t(code);
      prop.maxbyte = uint8_t(code + maxargs - minargs);
      prop.operand = (code < EGG_VM_ISTART);
      auto index = size_t(code);
      while (minargs++ <= maxargs) {
        assert(index <= 0xFF);
        assert(this->opcode[index] == OPCODE_reserved);
        this->opcode[index++] = code;
      }
    }
  };
  const OpcodeTable OpcodeTable::instance{};

  struct OperatorTable {
    static const OperatorTable instance;
    OperatorProperties properties[129];
  private:
    OperatorTable() {
      std::memset(this->properties, 0, sizeof(this->properties));
#define EGG_VM_OPERATORS_TABLE(oper, opclass, index, text) this->fill(oper, opclass, index, text);
      EGG_VM_OPERATORS(EGG_VM_OPERATORS_TABLE)
#undef EGG_VM_OPERATORS_TABLE
    }
    // cppcheck-suppress unusedPrivateFunction
    void fill(Operator oper, Opclass opclass, size_t index, const char* text) {
      assert(text != nullptr);
      assert((oper >= 0x00) && (oper <= 0x80)); // sic [0..128] inclusive
      auto& prop = this->properties[oper];
      assert(prop.name == 0);
      prop.name = text;
      prop.opclass = opclass;
      prop.index = index;
      prop.operands = 1 + size_t(oper) / EGG_VM_OOSTEP;
      switch (opclass) {
      case OPCLASS_UNARY:
        assert(prop.operands == 1);
        break;
      case OPCLASS_BINARY:
      case OPCLASS_COMPARE:
        assert(prop.operands == 2);
        break;
      case OPCLASS_TERNARY:
        assert(prop.operands == 3);
        break;
      default:
        assert(false);
        break;
      }
    }
  };
  const OperatorTable OperatorTable::instance{};

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
    const OpcodeProperties& attributeProperties;
  public:
    ModuleReader(IAllocator& allocator, std::istream& stream)
      : allocator(allocator), stream(stream), attributeProperties(OpcodeProperties::from(OPCODE_ATTRIBUTE)) {
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
          root = this->readNode(false);
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
      auto length = UTF8::sizeFromLeadByte(byte);
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
    Node readNode(bool insideAttribute) const {
      auto byte = this->readByte();
      auto opcode = Module::opcodeFromMachineByte(byte);
      if (opcode == OPCODE_reserved) {
        throw std::runtime_error("Invalid opcode in code section of binary module");
      }
      auto operand = std::numeric_limits<uint64_t>::max();
      auto& properties = OpcodeProperties::from(opcode);
      if (properties.operand) {
        operand = this->readUnsigned();
      }
      std::vector<Node> attributes;
      if (!insideAttribute) {
        // Attributes cannot have attributes!
        while (this->isAttribute(this->stream.peek())) {
          attributes.push_back(this->readNode(true));
        }
      }
      std::vector<Node> children;
      auto count = Module::childrenFromMachineByte(byte);
      if (!properties.validate(count, properties.operand)) {
        throw std::runtime_error("Invalid number of node children in binary module");
      }
      if (count == SIZE_MAX) {
        // This is a list terminated with an OPCODE_END sentinel
        while (this->stream.peek() != OPCODE_END) {
          children.push_back(this->readNode(insideAttribute));
        }
        this->stream.get(); // skip the sentinel
      } else {
        for (size_t child = 0; child < count; ++child) {
          children.push_back(this->readNode(insideAttribute));
        }
      }
      if (!properties.operand) {
        // No operand
        return NodeFactory::create(this->allocator, opcode, &children, &attributes);
      }
      EGG_WARNING_SUPPRESS_SWITCH_BEGIN();
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
      EGG_WARNING_SUPPRESS_SWITCH_END();
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
    bool isAttribute(int peek) const {
      return (peek >= this->attributeProperties.minbyte) && (peek <= this->attributeProperties.maxbyte);
    }
  };

  class ModuleWriter {
    ModuleWriter(const ModuleWriter&) = delete;
    ModuleWriter& operator=(const ModuleWriter&) = delete;
  private:
    const INode& root;
    std::map<Int, size_t> ivalues;
    std::map<std::pair<Int, Int>, size_t> fvalues;
    std::map<String, size_t> svalues;
    size_t positives;
  public:
    explicit ModuleWriter(const INode& root)
      : root(root), positives(0) {
      this->findConstants(this->root);
      this->prepareInts();
      this->prepareFloats();
      this->prepareStrings();
    }
    template<typename TARGET>
    void write(TARGET& target) const {
      this->writeMagic(target);
      this->writeInts(target);
      this->writeFloats(target);
      this->writeStrings(target);
      this->writeCode(target, this->root);
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
      case INode::Operand::Operator:
        // Keep track of the integer operators
        this->foundInt(Int(node.getOperator()));
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
    template<typename TARGET>
    void writeMagic(TARGET& target) const {
#define EGG_VM_MAGIC_BYTE(byte) this->writeByte(target, byte);
      EGG_VM_MAGIC(EGG_VM_MAGIC_BYTE)
#undef EGG_VM_MAGIC_BYTE
    }
    void prepareInts() {
      size_t index = 0;
      for (auto& i : this->ivalues) {
        if (i.first >= 0) {
          assert(i.second == SIZE_MAX);
          i.second = index++;
        }
      }
      assert(index == this->positives);
      for (auto& i : this->ivalues) {
        if (i.first < 0) {
          assert(i.second == SIZE_MAX);
          i.second = index++;
        }
      }
      assert(index == this->ivalues.size());
    }
    template<typename TARGET>
    void writeInts(TARGET& target) const {
      size_t index = 0;
      for (auto& i : this->ivalues) {
        assert(i.second != SIZE_MAX);
        if (i.first >= 0) {
          if (index++ == 0) {
            this->writeByte(target, SECTION_POSINTS);
            this->writeUnsigned(target, this->positives);
          }
          this->writeUnsigned(target, uint64_t(i.first));
        }
      }
      assert(index == this->positives);
      for (auto& i : this->ivalues) {
        if (i.first < 0) {
          assert(i.second != SIZE_MAX);
          if (index++ == this->positives) {
            this->writeByte(target, SECTION_NEGINTS);
            this->writeUnsigned(target, this->ivalues.size() - this->positives);
          }
          this->writeUnsigned(target, ~uint64_t(i.first));
        }
      }
      assert(index == this->ivalues.size());
    }
    void prepareFloats() {
      size_t index = 0;
      for (auto& i : this->fvalues) {
        assert(i.second == SIZE_MAX);
        i.second = index++;
      }
      assert(index == this->fvalues.size());
    }
    template<typename TARGET>
    void writeFloats(TARGET& target) const {
      size_t index = 0;
      for (auto& i : this->fvalues) {
        assert(i.second != SIZE_MAX);
        if (index++ == 0) {
          this->writeByte(target, SECTION_FLOATS);
          this->writeUnsigned(target, this->fvalues.size());
        }
        this->writeUnsigned(target, this->ivalues.at(i.first.first)); // mantissa
        this->writeUnsigned(target, this->ivalues.at(i.first.second)); // exponent
      }
      assert(index == this->fvalues.size());
    }
    void prepareStrings() {
      size_t index = 0;
      for (auto& i : this->svalues) {
        assert(i.second == SIZE_MAX);
        i.second = index++;
      }
      assert(index == this->svalues.size());
    }
    template<typename TARGET>
    void writeStrings(TARGET& target) const {
      size_t index = 0;
      for (auto& i : this->svalues) {
        assert(i.second != SIZE_MAX);
        if (index++ == 0) {
          this->writeByte(target, SECTION_STRINGS);
          this->writeUnsigned(target, this->svalues.size());
        }
        this->writeString(target, i.first);
      }
      assert(index == this->svalues.size());
    }
    template<typename TARGET>
    void writeString(TARGET& target, const String& str) const {
      if (!str.empty()) {
        this->writeBytes(target, str->begin(), str->bytes());
      }
      this->writeByte(target, 0xFF);
    }
    template<typename TARGET>
    void writeCode(TARGET& target, const INode& node) const {
      this->writeByte(target, SECTION_CODE);
      this->writeNode(target, node);
    }
    template<typename TARGET>
    void writeNode(TARGET& target, const INode& node) const {
      auto opcode = node.getOpcode();
      auto& properties = OpcodeProperties::from(opcode);
      auto n = node.getChildren();
      if ((n < properties.minargs) || (n > properties.maxargs)) {
        throw std::runtime_error("Invalid number of opcode arguments in binary module");
      }
      auto byte = opcode - properties.minargs + std::min(n, size_t(EGG_VM_NARGS));
      assert(byte <= 0xFF);
      this->writeByte(target, uint8_t(byte));
      if (properties.operand) {
        EGG_WARNING_SUPPRESS_SWITCH_BEGIN();
        switch (opcode) {
        case OPCODE_IVALUE:
          this->writeUnsigned(target, this->ivalues.at(node.getInt()));
          break;
        case OPCODE_FVALUE:
          MantissaExponent me;
          me.fromFloat(node.getFloat());
          this->writeUnsigned(target, this->fvalues.at(std::make_pair(me.mantissa, me.exponent)));
          break;
        case OPCODE_SVALUE:
          this->writeUnsigned(target, this->svalues.at(node.getString()));
          break;
        default:
          if (node.getOperand() == INode::Operand::Operator) {
            this->writeUnsigned(target, uint64_t(node.getOperator()));
          } else {
            this->writeUnsigned(target, uint64_t(node.getInt()));
          }
          break;
        }
        EGG_WARNING_SUPPRESS_SWITCH_END();
      }
      auto a = node.getAttributes();
      for (size_t i = 0; i < a; ++i) {
        this->writeNode(target, node.getAttribute(i));
      }
      for (size_t i = 0; i < n; ++i) {
        this->writeNode(target, node.getChild(i));
      }
      if (n >= EGG_VM_NARGS) {
        this->writeByte(target, OPCODE_END);
      }
    }
    template<typename TARGET>
    void writeUnsigned(TARGET& target, uint64_t value) const {
      if (value <= 0x80) {
        // Fast route for small values
        writeByte(target, uint8_t(value));
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
      this->writeBytes(target, p, size_t(std::end(buffer) - p));
    }
    // Target is an unsigned integer (for measuring)
    void writeBytes(size_t& target, const void*, size_t bytes) const {
      target += bytes;
    }
    void writeByte(size_t& target, uint8_t) const {
      target++;
    }
    // Target is direct memory
    void writeBytes(uint8_t*& target, const void* data, size_t bytes) const {
      std::memcpy(target, data, bytes);
      target += bytes;
    }
    void writeByte(uint8_t*& target, uint8_t byte) const {
      *target++ = byte;
    }
    // Target is an output stream
    void writeBytes(std::ostream& target, const void* data, size_t bytes) const {
      target.write(static_cast<const char*>(data), std::streamsize(bytes));
    }
    void writeByte(std::ostream& target, uint8_t byte) const {
      target.put(char(byte));
    }
  };

  class ModuleDefault : public HardReferenceCounted<IModule> {
    ModuleDefault(const ModuleDefault&) = delete;
    ModuleDefault& operator=(const ModuleDefault&) = delete;
  private:
    String resource;
    Node root;
  public:
    ModuleDefault(IAllocator& allocator, const String& resource, INode* root = nullptr)
      : HardReferenceCounted(allocator, 0),
        resource(resource),
        root(root) {
    }
    virtual String getResourceName() const override {
      return this->resource;
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

egg::ovum::Opcode egg::ovum::Module::opcodeFromMachineByte(uint8_t byte) {
  return OpcodeTable::instance.opcode[byte];
}

const egg::ovum::OpcodeProperties& egg::ovum::OpcodeProperties::from(Opcode opcode) {
  assert((opcode >= 1) && (opcode <= 255));
  return OpcodeTable::instance.properties[opcode];
}

std::string egg::ovum::OpcodeProperties::str(Opcode opcode) {
  if ((opcode >= 1) && (opcode <= 255)) {
    auto& props = OpcodeTable::instance.properties[opcode];
    if (props.name != nullptr) {
      return props.name;
    }
  }
  return "<unknown:" + std::to_string(int(opcode)) + ">";
}

const egg::ovum::OperatorProperties& egg::ovum::OperatorProperties::from(Operator oper) {
  assert((oper >= 0) && (oper <= 128));
  return OperatorTable::instance.properties[oper];
}

std::string egg::ovum::OperatorProperties::str(Operator oper) {
  if ((oper >= 1) && (oper <= 128)) {
    auto& props = OperatorTable::instance.properties[oper];
    if (props.name != nullptr) {
      return props.name;
    }
  }
  return "<unknown:" + std::to_string(int(oper)) + ">";
}

egg::ovum::Module egg::ovum::ModuleFactory::fromBinaryStream(IAllocator& allocator, const String& resource, std::istream& stream) {
  auto module = allocator.make<ModuleDefault>(resource);
  module->readFromStream(stream);
  return Module(module.get());
}

egg::ovum::Module egg::ovum::ModuleFactory::fromMemory(IAllocator& allocator, const String& resource, const uint8_t* begin, const uint8_t* end) {
  MemoryStream stream(begin, end);
  return ModuleFactory::fromBinaryStream(allocator, resource, stream);
}

egg::ovum::Module egg::ovum::ModuleFactory::fromRootNode(IAllocator& allocator, const String& resource, INode& root) {
  return allocator.make<ModuleDefault, Module>(resource, &root);
}

void egg::ovum::ModuleFactory::toBinaryStream(const IModule& module, std::ostream& stream) {
  ModuleWriter writer(module.getRootNode());
  writer.write(stream);
}

egg::ovum::Memory egg::ovum::ModuleFactory::toMemory(IAllocator& allocator, const IModule& module) {
  ModuleWriter writer(module.getRootNode());
  // First, measure the size required
  size_t bytes = 0;
  writer.write(bytes);
  auto memory = MemoryFactory::createMutable(allocator, bytes);
  // Finally, write the data bytes
  uint8_t* data = memory.begin();
  writer.write(data);
  assert(data == memory.end());
  return memory.build();
}

egg::ovum::ModuleBuilderBase::ModuleBuilderBase(IAllocator& allocator)
  : allocator(allocator) {
}

void egg::ovum::ModuleBuilderBase::addAttribute(const String& key, Node&& value) {
  assert(value != nullptr);
  auto name = NodeFactory::create(this->allocator, egg::ovum::OPCODE_SVALUE, nullptr, nullptr, key);
  auto attr = NodeFactory::create(this->allocator, egg::ovum::OPCODE_ATTRIBUTE, std::move(name), std::move(value));
  this->attributes.emplace_back(std::move(attr));
}

egg::ovum::Node egg::ovum::ModuleBuilderBase::createModule(Node&& block) {
  return this->createNode(OPCODE_MODULE, std::move(block));
}

egg::ovum::Node egg::ovum::ModuleBuilderBase::createValueInt(Int value) {
  Nodes attrs;
  std::swap(this->attributes, attrs);
  return NodeFactory::create(this->allocator, OPCODE_IVALUE, nullptr, &attrs, value);
}

egg::ovum::Node egg::ovum::ModuleBuilderBase::createValueFloat(Float value) {
  Nodes attrs;
  std::swap(this->attributes, attrs);
  return NodeFactory::create(this->allocator, OPCODE_FVALUE, nullptr, &attrs, value);
}

egg::ovum::Node egg::ovum::ModuleBuilderBase::createValueString(const String& value) {
  Nodes attrs;
  std::swap(this->attributes, attrs);
  return NodeFactory::create(this->allocator, OPCODE_SVALUE, nullptr, &attrs, value);
}

egg::ovum::Node egg::ovum::ModuleBuilderBase::createValueArray(const Nodes& elements) {
  return this->createNode(OPCODE_AVALUE, elements);
}

egg::ovum::Node egg::ovum::ModuleBuilderBase::createValueObject(const Nodes& fields) {
  return this->createNode(OPCODE_OVALUE, fields);
}

egg::ovum::Node egg::ovum::ModuleBuilderBase::createOperator(Opcode opcode, Operator oper, const Nodes& children) {
  assert(OpcodeProperties::from(opcode).validate(children.size(), true));
  assert(OperatorProperties::from(oper).validate(children.size()));
  Nodes attrs;
  std::swap(this->attributes, attrs);
  return NodeFactory::create(this->allocator, opcode, &children, &attrs, Int(oper));
}

egg::ovum::Node egg::ovum::ModuleBuilderBase::createNode(Opcode opcode) {
  if (this->attributes.empty()) {
    return NodeFactory::create(this->allocator, opcode);
  }
  Nodes attrs;
  std::swap(this->attributes, attrs);
  return NodeFactory::create(this->allocator, opcode, nullptr, &attrs);
}

egg::ovum::Node egg::ovum::ModuleBuilderBase::createNode(Opcode opcode, Node&& child0) {
  if (this->attributes.empty()) {
    return NodeFactory::create(this->allocator, opcode, std::move(child0));
  }
  Nodes nodes{ std::move(child0) };
  Nodes attrs;
  std::swap(this->attributes, attrs);
  return NodeFactory::create(this->allocator, opcode, &nodes, &attrs);
}

egg::ovum::Node egg::ovum::ModuleBuilderBase::createNode(Opcode opcode, Node&& child0, Node&& child1) {
  if (this->attributes.empty()) {
    return NodeFactory::create(this->allocator, opcode, std::move(child0), std::move(child1));
  }
  Nodes nodes{ std::move(child0), std::move(child1) };
  return ModuleBuilderBase::createNodeWithAttributes(opcode, &nodes);
}

egg::ovum::Node egg::ovum::ModuleBuilderBase::createNode(Opcode opcode, Node&& child0, Node&& child1, Node&& child2) {
  if (this->attributes.empty()) {
    return NodeFactory::create(this->allocator, opcode, std::move(child0), std::move(child1), std::move(child2));
  }
  Nodes nodes{ std::move(child0), std::move(child1), std::move(child2) };
  return ModuleBuilderBase::createNodeWithAttributes(opcode, &nodes);
}

egg::ovum::Node egg::ovum::ModuleBuilderBase::createNode(Opcode opcode, Node&& child0, Node&& child1, Node&& child2, Node&& child3) {
  if (this->attributes.empty()) {
    return NodeFactory::create(this->allocator, opcode, std::move(child0), std::move(child1), std::move(child2), std::move(child3));
  }
  Nodes nodes{ std::move(child0), std::move(child1), std::move(child2), std::move(child3) };
  return ModuleBuilderBase::createNodeWithAttributes(opcode, &nodes);
}

egg::ovum::Node egg::ovum::ModuleBuilderBase::createNode(Opcode opcode, const Nodes& children) {
  if (this->attributes.empty()) {
    return NodeFactory::create(this->allocator, opcode, children);
  }
  return ModuleBuilderBase::createNodeWithAttributes(opcode, &children);
}

egg::ovum::Node egg::ovum::ModuleBuilderBase::createNodeWithAttributes(Opcode opcode, const Nodes* children) {
  assert(!this->attributes.empty());
  Nodes attrs;
  std::swap(this->attributes, attrs);
  return NodeFactory::create(this->allocator, opcode, children, &attrs);
}
