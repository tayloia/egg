#include "ovum/test.h"
#include "ovum/node.h"
#include "ovum/module.h"

#define EGG_VM_MAGIC_BYTE(byte) byte,
#define MAGIC EGG_VM_MAGIC(EGG_VM_MAGIC_BYTE)

using namespace egg::ovum;

namespace {
  void expectFailureFromMemory(const uint8_t memory[], size_t bytes, const char* needle) {
    egg::test::Allocator allocator;
    ASSERT_THROW_E(ModuleFactory::fromMemory(allocator, "<memory>", memory, memory + bytes), std::runtime_error, ASSERT_STARTSWITH(e.what(), needle));
  }
  void toModuleArray(ModuleBuilder& builder, const Nodes& avalues, Module& out) {
    // Create a module that just constructs an array of values
    auto array = builder.createValueArray(avalues);
    ASSERT_NE(nullptr, array);
    auto block = builder.createNode(Opcode::BLOCK, std::move(array));
    ASSERT_NE(nullptr, block);
    auto module = builder.createModule(std::move(block));
    ASSERT_NE(nullptr, module);
    out = ModuleFactory::fromRootNode(builder.allocator, "<resource>", *module);
  }
  void toModuleMemoryArray(ModuleBuilder& builder, const Nodes& avalues, std::ostream& out) {
    // Create a module memory image that just constructs an array of values
    Module module;
    toModuleArray(builder, avalues, module);
    ModuleFactory::toBinaryStream(*module, out);
  }
  void fromModuleArray(const Module& in, Node& avalue) {
    // Extract an array of values from a module
    ASSERT_NE(nullptr, in);
    Node root{ &in->getRootNode() };
    ASSERT_EQ(Opcode::MODULE, root->getOpcode());
    ASSERT_EQ(1u, root->getChildren());
    Node child{ &root->getChild(0) };
    ASSERT_EQ(Opcode::BLOCK, child->getOpcode());
    ASSERT_EQ(1u, child->getChildren());
    avalue.set(&child->getChild(0));
    ASSERT_EQ(Opcode::AVALUE, avalue->getOpcode());
  }
  void fromModuleMemoryArray(IAllocator& allocator, std::istream& in, Node& avalue) {
    // Extract an array of values from a module memory image
    in.clear();
    ASSERT_TRUE(in.seekg(0).good());
    auto module = ModuleFactory::fromBinaryStream(allocator, "<memory>", in);
    fromModuleArray(module, avalue);
  }
  Node roundTripArray(ModuleBuilder& builder, const Nodes& avalues) {
    // Create a module memory image and then extract the array values
    std::stringstream ss;
    toModuleMemoryArray(builder, avalues, ss);
    Node avalue;
    fromModuleMemoryArray(builder.allocator, ss, avalue);
    return avalue;
  }
}

TEST(TestModule, ChildrenFromMachineByte) {
  ASSERT_EQ(0u, Module::childrenFromMachineByte(0));
  ASSERT_EQ(1u, Module::childrenFromMachineByte(1));
  ASSERT_EQ(2u, Module::childrenFromMachineByte(2));
  ASSERT_EQ(3u, Module::childrenFromMachineByte(3));
  ASSERT_EQ(4u, Module::childrenFromMachineByte(4));
  ASSERT_EQ(SIZE_MAX, Module::childrenFromMachineByte(5));
  ASSERT_EQ(0u, Module::childrenFromMachineByte(6));
  ASSERT_EQ(4u, Module::childrenFromMachineByte(250));
  ASSERT_EQ(SIZE_MAX, Module::childrenFromMachineByte(251));
  ASSERT_EQ(0u, Module::childrenFromMachineByte(252));
  ASSERT_EQ(1u, Module::childrenFromMachineByte(253));
  ASSERT_EQ(2u, Module::childrenFromMachineByte(254));
  ASSERT_EQ(3u, Module::childrenFromMachineByte(255));
}

TEST(TestModule, OpcodeFromMachineByte) {
  // Taken from egg-notes.xlsx
  ASSERT_EQ(Opcode::END, Module::opcodeFromMachineByte(0));
  ASSERT_EQ(Opcode::UNARY, Module::opcodeFromMachineByte(1));
  ASSERT_EQ(Opcode::BINARY, Module::opcodeFromMachineByte(2));
  ASSERT_EQ(Opcode::TERNARY, Module::opcodeFromMachineByte(3));
  ASSERT_EQ(Opcode::reserved, Module::opcodeFromMachineByte(4));
  ASSERT_EQ(Opcode::reserved, Module::opcodeFromMachineByte(5));
  ASSERT_EQ(Opcode::IVALUE, Module::opcodeFromMachineByte(6));
  ASSERT_EQ(Opcode::META, Module::opcodeFromMachineByte(7));
  ASSERT_EQ(Opcode::COMPARE, Module::opcodeFromMachineByte(8));
  ASSERT_EQ(Opcode::reserved, Module::opcodeFromMachineByte(9));
  ASSERT_EQ(Opcode::reserved, Module::opcodeFromMachineByte(10));
  ASSERT_EQ(Opcode::reserved, Module::opcodeFromMachineByte(11));
  ASSERT_EQ(Opcode::FVALUE, Module::opcodeFromMachineByte(12));
  ASSERT_EQ(Opcode::reserved, Module::opcodeFromMachineByte(13));
  ASSERT_EQ(Opcode::MUTATE, Module::opcodeFromMachineByte(14));
  ASSERT_EQ(Opcode::reserved, Module::opcodeFromMachineByte(15));
  ASSERT_EQ(Opcode::reserved, Module::opcodeFromMachineByte(16));
  ASSERT_EQ(Opcode::reserved, Module::opcodeFromMachineByte(17));
  ASSERT_EQ(Opcode::SVALUE, Module::opcodeFromMachineByte(18));
  ASSERT_EQ(Opcode::reserved, Module::opcodeFromMachineByte(19));
  ASSERT_EQ(Opcode::reserved, Module::opcodeFromMachineByte(20));
  ASSERT_EQ(Opcode::reserved, Module::opcodeFromMachineByte(21));
  ASSERT_EQ(Opcode::reserved, Module::opcodeFromMachineByte(22));
  ASSERT_EQ(Opcode::reserved, Module::opcodeFromMachineByte(23));
  ASSERT_EQ(Opcode::ANY, Module::opcodeFromMachineByte(24));
  ASSERT_EQ(Opcode::ASSERT, Module::opcodeFromMachineByte(25));
  ASSERT_EQ(Opcode::ASSIGN, Module::opcodeFromMachineByte(26));
  ASSERT_EQ(Opcode::CATCH, Module::opcodeFromMachineByte(27));
  ASSERT_EQ(Opcode::FOR, Module::opcodeFromMachineByte(28));
  ASSERT_EQ(Opcode::reserved, Module::opcodeFromMachineByte(29));
  ASSERT_EQ(Opcode::ANYQ, Module::opcodeFromMachineByte(30));
  ASSERT_EQ(Opcode::DECREMENT, Module::opcodeFromMachineByte(31));
  ASSERT_EQ(Opcode::BYNAME, Module::opcodeFromMachineByte(32));
  ASSERT_EQ(Opcode::FOREACH, Module::opcodeFromMachineByte(33));
  ASSERT_EQ(Opcode::INDEXABLE, Module::opcodeFromMachineByte(34));
  ASSERT_EQ(Opcode::reserved, Module::opcodeFromMachineByte(35));
  ASSERT_EQ(Opcode::BREAK, Module::opcodeFromMachineByte(36));
  ASSERT_EQ(Opcode::ELLIPSIS, Module::opcodeFromMachineByte(37));
  ASSERT_EQ(Opcode::DO, Module::opcodeFromMachineByte(38));
  ASSERT_EQ(Opcode::GUARD, Module::opcodeFromMachineByte(39));
  ASSERT_EQ(Opcode::reserved, Module::opcodeFromMachineByte(40));
  ASSERT_EQ(Opcode::reserved, Module::opcodeFromMachineByte(41));
  ASSERT_EQ(Opcode::CONTINUE, Module::opcodeFromMachineByte(42));
  ASSERT_EQ(Opcode::IDENTIFIER, Module::opcodeFromMachineByte(43));
  ASSERT_EQ(Opcode::INDEX, Module::opcodeFromMachineByte(44));
  ASSERT_EQ(Opcode::reserved, Module::opcodeFromMachineByte(45));
  ASSERT_EQ(Opcode::reserved, Module::opcodeFromMachineByte(46));
  ASSERT_EQ(Opcode::reserved, Module::opcodeFromMachineByte(47));
  ASSERT_EQ(Opcode::FALSE, Module::opcodeFromMachineByte(48));
  ASSERT_EQ(Opcode::INCREMENT, Module::opcodeFromMachineByte(49));
  ASSERT_EQ(Opcode::reserved, Module::opcodeFromMachineByte(50));
  ASSERT_EQ(Opcode::reserved, Module::opcodeFromMachineByte(51));
  ASSERT_EQ(Opcode::reserved, Module::opcodeFromMachineByte(52));
  ASSERT_EQ(Opcode::reserved, Module::opcodeFromMachineByte(53));
  ASSERT_EQ(Opcode::FINITE, Module::opcodeFromMachineByte(54));
  ASSERT_EQ(Opcode::ITERABLE, Module::opcodeFromMachineByte(55));
  ASSERT_EQ(Opcode::NAMED, Module::opcodeFromMachineByte(56));
  ASSERT_EQ(Opcode::reserved, Module::opcodeFromMachineByte(57));
  ASSERT_EQ(Opcode::reserved, Module::opcodeFromMachineByte(58));
  ASSERT_EQ(Opcode::reserved, Module::opcodeFromMachineByte(59));
  ASSERT_EQ(Opcode::INFERRED, Module::opcodeFromMachineByte(60));
  ASSERT_EQ(Opcode::NOT, Module::opcodeFromMachineByte(61));
  ASSERT_EQ(Opcode::PROPERTY, Module::opcodeFromMachineByte(62));
  ASSERT_EQ(Opcode::reserved, Module::opcodeFromMachineByte(63));
  ASSERT_EQ(Opcode::reserved, Module::opcodeFromMachineByte(64));
  ASSERT_EQ(Opcode::reserved, Module::opcodeFromMachineByte(65));
  ASSERT_EQ(Opcode::NOOP, Module::opcodeFromMachineByte(66));
  ASSERT_EQ(Opcode::POINTEE, Module::opcodeFromMachineByte(67));
  ASSERT_EQ(Opcode::PROPERTYQ, Module::opcodeFromMachineByte(68));
  ASSERT_EQ(Opcode::reserved, Module::opcodeFromMachineByte(69));
  ASSERT_EQ(Opcode::reserved, Module::opcodeFromMachineByte(70));
  ASSERT_EQ(Opcode::reserved, Module::opcodeFromMachineByte(71));
  ASSERT_EQ(Opcode::NULL_, Module::opcodeFromMachineByte(72));
  ASSERT_EQ(Opcode::POINTER, Module::opcodeFromMachineByte(73));
  ASSERT_EQ(Opcode::WHILE, Module::opcodeFromMachineByte(74));
  ASSERT_EQ(Opcode::reserved, Module::opcodeFromMachineByte(75));
  ASSERT_EQ(Opcode::reserved, Module::opcodeFromMachineByte(76));
  ASSERT_EQ(Opcode::reserved, Module::opcodeFromMachineByte(77));
  ASSERT_EQ(Opcode::TRUE, Module::opcodeFromMachineByte(78));
  ASSERT_EQ(Opcode::PREDICATE, Module::opcodeFromMachineByte(79));
  ASSERT_EQ(Opcode::reserved, Module::opcodeFromMachineByte(80));
  ASSERT_EQ(Opcode::reserved, Module::opcodeFromMachineByte(81));
  ASSERT_EQ(Opcode::reserved, Module::opcodeFromMachineByte(82));
  ASSERT_EQ(Opcode::reserved, Module::opcodeFromMachineByte(83));
  ASSERT_EQ(Opcode::VOID, Module::opcodeFromMachineByte(84));
  ASSERT_EQ(Opcode::reserved, Module::opcodeFromMachineByte(85));
  ASSERT_EQ(Opcode::reserved, Module::opcodeFromMachineByte(86));
  ASSERT_EQ(Opcode::reserved, Module::opcodeFromMachineByte(87));
  ASSERT_EQ(Opcode::reserved, Module::opcodeFromMachineByte(88));
  ASSERT_EQ(Opcode::reserved, Module::opcodeFromMachineByte(89));
  ASSERT_EQ(Opcode::reserved, Module::opcodeFromMachineByte(90));
  ASSERT_EQ(Opcode::reserved, Module::opcodeFromMachineByte(91));
  ASSERT_EQ(Opcode::DECLARE, Module::opcodeFromMachineByte(92));
  ASSERT_EQ(Opcode::DECLARE, Module::opcodeFromMachineByte(93));
  ASSERT_EQ(Opcode::reserved, Module::opcodeFromMachineByte(94));
  ASSERT_EQ(Opcode::reserved, Module::opcodeFromMachineByte(95));
  ASSERT_EQ(Opcode::reserved, Module::opcodeFromMachineByte(96));
  ASSERT_EQ(Opcode::reserved, Module::opcodeFromMachineByte(97));
  ASSERT_EQ(Opcode::FUNCTION, Module::opcodeFromMachineByte(98));
  ASSERT_EQ(Opcode::FUNCTION, Module::opcodeFromMachineByte(99));
  ASSERT_EQ(Opcode::reserved, Module::opcodeFromMachineByte(100));
  ASSERT_EQ(Opcode::reserved, Module::opcodeFromMachineByte(101));
  ASSERT_EQ(Opcode::BOOL, Module::opcodeFromMachineByte(102));
  ASSERT_EQ(Opcode::BOOL, Module::opcodeFromMachineByte(103));
  ASSERT_EQ(Opcode::GENERATOR, Module::opcodeFromMachineByte(104));
  ASSERT_EQ(Opcode::GENERATOR, Module::opcodeFromMachineByte(105));
  ASSERT_EQ(Opcode::reserved, Module::opcodeFromMachineByte(106));
  ASSERT_EQ(Opcode::reserved, Module::opcodeFromMachineByte(107));
  ASSERT_EQ(Opcode::RETURN, Module::opcodeFromMachineByte(108));
  ASSERT_EQ(Opcode::RETURN, Module::opcodeFromMachineByte(109));
  ASSERT_EQ(Opcode::IF, Module::opcodeFromMachineByte(110));
  ASSERT_EQ(Opcode::IF, Module::opcodeFromMachineByte(111));
  ASSERT_EQ(Opcode::reserved, Module::opcodeFromMachineByte(112));
  ASSERT_EQ(Opcode::reserved, Module::opcodeFromMachineByte(113));
  ASSERT_EQ(Opcode::THROW, Module::opcodeFromMachineByte(114));
  ASSERT_EQ(Opcode::THROW, Module::opcodeFromMachineByte(115));
  ASSERT_EQ(Opcode::TRY, Module::opcodeFromMachineByte(116));
  ASSERT_EQ(Opcode::TRY, Module::opcodeFromMachineByte(117));
  ASSERT_EQ(Opcode::TRY, Module::opcodeFromMachineByte(118));
  ASSERT_EQ(Opcode::TRY, Module::opcodeFromMachineByte(119));
  ASSERT_EQ(Opcode::YIELD, Module::opcodeFromMachineByte(120));
  ASSERT_EQ(Opcode::YIELD, Module::opcodeFromMachineByte(121));
  ASSERT_EQ(Opcode::SWITCH, Module::opcodeFromMachineByte(122));
  ASSERT_EQ(Opcode::SWITCH, Module::opcodeFromMachineByte(123));
  ASSERT_EQ(Opcode::SWITCH, Module::opcodeFromMachineByte(124));
  ASSERT_EQ(Opcode::SWITCH, Module::opcodeFromMachineByte(125));
  ASSERT_EQ(Opcode::reserved, Module::opcodeFromMachineByte(126));
  ASSERT_EQ(Opcode::reserved, Module::opcodeFromMachineByte(127));
  ASSERT_EQ(Opcode::CASE, Module::opcodeFromMachineByte(128));
  ASSERT_EQ(Opcode::CASE, Module::opcodeFromMachineByte(129));
  ASSERT_EQ(Opcode::CASE, Module::opcodeFromMachineByte(130));
  ASSERT_EQ(Opcode::CASE, Module::opcodeFromMachineByte(131));
  ASSERT_EQ(Opcode::reserved, Module::opcodeFromMachineByte(132));
  ASSERT_EQ(Opcode::reserved, Module::opcodeFromMachineByte(133));
  ASSERT_EQ(Opcode::VARARGS, Module::opcodeFromMachineByte(134));
  ASSERT_EQ(Opcode::VARARGS, Module::opcodeFromMachineByte(135));
  ASSERT_EQ(Opcode::VARARGS, Module::opcodeFromMachineByte(136));
  ASSERT_EQ(Opcode::VARARGS, Module::opcodeFromMachineByte(137));
  ASSERT_EQ(Opcode::reserved, Module::opcodeFromMachineByte(138));
  ASSERT_EQ(Opcode::OPTIONAL, Module::opcodeFromMachineByte(139));
  ASSERT_EQ(Opcode::OPTIONAL, Module::opcodeFromMachineByte(140));
  ASSERT_EQ(Opcode::reserved, Module::opcodeFromMachineByte(141));
  ASSERT_EQ(Opcode::reserved, Module::opcodeFromMachineByte(142));
  ASSERT_EQ(Opcode::reserved, Module::opcodeFromMachineByte(143));
  ASSERT_EQ(Opcode::reserved, Module::opcodeFromMachineByte(144));
  ASSERT_EQ(Opcode::REQUIRED, Module::opcodeFromMachineByte(145));
  ASSERT_EQ(Opcode::REQUIRED, Module::opcodeFromMachineByte(146));
  ASSERT_EQ(Opcode::reserved, Module::opcodeFromMachineByte(147));
  ASSERT_EQ(Opcode::reserved, Module::opcodeFromMachineByte(148));
  ASSERT_EQ(Opcode::reserved, Module::opcodeFromMachineByte(149));
  ASSERT_EQ(Opcode::reserved, Module::opcodeFromMachineByte(150));
  ASSERT_EQ(Opcode::ATTRIBUTE, Module::opcodeFromMachineByte(151));
  ASSERT_EQ(Opcode::ATTRIBUTE, Module::opcodeFromMachineByte(152));
  ASSERT_EQ(Opcode::ATTRIBUTE, Module::opcodeFromMachineByte(153));
  ASSERT_EQ(Opcode::ATTRIBUTE, Module::opcodeFromMachineByte(154));
  ASSERT_EQ(Opcode::ATTRIBUTE, Module::opcodeFromMachineByte(155));
  ASSERT_EQ(Opcode::reserved, Module::opcodeFromMachineByte(156));
  ASSERT_EQ(Opcode::BLOCK, Module::opcodeFromMachineByte(157));
  ASSERT_EQ(Opcode::BLOCK, Module::opcodeFromMachineByte(158));
  ASSERT_EQ(Opcode::BLOCK, Module::opcodeFromMachineByte(159));
  ASSERT_EQ(Opcode::BLOCK, Module::opcodeFromMachineByte(160));
  ASSERT_EQ(Opcode::BLOCK, Module::opcodeFromMachineByte(161));
  ASSERT_EQ(Opcode::reserved, Module::opcodeFromMachineByte(162));
  ASSERT_EQ(Opcode::CALL, Module::opcodeFromMachineByte(163));
  ASSERT_EQ(Opcode::CALL, Module::opcodeFromMachineByte(164));
  ASSERT_EQ(Opcode::CALL, Module::opcodeFromMachineByte(165));
  ASSERT_EQ(Opcode::CALL, Module::opcodeFromMachineByte(166));
  ASSERT_EQ(Opcode::CALL, Module::opcodeFromMachineByte(167));
  ASSERT_EQ(Opcode::reserved, Module::opcodeFromMachineByte(168));
  ASSERT_EQ(Opcode::CALLABLE, Module::opcodeFromMachineByte(169));
  ASSERT_EQ(Opcode::CALLABLE, Module::opcodeFromMachineByte(170));
  ASSERT_EQ(Opcode::CALLABLE, Module::opcodeFromMachineByte(171));
  ASSERT_EQ(Opcode::CALLABLE, Module::opcodeFromMachineByte(172));
  ASSERT_EQ(Opcode::CALLABLE, Module::opcodeFromMachineByte(173));
  ASSERT_EQ(Opcode::reserved, Module::opcodeFromMachineByte(174));
  ASSERT_EQ(Opcode::CHOICE, Module::opcodeFromMachineByte(175));
  ASSERT_EQ(Opcode::CHOICE, Module::opcodeFromMachineByte(176));
  ASSERT_EQ(Opcode::CHOICE, Module::opcodeFromMachineByte(177));
  ASSERT_EQ(Opcode::CHOICE, Module::opcodeFromMachineByte(178));
  ASSERT_EQ(Opcode::CHOICE, Module::opcodeFromMachineByte(179));
  ASSERT_EQ(Opcode::reserved, Module::opcodeFromMachineByte(180));
  ASSERT_EQ(Opcode::DEFAULT, Module::opcodeFromMachineByte(181));
  ASSERT_EQ(Opcode::DEFAULT, Module::opcodeFromMachineByte(182));
  ASSERT_EQ(Opcode::DEFAULT, Module::opcodeFromMachineByte(183));
  ASSERT_EQ(Opcode::DEFAULT, Module::opcodeFromMachineByte(184));
  ASSERT_EQ(Opcode::DEFAULT, Module::opcodeFromMachineByte(185));
  ASSERT_EQ(Opcode::reserved, Module::opcodeFromMachineByte(186));
  ASSERT_EQ(Opcode::EXTENSIBLE, Module::opcodeFromMachineByte(187));
  ASSERT_EQ(Opcode::EXTENSIBLE, Module::opcodeFromMachineByte(188));
  ASSERT_EQ(Opcode::EXTENSIBLE, Module::opcodeFromMachineByte(189));
  ASSERT_EQ(Opcode::EXTENSIBLE, Module::opcodeFromMachineByte(190));
  ASSERT_EQ(Opcode::EXTENSIBLE, Module::opcodeFromMachineByte(191));
  ASSERT_EQ(Opcode::reserved, Module::opcodeFromMachineByte(192));
  ASSERT_EQ(Opcode::LAMBDA, Module::opcodeFromMachineByte(193));
  ASSERT_EQ(Opcode::LAMBDA, Module::opcodeFromMachineByte(194));
  ASSERT_EQ(Opcode::LAMBDA, Module::opcodeFromMachineByte(195));
  ASSERT_EQ(Opcode::LAMBDA, Module::opcodeFromMachineByte(196));
  ASSERT_EQ(Opcode::LAMBDA, Module::opcodeFromMachineByte(197));
  ASSERT_EQ(Opcode::reserved, Module::opcodeFromMachineByte(198));
  ASSERT_EQ(Opcode::LENGTH, Module::opcodeFromMachineByte(199));
  ASSERT_EQ(Opcode::LENGTH, Module::opcodeFromMachineByte(200));
  ASSERT_EQ(Opcode::LENGTH, Module::opcodeFromMachineByte(201));
  ASSERT_EQ(Opcode::LENGTH, Module::opcodeFromMachineByte(202));
  ASSERT_EQ(Opcode::LENGTH, Module::opcodeFromMachineByte(203));
  ASSERT_EQ(Opcode::reserved, Module::opcodeFromMachineByte(204));
  ASSERT_EQ(Opcode::UNION, Module::opcodeFromMachineByte(205));
  ASSERT_EQ(Opcode::UNION, Module::opcodeFromMachineByte(206));
  ASSERT_EQ(Opcode::UNION, Module::opcodeFromMachineByte(207));
  ASSERT_EQ(Opcode::UNION, Module::opcodeFromMachineByte(208));
  ASSERT_EQ(Opcode::UNION, Module::opcodeFromMachineByte(209));
  ASSERT_EQ(Opcode::AVALUE, Module::opcodeFromMachineByte(210));
  ASSERT_EQ(Opcode::AVALUE, Module::opcodeFromMachineByte(211));
  ASSERT_EQ(Opcode::AVALUE, Module::opcodeFromMachineByte(212));
  ASSERT_EQ(Opcode::AVALUE, Module::opcodeFromMachineByte(213));
  ASSERT_EQ(Opcode::AVALUE, Module::opcodeFromMachineByte(214));
  ASSERT_EQ(Opcode::AVALUE, Module::opcodeFromMachineByte(215));
  ASSERT_EQ(Opcode::FLOAT, Module::opcodeFromMachineByte(216));
  ASSERT_EQ(Opcode::FLOAT, Module::opcodeFromMachineByte(217));
  ASSERT_EQ(Opcode::FLOAT, Module::opcodeFromMachineByte(218));
  ASSERT_EQ(Opcode::FLOAT, Module::opcodeFromMachineByte(219));
  ASSERT_EQ(Opcode::FLOAT, Module::opcodeFromMachineByte(220));
  ASSERT_EQ(Opcode::FLOAT, Module::opcodeFromMachineByte(221));
  ASSERT_EQ(Opcode::INT, Module::opcodeFromMachineByte(222));
  ASSERT_EQ(Opcode::INT, Module::opcodeFromMachineByte(223));
  ASSERT_EQ(Opcode::INT, Module::opcodeFromMachineByte(224));
  ASSERT_EQ(Opcode::INT, Module::opcodeFromMachineByte(225));
  ASSERT_EQ(Opcode::INT, Module::opcodeFromMachineByte(226));
  ASSERT_EQ(Opcode::INT, Module::opcodeFromMachineByte(227));
  ASSERT_EQ(Opcode::OBJECT, Module::opcodeFromMachineByte(228));
  ASSERT_EQ(Opcode::OBJECT, Module::opcodeFromMachineByte(229));
  ASSERT_EQ(Opcode::OBJECT, Module::opcodeFromMachineByte(230));
  ASSERT_EQ(Opcode::OBJECT, Module::opcodeFromMachineByte(231));
  ASSERT_EQ(Opcode::OBJECT, Module::opcodeFromMachineByte(232));
  ASSERT_EQ(Opcode::OBJECT, Module::opcodeFromMachineByte(233));
  ASSERT_EQ(Opcode::OVALUE, Module::opcodeFromMachineByte(234));
  ASSERT_EQ(Opcode::OVALUE, Module::opcodeFromMachineByte(235));
  ASSERT_EQ(Opcode::OVALUE, Module::opcodeFromMachineByte(236));
  ASSERT_EQ(Opcode::OVALUE, Module::opcodeFromMachineByte(237));
  ASSERT_EQ(Opcode::OVALUE, Module::opcodeFromMachineByte(238));
  ASSERT_EQ(Opcode::OVALUE, Module::opcodeFromMachineByte(239));
  ASSERT_EQ(Opcode::STRING, Module::opcodeFromMachineByte(240));
  ASSERT_EQ(Opcode::STRING, Module::opcodeFromMachineByte(241));
  ASSERT_EQ(Opcode::STRING, Module::opcodeFromMachineByte(242));
  ASSERT_EQ(Opcode::STRING, Module::opcodeFromMachineByte(243));
  ASSERT_EQ(Opcode::STRING, Module::opcodeFromMachineByte(244));
  ASSERT_EQ(Opcode::STRING, Module::opcodeFromMachineByte(245));
  ASSERT_EQ(Opcode::TYPE, Module::opcodeFromMachineByte(246));
  ASSERT_EQ(Opcode::TYPE, Module::opcodeFromMachineByte(247));
  ASSERT_EQ(Opcode::TYPE, Module::opcodeFromMachineByte(248));
  ASSERT_EQ(Opcode::TYPE, Module::opcodeFromMachineByte(249));
  ASSERT_EQ(Opcode::TYPE, Module::opcodeFromMachineByte(250));
  ASSERT_EQ(Opcode::TYPE, Module::opcodeFromMachineByte(251));
  ASSERT_EQ(Opcode::reserved, Module::opcodeFromMachineByte(252));
  ASSERT_EQ(Opcode::MODULE, Module::opcodeFromMachineByte(253));
  ASSERT_EQ(Opcode::MODULE, Module::opcodeFromMachineByte(254));
  ASSERT_EQ(Opcode::MODULE, Module::opcodeFromMachineByte(255));
}

TEST(TestModule, OpcodeEncode0) {
  ASSERT_EQ(72, OpcodeProperties::from(Opcode::NULL_).encode(0));
  ASSERT_EQ(0, OpcodeProperties::from(Opcode::NULL_).encode(1));
}

TEST(TestModule, OpcodeEncode1) {
  ASSERT_EQ(0, OpcodeProperties::from(Opcode::UNARY).encode(0));
  ASSERT_EQ(1, OpcodeProperties::from(Opcode::UNARY).encode(1));
  ASSERT_EQ(0, OpcodeProperties::from(Opcode::UNARY).encode(2));
}

TEST(TestModule, OpcodeEncode2) {
  ASSERT_EQ(0, OpcodeProperties::from(Opcode::BINARY).encode(0));
  ASSERT_EQ(0, OpcodeProperties::from(Opcode::BINARY).encode(1));
  ASSERT_EQ(2, OpcodeProperties::from(Opcode::BINARY).encode(2));
  ASSERT_EQ(0, OpcodeProperties::from(Opcode::BINARY).encode(3));
}

TEST(TestModule, OpcodeEncode3) {
  ASSERT_EQ(0, OpcodeProperties::from(Opcode::TERNARY).encode(0));
  ASSERT_EQ(0, OpcodeProperties::from(Opcode::TERNARY).encode(1));
  ASSERT_EQ(0, OpcodeProperties::from(Opcode::TERNARY).encode(2));
  ASSERT_EQ(3, OpcodeProperties::from(Opcode::TERNARY).encode(3));
  ASSERT_EQ(0, OpcodeProperties::from(Opcode::TERNARY).encode(4));
}

TEST(TestModule, OpcodeEncode4) {
  ASSERT_EQ(0, OpcodeProperties::from(Opcode::FOR).encode(0));
  ASSERT_EQ(0, OpcodeProperties::from(Opcode::FOR).encode(1));
  ASSERT_EQ(0, OpcodeProperties::from(Opcode::FOR).encode(2));
  ASSERT_EQ(0, OpcodeProperties::from(Opcode::FOR).encode(3));
  ASSERT_EQ(28, OpcodeProperties::from(Opcode::FOR).encode(4));
  ASSERT_EQ(0, OpcodeProperties::from(Opcode::FOR).encode(5));
}

TEST(TestModule, OpcodeEncode5) {
  ASSERT_EQ(210, OpcodeProperties::from(Opcode::AVALUE).encode(0));
  ASSERT_EQ(211, OpcodeProperties::from(Opcode::AVALUE).encode(1));
  ASSERT_EQ(212, OpcodeProperties::from(Opcode::AVALUE).encode(2));
  ASSERT_EQ(213, OpcodeProperties::from(Opcode::AVALUE).encode(3));
  ASSERT_EQ(214, OpcodeProperties::from(Opcode::AVALUE).encode(4));
  ASSERT_EQ(215, OpcodeProperties::from(Opcode::AVALUE).encode(5));
  ASSERT_EQ(215, OpcodeProperties::from(Opcode::AVALUE).encode(6));
  ASSERT_EQ(215, OpcodeProperties::from(Opcode::AVALUE).encode(7));
}

TEST(TestModule, OperatorUnary) {
  ASSERT_STREQ("-", OperatorProperties::from(Operator::NEG).name);
  ASSERT_EQ(Opclass::UNARY, OperatorProperties::from(Operator::NEG).opclass);
  ASSERT_EQ(1u, OperatorProperties::from(Operator::NEG).operands);
}

TEST(TestModule, OperatorBinary) {
  ASSERT_STREQ("-", OperatorProperties::from(Operator::SUB).name);
  ASSERT_EQ(Opclass::BINARY, OperatorProperties::from(Operator::SUB).opclass);
  ASSERT_EQ(2u, OperatorProperties::from(Operator::SUB).operands);
}

TEST(TestModule, OperatorTernary) {
  ASSERT_STREQ("?:", OperatorProperties::from(Operator::TERNARY).name);
  ASSERT_EQ(Opclass::TERNARY, OperatorProperties::from(Operator::TERNARY).opclass);
  ASSERT_EQ(3u, OperatorProperties::from(Operator::TERNARY).operands);
}

TEST(TestModule, OperatorCompare) {
  ASSERT_STREQ("<", OperatorProperties::from(Operator::LT).name);
  ASSERT_EQ(Opclass::COMPARE, OperatorProperties::from(Operator::LT).opclass);
  ASSERT_EQ(2u, OperatorProperties::from(Operator::LT).operands);
}

TEST(TestModule, Constants) {
  // Test that the magic header starts with a UTF-8 continuation byte
  const uint8_t magic[] = { MAGIC };
  ASSERT_EQ(0x80, magic[0] & 0xC0);
  // Test that the "end" opcode is zero
  ASSERT_EQ(int(Opcode::END), 0);
  // Test that well-known opcodes have implicit operands
  ASSERT_LT(int(Opcode::IVALUE), EGG_VM_ISTART);
  ASSERT_LT(int(Opcode::FVALUE), EGG_VM_ISTART);
  ASSERT_LT(int(Opcode::SVALUE), EGG_VM_ISTART);
  ASSERT_LT(int(Opcode::UNARY), EGG_VM_ISTART);
  ASSERT_LT(int(Opcode::BINARY), EGG_VM_ISTART);
  ASSERT_LT(int(Opcode::TERNARY), EGG_VM_ISTART);
  // Test that operator enums fit into [0..128] for operand fitting
  ASSERT_EQ(int(Operator::TERNARY), 128);
}

TEST(TestModule, FromMemoryBad) {
  const uint8_t zero[] = { 0 };
  expectFailureFromMemory(zero, sizeof(zero), "Invalid magic signature in binary module");
  const uint8_t magic[] = { MAGIC 99 }; // This is an invalid section number
  expectFailureFromMemory(magic, 0, "Truncated section in binary module");
  expectFailureFromMemory(magic, 1, "Truncated section in binary module");
  expectFailureFromMemory(magic, sizeof(magic) - 1, "Missing code section in binary module");
  expectFailureFromMemory(magic, sizeof(magic), "Unrecognized section in binary module");
}

TEST(TestModule, FromMemoryMinimal) {
  egg::test::Allocator allocator;
  const uint8_t minimal[] = { MAGIC SECTION_CODE, uint8_t(Opcode::MODULE), uint8_t(Opcode::BLOCK), uint8_t(Opcode::NOOP) };
  auto module = ModuleFactory::fromMemory(allocator, "<memory>", std::begin(minimal), std::end(minimal));
  ASSERT_NE(nullptr, module);
  Node root{ &module->getRootNode() };
  ASSERT_NE(nullptr, root);
  ASSERT_EQ(Opcode::MODULE, root->getOpcode());
  ASSERT_EQ(1u, root->getChildren());
  Node child{ &root->getChild(0) };
  ASSERT_EQ(Opcode::BLOCK, child->getOpcode());
  ASSERT_EQ(1u, child->getChildren());
  Node grandchild{ &child->getChild(0) };
  ASSERT_EQ(Opcode::NOOP, grandchild->getOpcode());
  ASSERT_EQ(0u, grandchild->getChildren());
}

TEST(TestModule, ToBinaryStream) {
  egg::test::Allocator allocator;
  const uint8_t minimal[] = { MAGIC SECTION_CODE, uint8_t(Opcode::MODULE), uint8_t(Opcode::BLOCK), uint8_t(Opcode::NOOP) };
  auto module = ModuleFactory::fromMemory(allocator, "<memory>", std::begin(minimal), std::end(minimal));
  ASSERT_NE(nullptr, module);
  std::ostringstream oss;
  ModuleFactory::toBinaryStream(*module, oss);
  auto binary = oss.str();
  ASSERT_EQ(sizeof(minimal), binary.length());
  ASSERT_EQ(0, std::memcmp(binary.data(), minimal, sizeof(minimal)));
}

TEST(TestModule, ToMemory) {
  egg::test::Allocator allocator;
  const uint8_t minimal[] = { MAGIC SECTION_CODE, uint8_t(Opcode::MODULE), uint8_t(Opcode::BLOCK), uint8_t(Opcode::NOOP) };
  auto module = ModuleFactory::fromMemory(allocator, "<memory>", std::begin(minimal), std::end(minimal));
  ASSERT_NE(nullptr, module);
  auto memory = ModuleFactory::toMemory(allocator, *module);
  ASSERT_NE(nullptr, memory);
  ASSERT_EQ(sizeof(minimal), memory->bytes());
  ASSERT_EQ(0, std::memcmp(memory->begin(), minimal, sizeof(minimal)));
}

TEST(TestModule, ModuleBuilder) {
  egg::test::Allocator allocator;
  ModuleBuilder builder(allocator);
  auto noop = builder.createNode(Opcode::NOOP);
  auto block = builder.createNode(Opcode::BLOCK, std::move(noop));
  auto original = builder.createModule(std::move(block));
  auto module = ModuleFactory::fromRootNode(allocator, "<resource>", *original);
  ASSERT_NE(nullptr, module);
  Node root{ &module->getRootNode() };
  ASSERT_EQ(original.get(), root.get());
  ASSERT_EQ(Opcode::MODULE, root->getOpcode());
  ASSERT_EQ(1u, root->getChildren());
  Node child{ &root->getChild(0) };
  ASSERT_EQ(Opcode::BLOCK, child->getOpcode());
  ASSERT_EQ(1u, child->getChildren());
  Node grandchild{ &child->getChild(0) };
  ASSERT_EQ(Opcode::NOOP, grandchild->getOpcode());
  ASSERT_EQ(0u, grandchild->getChildren());
}

TEST(TestModule, BuildConstantInt) {
  egg::test::Allocator allocator;
  ModuleBuilder builder(allocator);
  auto avalue = roundTripArray(builder, {
    builder.createValueInt(123456789),
    builder.createValueInt(-123456789)
  });
  ASSERT_EQ(2u, avalue->getChildren());
  Node value;
  value.set(&avalue->getChild(0));
  ASSERT_EQ(Opcode::IVALUE, value->getOpcode());
  ASSERT_EQ(123456789, value->getInt());
  ASSERT_EQ(0u, value->getChildren());
  value.set(&avalue->getChild(1));
  ASSERT_EQ(Opcode::IVALUE, value->getOpcode());
  ASSERT_EQ(-123456789, value->getInt());
  ASSERT_EQ(0u, value->getChildren());
}

TEST(TestModule, BuildConstantFloat) {
  egg::test::Allocator allocator;
  ModuleBuilder builder(allocator);
  auto avalue = roundTripArray(builder, {
    builder.createValueFloat(123456789),
    builder.createValueFloat(-123456789),
    builder.createValueFloat(-0.125),
    builder.createValueFloat(std::numeric_limits<double>::quiet_NaN())
  });
  ASSERT_EQ(4u, avalue->getChildren());
  Node value;
  value.set(&avalue->getChild(0));
  ASSERT_EQ(Opcode::FVALUE, value->getOpcode());
  ASSERT_EQ(123456789.0, value->getFloat());
  ASSERT_EQ(0u, value->getChildren());
  value.set(&avalue->getChild(1));
  ASSERT_EQ(Opcode::FVALUE, value->getOpcode());
  ASSERT_EQ(-123456789.0, value->getFloat());
  ASSERT_EQ(0u, value->getChildren());
  value.set(&avalue->getChild(2));
  ASSERT_EQ(Opcode::FVALUE, value->getOpcode());
  ASSERT_EQ(-0.125, value->getFloat());
  ASSERT_EQ(0u, value->getChildren());
  value.set(&avalue->getChild(3));
  ASSERT_EQ(Opcode::FVALUE, value->getOpcode());
  ASSERT_TRUE(std::isnan(value->getFloat()));
  ASSERT_EQ(0u, value->getChildren());
}

TEST(TestModule, BuildConstantString) {
  egg::test::Allocator allocator;
  ModuleBuilder builder(allocator);
  auto avalue = roundTripArray(builder, {
    builder.createValueString(""),
    builder.createValueString("hello")
  });
  ASSERT_EQ(2u, avalue->getChildren());
  Node value;
  value.set(&avalue->getChild(0));
  ASSERT_EQ(Opcode::SVALUE, value->getOpcode());
  ASSERT_STRING("", value->getString());
  ASSERT_EQ(0u, value->getChildren());
  value.set(&avalue->getChild(1));
  ASSERT_EQ(Opcode::SVALUE, value->getOpcode());
  ASSERT_STRING("hello", value->getString());
  ASSERT_EQ(0u, value->getChildren());
}

TEST(TestModule, BuildOperator) {
  egg::test::Allocator allocator;
  ModuleBuilder builder(allocator);
  auto avalue = roundTripArray(builder, {
    builder.createOperator(Opcode::UNARY, Operator::REF, { builder.createNode(Opcode::NULL_) })
  });
  ASSERT_EQ(1u, avalue->getChildren());
  Node value;
  value.set(&avalue->getChild(0));
  ASSERT_EQ(Opcode::UNARY, value->getOpcode());
  ASSERT_EQ(Int(Operator::REF), value->getInt()); // the integer operator code
  ASSERT_EQ(1u, value->getChildren());
  value.set(&value->getChild(0));
  ASSERT_EQ(Opcode::NULL_, value->getOpcode());
  ASSERT_EQ(0u, value->getChildren());
}

TEST(TestModule, BuildWithAttribute) {
  egg::test::Allocator allocator;
  ModuleBuilder builder(allocator);
  auto avalue = roundTripArray(builder, {
    builder.withAttribute("a", String("alpha")).withAttribute("b", 123).createOperator(Opcode::UNARY, Operator::REF,{ builder.createNode(Opcode::NULL_) })
  });
  ASSERT_EQ(1u, avalue->getChildren());
  Node value;
  value.set(&avalue->getChild(0));
  ASSERT_EQ(Opcode::UNARY, value->getOpcode());
  ASSERT_EQ(Int(Operator::REF), value->getInt()); // the integer operator code
  ASSERT_EQ(1u, value->getChildren());
  ASSERT_EQ(2u, value->getAttributes());
  value.set(&value->getAttribute(0));
  ASSERT_EQ(Opcode::ATTRIBUTE, value->getOpcode());
  ASSERT_EQ(2u, value->getChildren());
  value.set(&value->getChild(1));
  ASSERT_EQ(Opcode::SVALUE, value->getOpcode());
  ASSERT_EQ("alpha", value->getString().toUTF8());
}
