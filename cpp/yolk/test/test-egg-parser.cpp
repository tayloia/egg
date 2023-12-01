#include "yolk/test.h"
#include "yolk/lexers.h"
#include "yolk/egg-tokenizer.h"
#include "yolk/egg-parser.h"

using Issue = egg::yolk::IEggParser::Issue;
using Node = egg::yolk::IEggParser::Node;
using Result = egg::yolk::IEggParser::Result;

namespace {
  std::ostream& printNode(std::ostream& os, const Node& node, bool ranges);

  std::ostream& printValue(std::ostream& os, const egg::ovum::HardValue& value, char quote) {
    egg::ovum::Print::Options options{};
    options.quote = quote;
    egg::ovum::Print::write(os, value, options);
    return os;
  }

  std::ostream& printRange(std::ostream& os, const egg::ovum::SourceRange& value) {
    os << '@';
    egg::ovum::Print::write(os, value, egg::ovum::Print::Options::DEFAULT);
    return os;
  }

  template<typename T>
  std::ostream& printNodeExtra(std::ostream& os, const char* prefix, T extra, const Node& node, bool ranges) {
    os << '(' << prefix;
    if (ranges) {
      printRange(os, node.range);
    }
    os << ' ' << '\'';
    egg::ovum::Print::write(os, extra, egg::ovum::Print::Options::DEFAULT);
    os << '\'';
    for (auto& child : node.children) {
      os << ' ';
      printNode(os, *child, ranges);
    }
    return os << ')';
  }

  std::ostream& printNodeChildren(std::ostream& os, const char* prefix, const Node& node, bool ranges) {
    os << '(' << prefix;
    if (ranges) {
      printRange(os, node.range);
    }
    for (auto& child : node.children) {
      os << ' ';
      printNode(os, *child, ranges);
    }
    return os << ')';
  }

  std::ostream& printNode(std::ostream& os, const Node& node, bool ranges) {
    switch (node.kind) {
    case Node::Kind::ModuleRoot:
      for (auto& child : node.children) {
        printNode(os, *child, ranges) << std::endl;
      }
      break;
    case Node::Kind::StmtBlock:
      return printNodeChildren(os, "stmt-block", node, ranges);
    case Node::Kind::StmtDeclareVariable:
      return printNodeExtra(os, "stmt-declare-variable", node.value, node, ranges);
    case Node::Kind::StmtDefineVariable:
      return printNodeExtra(os, "stmt-define-variable", node.value, node, ranges);
    case Node::Kind::StmtCall:
      return printNodeChildren(os, "stmt-call", node, ranges);
    case Node::Kind::StmtForLoop:
      assert(node.children.size() == 4);
      return printNodeChildren(os, "stmt-for-loop", node, ranges);
    case Node::Kind::StmtForEach:
      assert(node.children.size() == 3);
      return printNodeChildren(os, "stmt-for-each", node, ranges);
    case Node::Kind::StmtMutate:
      return printNodeExtra(os, "stmt-mutate", node.op.valueMutationOp, node, ranges);
    case Node::Kind::ExprVariable:
      assert(node.children.empty());
      return printNodeExtra(os, "expr-variable", node.value, node, ranges);
    case Node::Kind::ExprUnary:
      assert(node.children.size() == 1);
      return printNodeExtra(os, "expr-unary", node.op.valueUnaryOp, node, ranges);
    case Node::Kind::ExprBinary:
      assert(node.children.size() == 2);
      return printNodeExtra(os, "expr-binary", node.op.valueBinaryOp, node, ranges);
    case Node::Kind::ExprTernary:
      assert(node.children.size() == 3);
      return printNodeExtra(os, "expr-ternary", node.op.valueTernaryOp, node, ranges);
    case Node::Kind::ExprCall:
      return printNodeChildren(os, "expr-call", node, ranges);
    case Node::Kind::ExprIndex:
      assert(node.children.size() == 2);
      return printNodeChildren(os, "expr-index", node, ranges);
    case Node::Kind::ExprProperty:
      assert(node.children.size() == 2);
      return printNodeChildren(os, "expr-property", node, ranges);
    case Node::Kind::TypeInfer:
      assert(node.children.empty());
      return printNodeChildren(os, "type-infer", node, ranges);
    case Node::Kind::TypeInferQ:
      assert(node.children.empty());
      return printNodeChildren(os, "type-infer-q", node, ranges);
    case Node::Kind::TypeVoid:
      assert(node.children.empty());
      return printNodeChildren(os, "type-void", node, ranges);
    case Node::Kind::TypeBool:
      assert(node.children.empty());
      return printNodeChildren(os, "type-bool", node, ranges);
    case Node::Kind::TypeInt:
      assert(node.children.empty());
      return printNodeChildren(os, "type-int", node, ranges);
    case Node::Kind::TypeFloat:
      assert(node.children.empty());
      return printNodeChildren(os, "type-float", node, ranges);
    case Node::Kind::TypeString:
      assert(node.children.empty());
      return printNodeChildren(os, "type-string", node, ranges);
    case Node::Kind::TypeObject:
      assert(node.children.empty());
      return printNodeChildren(os, "type-object", node, ranges);
    case Node::Kind::TypeAny:
      assert(node.children.empty());
      return printNodeChildren(os, "type-any", node, ranges);
    case Node::Kind::TypeUnary:
      assert(node.children.size() == 1);
      return printNodeExtra(os, "type-unary", node.op.typeUnaryOp, node, ranges);
    case Node::Kind::TypeBinary:
      return printNodeExtra(os, "type-binary", node.op.typeBinaryOp, node, ranges);
    case Node::Kind::Literal:
      assert(node.children.empty());
      return printValue(os, node.value, '"');
    }
    return os;
  }

  std::ostream& operator<<(std::ostream& os, const Issue& issue) {
    switch (issue.severity) {
    case Issue::Severity::Error:
      os << "<ERROR>: ";
      break;
    case Issue::Severity::Warning:
      os << "<WARNING>: ";
      break;
    case Issue::Severity::Information:
      break;
    }
    egg::ovum::Print::write(os, issue.range, egg::ovum::Print::Options::DEFAULT);
    return os << ": " << issue.message.toUTF8();
  }

  Result parseFromLines(egg::test::Allocator& allocator, std::initializer_list<std::string> lines) {
    std::ostringstream ss;
    std::copy(lines.begin(), lines.end(), std::ostream_iterator<std::string>(ss, "\n"));
    auto lexer = egg::yolk::LexerFactory::createFromString(ss.str());
    auto tokenizer = egg::yolk::EggTokenizerFactory::createFromLexer(allocator, lexer);
    auto parser = egg::yolk::EggParserFactory::createFromTokenizer(allocator, tokenizer);
    ASSERT_STRING("", parser->resource());
    auto result = parser->parse();
    for (auto& issue : result.issues) {
      std::cerr << issue << std::endl;
    }
    return result;
  }

  std::string outputFromLines(std::initializer_list<std::string> lines, bool ranges = false) {
    egg::test::Allocator allocator;
    auto result = parseFromLines(allocator, lines);
    std::ostringstream ss;
    for (auto& issue : result.issues) {
      ss << issue << std::endl;
    }
    if (result.root != nullptr) {
      printNode(ss, *result.root, ranges);
    }
    return ss.str();
  }
}

TEST(TestEggParser, Empty) {
  egg::test::Allocator allocator{ egg::test::Allocator::Expectation::NoAllocations };
  auto result = parseFromLines(allocator, { "  // comment" });
  ASSERT_TRUE(result.root != nullptr);
  ASSERT_EQ(Node::Kind::ModuleRoot, result.root->kind);
  ASSERT_EQ(0u, result.root->children.size());
  ASSERT_EQ(0u, result.issues.size());
}

TEST(TestEggParser, WhitespaceComment) {
  egg::test::Allocator allocator{ egg::test::Allocator::Expectation::NoAllocations };
  auto result = parseFromLines(allocator, { "  // comment" });
  ASSERT_TRUE(result.root != nullptr);
  ASSERT_EQ(Node::Kind::ModuleRoot, result.root->kind);
  ASSERT_EQ(0u, result.root->children.size());
  ASSERT_EQ(0u, result.issues.size());
}

TEST(TestEggParser, BadSyntax) {
  egg::test::Allocator allocator;
  auto result = parseFromLines(allocator, { "\n  $" });
  ASSERT_TRUE(result.root == nullptr);
  ASSERT_EQ(1u, result.issues.size());
  auto& actual = result.issues[0];
  ASSERT_EQ(Issue::Severity::Error, actual.severity);
  ASSERT_STRING("Unexpected character: '$'", actual.message);
  ASSERT_EQ(2u, actual.range.begin.line);
  ASSERT_EQ(3u, actual.range.begin.column);
  ASSERT_EQ(2u, actual.range.end.line);
  ASSERT_EQ(3u, actual.range.end.column);
}

TEST(TestEggParser, HelloWorld) {
  std::string actual = outputFromLines({
    "print(\"Hello, World!\");"
  });
  std::string expected = "(stmt-call (expr-call (expr-variable 'print') \"Hello, World!\"))\n";
  ASSERT_EQ(expected, actual);
}

TEST(TestEggParser, ExpressionUnary) {
  std::string actual = outputFromLines({
    "print(-a);"
    });
  std::string expected = "(stmt-call (expr-call (expr-variable 'print') (expr-unary '-' (expr-variable 'a'))))\n";
  ASSERT_EQ(expected, actual);
}

TEST(TestEggParser, ExpressionBinary) {
  std::string actual = outputFromLines({
    "print(a + b);"
    });
  std::string expected = "(stmt-call (expr-call (expr-variable 'print') (expr-binary '+' (expr-variable 'a') (expr-variable 'b'))))\n";
  ASSERT_EQ(expected, actual);
}

TEST(TestEggParser, ExpressionTernary) {
  std::string actual = outputFromLines({
    "print(a ? b : c);"
    });
  std::string expected = "(stmt-call (expr-call (expr-variable 'print') (expr-ternary '?:' (expr-variable 'a') (expr-variable 'b') (expr-variable 'c'))))\n";
  ASSERT_EQ(expected, actual);
}

TEST(TestEggParser, VariableDeclareExplicit) {
  std::string actual = outputFromLines({
    "int a;"
    });
  std::string expected = "(stmt-declare-variable 'a' (type-int))\n";
  ASSERT_EQ(expected, actual);
}

TEST(TestEggParser, VariableDeclareBad) {
  std::string actual = outputFromLines({
    "var a;"
    });
  std::string expected = "<ERROR>: (1,5): Cannot declare variable 'a' using 'var' without an initial value\n";
  ASSERT_EQ(expected, actual);
}

TEST(TestEggParser, VariableDeclareBadNullable) {
  std::string actual = outputFromLines({
    "var? a;"
    });
  std::string expected = "<ERROR>: (1,6): Cannot declare variable 'a' using 'var?' without an initial value\n";
  ASSERT_EQ(expected, actual);
}

TEST(TestEggParser, VariableDefineExplicit) {
  std::string actual = outputFromLines({
    "int a = 123;"
    });
  std::string expected = "(stmt-define-variable 'a' (type-int) 123)\n";
  ASSERT_EQ(expected, actual);
}

TEST(TestEggParser, VariableDefineInfer) {
  std::string actual = outputFromLines({
    "var a = 123;"
    });
  std::string expected = "(stmt-define-variable 'a' (type-infer) 123)\n";
  ASSERT_EQ(expected, actual);
}

TEST(TestEggParser, VariableDefineInferNullable) {
  std::string actual = outputFromLines({
    "var? a = 123;"
    });
  std::string expected = "(stmt-define-variable 'a' (type-infer-q) 123)\n";
  ASSERT_EQ(expected, actual);
}

TEST(TestEggParser, TypeUnaryNullable) {
  std::string actual = outputFromLines({
    "int? a;"
    });
  std::string expected = "(stmt-declare-variable 'a' (type-unary '?' (type-int)))\n";
  ASSERT_EQ(expected, actual);
}

TEST(TestEggParser, TypeUnaryNullableRepeated) {
  std::string actual = outputFromLines({
    "int? ? a;"
    });
  std::string expected = "<WARNING>: (1,6-8): Redundant repetition of type suffix '?'\n"
                         "(stmt-declare-variable 'a' (type-unary '?' (type-int)))\n";
  ASSERT_EQ(expected, actual);
}

TEST(TestEggParser, TypeUnaryPointer) {
  std::string actual = outputFromLines({
    "int* a;"
    });
  std::string expected = "(stmt-declare-variable 'a' (type-unary '*' (type-int)))\n";
  ASSERT_EQ(expected, actual);
}

TEST(TestEggParser, TypeUnaryPointerRepeated) {
  std::string actual = outputFromLines({
    "int** a;"
    });
  std::string expected = "(stmt-declare-variable 'a' (type-unary '*' (type-unary '*' (type-int))))\n";
  ASSERT_EQ(expected, actual);
}

TEST(TestEggParser, TypeUnaryIterator) {
  std::string actual = outputFromLines({
    "int! a;"
    });
  std::string expected = "(stmt-declare-variable 'a' (type-unary '!' (type-int)))\n";
  ASSERT_EQ(expected, actual);
}

TEST(TestEggParser, TypeUnaryIteratorRepeated) {
  std::string actual = outputFromLines({
    "int!! a;"
    });
  std::string expected = "(stmt-declare-variable 'a' (type-unary '!' (type-unary '!' (type-int))))\n";
  ASSERT_EQ(expected, actual);
}

TEST(TestEggParser, TypeUnaryArray) {
  std::string actual = outputFromLines({
    "int[] a;"
    });
  std::string expected = "(stmt-declare-variable 'a' (type-unary '[]' (type-int)))\n";
  ASSERT_EQ(expected, actual);
}

TEST(TestEggParser, TypeUnaryArrayRepeated) {
  std::string actual = outputFromLines({
    "int[][] a;"
    });
  std::string expected = "(stmt-declare-variable 'a' (type-unary '[]' (type-unary '[]' (type-int))))\n";
  ASSERT_EQ(expected, actual);
}

TEST(TestEggParser, TypeBinaryUnion) {
  std::string actual = outputFromLines({
    "int|float a;"
    });
  std::string expected = "(stmt-declare-variable 'a' (type-binary '|' (type-int) (type-float)))\n";
  ASSERT_EQ(expected, actual);
}

TEST(TestEggParser, TypeBinaryUnionRepeated) {
  std::string actual = outputFromLines({
    "int|float|string a;"
    });
  std::string expected = "(stmt-declare-variable 'a' (type-binary '|' (type-int) (type-binary '|' (type-float) (type-string))))\n";
  ASSERT_EQ(expected, actual);
}

TEST(TestEggParser, TypeBinaryMap) {
  std::string actual = outputFromLines({
    "int[string] a;"
    });
  std::string expected = "(stmt-declare-variable 'a' (type-binary '[]' (type-int) (type-string)))\n";
  ASSERT_EQ(expected, actual);
}

TEST(TestEggParser, TypeBinaryMapRepeated) {
  std::string actual = outputFromLines({
    "int[string][float] a;"
    });
  std::string expected = "(stmt-declare-variable 'a' (type-binary '[]' (type-binary '[]' (type-int) (type-string)) (type-float)))\n";
  ASSERT_EQ(expected, actual);
}

TEST(TestEggParser, ConstructString) {
  std::string actual = outputFromLines({
    "var s = string(\"Hello, \", \"World!\");"
    });
  std::string expected = "(stmt-define-variable 's' (type-infer) (expr-call (type-string) \"Hello, \" \"World!\"))\n";
  ASSERT_EQ(expected, actual);
}

TEST(TestEggParser, ValueCall) {
  std::string actual = outputFromLines({
    "var x = assert(false);"
    });
  std::string expected = "(stmt-define-variable 'x' (type-infer) (expr-call (expr-variable 'assert') false))\n";
  ASSERT_EQ(expected, actual);
}

TEST(TestEggParser, ValueIndex) {
  std::string actual = outputFromLines({
    "var x = assert[0];"
    });
  std::string expected = "(stmt-define-variable 'x' (type-infer) (expr-index (expr-variable 'assert') 0))\n";
  ASSERT_EQ(expected, actual);
}

TEST(TestEggParser, ValueProperty) {
  std::string actual = outputFromLines({
    "var x = assert.that;"
    });
  std::string expected = "(stmt-define-variable 'x' (type-infer) (expr-property (expr-variable 'assert') \"that\"))\n";
  ASSERT_EQ(expected, actual);
}

TEST(TestEggParser, ValueNudge) {
  std::string actual = outputFromLines({
    "var x = 0;",
    "++x;",
    "--x;"
    });
  std::string expected = "(stmt-define-variable 'x' (type-infer) 0)\n"
                         "(stmt-mutate '++' (expr-variable 'x'))\n"
                         "(stmt-mutate '--' (expr-variable 'x'))\n";
  ASSERT_EQ(expected, actual);
}

TEST(TestEggParser, StatementForLoop) {
  std::string actual = outputFromLines({
    "for (var i = 0; i < 10; ++i) {}"
    });
  std::string expected = "(stmt-for-loop (stmt-define-variable 'i' (type-infer) 0) (expr-binary '<' (expr-variable 'i') 10) (stmt-mutate '++' (expr-variable 'i')) (stmt-block))\n";
  ASSERT_EQ(expected, actual);
}

TEST(TestEggParser, StatementForEach) {
  std::string actual = outputFromLines({
    "for (var i : \"hello\") { }"
    });
  std::string expected = "(stmt-for-each (type-infer) \"hello\" (stmt-block))\n";
  ASSERT_EQ(expected, actual);
}

TEST(TestEggParser, Ranges) {
  std::string actual = outputFromLines({
  //          1         2         3
  // 123456789012345678901234567890123456789
    "assert(alpha * -beta >= gamma[delta]);"
  }, true);
  std::string expected = "(stmt-call@(1,1-38) (expr-call@(1,1-37) (expr-variable@(1,1-6) 'assert') (expr-binary@(1,8-36) '>=' (expr-binary@(1,8-20) '*' (expr-variable@(1,8-12) 'alpha') (expr-unary@(1,16-20) '-' (expr-variable@(1,17-20) 'beta'))) (expr-index@(1,25-36) (expr-variable@(1,25-29) 'gamma') (expr-variable@(1,31-35) 'delta')))))\n";
  ASSERT_EQ(expected, actual);
}
