#include "ovum/test.h"
#include "ovum/node.h"
#include "ovum/module.h"

using namespace egg::ovum;

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
