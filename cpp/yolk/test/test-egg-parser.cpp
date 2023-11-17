#include "yolk/test.h"
#include "yolk/lexers.h"
#include "yolk/egg-tokenizer.h"
#include "yolk/egg-parser.h"

using Issue = egg::yolk::IEggParser::Issue;
using Node = egg::yolk::IEggParser::Node;
using Result = egg::yolk::IEggParser::Result;

namespace {
  std::ostream& operator<<(std::ostream& os, const Node& node);

  template<typename T>
  std::ostream& printNodeOperator(std::ostream& os, const char* prefix, T op, const Node& node) {
    os << '(' << prefix << ' ' << '\'';
    egg::ovum::Print::write(os, op, egg::ovum::Print::Options::DEFAULT);
    os << '\'';
    for (auto& child : node.children) {
      os << ' ' << *child;
    }
    return os << ')';
  }

  std::ostream& printNodeChildren(std::ostream& os, const char* prefix, const Node& node) {
    os << '(' << prefix;
    for (auto& child : node.children) {
      os << ' ' << *child;
    }
    return os << ')';
  }

  std::ostream& printValue(std::ostream& os, const egg::ovum::HardValue& value, char quote = '\'') {
    const egg::ovum::Print::Options options{ quote };
    egg::ovum::Print::write(os, value, options);
    return os;
  }

  std::ostream& operator<<(std::ostream& os, const Node& node) {
    switch (node.kind) {
    case Node::Kind::ModuleRoot:
      for (auto& child : node.children) {
        os << *child << std::endl;
      }
      break;
    case Node::Kind::StmtCall:
      return printNodeChildren(os, "stmt-call", node);
    case Node::Kind::ExprVar:
      assert(node.children.empty());
      return printValue(os << "(expr-var ", node.value) << ')';
    case Node::Kind::ExprUnary:
      assert(node.children.size() == 1);
      return printNodeOperator(os, "expr-unary", node.op.unary, node);
    case Node::Kind::ExprBinary:
      assert(node.children.size() == 2);
      return printNodeOperator(os, "expr-binary", node.op.binary, node);
    case Node::Kind::ExprTernary:
      assert(node.children.size() == 3);
      return printNodeOperator(os, "expr-ternary", node.op.ternary, node);
    case Node::Kind::ExprCall:
      return printNodeChildren(os, "expr-call", node);
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
    os << '(' << issue.begin.line << ',' << issue.begin.column;
    if ((issue.begin.line > issue.end.line) || (issue.begin.column > issue.end.column)) {
      os << ',' << issue.end.line << ',' << issue.end.column;
    }
    return os << ')' << ':' << ' ' << issue.message.toUTF8();
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

  std::string outputFromLines(std::initializer_list<std::string> lines) {
    egg::test::Allocator allocator;
    auto result = parseFromLines(allocator, lines);
    std::ostringstream ss;
    if (result.root != nullptr) {
      ss << *result.root;
    } else {
      for (auto& issue : result.issues) {
        ss << issue << std::endl;
      }
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
  ASSERT_EQ(2u, actual.begin.line);
  ASSERT_EQ(3u, actual.begin.column);
  ASSERT_EQ(2u, actual.end.line);
  ASSERT_EQ(3u, actual.end.column);
}

TEST(TestEggParser, HelloWorld) {
  std::string actual = outputFromLines({
    "print(\"Hello, World!\");"
  });
  std::string expected = "(stmt-call (expr-call (expr-var 'print') \"Hello, World!\"))\n";
  ASSERT_EQ(expected, actual);
}

TEST(TestEggParser, ExpressionUnary) {
  std::string actual = outputFromLines({
    "print(-a);"
    });
  std::string expected = "(stmt-call (expr-call (expr-var 'print') (expr-unary '-' (expr-var 'a'))))\n";
  ASSERT_EQ(expected, actual);
}

TEST(TestEggParser, ExpressionBinary) {
  std::string actual = outputFromLines({
    "print(a + b);"
    });
  std::string expected = "(stmt-call (expr-call (expr-var 'print') (expr-binary '+' (expr-var 'a') (expr-var 'b'))))\n";
  ASSERT_EQ(expected, actual);
}

TEST(TestEggParser, ExpressionTernary) {
  std::string actual = outputFromLines({
    "print(a ? b : c);"
    });
  std::string expected = "(stmt-call (expr-call (expr-var 'print') (expr-ternary '?:' (expr-var 'a') (expr-var 'b') (expr-var 'c'))))\n";
  ASSERT_EQ(expected, actual);
}
