#include "ovum/test.h"
#include "ovum/lexer.h"
#include "ovum/egg-tokenizer.h"
#include "ovum/egg-parser.h"

using Issue = egg::ovum::IEggParser::Issue;
using Node = egg::ovum::IEggParser::Node;
using Result = egg::ovum::IEggParser::Result;

namespace {
  using namespace egg::ovum;

  std::ostream& printNode(std::ostream& os, const Node& node, bool ranges);

  std::ostream& printValue(std::ostream& os, const HardValue& value, char quote) {
    Print::Options options{};
    options.quote = quote;
    Printer{ os, options } << value;
    return os;
  }

  std::ostream& printRange(std::ostream& os, const SourceRange& value) {
    os << '@';
    Print::write(os, value, Print::Options::DEFAULT);
    return os;
  }

  template<typename T>
  std::ostream& printNodeExtra(std::ostream& os, const char* prefix, T extra, const Node& node, bool ranges) {
    os << '(' << prefix;
    if (ranges) {
      printRange(os, node.range);
    }
    os << ' ' << '\'';
    Printer{ os, Print::Options::DEFAULT } << extra;
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
    case Node::Kind::StmtDefineFunction:
      return printNodeExtra(os, "stmt-define-function", node.value, node, ranges);
    case Node::Kind::StmtDefineType:
      return printNodeExtra(os, "stmt-define-type", node.value, node, ranges);
    case Node::Kind::StmtForLoop:
      assert(node.children.size() == 4);
      return printNodeChildren(os, "stmt-for-loop", node, ranges);
    case Node::Kind::StmtForEach:
      assert(node.children.size() == 3);
      return printNodeChildren(os, "stmt-for-each", node, ranges);
    case Node::Kind::StmtIf:
      assert((node.children.size() == 2) || (node.children.size() == 3));
      return printNodeChildren(os, "stmt-if", node, ranges);
    case Node::Kind::StmtReturn:
      assert(node.children.size() <= 1);
      return printNodeChildren(os, "stmt-return", node, ranges);
    case Node::Kind::StmtYield:
      assert(node.children.size() <= 1);
      return printNodeChildren(os, "stmt-yield", node, ranges);
    case Node::Kind::StmtThrow:
      assert(node.children.size() <= 1);
      return printNodeChildren(os, "stmt-throw", node, ranges);
    case Node::Kind::StmtTry:
      assert(node.children.size() >= 2);
      return printNodeChildren(os, "stmt-try", node, ranges);
    case Node::Kind::StmtCatch:
      assert(node.children.size() == 2);
      return printNodeExtra(os, "stmt-catch", node.value, node, ranges);
    case Node::Kind::StmtFinally:
      assert(node.children.size() == 1);
      return printNodeChildren(os, "stmt-finally", node, ranges);
    case Node::Kind::StmtWhile:
      assert(node.children.size() == 2);
      return printNodeChildren(os, "stmt-while", node, ranges);
    case Node::Kind::StmtDo:
      assert(node.children.size() == 2);
      return printNodeChildren(os, "stmt-do", node, ranges);
    case Node::Kind::StmtSwitch:
      assert(!node.children.empty());
      return printNodeChildren(os, "stmt-switch", node, ranges);
    case Node::Kind::StmtCase:
      assert(node.children.size() == 1);
      return printNodeChildren(os, "stmt-case", node, ranges);
    case Node::Kind::StmtDefault:
      assert(node.children.empty());
      return printNodeChildren(os, "stmt-default", node, ranges);
    case Node::Kind::StmtBreak:
      assert(node.children.empty());
      return printNodeChildren(os, "stmt-break", node, ranges);
    case Node::Kind::StmtContinue:
      assert(node.children.empty());
      return printNodeChildren(os, "stmt-continue", node, ranges);
    case Node::Kind::StmtMutate:
      return printNodeExtra(os, "stmt-mutate", node.op.valueMutationOp, node, ranges);
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
    case Node::Kind::ExprReference:
      assert(node.children.size() == 1);
      return printNodeChildren(os, "expr-reference", node, ranges);
    case Node::Kind::ExprDereference:
      assert(node.children.size() == 1);
      return printNodeChildren(os, "expr-dereference", node, ranges);
    case Node::Kind::ExprIndex:
      assert(node.children.size() == 2);
      return printNodeChildren(os, "expr-index", node, ranges);
    case Node::Kind::ExprProperty:
      assert(node.children.size() == 2);
      return printNodeChildren(os, "expr-property", node, ranges);
    case Node::Kind::ExprArray:
      return printNodeChildren(os, "expr-array", node, ranges);
    case Node::Kind::ExprEon:
      return printNodeChildren(os, "expr-eon", node, ranges);
    case Node::Kind::ExprObject:
      return printNodeChildren(os, "expr-object", node, ranges);
    case Node::Kind::ExprEllipsis:
      assert(node.children.size() == 1);
      return printNodeChildren(os, "expr-ellipsis", node, ranges);
    case Node::Kind::ExprGuard:
      assert(node.children.size() == 2);
      return printNodeChildren(os, "expr-guard", node, ranges);
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
    case Node::Kind::TypeType:
      assert(node.children.empty());
      return printNodeChildren(os, "type-type", node, ranges);
    case Node::Kind::TypeUnary:
      assert(node.children.size() == 1);
      return printNodeExtra(os, "type-unary", node.op.typeUnaryOp, node, ranges);
    case Node::Kind::TypeBinary:
      return printNodeExtra(os, "type-binary", node.op.typeBinaryOp, node, ranges);
    case Node::Kind::TypeFunctionSignature:
      return printNodeExtra(os, "type-signature", node.value, node, ranges);
    case Node::Kind::TypeFunctionSignatureParameter:
      switch (node.op.parameterOp) {
      case Node::ParameterOp::Required:
        return printNodeExtra(os, "type-parameter 'required'", node.value, node, ranges);
      case Node::ParameterOp::Optional:
        return printNodeExtra(os, "type-parameter 'optional'", node.value, node, ranges);
      default:
        return printNodeExtra(os, "type-parameter <unknown>", node.value, node, ranges);
      }
      break;
    case Node::Kind::TypeSpecification:
      return printNodeChildren(os, "type-specification", node, ranges);
    case Node::Kind::TypeSpecificationStaticData:
      return printNodeExtra(os, "type-specification-static-data", node.value, node, ranges);
    case Node::Kind::TypeSpecificationStaticFunction:
      return printNodeExtra(os, "type-specification-static-function", node.value, node, ranges);
    case Node::Kind::TypeSpecificationInstanceData:
      return printNodeExtra(os, "type-specification-instance-data", node.value, node, ranges);
    case Node::Kind::TypeSpecificationInstanceFunction:
      return printNodeExtra(os, "type-specification-instance-function", node.value, node, ranges);
    case Node::Kind::TypeSpecificationAccess:
      return printNodeExtra(os, "type-specification-access", node.value, node, ranges);
    case Node::Kind::ObjectSpecification:
      return printNodeExtra(os, "object-specification", node.value, node, ranges);
    case Node::Kind::ObjectSpecificationData:
      return printNodeExtra(os, "object-specification-data", node.value, node, ranges);
    case Node::Kind::ObjectSpecificationFunction:
      return printNodeExtra(os, "object-specification-function", node.value, node, ranges);
    case Node::Kind::Literal:
      assert(node.children.empty());
      return printValue(os, node.value, '"');
    case Node::Kind::Variable:
      assert(node.children.empty());
      return printNodeExtra(os, "variable", node.value, node, ranges);
    case Node::Kind::Named:
      assert(node.children.size() == 1);
      return printNodeExtra(os, "named", node.value, node, ranges);
    case Node::Kind::Missing:
      assert(node.children.empty());
      return os << "<missing>";
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
    Print::write(os, issue.range, Print::Options::DEFAULT);
    return os << ": " << issue.message.toUTF8();
  }

  Result parseFromLines(egg::test::Allocator& allocator, std::initializer_list<std::string> lines) {
    std::ostringstream ss;
    std::copy(lines.begin(), lines.end(), std::ostream_iterator<std::string>(ss, "\n"));
    auto lexer = LexerFactory::createFromString(ss.str());
    auto tokenizer = EggTokenizerFactory::createFromLexer(allocator, lexer);
    auto parser = EggParserFactory::createFromTokenizer(allocator, tokenizer);
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

TEST(TestEggParser, Ranges) {
  std::string actual = outputFromLines({
    //          1         2         3
    // 123456789012345678901234567890123456789
      "assert(alpha * -beta >= gamma[delta]);"
    }, true);
  std::string expected = "(expr-call@(1,1-37) (variable@(1,1-6) 'assert') (expr-binary@(1,8-36) '>=' (expr-binary@(1,8-20) '*' (variable@(1,8-12) 'alpha') (expr-unary@(1,16-20) '-' (variable@(1,17-20) 'beta'))) (expr-index@(1,25-36) (variable@(1,25-29) 'gamma') (variable@(1,31-35) 'delta'))))\n";
  ASSERT_EQ(expected, actual);
}

TEST(TestEggParser, Empty) {
  egg::test::Allocator allocator{ egg::test::Allocator::Expectation::NoAllocations };
  auto result = parseFromLines(allocator, { "" });
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
  std::string expected = "(expr-call (variable 'print') \"Hello, World!\")\n";
  ASSERT_EQ(expected, actual);
}

TEST(TestEggParser, ExpressionUnary) {
  std::string actual = outputFromLines({
    "print(-a);"
    });
  std::string expected = "(expr-call (variable 'print') (expr-unary '-' (variable 'a')))\n";
  ASSERT_EQ(expected, actual);
}

TEST(TestEggParser, ExpressionBinary) {
  std::string actual = outputFromLines({
    "print(a + b);"
    });
  std::string expected = "(expr-call (variable 'print') (expr-binary '+' (variable 'a') (variable 'b')))\n";
  ASSERT_EQ(expected, actual);
}

TEST(TestEggParser, ExpressionTernary) {
  std::string actual = outputFromLines({
    "print(a ? b : c);"
    });
  std::string expected = "(expr-call (variable 'print') (expr-ternary '?:' (variable 'a') (variable 'b') (variable 'c')))\n";
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
    "int?? a;"
    });
  std::string expected = "<WARNING>: (1,4-5): Redundant repetition of type suffix '?'\n"
                         "(stmt-declare-variable 'a' (type-unary '?' (type-int)))\n";
  ASSERT_EQ(expected, actual);
}

TEST(TestEggParser, TypeUnaryNullableRepeatedWhitespace) {
  std::string actual = outputFromLines({
    "int? ? a;"
    });
  std::string expected = "<WARNING>: (1,6): Redundant repetition of type suffix '?'\n"
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

TEST(TestEggParser, TypeUnaryPointerRepeatedWhitespace) {
  std::string actual = outputFromLines({
    "int* * a;"
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

TEST(TestEggParser, TypeUnaryIteratorRepeatedWhitespace) {
  std::string actual = outputFromLines({
    "int! ! a;"
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
  std::string expected = "(stmt-define-variable 'x' (type-infer) (expr-call (variable 'assert') false))\n";
  ASSERT_EQ(expected, actual);
}

TEST(TestEggParser, ValueIndex) {
  std::string actual = outputFromLines({
    "var x = assert[0];"
    });
  std::string expected = "(stmt-define-variable 'x' (type-infer) (expr-index (variable 'assert') 0))\n";
  ASSERT_EQ(expected, actual);
}

TEST(TestEggParser, ValueProperty) {
  std::string actual = outputFromLines({
    "var x = assert.that;"
    });
  std::string expected = "(stmt-define-variable 'x' (type-infer) (expr-property (variable 'assert') \"that\"))\n";
  ASSERT_EQ(expected, actual);
}

TEST(TestEggParser, ValueArray) {
  std::string actual = outputFromLines({
    "var x = [1,\"hello\",3.14159];"
    });
  std::string expected = "(stmt-define-variable 'x' (type-infer) (expr-array 1 \"hello\" 3.14159))\n";
  ASSERT_EQ(expected, actual);
}

TEST(TestEggParser, ValueEon) {
  std::string actual = outputFromLines({
    "var x = {a:1,b:\"hello\",c:3.14159};"
    });
  std::string expected = "(stmt-define-variable 'x' (type-infer) (expr-eon (named 'a' 1) (named 'b' \"hello\") (named 'c' 3.14159)))\n";
  ASSERT_EQ(expected, actual);
}

TEST(TestEggParser, ValueObject) {
  std::string actual = outputFromLines({
    "var x = object {"
    " int a = 1;"
    " int b(string s) {}"
    "};"
    });
  std::string expected = "(stmt-define-variable 'x' (type-infer) (expr-object (type-object)"
                         " (object-specification-data 'a' (type-int) 1)"
                         " (object-specification-function 'b' (type-signature 'b' (type-int) (type-parameter 'required' 's' (type-string))) (stmt-block))"
                         "))\n";
  ASSERT_EQ(expected, actual);
}

TEST(TestEggParser, ValueClass) {
  std::string actual = outputFromLines({
    "var x = Class {"
    " int a = 1;"
    " int b(string s) {}"
    "};"
    });
  std::string expected = "(stmt-define-variable 'x' (type-infer) (expr-object (variable 'Class')"
                         " (object-specification-data 'a' (type-int) 1)"
                         " (object-specification-function 'b' (type-signature 'b' (type-int) (type-parameter 'required' 's' (type-string))) (stmt-block))"
                         "))\n";
  ASSERT_EQ(expected, actual);
}

TEST(TestEggParser, StatementNudge) {
  std::string actual = outputFromLines({
    "var x = 0;",
    "++x;",
    "--x;"
    });
  std::string expected = "(stmt-define-variable 'x' (type-infer) 0)\n"
                         "(stmt-mutate '++' (variable 'x'))\n"
                         "(stmt-mutate '--' (variable 'x'))\n";
  ASSERT_EQ(expected, actual);
}

TEST(TestEggParser, StatementForLoop) {
  std::string actual = outputFromLines({
    "for (var i = 0; i < 10; ++i) {}"
    });
  std::string expected = "(stmt-for-loop (stmt-define-variable 'i' (type-infer) 0) (expr-binary '<' (variable 'i') 10) (stmt-mutate '++' (variable 'i')) (stmt-block))\n";
  ASSERT_EQ(expected, actual);
}

TEST(TestEggParser, StatementForEach) {
  std::string actual = outputFromLines({
    "for (var i : \"hello\") {}"
    });
  std::string expected = "(stmt-for-each (type-infer) \"hello\" (stmt-block))\n";
  ASSERT_EQ(expected, actual);
}

TEST(TestEggParser, StatementDefineFunction) {
  std::string actual = outputFromLines({
    "int f(string a, float? b = null) {}"
    });
  std::string expected = "(stmt-define-function 'f' (type-signature 'f' (type-int) "
                         "(type-parameter 'required' 'a' (type-string)) "
                         "(type-parameter 'optional' 'b' (type-unary '?' (type-float)))) (stmt-block))\n";
  ASSERT_EQ(expected, actual);
}

TEST(TestEggParser, StatementIf) {
  std::string actual = outputFromLines({
    "if (true) {}"
    });
  std::string expected = "(stmt-if true (stmt-block))\n";
  ASSERT_EQ(expected, actual);
}

TEST(TestEggParser, StatementIfGuardExplicit) {
  std::string actual = outputFromLines({
    "if (int a = 123) {}"
    });
  std::string expected = "(stmt-if (expr-guard (type-int) 123) (stmt-block))\n";
  ASSERT_EQ(expected, actual);
}

TEST(TestEggParser, StatementIfGuardInfer) {
  std::string actual = outputFromLines({
    "if (var a = 123) {}"
    });
  std::string expected = "(stmt-if (expr-guard (type-infer) 123) (stmt-block))\n";
  ASSERT_EQ(expected, actual);
}

TEST(TestEggParser, StatementIfGuardInferNullable) {
  std::string actual = outputFromLines({
    "if (var? a = 123) {}"
    });
  std::string expected = "(stmt-if (expr-guard (type-infer-q) 123) (stmt-block))\n";
  ASSERT_EQ(expected, actual);
}

TEST(TestEggParser, StatementIfElse) {
  std::string actual = outputFromLines({
    "if (false) {} else {}"
    });
  std::string expected = "(stmt-if false (stmt-block) (stmt-block))\n";
  ASSERT_EQ(expected, actual);
}

TEST(TestEggParser, StatementIfElseGuardExplicit) {
  std::string actual = outputFromLines({
    "if (int a = 123) {} else {}"
    });
  std::string expected = "(stmt-if (expr-guard (type-int) 123) (stmt-block) (stmt-block))\n";
  ASSERT_EQ(expected, actual);
}

TEST(TestEggParser, StatementIfElseGuardInfer) {
  std::string actual = outputFromLines({
    "if (var a = 123) {} else {}"
    });
  std::string expected = "(stmt-if (expr-guard (type-infer) 123) (stmt-block) (stmt-block))\n";
  ASSERT_EQ(expected, actual);
}

TEST(TestEggParser, StatementIfElseGuardInferNullable) {
  std::string actual = outputFromLines({
    "if (var? a = 123) {} else {}"
    });
  std::string expected = "(stmt-if (expr-guard (type-infer-q) 123) (stmt-block) (stmt-block))\n";
  ASSERT_EQ(expected, actual);
}

TEST(TestEggParser, StatementIfElseIf) {
  std::string actual = outputFromLines({
    "if (true) {} else if (false) {}"
    });
  std::string expected = "(stmt-if true (stmt-block) (stmt-if false (stmt-block)))\n";
  ASSERT_EQ(expected, actual);
}

TEST(TestEggParser, StatementReturn) {
  std::string actual = outputFromLines({
    "return;"
    });
  std::string expected = "(stmt-return)\n";
  ASSERT_EQ(expected, actual);
}

TEST(TestEggParser, StatementReturnValue) {
  std::string actual = outputFromLines({
    "return 123;"
    });
  std::string expected = "(stmt-return 123)\n";
  ASSERT_EQ(expected, actual);
}

TEST(TestEggParser, StatementYield) {
  std::string actual = outputFromLines({
    "yield;"
    });
  std::string expected = "<ERROR>: (1,1-6): Expected expression, 'break' or 'continue' after keyword 'yield', but instead got operator ';'\n";
  ASSERT_EQ(expected, actual);
}

TEST(TestEggParser, StatementYieldValue) {
  std::string actual = outputFromLines({
    "yield 123;"
    });
  std::string expected = "(stmt-yield 123)\n";
  ASSERT_EQ(expected, actual);
}

TEST(TestEggParser, StatementYieldBreak) {
  std::string actual = outputFromLines({
    "yield break;"
    });
  std::string expected = "(stmt-yield (stmt-break))\n";
  ASSERT_EQ(expected, actual);
}

TEST(TestEggParser, StatementYieldContinue) {
  std::string actual = outputFromLines({
    "yield continue;"
    });
  std::string expected = "(stmt-yield (stmt-continue))\n";
  ASSERT_EQ(expected, actual);
}

TEST(TestEggParser, StatementTryCatch) {
  std::string actual = outputFromLines({
    "try {} catch (any e) {}"
    });
  std::string expected = "(stmt-try (stmt-block) (stmt-catch 'e' (type-any) (stmt-block)))\n";
  ASSERT_EQ(expected, actual);
}

TEST(TestEggParser, StatementTryCatchCatch) {
  std::string actual = outputFromLines({
    "try {} catch (int i) {} catch (any a) {}"
    });
  std::string expected = "(stmt-try (stmt-block) (stmt-catch 'i' (type-int) (stmt-block)) (stmt-catch 'a' (type-any) (stmt-block)))\n";
  ASSERT_EQ(expected, actual);
}

TEST(TestEggParser, StatementTryCatchCatchFinally) {
  std::string actual = outputFromLines({
    "try {} catch (int i) {} catch (any a) {} finally {}"
    });
  std::string expected = "(stmt-try (stmt-block) (stmt-catch 'i' (type-int) (stmt-block)) (stmt-catch 'a' (type-any) (stmt-block)) (stmt-finally (stmt-block)))\n";
  ASSERT_EQ(expected, actual);
}

TEST(TestEggParser, StatementTryCatchFinally) {
  std::string actual = outputFromLines({
    "try {} catch (any e) {} finally {}"
    });
  std::string expected = "(stmt-try (stmt-block) (stmt-catch 'e' (type-any) (stmt-block)) (stmt-finally (stmt-block)))\n";
  ASSERT_EQ(expected, actual);
}

TEST(TestEggParser, StatementTryFinally) {
  std::string actual = outputFromLines({
    "try {} finally {}"
    });
  std::string expected = "(stmt-try (stmt-block) (stmt-finally (stmt-block)))\n";
  ASSERT_EQ(expected, actual);
}

TEST(TestEggParser, StatementThrow) {
  std::string actual = outputFromLines({
    "throw i;"
    });
  std::string expected = "(stmt-throw (variable 'i'))\n";
  ASSERT_EQ(expected, actual);
}

TEST(TestEggParser, StatementRethrow) {
  std::string actual = outputFromLines({
    "throw;"
    });
  std::string expected = "(stmt-throw)\n";
  ASSERT_EQ(expected, actual);
}

TEST(TestEggParser, StatementWhileLoop) {
  std::string actual = outputFromLines({
    "while (i < 10) {}"
    });
  std::string expected = "(stmt-while (expr-binary '<' (variable 'i') 10) (stmt-block))\n";
  ASSERT_EQ(expected, actual);
}

TEST(TestEggParser, StatementDoLoop) {
  std::string actual = outputFromLines({
    "do {} while (i < 10);"
    });
  std::string expected = "(stmt-do (stmt-block) (expr-binary '<' (variable 'i') 10))\n";
  ASSERT_EQ(expected, actual);
}

TEST(TestEggParser, StatementBreak) {
  std::string actual = outputFromLines({
    "break;"
    });
  std::string expected = "(stmt-break)\n";
  ASSERT_EQ(expected, actual);
}

TEST(TestEggParser, StatementContinue) {
  std::string actual = outputFromLines({
    "continue;"
    });
  std::string expected = "(stmt-continue)\n";
  ASSERT_EQ(expected, actual);
}

TEST(TestEggParser, StatementSwitch) {
  std::string actual = outputFromLines({
    "switch (i) { case 123: default: break; case 321: continue; }"
    });
  std::string expected = "(stmt-switch (variable 'i') (stmt-block (stmt-case 123) (stmt-default) (stmt-break) (stmt-case 321) (stmt-continue)))\n";
  ASSERT_EQ(expected, actual);
}

TEST(TestEggParser, StatementDiscard) {
  std::string actual = outputFromLines({
    "void(123);"
    });
  std::string expected = "(expr-call (type-void) 123)\n";
  ASSERT_EQ(expected, actual);
}

TEST(TestEggParser, StatementBlock) {
  std::string actual = outputFromLines({
    "{ var i = 1; }",
    "{ var i = 2; }"
    });
  std::string expected = "(stmt-block (stmt-define-variable 'i' (type-infer) 1))\n(stmt-block (stmt-define-variable 'i' (type-infer) 2))\n";
  ASSERT_EQ(expected, actual);
}

TEST(TestEggParser, StatementOrphanCatch) {
  std::string actual = outputFromLines({
    "catch (...) {}"
    });
  std::string expected = "<ERROR>: (1,1-5): Unexpected 'catch' without preceding 'try' statement\n";
  ASSERT_EQ(expected, actual);
}

TEST(TestEggParser, StatementOrphanElse) {
  std::string actual = outputFromLines({
    "else {}"
    });
  std::string expected = "<ERROR>: (1,1-4): Unexpected 'else' without preceding 'if' statement\n";
  ASSERT_EQ(expected, actual);
}

TEST(TestEggParser, StatementOrphanFinally) {
  std::string actual = outputFromLines({
    "finally {}"
    });
  std::string expected = "<ERROR>: (1,1-7): Unexpected 'finally' without preceding 'try' statement\n";
  ASSERT_EQ(expected, actual);
}

TEST(TestEggParser, StatementTypeAlias) {
  std::string actual = outputFromLines({
    "type Number = int|float;"
    });
  std::string expected = "(stmt-define-type 'Number' (type-binary '|' (type-int) (type-float)))\n";
  ASSERT_EQ(expected, actual);
}

TEST(TestEggParser, StatementTypeEmpty) {
  std::string actual = outputFromLines({
    "type Empty {};"
    });
  std::string expected = "(stmt-define-type 'Empty' (type-specification))\n";
  ASSERT_EQ(expected, actual);
}

TEST(TestEggParser, StatementTypeStaticData) {
  std::string actual = outputFromLines({
    "type Class {",
    " static int i = 123;",
    "};"
    });
  std::string expected = "(stmt-define-type 'Class' (type-specification"
                         " (type-specification-static-data 'i' (type-int) 123)"
                         "))\n";
  ASSERT_EQ(expected, actual);
}

TEST(TestEggParser, StatementTypeStaticFunction) {
  std::string actual = outputFromLines({
    "type Class {",
    " static int f() { return 123; }",
    "};"
    });
  std::string expected = "(stmt-define-type 'Class' (type-specification"
                         " (type-specification-static-function 'f' (type-signature 'f' (type-int)) (stmt-block (stmt-return 123)))"
                         "))\n";
  ASSERT_EQ(expected, actual);
}

TEST(TestEggParser, StatementTypeInstanceData) {
  std::string actual = outputFromLines({
    "type Class {",
    " int i;",
    "};"
    });
  std::string expected = "(stmt-define-type 'Class' (type-specification"
    " (type-specification-instance-data 'i' (type-int))"
    "))\n";
  ASSERT_EQ(expected, actual);
}

TEST(TestEggParser, StatementTypeInstanceFunction) {
  std::string actual = outputFromLines({
    "type Class {",
    " int f();",
    "};"
    });
  std::string expected = "(stmt-define-type 'Class' (type-specification"
    " (type-specification-instance-function 'f' (type-signature 'f' (type-int)))"
    "))\n";
  ASSERT_EQ(expected, actual);
}

TEST(TestEggParser, StatementTypeStaticFunctionError) {
  std::string actual = outputFromLines({
    "type Class {",
    " static int f();",
    "};"
    });
  std::string expected = "<ERROR>: (2,2-16): Forward declaration of static function 'f' not yet supported\n";
  ASSERT_EQ(expected, actual);
}

TEST(TestEggParser, StatementTypeNonStaticDataError) {
  std::string actual = outputFromLines({
    "type Class {",
    " int i = 123;",
    "};"
    });
  std::string expected = "<ERROR>: (2,2-8): Expected ';' after identifier 'i' in declaration of property, but instead got operator '='\n";
  ASSERT_EQ(expected, actual);
}

TEST(TestEggParser, StatementTypeStaticDataError) {
  std::string actual = outputFromLines({
    "type Class {",
    " static int i;",
    "};"
    });
  std::string expected = "<ERROR>: (2,2-14): Forward declaration of static property 'i' not yet supported\n";
  ASSERT_EQ(expected, actual);
}

TEST(TestEggParser, StatementTypeNonStaticFunctionError) {
  std::string actual = outputFromLines({
    "type Class {",
    " int f() { return 123; }",
    "};"
    });
  std::string expected = "<ERROR>: (2,2-10): Expected ';' after ')' in declaration of non-static function 'f', but instead got operator '{'\n";
  ASSERT_EQ(expected, actual);
}

TEST(TestEggParser, StatementTypeStaticGet) {
  std::string actual = outputFromLines({
    "type Class {",
    " static int i = 123;",
    "};",
    "print(Class.i);"
    });
  std::string expected = "(stmt-define-type 'Class' (type-specification (type-specification-static-data 'i' (type-int) 123)))\n"
                         "(expr-call (variable 'print') (expr-property (variable 'Class') \"i\"))\n";
  ASSERT_EQ(expected, actual);
}

TEST(TestEggParser, StatementTypeStaticSet) {
  std::string actual = outputFromLines({
    "type Class {",
    " static int i = 123;",
    "};",
    "Class.i = 321;"
    });
  std::string expected = "(stmt-define-type 'Class' (type-specification (type-specification-static-data 'i' (type-int) 123)))\n"
                         "(stmt-mutate '=' (expr-property (variable 'Class') \"i\") 321)\n";
  ASSERT_EQ(expected, actual);
}

TEST(TestEggParser, StatementTypeModifiabilityAll) {
  std::string actual = outputFromLines({
    "type Class {",
    " int i { get; set; mut; del; }",
    "};"
    });
  std::string expected = "(stmt-define-type 'Class' (type-specification (type-specification-instance-data 'i' (type-int) (type-specification-access 'get') (type-specification-access 'set') (type-specification-access 'mut') (type-specification-access 'del'))))\n";
  ASSERT_EQ(expected, actual);
}

TEST(TestEggParser, StatementTypeModifiabilityGet) {
  std::string actual = outputFromLines({
    "type Class {",
    " int i { get; }",
    "};"
    });
  std::string expected = "(stmt-define-type 'Class' (type-specification (type-specification-instance-data 'i' (type-int) (type-specification-access 'get'))))\n";
  ASSERT_EQ(expected, actual);
}

TEST(TestEggParser, StatementTypeModifiabilitySetGet) {
  std::string actual = outputFromLines({
    "type Class {",
    " int i { set; get; }",
    "};"
    });
  std::string expected = "(stmt-define-type 'Class' (type-specification (type-specification-instance-data 'i' (type-int) (type-specification-access 'set') (type-specification-access 'get'))))\n";
  ASSERT_EQ(expected, actual);
}

TEST(TestEggParser, StatementTypeModifiabilityEmpty) {
  std::string actual = outputFromLines({
    "type Class {",
    " int i {}",
    "};"
    });
  std::string expected = "<ERROR>: (2,8-9): Expected at least one 'get', 'set', 'mut', 'ref' or 'del' in access clause of declaration of property 'i'\n";
  ASSERT_EQ(expected, actual);
}

TEST(TestEggParser, StatementTypeModifiabilityDuplicate) {
  std::string actual = outputFromLines({
    "type Class {",
    " int i { get; get; }",
    "};"
    });
  std::string expected = "(stmt-define-type 'Class' (type-specification (type-specification-instance-data 'i' (type-int) (type-specification-access 'get') (type-specification-access 'get'))))\n";
  ASSERT_EQ(expected, actual);
}

TEST(TestEggParser, StatementTypeModifiabilityUnknown) {
  std::string actual = outputFromLines({
    "type Class {",
    " int i { foo; }",
    "};"
    });
  std::string expected = "<ERROR>: (2,10-12): Expected 'get', 'set', 'mut', 'ref' or 'del' in access clause of declaration of property 'i', but instead got 'foo'\n";
  ASSERT_EQ(expected, actual);
}

TEST(TestEggParser, AmbiguousFunctionCall) {
  std::string actual = outputFromLines({
    "a.b(c.d);"
    });
  std::string expected = "(expr-call (expr-property (variable 'a') \"b\") (expr-property (variable 'c') \"d\"))\n";
  ASSERT_EQ(expected, actual);
}

TEST(TestEggParser, AmbiguousFunctionDeclaration) {
  std::string actual = outputFromLines({
    "a.b f(c.d e) {}"
    });
  std::string expected = "(stmt-define-function 'f' (type-signature 'f' (expr-property (variable 'a') \"b\") (type-parameter 'required' 'e' (expr-property (variable 'c') \"d\"))) (stmt-block))\n";
  ASSERT_EQ(expected, actual);
}

TEST(TestEggParser, AmbiguousObjectAll) {
  std::string actual = outputFromLines({
    "object.del(a, \"b\");"
    });
  std::string expected = "(expr-call (expr-property (type-object) \"del\") (variable 'a') \"b\")\n";
  ASSERT_EQ(expected, actual);
}
