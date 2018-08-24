#include "ovum/test.h"
#include "ovum/node.h"

using namespace egg::ovum;

TEST(TestNode, ChildrenFromMachineByte) {
  ASSERT_EQ(0u, childrenFromMachineByte(0));
  ASSERT_EQ(1u, childrenFromMachineByte(1));
  ASSERT_EQ(2u, childrenFromMachineByte(2));
  ASSERT_EQ(3u, childrenFromMachineByte(3));
  ASSERT_EQ(4u, childrenFromMachineByte(4));
  ASSERT_EQ(SIZE_MAX, childrenFromMachineByte(5));
  ASSERT_EQ(0u, childrenFromMachineByte(6));
  ASSERT_EQ(4u, childrenFromMachineByte(250));
  ASSERT_EQ(SIZE_MAX, childrenFromMachineByte(251));
  ASSERT_EQ(0u, childrenFromMachineByte(252));
  ASSERT_EQ(1u, childrenFromMachineByte(253));
  ASSERT_EQ(2u, childrenFromMachineByte(254));
  ASSERT_EQ(3u, childrenFromMachineByte(255));
}

TEST(TestNode, OpcodeFromMachineByte) {
  // Taken from egg-notes.xlsx
  ASSERT_EQ(OPCODE_END, opcodeFromMachineByte(0));
  ASSERT_EQ(OPCODE_UNARY, opcodeFromMachineByte(1));
  ASSERT_EQ(OPCODE_BINARY, opcodeFromMachineByte(2));
  ASSERT_EQ(OPCODE_TERNARY, opcodeFromMachineByte(3));
  ASSERT_EQ(OPCODE_reserved, opcodeFromMachineByte(4));
  ASSERT_EQ(OPCODE_reserved, opcodeFromMachineByte(5));
  ASSERT_EQ(OPCODE_IVALUE, opcodeFromMachineByte(6));
  ASSERT_EQ(OPCODE_META, opcodeFromMachineByte(7));
  ASSERT_EQ(OPCODE_COMPARE, opcodeFromMachineByte(8));
  ASSERT_EQ(OPCODE_reserved, opcodeFromMachineByte(9));
  ASSERT_EQ(OPCODE_reserved, opcodeFromMachineByte(10));
  ASSERT_EQ(OPCODE_reserved, opcodeFromMachineByte(11));
  ASSERT_EQ(OPCODE_FVALUE, opcodeFromMachineByte(12));
  ASSERT_EQ(OPCODE_reserved, opcodeFromMachineByte(13));
  ASSERT_EQ(OPCODE_MUTATE, opcodeFromMachineByte(14));
  ASSERT_EQ(OPCODE_reserved, opcodeFromMachineByte(15));
  ASSERT_EQ(OPCODE_reserved, opcodeFromMachineByte(16));
  ASSERT_EQ(OPCODE_reserved, opcodeFromMachineByte(17));
  ASSERT_EQ(OPCODE_SVALUE, opcodeFromMachineByte(18));
  ASSERT_EQ(OPCODE_reserved, opcodeFromMachineByte(19));
  ASSERT_EQ(OPCODE_reserved, opcodeFromMachineByte(20));
  ASSERT_EQ(OPCODE_reserved, opcodeFromMachineByte(21));
  ASSERT_EQ(OPCODE_reserved, opcodeFromMachineByte(22));
  ASSERT_EQ(OPCODE_reserved, opcodeFromMachineByte(23));
  ASSERT_EQ(OPCODE_ANY, opcodeFromMachineByte(24));
  ASSERT_EQ(OPCODE_ASSERT, opcodeFromMachineByte(25));
  ASSERT_EQ(OPCODE_ASSIGN, opcodeFromMachineByte(26));
  ASSERT_EQ(OPCODE_CATCH, opcodeFromMachineByte(27));
  ASSERT_EQ(OPCODE_FOR, opcodeFromMachineByte(28));
  ASSERT_EQ(OPCODE_reserved, opcodeFromMachineByte(29));
  ASSERT_EQ(OPCODE_ANYQ, opcodeFromMachineByte(30));
  ASSERT_EQ(OPCODE_DECREMENT, opcodeFromMachineByte(31));
  ASSERT_EQ(OPCODE_BYNAME, opcodeFromMachineByte(32));
  ASSERT_EQ(OPCODE_FOREACH, opcodeFromMachineByte(33));
  ASSERT_EQ(OPCODE_INDEXABLE, opcodeFromMachineByte(34));
  ASSERT_EQ(OPCODE_reserved, opcodeFromMachineByte(35));
  ASSERT_EQ(OPCODE_BREAK, opcodeFromMachineByte(36));
  ASSERT_EQ(OPCODE_ELLIPSIS, opcodeFromMachineByte(37));
  ASSERT_EQ(OPCODE_DO, opcodeFromMachineByte(38));
  ASSERT_EQ(OPCODE_GUARD, opcodeFromMachineByte(39));
  ASSERT_EQ(OPCODE_reserved, opcodeFromMachineByte(40));
  ASSERT_EQ(OPCODE_reserved, opcodeFromMachineByte(41));
  ASSERT_EQ(OPCODE_CONTINUE, opcodeFromMachineByte(42));
  ASSERT_EQ(OPCODE_IDENTIFIER, opcodeFromMachineByte(43));
  ASSERT_EQ(OPCODE_INDEX, opcodeFromMachineByte(44));
  ASSERT_EQ(OPCODE_reserved, opcodeFromMachineByte(45));
  ASSERT_EQ(OPCODE_reserved, opcodeFromMachineByte(46));
  ASSERT_EQ(OPCODE_reserved, opcodeFromMachineByte(47));
  ASSERT_EQ(OPCODE_FALSE, opcodeFromMachineByte(48));
  ASSERT_EQ(OPCODE_INCREMENT, opcodeFromMachineByte(49));
  ASSERT_EQ(OPCODE_reserved, opcodeFromMachineByte(50));
  ASSERT_EQ(OPCODE_reserved, opcodeFromMachineByte(51));
  ASSERT_EQ(OPCODE_reserved, opcodeFromMachineByte(52));
  ASSERT_EQ(OPCODE_reserved, opcodeFromMachineByte(53));
  ASSERT_EQ(OPCODE_FINITE, opcodeFromMachineByte(54));
  ASSERT_EQ(OPCODE_ITERABLE, opcodeFromMachineByte(55));
  ASSERT_EQ(OPCODE_NAMED, opcodeFromMachineByte(56));
  ASSERT_EQ(OPCODE_reserved, opcodeFromMachineByte(57));
  ASSERT_EQ(OPCODE_reserved, opcodeFromMachineByte(58));
  ASSERT_EQ(OPCODE_reserved, opcodeFromMachineByte(59));
  ASSERT_EQ(OPCODE_INFERRED, opcodeFromMachineByte(60));
  ASSERT_EQ(OPCODE_NOT, opcodeFromMachineByte(61));
  ASSERT_EQ(OPCODE_PROPERTY, opcodeFromMachineByte(62));
  ASSERT_EQ(OPCODE_reserved, opcodeFromMachineByte(63));
  ASSERT_EQ(OPCODE_reserved, opcodeFromMachineByte(64));
  ASSERT_EQ(OPCODE_reserved, opcodeFromMachineByte(65));
  ASSERT_EQ(OPCODE_NOOP, opcodeFromMachineByte(66));
  ASSERT_EQ(OPCODE_POINTEE, opcodeFromMachineByte(67));
  ASSERT_EQ(OPCODE_PROPERTYQ, opcodeFromMachineByte(68));
  ASSERT_EQ(OPCODE_reserved, opcodeFromMachineByte(69));
  ASSERT_EQ(OPCODE_reserved, opcodeFromMachineByte(70));
  ASSERT_EQ(OPCODE_reserved, opcodeFromMachineByte(71));
  ASSERT_EQ(OPCODE_NULL, opcodeFromMachineByte(72));
  ASSERT_EQ(OPCODE_POINTER, opcodeFromMachineByte(73));
  ASSERT_EQ(OPCODE_WHILE, opcodeFromMachineByte(74));
  ASSERT_EQ(OPCODE_reserved, opcodeFromMachineByte(75));
  ASSERT_EQ(OPCODE_reserved, opcodeFromMachineByte(76));
  ASSERT_EQ(OPCODE_reserved, opcodeFromMachineByte(77));
  ASSERT_EQ(OPCODE_TRUE, opcodeFromMachineByte(78));
  ASSERT_EQ(OPCODE_REGEX, opcodeFromMachineByte(79));
  ASSERT_EQ(OPCODE_reserved, opcodeFromMachineByte(80));
  ASSERT_EQ(OPCODE_reserved, opcodeFromMachineByte(81));
  ASSERT_EQ(OPCODE_reserved, opcodeFromMachineByte(82));
  ASSERT_EQ(OPCODE_reserved, opcodeFromMachineByte(83));
  ASSERT_EQ(OPCODE_VOID, opcodeFromMachineByte(84));
  ASSERT_EQ(OPCODE_reserved, opcodeFromMachineByte(85));
  ASSERT_EQ(OPCODE_reserved, opcodeFromMachineByte(86));
  ASSERT_EQ(OPCODE_reserved, opcodeFromMachineByte(87));
  ASSERT_EQ(OPCODE_reserved, opcodeFromMachineByte(88));
  ASSERT_EQ(OPCODE_reserved, opcodeFromMachineByte(89));
  ASSERT_EQ(OPCODE_reserved, opcodeFromMachineByte(90));
  ASSERT_EQ(OPCODE_reserved, opcodeFromMachineByte(91));
  ASSERT_EQ(OPCODE_DECLARE, opcodeFromMachineByte(92));
  ASSERT_EQ(OPCODE_DECLARE, opcodeFromMachineByte(93));
  ASSERT_EQ(OPCODE_reserved, opcodeFromMachineByte(94));
  ASSERT_EQ(OPCODE_reserved, opcodeFromMachineByte(95));
  ASSERT_EQ(OPCODE_reserved, opcodeFromMachineByte(96));
  ASSERT_EQ(OPCODE_reserved, opcodeFromMachineByte(97));
  ASSERT_EQ(OPCODE_FUNCTION, opcodeFromMachineByte(98));
  ASSERT_EQ(OPCODE_FUNCTION, opcodeFromMachineByte(99));
  ASSERT_EQ(OPCODE_reserved, opcodeFromMachineByte(100));
  ASSERT_EQ(OPCODE_reserved, opcodeFromMachineByte(101));
  ASSERT_EQ(OPCODE_BOOL, opcodeFromMachineByte(102));
  ASSERT_EQ(OPCODE_BOOL, opcodeFromMachineByte(103));
  ASSERT_EQ(OPCODE_GENERATOR, opcodeFromMachineByte(104));
  ASSERT_EQ(OPCODE_GENERATOR, opcodeFromMachineByte(105));
  ASSERT_EQ(OPCODE_reserved, opcodeFromMachineByte(106));
  ASSERT_EQ(OPCODE_reserved, opcodeFromMachineByte(107));
  ASSERT_EQ(OPCODE_RETURN, opcodeFromMachineByte(108));
  ASSERT_EQ(OPCODE_RETURN, opcodeFromMachineByte(109));
  ASSERT_EQ(OPCODE_IF, opcodeFromMachineByte(110));
  ASSERT_EQ(OPCODE_IF, opcodeFromMachineByte(111));
  ASSERT_EQ(OPCODE_reserved, opcodeFromMachineByte(112));
  ASSERT_EQ(OPCODE_reserved, opcodeFromMachineByte(113));
  ASSERT_EQ(OPCODE_THROW, opcodeFromMachineByte(114));
  ASSERT_EQ(OPCODE_THROW, opcodeFromMachineByte(115));
  ASSERT_EQ(OPCODE_VARARGS, opcodeFromMachineByte(116));
  ASSERT_EQ(OPCODE_VARARGS, opcodeFromMachineByte(117));
  ASSERT_EQ(OPCODE_VARARGS, opcodeFromMachineByte(118));
  ASSERT_EQ(OPCODE_VARARGS, opcodeFromMachineByte(119));
  ASSERT_EQ(OPCODE_YIELD, opcodeFromMachineByte(120));
  ASSERT_EQ(OPCODE_YIELD, opcodeFromMachineByte(121));
  ASSERT_EQ(OPCODE_SWITCH, opcodeFromMachineByte(122));
  ASSERT_EQ(OPCODE_SWITCH, opcodeFromMachineByte(123));
  ASSERT_EQ(OPCODE_SWITCH, opcodeFromMachineByte(124));
  ASSERT_EQ(OPCODE_SWITCH, opcodeFromMachineByte(125));
  ASSERT_EQ(OPCODE_reserved, opcodeFromMachineByte(126));
  ASSERT_EQ(OPCODE_reserved, opcodeFromMachineByte(127));
  ASSERT_EQ(OPCODE_CASE, opcodeFromMachineByte(128));
  ASSERT_EQ(OPCODE_CASE, opcodeFromMachineByte(129));
  ASSERT_EQ(OPCODE_CASE, opcodeFromMachineByte(130));
  ASSERT_EQ(OPCODE_CASE, opcodeFromMachineByte(131));
  ASSERT_EQ(OPCODE_reserved, opcodeFromMachineByte(132));
  ASSERT_EQ(OPCODE_reserved, opcodeFromMachineByte(133));
  ASSERT_EQ(OPCODE_reserved, opcodeFromMachineByte(134));
  ASSERT_EQ(OPCODE_reserved, opcodeFromMachineByte(135));
  ASSERT_EQ(OPCODE_reserved, opcodeFromMachineByte(136));
  ASSERT_EQ(OPCODE_reserved, opcodeFromMachineByte(137));
  ASSERT_EQ(OPCODE_reserved, opcodeFromMachineByte(138));
  ASSERT_EQ(OPCODE_OPTIONAL, opcodeFromMachineByte(139));
  ASSERT_EQ(OPCODE_OPTIONAL, opcodeFromMachineByte(140));
  ASSERT_EQ(OPCODE_reserved, opcodeFromMachineByte(141));
  ASSERT_EQ(OPCODE_reserved, opcodeFromMachineByte(142));
  ASSERT_EQ(OPCODE_reserved, opcodeFromMachineByte(143));
  ASSERT_EQ(OPCODE_reserved, opcodeFromMachineByte(144));
  ASSERT_EQ(OPCODE_REQUIRED, opcodeFromMachineByte(145));
  ASSERT_EQ(OPCODE_REQUIRED, opcodeFromMachineByte(146));
  ASSERT_EQ(OPCODE_TRY, opcodeFromMachineByte(147));
  ASSERT_EQ(OPCODE_TRY, opcodeFromMachineByte(148));
  ASSERT_EQ(OPCODE_TRY, opcodeFromMachineByte(149));
  ASSERT_EQ(OPCODE_reserved, opcodeFromMachineByte(150));
  ASSERT_EQ(OPCODE_ATTRIBUTE, opcodeFromMachineByte(151));
  ASSERT_EQ(OPCODE_ATTRIBUTE, opcodeFromMachineByte(152));
  ASSERT_EQ(OPCODE_ATTRIBUTE, opcodeFromMachineByte(153));
  ASSERT_EQ(OPCODE_ATTRIBUTE, opcodeFromMachineByte(154));
  ASSERT_EQ(OPCODE_ATTRIBUTE, opcodeFromMachineByte(155));
  ASSERT_EQ(OPCODE_reserved, opcodeFromMachineByte(156));
  ASSERT_EQ(OPCODE_BLOCK, opcodeFromMachineByte(157));
  ASSERT_EQ(OPCODE_BLOCK, opcodeFromMachineByte(158));
  ASSERT_EQ(OPCODE_BLOCK, opcodeFromMachineByte(159));
  ASSERT_EQ(OPCODE_BLOCK, opcodeFromMachineByte(160));
  ASSERT_EQ(OPCODE_BLOCK, opcodeFromMachineByte(161));
  ASSERT_EQ(OPCODE_reserved, opcodeFromMachineByte(162));
  ASSERT_EQ(OPCODE_CALL, opcodeFromMachineByte(163));
  ASSERT_EQ(OPCODE_CALL, opcodeFromMachineByte(164));
  ASSERT_EQ(OPCODE_CALL, opcodeFromMachineByte(165));
  ASSERT_EQ(OPCODE_CALL, opcodeFromMachineByte(166));
  ASSERT_EQ(OPCODE_CALL, opcodeFromMachineByte(167));
  ASSERT_EQ(OPCODE_reserved, opcodeFromMachineByte(168));
  ASSERT_EQ(OPCODE_CALLABLE, opcodeFromMachineByte(169));
  ASSERT_EQ(OPCODE_CALLABLE, opcodeFromMachineByte(170));
  ASSERT_EQ(OPCODE_CALLABLE, opcodeFromMachineByte(171));
  ASSERT_EQ(OPCODE_CALLABLE, opcodeFromMachineByte(172));
  ASSERT_EQ(OPCODE_CALLABLE, opcodeFromMachineByte(173));
  ASSERT_EQ(OPCODE_reserved, opcodeFromMachineByte(174));
  ASSERT_EQ(OPCODE_CHOICE, opcodeFromMachineByte(175));
  ASSERT_EQ(OPCODE_CHOICE, opcodeFromMachineByte(176));
  ASSERT_EQ(OPCODE_CHOICE, opcodeFromMachineByte(177));
  ASSERT_EQ(OPCODE_CHOICE, opcodeFromMachineByte(178));
  ASSERT_EQ(OPCODE_CHOICE, opcodeFromMachineByte(179));
  ASSERT_EQ(OPCODE_reserved, opcodeFromMachineByte(180));
  ASSERT_EQ(OPCODE_DEFAULT, opcodeFromMachineByte(181));
  ASSERT_EQ(OPCODE_DEFAULT, opcodeFromMachineByte(182));
  ASSERT_EQ(OPCODE_DEFAULT, opcodeFromMachineByte(183));
  ASSERT_EQ(OPCODE_DEFAULT, opcodeFromMachineByte(184));
  ASSERT_EQ(OPCODE_DEFAULT, opcodeFromMachineByte(185));
  ASSERT_EQ(OPCODE_reserved, opcodeFromMachineByte(186));
  ASSERT_EQ(OPCODE_EXTENSIBLE, opcodeFromMachineByte(187));
  ASSERT_EQ(OPCODE_EXTENSIBLE, opcodeFromMachineByte(188));
  ASSERT_EQ(OPCODE_EXTENSIBLE, opcodeFromMachineByte(189));
  ASSERT_EQ(OPCODE_EXTENSIBLE, opcodeFromMachineByte(190));
  ASSERT_EQ(OPCODE_EXTENSIBLE, opcodeFromMachineByte(191));
  ASSERT_EQ(OPCODE_reserved, opcodeFromMachineByte(192));
  ASSERT_EQ(OPCODE_LAMBDA, opcodeFromMachineByte(193));
  ASSERT_EQ(OPCODE_LAMBDA, opcodeFromMachineByte(194));
  ASSERT_EQ(OPCODE_LAMBDA, opcodeFromMachineByte(195));
  ASSERT_EQ(OPCODE_LAMBDA, opcodeFromMachineByte(196));
  ASSERT_EQ(OPCODE_LAMBDA, opcodeFromMachineByte(197));
  ASSERT_EQ(OPCODE_reserved, opcodeFromMachineByte(198));
  ASSERT_EQ(OPCODE_LENGTH, opcodeFromMachineByte(199));
  ASSERT_EQ(OPCODE_LENGTH, opcodeFromMachineByte(200));
  ASSERT_EQ(OPCODE_LENGTH, opcodeFromMachineByte(201));
  ASSERT_EQ(OPCODE_LENGTH, opcodeFromMachineByte(202));
  ASSERT_EQ(OPCODE_LENGTH, opcodeFromMachineByte(203));
  ASSERT_EQ(OPCODE_reserved, opcodeFromMachineByte(204));
  ASSERT_EQ(OPCODE_UNION, opcodeFromMachineByte(205));
  ASSERT_EQ(OPCODE_UNION, opcodeFromMachineByte(206));
  ASSERT_EQ(OPCODE_UNION, opcodeFromMachineByte(207));
  ASSERT_EQ(OPCODE_UNION, opcodeFromMachineByte(208));
  ASSERT_EQ(OPCODE_UNION, opcodeFromMachineByte(209));
  ASSERT_EQ(OPCODE_AVALUE, opcodeFromMachineByte(210));
  ASSERT_EQ(OPCODE_AVALUE, opcodeFromMachineByte(211));
  ASSERT_EQ(OPCODE_AVALUE, opcodeFromMachineByte(212));
  ASSERT_EQ(OPCODE_AVALUE, opcodeFromMachineByte(213));
  ASSERT_EQ(OPCODE_AVALUE, opcodeFromMachineByte(214));
  ASSERT_EQ(OPCODE_AVALUE, opcodeFromMachineByte(215));
  ASSERT_EQ(OPCODE_FLOAT, opcodeFromMachineByte(216));
  ASSERT_EQ(OPCODE_FLOAT, opcodeFromMachineByte(217));
  ASSERT_EQ(OPCODE_FLOAT, opcodeFromMachineByte(218));
  ASSERT_EQ(OPCODE_FLOAT, opcodeFromMachineByte(219));
  ASSERT_EQ(OPCODE_FLOAT, opcodeFromMachineByte(220));
  ASSERT_EQ(OPCODE_FLOAT, opcodeFromMachineByte(221));
  ASSERT_EQ(OPCODE_INT, opcodeFromMachineByte(222));
  ASSERT_EQ(OPCODE_INT, opcodeFromMachineByte(223));
  ASSERT_EQ(OPCODE_INT, opcodeFromMachineByte(224));
  ASSERT_EQ(OPCODE_INT, opcodeFromMachineByte(225));
  ASSERT_EQ(OPCODE_INT, opcodeFromMachineByte(226));
  ASSERT_EQ(OPCODE_INT, opcodeFromMachineByte(227));
  ASSERT_EQ(OPCODE_OBJECT, opcodeFromMachineByte(228));
  ASSERT_EQ(OPCODE_OBJECT, opcodeFromMachineByte(229));
  ASSERT_EQ(OPCODE_OBJECT, opcodeFromMachineByte(230));
  ASSERT_EQ(OPCODE_OBJECT, opcodeFromMachineByte(231));
  ASSERT_EQ(OPCODE_OBJECT, opcodeFromMachineByte(232));
  ASSERT_EQ(OPCODE_OBJECT, opcodeFromMachineByte(233));
  ASSERT_EQ(OPCODE_OVALUE, opcodeFromMachineByte(234));
  ASSERT_EQ(OPCODE_OVALUE, opcodeFromMachineByte(235));
  ASSERT_EQ(OPCODE_OVALUE, opcodeFromMachineByte(236));
  ASSERT_EQ(OPCODE_OVALUE, opcodeFromMachineByte(237));
  ASSERT_EQ(OPCODE_OVALUE, opcodeFromMachineByte(238));
  ASSERT_EQ(OPCODE_OVALUE, opcodeFromMachineByte(239));
  ASSERT_EQ(OPCODE_STRING, opcodeFromMachineByte(240));
  ASSERT_EQ(OPCODE_STRING, opcodeFromMachineByte(241));
  ASSERT_EQ(OPCODE_STRING, opcodeFromMachineByte(242));
  ASSERT_EQ(OPCODE_STRING, opcodeFromMachineByte(243));
  ASSERT_EQ(OPCODE_STRING, opcodeFromMachineByte(244));
  ASSERT_EQ(OPCODE_STRING, opcodeFromMachineByte(245));
  ASSERT_EQ(OPCODE_TYPE, opcodeFromMachineByte(246));
  ASSERT_EQ(OPCODE_TYPE, opcodeFromMachineByte(247));
  ASSERT_EQ(OPCODE_TYPE, opcodeFromMachineByte(248));
  ASSERT_EQ(OPCODE_TYPE, opcodeFromMachineByte(249));
  ASSERT_EQ(OPCODE_TYPE, opcodeFromMachineByte(250));
  ASSERT_EQ(OPCODE_TYPE, opcodeFromMachineByte(251));
  ASSERT_EQ(OPCODE_reserved, opcodeFromMachineByte(252));
  ASSERT_EQ(OPCODE_MODULE, opcodeFromMachineByte(253));
  ASSERT_EQ(OPCODE_MODULE, opcodeFromMachineByte(254));
  ASSERT_EQ(OPCODE_MODULE, opcodeFromMachineByte(255));
}

TEST(TestNode, OpcodeEncode0) {
  ASSERT_EQ(72, opcodeProperties(OPCODE_NULL).encode(0));
  ASSERT_EQ(0, opcodeProperties(OPCODE_NULL).encode(1));
}

TEST(TestNode, OpcodeEncode1) {
  ASSERT_EQ(0, opcodeProperties(OPCODE_UNARY).encode(0));
  ASSERT_EQ(1, opcodeProperties(OPCODE_UNARY).encode(1));
  ASSERT_EQ(0, opcodeProperties(OPCODE_UNARY).encode(2));
}

TEST(TestNode, OpcodeEncode2) {
  ASSERT_EQ(0, opcodeProperties(OPCODE_BINARY).encode(0));
  ASSERT_EQ(0, opcodeProperties(OPCODE_BINARY).encode(1));
  ASSERT_EQ(2, opcodeProperties(OPCODE_BINARY).encode(2));
  ASSERT_EQ(0, opcodeProperties(OPCODE_BINARY).encode(3));
}

TEST(TestNode, OpcodeEncode3) {
  ASSERT_EQ(0, opcodeProperties(OPCODE_TERNARY).encode(0));
  ASSERT_EQ(0, opcodeProperties(OPCODE_TERNARY).encode(1));
  ASSERT_EQ(0, opcodeProperties(OPCODE_TERNARY).encode(2));
  ASSERT_EQ(3, opcodeProperties(OPCODE_TERNARY).encode(3));
  ASSERT_EQ(0, opcodeProperties(OPCODE_TERNARY).encode(4));
}

TEST(TestNode, OpcodeEncode4) {
  ASSERT_EQ(0, opcodeProperties(OPCODE_FOR).encode(0));
  ASSERT_EQ(0, opcodeProperties(OPCODE_FOR).encode(1));
  ASSERT_EQ(0, opcodeProperties(OPCODE_FOR).encode(2));
  ASSERT_EQ(0, opcodeProperties(OPCODE_FOR).encode(3));
  ASSERT_EQ(28, opcodeProperties(OPCODE_FOR).encode(4));
  ASSERT_EQ(0, opcodeProperties(OPCODE_FOR).encode(5));
}

TEST(TestNode, OpcodeEncode5) {
  ASSERT_EQ(210, opcodeProperties(OPCODE_AVALUE).encode(0));
  ASSERT_EQ(211, opcodeProperties(OPCODE_AVALUE).encode(1));
  ASSERT_EQ(212, opcodeProperties(OPCODE_AVALUE).encode(2));
  ASSERT_EQ(213, opcodeProperties(OPCODE_AVALUE).encode(3));
  ASSERT_EQ(214, opcodeProperties(OPCODE_AVALUE).encode(4));
  ASSERT_EQ(215, opcodeProperties(OPCODE_AVALUE).encode(5));
  ASSERT_EQ(215, opcodeProperties(OPCODE_AVALUE).encode(6));
  ASSERT_EQ(215, opcodeProperties(OPCODE_AVALUE).encode(7));
}

TEST(TestNode, OperatorUnary) {
  ASSERT_STREQ("-", operatorProperties(OPERATOR_NEG).name);
  ASSERT_EQ(OPCLASS_UNARY, operatorProperties(OPERATOR_NEG).opclass);
  ASSERT_EQ(1u, operatorProperties(OPERATOR_NEG).operands);
}

TEST(TestNode, Create0) {
  egg::test::Allocator allocator;
  auto parent = NodeFactory::create(allocator, OPCODE_NOOP);
  ASSERT_EQ(OPCODE_NOOP, parent->getOpcode());
  ASSERT_EQ(0u, parent->getChildren());
  ASSERT_THROW(parent->getChild(0), std::out_of_range);
  ASSERT_EQ(0u, parent->getAttributes());
  ASSERT_THROW(parent->getAttribute(0), std::out_of_range);
  ASSERT_THROW(parent->getInt(), std::runtime_error);
  ASSERT_THROW(parent->getFloat(), std::runtime_error);
  ASSERT_THROW(parent->getString(), std::runtime_error);
}

TEST(TestNode, Create1) {
  egg::test::Allocator allocator;
  auto child = NodeFactory::create(allocator, OPCODE_NULL);
  auto parent = NodeFactory::create(allocator, OPCODE_AVALUE, std::move(child));
  ASSERT_EQ(OPCODE_AVALUE, parent->getOpcode());
  ASSERT_EQ(1u, parent->getChildren());
  ASSERT_THROW(parent->getChild(1), std::out_of_range);
  ASSERT_EQ(0u, parent->getAttributes());
  ASSERT_THROW(parent->getAttribute(0), std::out_of_range);
  ASSERT_THROW(parent->getInt(), std::runtime_error);
  ASSERT_THROW(parent->getFloat(), std::runtime_error);
  ASSERT_THROW(parent->getString(), std::runtime_error);
  ASSERT_EQ(child.get(), &parent->getChild(0));
}

TEST(TestNode, Create2) {
  egg::test::Allocator allocator;
  auto child0 = NodeFactory::create(allocator, OPCODE_FALSE);
  auto child1 = NodeFactory::create(allocator, OPCODE_TRUE);
  auto parent = NodeFactory::create(allocator, OPCODE_AVALUE, std::move(child0), std::move(child1));
  ASSERT_EQ(OPCODE_AVALUE, parent->getOpcode());
  ASSERT_EQ(2u, parent->getChildren());
  ASSERT_THROW(parent->getChild(2), std::out_of_range);
  ASSERT_EQ(0u, parent->getAttributes());
  ASSERT_THROW(parent->getAttribute(0), std::out_of_range);
  ASSERT_THROW(parent->getInt(), std::runtime_error);
  ASSERT_THROW(parent->getFloat(), std::runtime_error);
  ASSERT_THROW(parent->getString(), std::runtime_error);
  ASSERT_EQ(child0.get(), &parent->getChild(0));
  ASSERT_EQ(child1.get(), &parent->getChild(1));
}

TEST(TestNode, Create3) {
  egg::test::Allocator allocator;
  auto child0 = NodeFactory::create(allocator, OPCODE_NULL);
  auto child1 = NodeFactory::create(allocator, OPCODE_FALSE);
  auto child2 = NodeFactory::create(allocator, OPCODE_TRUE);
  auto parent = NodeFactory::create(allocator, OPCODE_AVALUE, std::move(child0), std::move(child1), std::move(child2));
  ASSERT_EQ(OPCODE_AVALUE, parent->getOpcode());
  ASSERT_EQ(3u, parent->getChildren());
  ASSERT_THROW(parent->getChild(3), std::out_of_range);
  ASSERT_EQ(0u, parent->getAttributes());
  ASSERT_THROW(parent->getAttribute(0), std::out_of_range);
  ASSERT_THROW(parent->getInt(), std::runtime_error);
  ASSERT_THROW(parent->getFloat(), std::runtime_error);
  ASSERT_THROW(parent->getString(), std::runtime_error);
  ASSERT_EQ(child0.get(), &parent->getChild(0));
  ASSERT_EQ(child1.get(), &parent->getChild(1));
  ASSERT_EQ(child2.get(), &parent->getChild(2));
}

TEST(TestNode, Create4) {
  egg::test::Allocator allocator;
  auto child0 = NodeFactory::create(allocator, OPCODE_NULL);
  auto child1 = NodeFactory::create(allocator, OPCODE_FALSE);
  auto child2 = NodeFactory::create(allocator, OPCODE_TRUE);
  auto child3 = NodeFactory::create(allocator, OPCODE_VOID);
  auto parent = NodeFactory::create(allocator, OPCODE_AVALUE, std::move(child0), std::move(child1), std::move(child2), std::move(child3));
  ASSERT_EQ(OPCODE_AVALUE, parent->getOpcode());
  ASSERT_EQ(4u, parent->getChildren());
  ASSERT_THROW(parent->getChild(4), std::out_of_range);
  ASSERT_EQ(0u, parent->getAttributes());
  ASSERT_THROW(parent->getAttribute(0), std::out_of_range);
  ASSERT_THROW(parent->getInt(), std::runtime_error);
  ASSERT_THROW(parent->getFloat(), std::runtime_error);
  ASSERT_THROW(parent->getString(), std::runtime_error);
  ASSERT_EQ(child0.get(), &parent->getChild(0));
  ASSERT_EQ(child1.get(), &parent->getChild(1));
  ASSERT_EQ(child2.get(), &parent->getChild(2));
  ASSERT_EQ(child3.get(), &parent->getChild(3));
}

TEST(TestNode, Create5) {
  egg::test::Allocator allocator;
  Nodes children{
    NodeFactory::create(allocator, OPCODE_NULL),
    NodeFactory::create(allocator, OPCODE_FALSE),
    NodeFactory::create(allocator, OPCODE_TRUE),
    NodeFactory::create(allocator, OPCODE_VOID),
    NodeFactory::create(allocator, OPCODE_NOOP)
  };
  auto parent = NodeFactory::create(allocator, OPCODE_AVALUE, Nodes(children)); // prevent move of children
  ASSERT_EQ(OPCODE_AVALUE, parent->getOpcode());
  ASSERT_EQ(5u, parent->getChildren());
  ASSERT_THROW(parent->getChild(5), std::out_of_range);
  ASSERT_EQ(0u, parent->getAttributes());
  ASSERT_THROW(parent->getAttribute(0), std::out_of_range);
  ASSERT_THROW(parent->getInt(), std::runtime_error);
  ASSERT_THROW(parent->getFloat(), std::runtime_error);
  ASSERT_THROW(parent->getString(), std::runtime_error);
  ASSERT_EQ(children[0].get(), &parent->getChild(0));
  ASSERT_EQ(children[1].get(), &parent->getChild(1));
  ASSERT_EQ(children[2].get(), &parent->getChild(2));
  ASSERT_EQ(children[3].get(), &parent->getChild(3));
  ASSERT_EQ(children[4].get(), &parent->getChild(4));
}

TEST(TestNode, CreateWithInt0) {
  egg::test::Allocator allocator;
  Int operand{ 123456789 };
  auto parent = NodeFactory::create(allocator, OPCODE_IVALUE, {}, {}, operand);
  ASSERT_EQ(OPCODE_IVALUE, parent->getOpcode());
  ASSERT_EQ(0u, parent->getChildren());
  ASSERT_THROW(parent->getChild(0), std::out_of_range);
  ASSERT_EQ(0u, parent->getAttributes());
  ASSERT_THROW(parent->getAttribute(0), std::out_of_range);
  ASSERT_EQ(operand, parent->getInt());
  ASSERT_THROW(parent->getFloat(), std::runtime_error);
  ASSERT_THROW(parent->getString(), std::runtime_error);
}

TEST(TestNode, CreateWithFloat0) {
  egg::test::Allocator allocator;
  Float operand{ 3.14159 };
  auto parent = NodeFactory::create(allocator, OPCODE_FVALUE, {}, {}, operand);
  ASSERT_EQ(OPCODE_FVALUE, parent->getOpcode());
  ASSERT_EQ(0u, parent->getChildren());
  ASSERT_THROW(parent->getChild(0), std::out_of_range);
  ASSERT_EQ(0u, parent->getAttributes());
  ASSERT_THROW(parent->getAttribute(0), std::out_of_range);
  ASSERT_THROW(parent->getInt(), std::runtime_error);
  ASSERT_EQ(operand, parent->getFloat());
  ASSERT_THROW(parent->getString(), std::runtime_error);
}

TEST(TestNode, CreateWithString0) {
  egg::test::Allocator allocator;
  String operand{ "hello" };
  auto parent = NodeFactory::create(allocator, OPCODE_SVALUE, {}, {}, operand);
  ASSERT_EQ(OPCODE_SVALUE, parent->getOpcode());
  ASSERT_EQ(0u, parent->getChildren());
  ASSERT_THROW(parent->getChild(0), std::out_of_range);
  ASSERT_EQ(0u, parent->getAttributes());
  ASSERT_THROW(parent->getAttribute(0), std::out_of_range);
  ASSERT_THROW(parent->getInt(), std::runtime_error);
  ASSERT_THROW(parent->getFloat(), std::runtime_error);
  ASSERT_STRING(operand, parent->getString());
}

TEST(TestNode, CreateWithInt1) {
  egg::test::Allocator allocator;
  Nodes children{ NodeFactory::create(allocator, OPCODE_NULL) };
  Int operand{ 123456789 };
  auto parent = NodeFactory::create(allocator, OPCODE_UNARY, &children, nullptr, operand);
  ASSERT_EQ(OPCODE_UNARY, parent->getOpcode());
  ASSERT_EQ(1u, parent->getChildren());
  ASSERT_THROW(parent->getChild(1), std::out_of_range);
  ASSERT_EQ(0u, parent->getAttributes());
  ASSERT_THROW(parent->getAttribute(0), std::out_of_range);
  ASSERT_EQ(operand, parent->getInt());
  ASSERT_THROW(parent->getFloat(), std::runtime_error);
  ASSERT_THROW(parent->getString(), std::runtime_error);
  ASSERT_EQ(children[0].get(), &parent->getChild(0));
}

TEST(TestNode, CreateWithInt2) {
  egg::test::Allocator allocator;
  Nodes children{ NodeFactory::create(allocator, OPCODE_FALSE), NodeFactory::create(allocator, OPCODE_TRUE) };
  Int operand{ 123456789 };
  auto parent = NodeFactory::create(allocator, OPCODE_BINARY, &children, nullptr, operand);
  ASSERT_EQ(OPCODE_BINARY, parent->getOpcode());
  ASSERT_EQ(2u, parent->getChildren());
  ASSERT_THROW(parent->getChild(2), std::out_of_range);
  ASSERT_EQ(0u, parent->getAttributes());
  ASSERT_THROW(parent->getAttribute(0), std::out_of_range);
  ASSERT_EQ(operand, parent->getInt());
  ASSERT_THROW(parent->getFloat(), std::runtime_error);
  ASSERT_THROW(parent->getString(), std::runtime_error);
  ASSERT_EQ(children[0].get(), &parent->getChild(0));
  ASSERT_EQ(children[1].get(), &parent->getChild(1));
}

TEST(TestNode, CreateWithInt3) {
  egg::test::Allocator allocator;
  Nodes children{ NodeFactory::create(allocator, OPCODE_NULL), NodeFactory::create(allocator, OPCODE_FALSE), NodeFactory::create(allocator, OPCODE_TRUE) };
  Nodes attributes{};
  Int operand{ 123456789 };
  auto parent = NodeFactory::create(allocator, OPCODE_TERNARY, &children, nullptr, operand);
  ASSERT_EQ(OPCODE_TERNARY, parent->getOpcode());
  ASSERT_EQ(3u, parent->getChildren());
  ASSERT_THROW(parent->getChild(3), std::out_of_range);
  ASSERT_EQ(0u, parent->getAttributes());
  ASSERT_THROW(parent->getAttribute(0), std::out_of_range);
  ASSERT_EQ(operand, parent->getInt());
  ASSERT_THROW(parent->getFloat(), std::runtime_error);
  ASSERT_THROW(parent->getString(), std::runtime_error);
  ASSERT_EQ(children[0].get(), &parent->getChild(0));
  ASSERT_EQ(children[1].get(), &parent->getChild(1));
  ASSERT_EQ(children[2].get(), &parent->getChild(2));
}

TEST(TestNode, CreateWithAttributes0) {
  egg::test::Allocator allocator;
  Nodes children{};
  auto attribute = NodeFactory::create(allocator, OPCODE_ATTRIBUTE, NodeFactory::create(allocator, OPCODE_NULL));
  Nodes attributes{ attribute };
  Int operand{ 123456789 };
  auto parent = NodeFactory::create(allocator, OPCODE_IVALUE, &children, &attributes, operand);
  ASSERT_EQ(OPCODE_IVALUE, parent->getOpcode());
  ASSERT_EQ(0u, parent->getChildren());
  ASSERT_THROW(parent->getChild(0), std::out_of_range);
  ASSERT_EQ(1u, parent->getAttributes());
  ASSERT_THROW(parent->getAttribute(1), std::out_of_range);
  ASSERT_EQ(operand, parent->getInt());
  ASSERT_THROW(parent->getFloat(), std::runtime_error);
  ASSERT_THROW(parent->getString(), std::runtime_error);
  ASSERT_EQ(attributes[0].get(), &parent->getAttribute(0));
  ASSERT_EQ(attribute.get(), &parent->getAttribute(0));
}

TEST(TestNode, CreateWithAttributes1) {
  egg::test::Allocator allocator;
  Nodes children{ NodeFactory::create(allocator, OPCODE_FALSE) };
  auto attribute = NodeFactory::create(allocator, OPCODE_ATTRIBUTE, NodeFactory::create(allocator, OPCODE_NULL));
  Nodes attributes{ attribute };
  Int operand{ 123456789 };
  auto parent = NodeFactory::create(allocator, OPCODE_UNARY, &children, &attributes, operand);
  ASSERT_EQ(OPCODE_UNARY, parent->getOpcode());
  ASSERT_EQ(1u, parent->getChildren());
  ASSERT_THROW(parent->getChild(1), std::out_of_range);
  ASSERT_EQ(1u, parent->getAttributes());
  ASSERT_THROW(parent->getAttribute(1), std::out_of_range);
  ASSERT_EQ(operand, parent->getInt());
  ASSERT_THROW(parent->getFloat(), std::runtime_error);
  ASSERT_THROW(parent->getString(), std::runtime_error);
  ASSERT_EQ(children[0].get(), &parent->getChild(0));
  ASSERT_EQ(attributes[0].get(), &parent->getAttribute(0));
  ASSERT_EQ(attribute.get(), &parent->getAttribute(0));
}

TEST(TestNode, CreateWithAttributes2) {
  egg::test::Allocator allocator;
  Nodes children{ NodeFactory::create(allocator, OPCODE_FALSE), NodeFactory::create(allocator, OPCODE_TRUE) };
  auto attribute = NodeFactory::create(allocator, OPCODE_ATTRIBUTE, NodeFactory::create(allocator, OPCODE_NULL));
  Nodes attributes{ attribute };
  Int operand{ 123456789 };
  auto parent = NodeFactory::create(allocator, OPCODE_BINARY, &children, &attributes, operand);
  ASSERT_EQ(OPCODE_BINARY, parent->getOpcode());
  ASSERT_EQ(2u, parent->getChildren());
  ASSERT_THROW(parent->getChild(2), std::out_of_range);
  ASSERT_EQ(1u, parent->getAttributes());
  ASSERT_THROW(parent->getAttribute(1), std::out_of_range);
  ASSERT_EQ(operand, parent->getInt());
  ASSERT_THROW(parent->getFloat(), std::runtime_error);
  ASSERT_THROW(parent->getString(), std::runtime_error);
  ASSERT_EQ(children[0].get(), &parent->getChild(0));
  ASSERT_EQ(children[1].get(), &parent->getChild(1));
  ASSERT_EQ(attributes[0].get(), &parent->getAttribute(0));
  ASSERT_EQ(attribute.get(), &parent->getAttribute(0));
}
