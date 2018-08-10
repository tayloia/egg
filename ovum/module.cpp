#include "ovum/ovum.h"
#include "ovum/ast.h"
#include "ovum/utf8.h"

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

  class ModuleDefault : public HardReferenceCounted<IModule> {
    ModuleDefault(const ModuleDefault&) = delete;
    ModuleDefault& operator=(const ModuleDefault&) = delete;
  private:
    std::vector<Int> ivalue;
    std::vector<Float> fvalue;
    std::vector<String> svalue;
  public:
    explicit ModuleDefault(IAllocator& allocator)
      : HardReferenceCounted(allocator) {
    }
    void readFromStream(std::istream& stream) {
      enum class Section {
#define EGG_VM_SECTIONS_ENUM(section, value) section = value,
        EGG_VM_SECTIONS(EGG_VM_SECTIONS_ENUM)
#undef EGG_VM_SECTIONS_ENUM
      };
      if (!readMagic(stream)) {
        throw std::runtime_error("Invalid magic signature in binary module");
      }
      char ch;
      while (stream.get(ch)) {
        switch (Section(uint8_t(ch))) {
        case Section::SECTION_MAGIC:
          throw std::runtime_error("Duplicated magic section in binary module");
        case Section::SECTION_POSINTS:
          readInts(stream, false);
          break;
        case Section::SECTION_NEGINTS:
          readInts(stream, true);
          break;
        case Section::SECTION_FLOATS:
          readFloats(stream);
          break;
        case Section::SECTION_STRINGS:
          readStrings(stream);
          break;
        case Section::SECTION_CODE:
          readCode(stream);
          if (!stream.get(ch)) {
            // No source section
            return;
          }
          if (Section(uint8_t(ch)) != Section::SECTION_SOURCE) {
            throw std::runtime_error("Only source sections can follow code sections in binary module");
          }
          return;
        case Section::SECTION_SOURCE:
          throw std::runtime_error("Source section without preceding code section in binary module");
        default:
          throw std::runtime_error("Unrecognized section in binary module");
        }
      }
      throw std::runtime_error("Missing code section in binary module");
    }
  private:
    static bool readMagic(std::istream& stream) {
#define EGG_VM_MAGIC_BYTE(byte) if (readByte(stream) != byte) { return false; }
      EGG_VM_MAGIC(EGG_VM_MAGIC_BYTE)
#undef EGG_VM_MAGIC_BYTE
      return true;
    }
    void readInts(std::istream& stream, bool negative) {
      // Read a sequence of 64-bit signed values
      auto count = readUnsigned(stream);
      this->ivalue.reserve(this->ivalue.size() + size_t(count));
      while (count > 0) {
        this->ivalue.push_back(readInt(stream, negative));
        count--;
      }
    }
    static Int readInt(std::istream& stream, bool negative) {
      // Read a single 64-bit signed value
      auto i = Int(readUnsigned(stream));
      if (negative) {
        i = ~i;
      }
      return i;
    }
    void readFloats(std::istream& stream) {
      // Read a sequence of 64-bit floating-point values
      auto count = readUnsigned(stream);
      this->fvalue.reserve(this->fvalue.size() + size_t(count));
      while (count > 0) {
        this->fvalue.push_back(readFloat(stream));
        count--;
      }
    }
    Float readFloat(std::istream& stream) {
      // Read a single 64-bit floating-point value
      ast::MantissaExponent me;
      me.mantissa = indexInt(stream);
      me.exponent = indexInt(stream);
      return me.toFloat();
    }
    void readStrings(std::istream& stream) {
      // Read a sequence of string values
      auto count = readUnsigned(stream);
      this->svalue.reserve(this->svalue.size() + size_t(count));
      while (count > 0) {
        this->svalue.push_back(readString(stream));
        count--;
      }
    }
    String readString(std::istream& stream) {
      size_t codepoints = 0;
      std::stringstream ss;
      while (readCodePoint(stream, ss)) {
        codepoints++;
      }
      return String(ss.str(), codepoints);
    }
    static bool readCodePoint(std::istream& in, std::ostream& out) {
      char ch;
      if (!in.get(ch)) {
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
        if (!in.get(ch)) {
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
    void readCode(std::istream& stream) {
      // WIBBLE
      (void)stream;
    }
    Int indexInt(std::istream& stream) const {
      auto index = readUnsigned(stream);
      if (index >= this->ivalue.size()) {
        throw std::runtime_error("Invalid integer constant index in binary module");
      }
      return this->ivalue.at(index);
    }
    static uint64_t readUnsigned(std::istream& stream) {
      // Read up to 63 bits as an unsigned integer
      auto byte = readByte(stream);
      if (byte <= 0x80) {
        // Fast return for small values
        return byte;
      }
      uint64_t result = byte;
      size_t bits = 0;
      while (byte >= 0x80) {
        byte = readByte(stream);
        bits += 7;
        if (bits > 63) {
          throw std::runtime_error("Unsigned integer overflow in binary module");
        }
        result = ((result - 0x80) << 7) + byte;
      }
      assert(result < 0x8000000000000000);
      return result;
    }
    static uint8_t readByte(std::istream& stream) {
      // Read a single 8-bit unsigned integer
      char ch;
      if (!stream.get(ch)) {
        throw std::runtime_error("Truncated section in binary module");
      }
      return uint8_t(ch);
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
