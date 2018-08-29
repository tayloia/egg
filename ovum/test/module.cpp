#include "ovum/test.h"
#include "ovum/node.h"
#include "ovum/module.h"

#include <cmath>

#define EGG_VM_MAGIC_BYTE(byte) byte,
#define MAGIC EGG_VM_MAGIC(EGG_VM_MAGIC_BYTE)

using namespace egg::ovum;

namespace {
  void expectFailureFromMemory(const uint8_t memory[], size_t bytes, const char* needle) {
    egg::test::Allocator allocator;
    ASSERT_THROW_E(ModuleFactory::fromMemory(allocator, memory, memory + bytes), std::runtime_error, ASSERT_STARTSWITH(e.what(), needle));
  }
  void toModuleArray(ModuleBuilder& builder, const Nodes& avalues, Module& out) {
    // Create a module that just constructs an array of values
    auto array = builder.createValueArray(avalues);
    ASSERT_NE(nullptr, array);
    auto block = builder.createNode(OPCODE_BLOCK, std::move(array));
    ASSERT_NE(nullptr, block);
    auto module = builder.createModule(std::move(block));
    ASSERT_NE(nullptr, module);
    out = ModuleFactory::fromRootNode(builder.allocator, *module);
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
    ASSERT_EQ(OPCODE_MODULE, root->getOpcode());
    ASSERT_EQ(1u, root->getChildren());
    Node child{ &root->getChild(0) };
    ASSERT_EQ(OPCODE_BLOCK, child->getOpcode());
    ASSERT_EQ(1u, child->getChildren());
    avalue.set(&child->getChild(0));
    ASSERT_EQ(OPCODE_AVALUE, avalue->getOpcode());
  }
  void fromModuleMemoryArray(IAllocator& allocator, std::istream& in, Node& avalue) {
    // Extract an array of values from a module memory image
    in.clear();
    ASSERT_TRUE(in.seekg(0).good());
    auto module = ModuleFactory::fromBinaryStream(allocator, in);
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
  ASSERT_EQ(OPCODE_END, Module::opcodeFromMachineByte(0));
  ASSERT_EQ(OPCODE_UNARY, Module::opcodeFromMachineByte(1));
  ASSERT_EQ(OPCODE_BINARY, Module::opcodeFromMachineByte(2));
  ASSERT_EQ(OPCODE_TERNARY, Module::opcodeFromMachineByte(3));
  ASSERT_EQ(OPCODE_reserved, Module::opcodeFromMachineByte(4));
  ASSERT_EQ(OPCODE_reserved, Module::opcodeFromMachineByte(5));
  ASSERT_EQ(OPCODE_IVALUE, Module::opcodeFromMachineByte(6));
  ASSERT_EQ(OPCODE_META, Module::opcodeFromMachineByte(7));
  ASSERT_EQ(OPCODE_COMPARE, Module::opcodeFromMachineByte(8));
  ASSERT_EQ(OPCODE_reserved, Module::opcodeFromMachineByte(9));
  ASSERT_EQ(OPCODE_reserved, Module::opcodeFromMachineByte(10));
  ASSERT_EQ(OPCODE_reserved, Module::opcodeFromMachineByte(11));
  ASSERT_EQ(OPCODE_FVALUE, Module::opcodeFromMachineByte(12));
  ASSERT_EQ(OPCODE_reserved, Module::opcodeFromMachineByte(13));
  ASSERT_EQ(OPCODE_MUTATE, Module::opcodeFromMachineByte(14));
  ASSERT_EQ(OPCODE_reserved, Module::opcodeFromMachineByte(15));
  ASSERT_EQ(OPCODE_reserved, Module::opcodeFromMachineByte(16));
  ASSERT_EQ(OPCODE_reserved, Module::opcodeFromMachineByte(17));
  ASSERT_EQ(OPCODE_SVALUE, Module::opcodeFromMachineByte(18));
  ASSERT_EQ(OPCODE_reserved, Module::opcodeFromMachineByte(19));
  ASSERT_EQ(OPCODE_reserved, Module::opcodeFromMachineByte(20));
  ASSERT_EQ(OPCODE_reserved, Module::opcodeFromMachineByte(21));
  ASSERT_EQ(OPCODE_reserved, Module::opcodeFromMachineByte(22));
  ASSERT_EQ(OPCODE_reserved, Module::opcodeFromMachineByte(23));
  ASSERT_EQ(OPCODE_ANY, Module::opcodeFromMachineByte(24));
  ASSERT_EQ(OPCODE_ASSERT, Module::opcodeFromMachineByte(25));
  ASSERT_EQ(OPCODE_ASSIGN, Module::opcodeFromMachineByte(26));
  ASSERT_EQ(OPCODE_CATCH, Module::opcodeFromMachineByte(27));
  ASSERT_EQ(OPCODE_FOR, Module::opcodeFromMachineByte(28));
  ASSERT_EQ(OPCODE_reserved, Module::opcodeFromMachineByte(29));
  ASSERT_EQ(OPCODE_ANYQ, Module::opcodeFromMachineByte(30));
  ASSERT_EQ(OPCODE_DECREMENT, Module::opcodeFromMachineByte(31));
  ASSERT_EQ(OPCODE_BYNAME, Module::opcodeFromMachineByte(32));
  ASSERT_EQ(OPCODE_FOREACH, Module::opcodeFromMachineByte(33));
  ASSERT_EQ(OPCODE_INDEXABLE, Module::opcodeFromMachineByte(34));
  ASSERT_EQ(OPCODE_reserved, Module::opcodeFromMachineByte(35));
  ASSERT_EQ(OPCODE_BREAK, Module::opcodeFromMachineByte(36));
  ASSERT_EQ(OPCODE_ELLIPSIS, Module::opcodeFromMachineByte(37));
  ASSERT_EQ(OPCODE_DO, Module::opcodeFromMachineByte(38));
  ASSERT_EQ(OPCODE_GUARD, Module::opcodeFromMachineByte(39));
  ASSERT_EQ(OPCODE_reserved, Module::opcodeFromMachineByte(40));
  ASSERT_EQ(OPCODE_reserved, Module::opcodeFromMachineByte(41));
  ASSERT_EQ(OPCODE_CONTINUE, Module::opcodeFromMachineByte(42));
  ASSERT_EQ(OPCODE_IDENTIFIER, Module::opcodeFromMachineByte(43));
  ASSERT_EQ(OPCODE_INDEX, Module::opcodeFromMachineByte(44));
  ASSERT_EQ(OPCODE_reserved, Module::opcodeFromMachineByte(45));
  ASSERT_EQ(OPCODE_reserved, Module::opcodeFromMachineByte(46));
  ASSERT_EQ(OPCODE_reserved, Module::opcodeFromMachineByte(47));
  ASSERT_EQ(OPCODE_FALSE, Module::opcodeFromMachineByte(48));
  ASSERT_EQ(OPCODE_INCREMENT, Module::opcodeFromMachineByte(49));
  ASSERT_EQ(OPCODE_reserved, Module::opcodeFromMachineByte(50));
  ASSERT_EQ(OPCODE_reserved, Module::opcodeFromMachineByte(51));
  ASSERT_EQ(OPCODE_reserved, Module::opcodeFromMachineByte(52));
  ASSERT_EQ(OPCODE_reserved, Module::opcodeFromMachineByte(53));
  ASSERT_EQ(OPCODE_FINITE, Module::opcodeFromMachineByte(54));
  ASSERT_EQ(OPCODE_ITERABLE, Module::opcodeFromMachineByte(55));
  ASSERT_EQ(OPCODE_NAMED, Module::opcodeFromMachineByte(56));
  ASSERT_EQ(OPCODE_reserved, Module::opcodeFromMachineByte(57));
  ASSERT_EQ(OPCODE_reserved, Module::opcodeFromMachineByte(58));
  ASSERT_EQ(OPCODE_reserved, Module::opcodeFromMachineByte(59));
  ASSERT_EQ(OPCODE_INFERRED, Module::opcodeFromMachineByte(60));
  ASSERT_EQ(OPCODE_NOT, Module::opcodeFromMachineByte(61));
  ASSERT_EQ(OPCODE_PROPERTY, Module::opcodeFromMachineByte(62));
  ASSERT_EQ(OPCODE_reserved, Module::opcodeFromMachineByte(63));
  ASSERT_EQ(OPCODE_reserved, Module::opcodeFromMachineByte(64));
  ASSERT_EQ(OPCODE_reserved, Module::opcodeFromMachineByte(65));
  ASSERT_EQ(OPCODE_NOOP, Module::opcodeFromMachineByte(66));
  ASSERT_EQ(OPCODE_POINTEE, Module::opcodeFromMachineByte(67));
  ASSERT_EQ(OPCODE_PROPERTYQ, Module::opcodeFromMachineByte(68));
  ASSERT_EQ(OPCODE_reserved, Module::opcodeFromMachineByte(69));
  ASSERT_EQ(OPCODE_reserved, Module::opcodeFromMachineByte(70));
  ASSERT_EQ(OPCODE_reserved, Module::opcodeFromMachineByte(71));
  ASSERT_EQ(OPCODE_NULL, Module::opcodeFromMachineByte(72));
  ASSERT_EQ(OPCODE_POINTER, Module::opcodeFromMachineByte(73));
  ASSERT_EQ(OPCODE_WHILE, Module::opcodeFromMachineByte(74));
  ASSERT_EQ(OPCODE_reserved, Module::opcodeFromMachineByte(75));
  ASSERT_EQ(OPCODE_reserved, Module::opcodeFromMachineByte(76));
  ASSERT_EQ(OPCODE_reserved, Module::opcodeFromMachineByte(77));
  ASSERT_EQ(OPCODE_TRUE, Module::opcodeFromMachineByte(78));
  ASSERT_EQ(OPCODE_REGEX, Module::opcodeFromMachineByte(79));
  ASSERT_EQ(OPCODE_reserved, Module::opcodeFromMachineByte(80));
  ASSERT_EQ(OPCODE_reserved, Module::opcodeFromMachineByte(81));
  ASSERT_EQ(OPCODE_reserved, Module::opcodeFromMachineByte(82));
  ASSERT_EQ(OPCODE_reserved, Module::opcodeFromMachineByte(83));
  ASSERT_EQ(OPCODE_VOID, Module::opcodeFromMachineByte(84));
  ASSERT_EQ(OPCODE_reserved, Module::opcodeFromMachineByte(85));
  ASSERT_EQ(OPCODE_reserved, Module::opcodeFromMachineByte(86));
  ASSERT_EQ(OPCODE_reserved, Module::opcodeFromMachineByte(87));
  ASSERT_EQ(OPCODE_reserved, Module::opcodeFromMachineByte(88));
  ASSERT_EQ(OPCODE_reserved, Module::opcodeFromMachineByte(89));
  ASSERT_EQ(OPCODE_reserved, Module::opcodeFromMachineByte(90));
  ASSERT_EQ(OPCODE_reserved, Module::opcodeFromMachineByte(91));
  ASSERT_EQ(OPCODE_DECLARE, Module::opcodeFromMachineByte(92));
  ASSERT_EQ(OPCODE_DECLARE, Module::opcodeFromMachineByte(93));
  ASSERT_EQ(OPCODE_reserved, Module::opcodeFromMachineByte(94));
  ASSERT_EQ(OPCODE_reserved, Module::opcodeFromMachineByte(95));
  ASSERT_EQ(OPCODE_reserved, Module::opcodeFromMachineByte(96));
  ASSERT_EQ(OPCODE_reserved, Module::opcodeFromMachineByte(97));
  ASSERT_EQ(OPCODE_FUNCTION, Module::opcodeFromMachineByte(98));
  ASSERT_EQ(OPCODE_FUNCTION, Module::opcodeFromMachineByte(99));
  ASSERT_EQ(OPCODE_reserved, Module::opcodeFromMachineByte(100));
  ASSERT_EQ(OPCODE_reserved, Module::opcodeFromMachineByte(101));
  ASSERT_EQ(OPCODE_BOOL, Module::opcodeFromMachineByte(102));
  ASSERT_EQ(OPCODE_BOOL, Module::opcodeFromMachineByte(103));
  ASSERT_EQ(OPCODE_GENERATOR, Module::opcodeFromMachineByte(104));
  ASSERT_EQ(OPCODE_GENERATOR, Module::opcodeFromMachineByte(105));
  ASSERT_EQ(OPCODE_reserved, Module::opcodeFromMachineByte(106));
  ASSERT_EQ(OPCODE_reserved, Module::opcodeFromMachineByte(107));
  ASSERT_EQ(OPCODE_RETURN, Module::opcodeFromMachineByte(108));
  ASSERT_EQ(OPCODE_RETURN, Module::opcodeFromMachineByte(109));
  ASSERT_EQ(OPCODE_IF, Module::opcodeFromMachineByte(110));
  ASSERT_EQ(OPCODE_IF, Module::opcodeFromMachineByte(111));
  ASSERT_EQ(OPCODE_reserved, Module::opcodeFromMachineByte(112));
  ASSERT_EQ(OPCODE_reserved, Module::opcodeFromMachineByte(113));
  ASSERT_EQ(OPCODE_THROW, Module::opcodeFromMachineByte(114));
  ASSERT_EQ(OPCODE_THROW, Module::opcodeFromMachineByte(115));
  ASSERT_EQ(OPCODE_TRY, Module::opcodeFromMachineByte(116));
  ASSERT_EQ(OPCODE_TRY, Module::opcodeFromMachineByte(117));
  ASSERT_EQ(OPCODE_TRY, Module::opcodeFromMachineByte(118));
  ASSERT_EQ(OPCODE_TRY, Module::opcodeFromMachineByte(119));
  ASSERT_EQ(OPCODE_YIELD, Module::opcodeFromMachineByte(120));
  ASSERT_EQ(OPCODE_YIELD, Module::opcodeFromMachineByte(121));
  ASSERT_EQ(OPCODE_SWITCH, Module::opcodeFromMachineByte(122));
  ASSERT_EQ(OPCODE_SWITCH, Module::opcodeFromMachineByte(123));
  ASSERT_EQ(OPCODE_SWITCH, Module::opcodeFromMachineByte(124));
  ASSERT_EQ(OPCODE_SWITCH, Module::opcodeFromMachineByte(125));
  ASSERT_EQ(OPCODE_reserved, Module::opcodeFromMachineByte(126));
  ASSERT_EQ(OPCODE_reserved, Module::opcodeFromMachineByte(127));
  ASSERT_EQ(OPCODE_CASE, Module::opcodeFromMachineByte(128));
  ASSERT_EQ(OPCODE_CASE, Module::opcodeFromMachineByte(129));
  ASSERT_EQ(OPCODE_CASE, Module::opcodeFromMachineByte(130));
  ASSERT_EQ(OPCODE_CASE, Module::opcodeFromMachineByte(131));
  ASSERT_EQ(OPCODE_reserved, Module::opcodeFromMachineByte(132));
  ASSERT_EQ(OPCODE_reserved, Module::opcodeFromMachineByte(133));
  ASSERT_EQ(OPCODE_VARARGS, Module::opcodeFromMachineByte(134));
  ASSERT_EQ(OPCODE_VARARGS, Module::opcodeFromMachineByte(135));
  ASSERT_EQ(OPCODE_VARARGS, Module::opcodeFromMachineByte(136));
  ASSERT_EQ(OPCODE_VARARGS, Module::opcodeFromMachineByte(137));
  ASSERT_EQ(OPCODE_reserved, Module::opcodeFromMachineByte(138));
  ASSERT_EQ(OPCODE_OPTIONAL, Module::opcodeFromMachineByte(139));
  ASSERT_EQ(OPCODE_OPTIONAL, Module::opcodeFromMachineByte(140));
  ASSERT_EQ(OPCODE_reserved, Module::opcodeFromMachineByte(141));
  ASSERT_EQ(OPCODE_reserved, Module::opcodeFromMachineByte(142));
  ASSERT_EQ(OPCODE_reserved, Module::opcodeFromMachineByte(143));
  ASSERT_EQ(OPCODE_reserved, Module::opcodeFromMachineByte(144));
  ASSERT_EQ(OPCODE_REQUIRED, Module::opcodeFromMachineByte(145));
  ASSERT_EQ(OPCODE_REQUIRED, Module::opcodeFromMachineByte(146));
  ASSERT_EQ(OPCODE_reserved, Module::opcodeFromMachineByte(147));
  ASSERT_EQ(OPCODE_reserved, Module::opcodeFromMachineByte(148));
  ASSERT_EQ(OPCODE_reserved, Module::opcodeFromMachineByte(149));
  ASSERT_EQ(OPCODE_reserved, Module::opcodeFromMachineByte(150));
  ASSERT_EQ(OPCODE_ATTRIBUTE, Module::opcodeFromMachineByte(151));
  ASSERT_EQ(OPCODE_ATTRIBUTE, Module::opcodeFromMachineByte(152));
  ASSERT_EQ(OPCODE_ATTRIBUTE, Module::opcodeFromMachineByte(153));
  ASSERT_EQ(OPCODE_ATTRIBUTE, Module::opcodeFromMachineByte(154));
  ASSERT_EQ(OPCODE_ATTRIBUTE, Module::opcodeFromMachineByte(155));
  ASSERT_EQ(OPCODE_reserved, Module::opcodeFromMachineByte(156));
  ASSERT_EQ(OPCODE_BLOCK, Module::opcodeFromMachineByte(157));
  ASSERT_EQ(OPCODE_BLOCK, Module::opcodeFromMachineByte(158));
  ASSERT_EQ(OPCODE_BLOCK, Module::opcodeFromMachineByte(159));
  ASSERT_EQ(OPCODE_BLOCK, Module::opcodeFromMachineByte(160));
  ASSERT_EQ(OPCODE_BLOCK, Module::opcodeFromMachineByte(161));
  ASSERT_EQ(OPCODE_reserved, Module::opcodeFromMachineByte(162));
  ASSERT_EQ(OPCODE_CALL, Module::opcodeFromMachineByte(163));
  ASSERT_EQ(OPCODE_CALL, Module::opcodeFromMachineByte(164));
  ASSERT_EQ(OPCODE_CALL, Module::opcodeFromMachineByte(165));
  ASSERT_EQ(OPCODE_CALL, Module::opcodeFromMachineByte(166));
  ASSERT_EQ(OPCODE_CALL, Module::opcodeFromMachineByte(167));
  ASSERT_EQ(OPCODE_reserved, Module::opcodeFromMachineByte(168));
  ASSERT_EQ(OPCODE_CALLABLE, Module::opcodeFromMachineByte(169));
  ASSERT_EQ(OPCODE_CALLABLE, Module::opcodeFromMachineByte(170));
  ASSERT_EQ(OPCODE_CALLABLE, Module::opcodeFromMachineByte(171));
  ASSERT_EQ(OPCODE_CALLABLE, Module::opcodeFromMachineByte(172));
  ASSERT_EQ(OPCODE_CALLABLE, Module::opcodeFromMachineByte(173));
  ASSERT_EQ(OPCODE_reserved, Module::opcodeFromMachineByte(174));
  ASSERT_EQ(OPCODE_CHOICE, Module::opcodeFromMachineByte(175));
  ASSERT_EQ(OPCODE_CHOICE, Module::opcodeFromMachineByte(176));
  ASSERT_EQ(OPCODE_CHOICE, Module::opcodeFromMachineByte(177));
  ASSERT_EQ(OPCODE_CHOICE, Module::opcodeFromMachineByte(178));
  ASSERT_EQ(OPCODE_CHOICE, Module::opcodeFromMachineByte(179));
  ASSERT_EQ(OPCODE_reserved, Module::opcodeFromMachineByte(180));
  ASSERT_EQ(OPCODE_DEFAULT, Module::opcodeFromMachineByte(181));
  ASSERT_EQ(OPCODE_DEFAULT, Module::opcodeFromMachineByte(182));
  ASSERT_EQ(OPCODE_DEFAULT, Module::opcodeFromMachineByte(183));
  ASSERT_EQ(OPCODE_DEFAULT, Module::opcodeFromMachineByte(184));
  ASSERT_EQ(OPCODE_DEFAULT, Module::opcodeFromMachineByte(185));
  ASSERT_EQ(OPCODE_reserved, Module::opcodeFromMachineByte(186));
  ASSERT_EQ(OPCODE_EXTENSIBLE, Module::opcodeFromMachineByte(187));
  ASSERT_EQ(OPCODE_EXTENSIBLE, Module::opcodeFromMachineByte(188));
  ASSERT_EQ(OPCODE_EXTENSIBLE, Module::opcodeFromMachineByte(189));
  ASSERT_EQ(OPCODE_EXTENSIBLE, Module::opcodeFromMachineByte(190));
  ASSERT_EQ(OPCODE_EXTENSIBLE, Module::opcodeFromMachineByte(191));
  ASSERT_EQ(OPCODE_reserved, Module::opcodeFromMachineByte(192));
  ASSERT_EQ(OPCODE_LAMBDA, Module::opcodeFromMachineByte(193));
  ASSERT_EQ(OPCODE_LAMBDA, Module::opcodeFromMachineByte(194));
  ASSERT_EQ(OPCODE_LAMBDA, Module::opcodeFromMachineByte(195));
  ASSERT_EQ(OPCODE_LAMBDA, Module::opcodeFromMachineByte(196));
  ASSERT_EQ(OPCODE_LAMBDA, Module::opcodeFromMachineByte(197));
  ASSERT_EQ(OPCODE_reserved, Module::opcodeFromMachineByte(198));
  ASSERT_EQ(OPCODE_LENGTH, Module::opcodeFromMachineByte(199));
  ASSERT_EQ(OPCODE_LENGTH, Module::opcodeFromMachineByte(200));
  ASSERT_EQ(OPCODE_LENGTH, Module::opcodeFromMachineByte(201));
  ASSERT_EQ(OPCODE_LENGTH, Module::opcodeFromMachineByte(202));
  ASSERT_EQ(OPCODE_LENGTH, Module::opcodeFromMachineByte(203));
  ASSERT_EQ(OPCODE_reserved, Module::opcodeFromMachineByte(204));
  ASSERT_EQ(OPCODE_UNION, Module::opcodeFromMachineByte(205));
  ASSERT_EQ(OPCODE_UNION, Module::opcodeFromMachineByte(206));
  ASSERT_EQ(OPCODE_UNION, Module::opcodeFromMachineByte(207));
  ASSERT_EQ(OPCODE_UNION, Module::opcodeFromMachineByte(208));
  ASSERT_EQ(OPCODE_UNION, Module::opcodeFromMachineByte(209));
  ASSERT_EQ(OPCODE_AVALUE, Module::opcodeFromMachineByte(210));
  ASSERT_EQ(OPCODE_AVALUE, Module::opcodeFromMachineByte(211));
  ASSERT_EQ(OPCODE_AVALUE, Module::opcodeFromMachineByte(212));
  ASSERT_EQ(OPCODE_AVALUE, Module::opcodeFromMachineByte(213));
  ASSERT_EQ(OPCODE_AVALUE, Module::opcodeFromMachineByte(214));
  ASSERT_EQ(OPCODE_AVALUE, Module::opcodeFromMachineByte(215));
  ASSERT_EQ(OPCODE_FLOAT, Module::opcodeFromMachineByte(216));
  ASSERT_EQ(OPCODE_FLOAT, Module::opcodeFromMachineByte(217));
  ASSERT_EQ(OPCODE_FLOAT, Module::opcodeFromMachineByte(218));
  ASSERT_EQ(OPCODE_FLOAT, Module::opcodeFromMachineByte(219));
  ASSERT_EQ(OPCODE_FLOAT, Module::opcodeFromMachineByte(220));
  ASSERT_EQ(OPCODE_FLOAT, Module::opcodeFromMachineByte(221));
  ASSERT_EQ(OPCODE_INT, Module::opcodeFromMachineByte(222));
  ASSERT_EQ(OPCODE_INT, Module::opcodeFromMachineByte(223));
  ASSERT_EQ(OPCODE_INT, Module::opcodeFromMachineByte(224));
  ASSERT_EQ(OPCODE_INT, Module::opcodeFromMachineByte(225));
  ASSERT_EQ(OPCODE_INT, Module::opcodeFromMachineByte(226));
  ASSERT_EQ(OPCODE_INT, Module::opcodeFromMachineByte(227));
  ASSERT_EQ(OPCODE_OBJECT, Module::opcodeFromMachineByte(228));
  ASSERT_EQ(OPCODE_OBJECT, Module::opcodeFromMachineByte(229));
  ASSERT_EQ(OPCODE_OBJECT, Module::opcodeFromMachineByte(230));
  ASSERT_EQ(OPCODE_OBJECT, Module::opcodeFromMachineByte(231));
  ASSERT_EQ(OPCODE_OBJECT, Module::opcodeFromMachineByte(232));
  ASSERT_EQ(OPCODE_OBJECT, Module::opcodeFromMachineByte(233));
  ASSERT_EQ(OPCODE_OVALUE, Module::opcodeFromMachineByte(234));
  ASSERT_EQ(OPCODE_OVALUE, Module::opcodeFromMachineByte(235));
  ASSERT_EQ(OPCODE_OVALUE, Module::opcodeFromMachineByte(236));
  ASSERT_EQ(OPCODE_OVALUE, Module::opcodeFromMachineByte(237));
  ASSERT_EQ(OPCODE_OVALUE, Module::opcodeFromMachineByte(238));
  ASSERT_EQ(OPCODE_OVALUE, Module::opcodeFromMachineByte(239));
  ASSERT_EQ(OPCODE_STRING, Module::opcodeFromMachineByte(240));
  ASSERT_EQ(OPCODE_STRING, Module::opcodeFromMachineByte(241));
  ASSERT_EQ(OPCODE_STRING, Module::opcodeFromMachineByte(242));
  ASSERT_EQ(OPCODE_STRING, Module::opcodeFromMachineByte(243));
  ASSERT_EQ(OPCODE_STRING, Module::opcodeFromMachineByte(244));
  ASSERT_EQ(OPCODE_STRING, Module::opcodeFromMachineByte(245));
  ASSERT_EQ(OPCODE_TYPE, Module::opcodeFromMachineByte(246));
  ASSERT_EQ(OPCODE_TYPE, Module::opcodeFromMachineByte(247));
  ASSERT_EQ(OPCODE_TYPE, Module::opcodeFromMachineByte(248));
  ASSERT_EQ(OPCODE_TYPE, Module::opcodeFromMachineByte(249));
  ASSERT_EQ(OPCODE_TYPE, Module::opcodeFromMachineByte(250));
  ASSERT_EQ(OPCODE_TYPE, Module::opcodeFromMachineByte(251));
  ASSERT_EQ(OPCODE_reserved, Module::opcodeFromMachineByte(252));
  ASSERT_EQ(OPCODE_MODULE, Module::opcodeFromMachineByte(253));
  ASSERT_EQ(OPCODE_MODULE, Module::opcodeFromMachineByte(254));
  ASSERT_EQ(OPCODE_MODULE, Module::opcodeFromMachineByte(255));
}

TEST(TestModule, OpcodeEncode0) {
  ASSERT_EQ(72, OpcodeProperties::from(OPCODE_NULL).encode(0));
  ASSERT_EQ(0, OpcodeProperties::from(OPCODE_NULL).encode(1));
}

TEST(TestModule, OpcodeEncode1) {
  ASSERT_EQ(0, OpcodeProperties::from(OPCODE_UNARY).encode(0));
  ASSERT_EQ(1, OpcodeProperties::from(OPCODE_UNARY).encode(1));
  ASSERT_EQ(0, OpcodeProperties::from(OPCODE_UNARY).encode(2));
}

TEST(TestModule, OpcodeEncode2) {
  ASSERT_EQ(0, OpcodeProperties::from(OPCODE_BINARY).encode(0));
  ASSERT_EQ(0, OpcodeProperties::from(OPCODE_BINARY).encode(1));
  ASSERT_EQ(2, OpcodeProperties::from(OPCODE_BINARY).encode(2));
  ASSERT_EQ(0, OpcodeProperties::from(OPCODE_BINARY).encode(3));
}

TEST(TestModule, OpcodeEncode3) {
  ASSERT_EQ(0, OpcodeProperties::from(OPCODE_TERNARY).encode(0));
  ASSERT_EQ(0, OpcodeProperties::from(OPCODE_TERNARY).encode(1));
  ASSERT_EQ(0, OpcodeProperties::from(OPCODE_TERNARY).encode(2));
  ASSERT_EQ(3, OpcodeProperties::from(OPCODE_TERNARY).encode(3));
  ASSERT_EQ(0, OpcodeProperties::from(OPCODE_TERNARY).encode(4));
}

TEST(TestModule, OpcodeEncode4) {
  ASSERT_EQ(0, OpcodeProperties::from(OPCODE_FOR).encode(0));
  ASSERT_EQ(0, OpcodeProperties::from(OPCODE_FOR).encode(1));
  ASSERT_EQ(0, OpcodeProperties::from(OPCODE_FOR).encode(2));
  ASSERT_EQ(0, OpcodeProperties::from(OPCODE_FOR).encode(3));
  ASSERT_EQ(28, OpcodeProperties::from(OPCODE_FOR).encode(4));
  ASSERT_EQ(0, OpcodeProperties::from(OPCODE_FOR).encode(5));
}

TEST(TestModule, OpcodeEncode5) {
  ASSERT_EQ(210, OpcodeProperties::from(OPCODE_AVALUE).encode(0));
  ASSERT_EQ(211, OpcodeProperties::from(OPCODE_AVALUE).encode(1));
  ASSERT_EQ(212, OpcodeProperties::from(OPCODE_AVALUE).encode(2));
  ASSERT_EQ(213, OpcodeProperties::from(OPCODE_AVALUE).encode(3));
  ASSERT_EQ(214, OpcodeProperties::from(OPCODE_AVALUE).encode(4));
  ASSERT_EQ(215, OpcodeProperties::from(OPCODE_AVALUE).encode(5));
  ASSERT_EQ(215, OpcodeProperties::from(OPCODE_AVALUE).encode(6));
  ASSERT_EQ(215, OpcodeProperties::from(OPCODE_AVALUE).encode(7));
}

TEST(TestModule, OperatorUnary) {
  ASSERT_STREQ("-", OperatorProperties::from(OPERATOR_NEG).name);
  ASSERT_EQ(OPCLASS_UNARY, OperatorProperties::from(OPERATOR_NEG).opclass);
  ASSERT_EQ(1u, OperatorProperties::from(OPERATOR_NEG).operands);
}

TEST(TestModule, OperatorBinary) {
  ASSERT_STREQ("-", OperatorProperties::from(OPERATOR_SUB).name);
  ASSERT_EQ(OPCLASS_BINARY, OperatorProperties::from(OPERATOR_SUB).opclass);
  ASSERT_EQ(2u, OperatorProperties::from(OPERATOR_SUB).operands);
}

TEST(TestModule, OperatorTernary) {
  ASSERT_STREQ("?:", OperatorProperties::from(OPERATOR_TERNARY).name);
  ASSERT_EQ(OPCLASS_TERNARY, OperatorProperties::from(OPERATOR_TERNARY).opclass);
  ASSERT_EQ(3u, OperatorProperties::from(OPERATOR_TERNARY).operands);
}

TEST(TestModule, OperatorCompare) {
  ASSERT_STREQ("<", OperatorProperties::from(OPERATOR_LT).name);
  ASSERT_EQ(OPCLASS_COMPARE, OperatorProperties::from(OPERATOR_LT).opclass);
  ASSERT_EQ(2u, OperatorProperties::from(OPERATOR_LT).operands);
}

TEST(TestModule, Constants) {
  // Test that the magic header starts with a UTF-8 continuation byte
  const uint8_t magic[] = { MAGIC };
  ASSERT_EQ(0x80, magic[0] & 0xC0);
  // Test that the "end" opcode is zero
  ASSERT_EQ(OPCODE_END, 0);
  // Test that well-known opcodes have implicit operands
  ASSERT_LT(OPCODE_IVALUE, EGG_VM_ISTART);
  ASSERT_LT(OPCODE_FVALUE, EGG_VM_ISTART);
  ASSERT_LT(OPCODE_SVALUE, EGG_VM_ISTART);
  ASSERT_LT(OPCODE_UNARY, EGG_VM_ISTART);
  ASSERT_LT(OPCODE_BINARY, EGG_VM_ISTART);
  ASSERT_LT(OPCODE_TERNARY, EGG_VM_ISTART);
  // Test that operator enums fit into [0..128] for operand fitting
  ASSERT_EQ(OPERATOR_TERNARY, 128);
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
  const uint8_t minimal[] = { MAGIC SECTION_CODE, OPCODE_MODULE, OPCODE_BLOCK, OPCODE_NOOP };
  auto module = ModuleFactory::fromMemory(allocator, std::begin(minimal), std::end(minimal));
  ASSERT_NE(nullptr, module);
  Node root{ &module->getRootNode() };
  ASSERT_NE(nullptr, root);
  ASSERT_EQ(OPCODE_MODULE, root->getOpcode());
  ASSERT_EQ(1u, root->getChildren());
  Node child{ &root->getChild(0) };
  ASSERT_EQ(OPCODE_BLOCK, child->getOpcode());
  ASSERT_EQ(1u, child->getChildren());
  Node grandchild{ &child->getChild(0) };
  ASSERT_EQ(OPCODE_NOOP, grandchild->getOpcode());
  ASSERT_EQ(0u, grandchild->getChildren());
}

TEST(TestModule, ToBinaryStream) {
  egg::test::Allocator allocator;
  const uint8_t minimal[] = { MAGIC SECTION_CODE, OPCODE_MODULE, OPCODE_BLOCK, OPCODE_NOOP };
  auto module = ModuleFactory::fromMemory(allocator, std::begin(minimal), std::end(minimal));
  ASSERT_NE(nullptr, module);
  std::stringstream ss;
  ModuleFactory::toBinaryStream(*module, ss);
  auto binary = ss.str();
  ASSERT_EQ(sizeof(minimal), binary.length());
  ASSERT_EQ(0, std::memcmp(binary.data(), minimal, sizeof(minimal)));
}

TEST(TestModule, ToMemory) {
  egg::test::Allocator allocator;
  const uint8_t minimal[] = { MAGIC SECTION_CODE, OPCODE_MODULE, OPCODE_BLOCK, OPCODE_NOOP };
  auto module = ModuleFactory::fromMemory(allocator, std::begin(minimal), std::end(minimal));
  ASSERT_NE(nullptr, module);
  auto memory = ModuleFactory::toMemory(allocator, *module);
  ASSERT_NE(nullptr, memory);
  ASSERT_EQ(sizeof(minimal), memory->bytes());
  ASSERT_EQ(0, std::memcmp(memory->begin(), minimal, sizeof(minimal)));
}

TEST(TestModule, ModuleBuilder) {
  egg::test::Allocator allocator;
  ModuleBuilder builder(allocator);
  auto noop = builder.createNode(OPCODE_NOOP);
  auto block = builder.createNode(OPCODE_BLOCK, std::move(noop));
  auto original = builder.createModule(std::move(block));
  auto module = ModuleFactory::fromRootNode(allocator, *original);
  ASSERT_NE(nullptr, module);
  Node root{ &module->getRootNode() };
  ASSERT_EQ(original.get(), root.get());
  ASSERT_EQ(OPCODE_MODULE, root->getOpcode());
  ASSERT_EQ(1u, root->getChildren());
  Node child{ &root->getChild(0) };
  ASSERT_EQ(OPCODE_BLOCK, child->getOpcode());
  ASSERT_EQ(1u, child->getChildren());
  Node grandchild{ &child->getChild(0) };
  ASSERT_EQ(OPCODE_NOOP, grandchild->getOpcode());
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
  ASSERT_EQ(OPCODE_IVALUE, value->getOpcode());
  ASSERT_EQ(123456789, value->getInt());
  ASSERT_EQ(0u, value->getChildren());
  value.set(&avalue->getChild(1));
  ASSERT_EQ(OPCODE_IVALUE, value->getOpcode());
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
  ASSERT_EQ(OPCODE_FVALUE, value->getOpcode());
  ASSERT_EQ(123456789.0, value->getFloat());
  ASSERT_EQ(0u, value->getChildren());
  value.set(&avalue->getChild(1));
  ASSERT_EQ(OPCODE_FVALUE, value->getOpcode());
  ASSERT_EQ(-123456789.0, value->getFloat());
  ASSERT_EQ(0u, value->getChildren());
  value.set(&avalue->getChild(2));
  ASSERT_EQ(OPCODE_FVALUE, value->getOpcode());
  ASSERT_EQ(-0.125, value->getFloat());
  ASSERT_EQ(0u, value->getChildren());
  value.set(&avalue->getChild(3));
  ASSERT_EQ(OPCODE_FVALUE, value->getOpcode());
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
  ASSERT_EQ(OPCODE_SVALUE, value->getOpcode());
  ASSERT_STRING("", value->getString());
  ASSERT_EQ(0u, value->getChildren());
  value.set(&avalue->getChild(1));
  ASSERT_EQ(OPCODE_SVALUE, value->getOpcode());
  ASSERT_STRING("hello", value->getString());
  ASSERT_EQ(0u, value->getChildren());
}

TEST(TestModule, BuildOperator) {
  egg::test::Allocator allocator;
  ModuleBuilder builder(allocator);
  auto avalue = roundTripArray(builder, {
    builder.createOperator(OPCODE_UNARY, OPERATOR_REF, { builder.createNode(OPCODE_NULL) })
  });
  ASSERT_EQ(1u, avalue->getChildren());
  Node value;
  value.set(&avalue->getChild(0));
  ASSERT_EQ(OPCODE_UNARY, value->getOpcode());
  ASSERT_EQ(OPERATOR_REF, value->getInt()); // the integer operator code
  ASSERT_EQ(1u, value->getChildren());
  value.set(&value->getChild(0));
  ASSERT_EQ(OPCODE_NULL, value->getOpcode());
  ASSERT_EQ(0u, value->getChildren());
}

TEST(TestModule, BuildWithAttribute) {
  egg::test::Allocator allocator;
  ModuleBuilder builder(allocator);
  auto avalue = roundTripArray(builder, {
    builder.withAttribute("a", String("alpha")).withAttribute("b", 123).createOperator(OPCODE_UNARY, OPERATOR_REF,{ builder.createNode(OPCODE_NULL) })
  });
  ASSERT_EQ(1u, avalue->getChildren());
  Node value;
  value.set(&avalue->getChild(0));
  ASSERT_EQ(OPCODE_UNARY, value->getOpcode());
  ASSERT_EQ(OPERATOR_REF, value->getInt()); // the integer operator code
  ASSERT_EQ(1u, value->getChildren());
  ASSERT_EQ(2u, value->getAttributes());
  value.set(&value->getAttribute(0));
  ASSERT_EQ(OPCODE_ATTRIBUTE, value->getOpcode());
  ASSERT_EQ(2u, value->getChildren());
  value.set(&value->getChild(1));
  ASSERT_EQ(OPCODE_SVALUE, value->getOpcode());
  ASSERT_EQ("alpha", value->getString().toUTF8());
}
