// EBNF of the Egg programming language in JSONP form
egg.bnf({
  rules: {
    "module": {oneOrMore: {sequence: [{zeroOrMore:"attribute"}, "statement"]}, inline: false, left: 1, right: 1},
    "attribute": {sequence: [{token: "@"}, {list: "identifier-attribute", separator: {token: "."}}, {zeroOrOne: {sequence: [{token: "("}, "parameter-list", {token: ")"}]}}], inline: false, left: 1},
    "statement": {choice: [
      "statement-compound",
      {sequence: ["statement-simple", {token: ";"}]},
      "definition-function",
      "statement-if",
      "statement-switch",
      "statement-while",
      "statement-do",
      "statement-for",
      "statement-foreach",
      "statement-break",
      "statement-continue",
      "statement-throw",
      "statement-try",
      "statement-try-finally",
      "statement-return",
      "statement-yield"
    ]},
    "statement-simple": {choice: [
      "statement-action",
      "definition-type",
      "definition-variable"
    ]},
    "statement-action": {choice: [
      "statement-assign",
      "statement-mutate",
      "statement-call"
    ]},
    "statement-assign": {sequence: ["assignment-target", "operator-assignment", "expression"]},
    "statement-mutate": {sequence: ["operator-mutation", "assignment-target"]},
    "statement-call": {sequence: ["expression-primary", {token: "("}, {zeroOrOne: "parameter-list"}, {token: ")"}], inline: false, left: 1, right: 1},
    "statement-compound": {sequence: [{token: "{"}, {zeroOrMore: "statement"}, {token: "}"}], inline: true},
    "statement-if": {sequence: [{token: "if"}, {token: "("}, "condition", {token: ")"}, "statement-compound", {zeroOrOne: {sequence: [{token: "else"}, "statement-else"]}}], inline: false, left: 1},
    "statement-else": {choice: [
      "statement-compound",
      "statement-if"
    ], railroad: false},
    "statement-switch": {sequence: [{token: "switch"}, {token: "("}, "expression", {token: ")"}, {token: "{"}, {oneOrMore: "statement-case"}, {token: "}"}], inline: false, left: 1, right: 1},
    "statement-case": {sequence: [{oneOrMore: "statement-case-value"}, {oneOrMore: "statement"}]},
    "statement-case-value": {choice: [
      {sequence: [{token: "case"}, "expression", {token: ":"}]},
      {sequence: [{token: "default"}, {token: ":"}]}
    ], railroad: false},
    "statement-while": {sequence: [{token: "while"}, {token: "("}, "condition", {token: ")"}, "statement-compound"], inline: false, left: 1, right: 1},
    "statement-do": {sequence: [{token: "do"}, "statement-compound", {token: "while"}, {token: "("}, "expression", {token: ")"}, {token: ";"}], inline: false, left: 1, right: 1},
    "statement-for": {sequence: [{token: "for"}, {token: "("}, {zeroOrOne:"statement-simple"}, {token: ";"}, {zeroOrOne: "condition"}, {token: ";"}, {zeroOrOne: "statement-action"}, {token: ")"}, "statement-compound"], inline: false, left: 1, right: 1},
    "statement-foreach": {sequence: [{token: "for"}, {token: "("}, "statement-foreach-type", "identifier-variable", {token: ":"}, "expression", {token: ")"}, "statement-compound"], inline: false, left: 1, right: 1},
    "statement-foreach-type": {choice: [
      {sequence: [{token: "var"}, {zeroOrOne: {token: "?"}}]},
      "type-expression"
    ], railroad: false},
    "statement-try": {sequence: [{token: "try"}, "statement-compound", {oneOrMore:"statement-catch"}], inline: false, left: 1, right: 1},
    "statement-try-finally": {sequence: [{token: "try"}, "statement-compound", { zeroOrMore: "statement-catch" }, {token: "finally"}, "statement-compound"], inline: false, left: 1, right: 1},
    "statement-catch": {sequence: [{token: "catch"}, {token: "("}, "type-expression", "identifier-variable", {token: ")"}, "statement-compound"], inline: false, left: 1, right: 1},
    "statement-break": {sequence: [{token: "break"}, {token: ";"}], inline: false, left: 1, right: 1},
    "statement-continue": {sequence: [{token: "continue"}, {token: ";"}], inline: false, left: 1, right: 1},
    "statement-return": {sequence: [{token: "return"}, {zeroOrOne: "expression"}, {token: ";"}], inline: false, left: 1, right: 1},
    "statement-yield": {sequence: [{token: "yield"}, {zeroOrOne: {token: "..."}}, "expression", {token: ";"}], inline: false, left: 1, right: 1},
    "statement-throw": {sequence: [{token: "throw"}, {zeroOrOne: "expression"}, {token: ";"}], inline: false, left: 1, right: 1},
    "definition-type": {sequence: [{token: "type"}, "identifier-type", {zeroOrOne: {sequence: [{token: "<"}, "definition-type-list", {token: ">"}]}}, {token: "="}, "definition-type-value"], inline: false, left: 1, right: 1},
    "definition-type-list": {list: "identifier-type", separator: {token: ","}},
    "definition-type-value": {choice: [
      "type-expression",
      "literal-type"
    ], railroad: false},
    "definition-variable": {choice: [
      {sequence: [{ token: "var" }, {zeroOrOne: {token: "?"}}, "identifier-variable", {token: "="}, "expression"]},
      {sequence: ["type-expression", "identifier-variable", {zeroOrOne: {sequence: [{token: "="}, "expression"]}}]}
    ], inline: false},
    "definition-function": { sequence: ["type-expression", "identifier-variable", {token: "("}, {zeroOrOne: "definition-function-parameter-list"}, {token: ")"}, "statement-compound"], inline: false, left: 1, right: 1 },
    "definition-function-parameter-list": {list: "definition-function-parameter", separator: {token: ","}},
    "definition-function-parameter": {sequence: [{zeroOrMore: "attribute"}, "definition-function-parameter-declaration"], inline: false, left: 1},
    "definition-function-parameter-declaration": {choice: [
      {sequence: ["type-expression", "identifier-variable", {zeroOrOne: {sequence: [{token: "="}, {token: "null"}]}}]},
      {sequence: [{token: "..."}, "type-expression", "identifier-variable"]},
      {sequence: [{token: "..."}, "type-expression", {token: "["}, {token: "]"}, "identifier-variable"]}
    ], railroad: false},
    "type-list": {list: "type-expression", separator: {token: ","}},
    "type-expression": {list: "type-expression-postfix", separator: {token: "|" }, inline: false, left: 1, right: 1},
    "type-expression-postfix": {sequence: ["type-expression-primary", {zeroOrMore: "type-expression-suffix"}]},
    "type-expression-primary": {choice: [
      {sequence: ["identifier-type", {zeroOrOne: {sequence: [{token: "<"}, "type-list", {token: ">"}]}}]},
      {token: "void"},
      {token: "bool"},
      {token: "int"},
      {token: "float"},
      {token: "string"},
      {token: "object"},
      {token: "any"},
      {sequence: [{ token: "type" }, {token: "null"}]},
      {sequence: [{token: "type"}, "literal-type"]},
      {sequence: [{token: "("}, "type-expression", {token: ")"}]}
    ]},
    "type-expression-suffix": {choice: [
      {token: "?"},
      {token: "!"},
      {token: "*"},
      {sequence: [{token: "["}, {zeroOrOne: "type-expression"}, {token: "]"}]},
      {sequence: [{token: "("}, {zeroOrOne: "definition-function-parameter-list"}, {token: ")"}]}
    ], railroad: false},
    "expression": {choice: ["expression-ternary"]},
    "expression-ternary": {choice: [
      "expression-binary",
      {sequence: ["expression-ternary", {token: "?"}, "expression", {token: ":"}, "expression-binary"]}
    ]},
    "expression-binary": {choice: [
      "expression-logical-or",
      {sequence: ["expression-binary", {token: "??"}, "expression-logical-or"]}
    ]},
    "expression-logical-or": {choice: [
      "expression-logical-and",
      {sequence: ["expression-logical-or", {token: "||"}, "expression-logical-and"]}
    ]},
    "expression-logical-and": {choice: [
      "expression-inclusive-or",
      {sequence: ["expression-logical-and", {token: "&&"}, "expression-inclusive-or"]}
    ]},
    "expression-inclusive-or": {choice: [
      "expression-exclusive-or",
      {sequence: ["expression-inclusive-or", {token: "|"}, "expression-exclusive-or"]}
    ]},
    "expression-exclusive-or": {choice: [
      "expression-and",
      {sequence: ["expression-exclusive-or", {token: "^"}, "expression-and"]}
    ]},
    "expression-and": {choice: [
      "expression-equality",
      {sequence: ["expression-and", {token: "&"}, "expression-equality"]}
    ]},
    "expression-equality": {choice: [
      "expression-relational",
      {sequence: ["expression-equality", {token: "=="}, "expression-relational"]},
      {sequence: ["expression-equality", {token: "!="}, "expression-relational"]}
    ]},
    "expression-relational": {choice: [
      "expression-shift",
      {sequence: ["expression-relational", {token: "<"}, "expression-shift"]},
      {sequence: ["expression-relational", {token: ">"}, "expression-shift"]},
      {sequence: ["expression-relational", {token: "<="}, "expression-shift"]},
      {sequence: ["expression-relational", {token: ">="}, "expression-shift"]}
    ]},
    "expression-shift": {choice: [
      "expression-additive",
      {sequence: ["expression-shift", {token: "<<"}, "expression-additive"]},
      {sequence: ["expression-shift", {token: ">>"}, "expression-additive"]},
      {sequence: ["expression-shift", {token: ">>>"}, "expression-additive"]}
    ]},
    "expression-additive": {choice: [
      "expression-multiplicative",
      {sequence: ["expression-additive", {token: "+"}, "expression-multiplicative"]},
      {sequence: ["expression-additive", {token: "-"}, "expression-multiplicative"]}
    ]},
    "expression-multiplicative": {choice: [
      "expression-unary",
      {sequence: ["expression-multiplicative", {token: "*"}, "expression-unary"]},
      {sequence: ["expression-multiplicative", {token: "/"}, "expression-unary"]},
      {sequence: ["expression-multiplicative", {token: "%"}, "expression-unary"]}
    ]},
    "expression-unary": {sequence: [
      {zeroOrMore: "operator-unary"},
      "expression-primary"
    ], inline: false, left: 1, right: 1},
    "expression-primary": {sequence: ["expression-prefix", {zeroOrMore: "expression-suffix"}], inline: false, right: 1},
    "expression-prefix": {choice: [
      "identifier-variable",
      "literal",
      "expression-function",
      "expression-lambda",
      {sequence: [{token: "("}, "expression", {token: ")"}]}
    ], railroad: false},
    "expression-suffix": {choice: [
      {sequence: [{token: "["}, "expression", {token: "]"}]},
      {sequence: [{token: "."}, "identifier-property"]},
      {sequence: [{token: "("}, {zeroOrOne: "parameter-list"}, {token: ")"}]}
    ], railroad: false},
    "expression-function": {sequence: ["type-expression", {token: "("}, {zeroOrOne: "definition-function-parameter-list"}, {token: ")"}, "statement-compound"], inline: false, left: 1, right: 1},
    "expression-lambda": {sequence: ["expression-lambda-parameters", {token: "=>"}, "expression-lambda-body"], inline: false},
    "expression-lambda-parameters": {choice: [
      "identifier-variable",
      {sequence: [{token: "("}, {zeroOrOne: "expression-lambda-parameter-list"}, {token: ")"}]}
    ], railroad: false},
    "expression-lambda-parameter-list": {list: "identifier-variable", separator: {token: ","}},
    "expression-lambda-body": {choice: [
      "expression",
      "statement-compound"
    ], railroad: false},
    "operator-unary": {tokens: ["*", "-", "~", "!"], railroad: false},
    "operator-binary": {tokens: ["??", "||", "&&", "|", "^", "&", "==", "!=", "<", ">", "<=", ">=", "<<", ">>", ">>>", "+", "-", "*", "/", "%"], railroad: false},
    "operator-assignment": {tokens: ["=", "+=", "-=", "*=", "/=", "%=", "<<=", ">>=", ">>>=", "&=", "|=", "^=", "&&=", "||=", "??="], railroad: false},
    "operator-mutation": {tokens: ["++", "--"], railroad: false},
    "condition": {choice: [
      "expression",
      {sequence: ["condition-variable-type", "identifier-variable", {token: "="}, "expression"]}
    ]},
    "condition-variable-type": {choice: [
      {sequence: [{token: "var"}, {zeroOrOne: {token: "?"}}]},
      "type-expression"
    ], railroad: false},
    "assignment-target": {choice: [
      "identifier-variable",
      {sequence: [{token: "*"}, "expression-unary"]},
      {sequence: ["expression-primary", {token: "["}, "expression", {token: "]"}]},
      {sequence: ["expression-primary", {token: "."}, "identifier-property"]}
    ]},
    "parameter-list": {choice: [
      "parameter-positional-list",
      {sequence: ["parameter-positional-list", {token: ","}, "parameter-named-list"]},
      "parameter-named-list"
    ]},
    "parameter": {choice: [
      "expression",
      {sequence: [{token: "..."}, "expression"]},
      {sequence: ["identifier-variable", {token: ":"}, "expression"]}
    ], railroad: false},
    "parameter-positional-list": {list: "parameter-positional", separator: {token: ","}},
    "parameter-positional": {choice: [
      "expression",
      {sequence: [{token: "..."}, "expression"]}
    ]},
    "parameter-named-list": {list: "parameter-named", separator: {token: ","}},
    "parameter-named": {sequence: ["identifier-variable", {token: ":"}, "expression"]},
    "identifier-attribute": {alias: "identifier"},
    "identifier-property": {alias: "identifier"},
    "identifier-type": {alias: "identifier"},
    "identifier-variable": {alias: "identifier"},
    "identifier": {sequence: ["identifier-head", {zeroOrMore: "identifier-tail"}], inline: false},
    "identifier-head": {choice: [
      {terminal: "ascii-letter"},
      {token: "_"}
    ], railroad: false},
    "identifier-tail": {choice: [
      {terminal: "ascii-alphanumeric"},
      {terminal: "non-ascii-unicode"},
      {token: "_"}
    ], railroad: false},
    "literal": {choice: [
      {token: "null"},
      {token: "false"},
      {token: "true"},
      "literal-int",
      "literal-float",
      "literal-string",
      "literal-object",
      "literal-array"
    ]},
    "literal-int": {sequence: [{zeroOrOne: "literal-sign"}, "literal-digits"], inline: false, right: 1},
    "literal-sign": {tokens: ["+", "-"], railroad: false},
    "literal-digits": {oneOrMore: {terminal: "digit"}},
    "literal-float": {sequence: [{zeroOrOne: "literal-sign"}, "literal-digits", {token: "."}, "literal-digits", {zeroOrOne: {sequence: ["literal-exponent", {zeroOrOne: "literal-sign"}, "literal-digits"]}}], inline: false},
    "literal-exponent": {choice: [
      {token: "E"},
      {token: "e"}
    ], railroad: false},
    "literal-string": {sequence: [{token: "\""}, {zeroOrMore: "literal-string-character"}, {token: "\""}], inline: false, left: 1, right: 1},
    "literal-string-character": {choice: [
      {terminal: "any-character-except-backslash-and-double-quote"},
      {sequence: [{token: "\\"}, {token: "\\"}]},
      {sequence: [{token: "\\"}, {token: "\""}]},
      {sequence: [{token: "\\"}, {token: "b"}]},
      {sequence: [{token: "\\"}, {token: "f"}]},
      {sequence: [{token: "\\"}, {token: "n"}]},
      {sequence: [{token: "\\"}, {token: "r"}]},
      {sequence: [{token: "\\"}, {token: "t"}]},
      {sequence: [{token: "\\"}, {token: "v"}]},
      {sequence: [{token: "\\"}, {token: "0"}]},
      {sequence: [{token: "\\"}, {token: "U"}, {token: "+"}, {oneOrMore: {terminal: "hexadecimal-digit"}}, {token: ";"}]}
    ], railroad: false},
    "literal-object": {sequence: [{token: "{"}, {zeroOrOne: "literal-object-list"}, {token: "}"}], inline: false, left: 1, right: 1},
    "literal-object-list": {list: "literal-object-entry", separator: {token: ","}},
    "literal-object-entry": {choice: [
      {sequence: ["literal-object-key", {token: ":"}, "expression"]},
      {sequence: [{token: "..."}, "expression"]}
    ], railroad: false},
    "literal-object-key": {choice: [
      "identifier-property",
      "literal-string"
    ], railroad: false},
    "literal-array": {sequence: [{token: "["}, {zeroOrOne: "literal-array-list"}, {token: "]"}], inline: false, left: 1, right: 1},
    "literal-array-list": {list: "literal-array-entry", separator: {token: ","}},
    "literal-array-entry": {choice: [
      "expression",
      {sequence: [{token: "..."}, "expression"]}
    ], railroad: false},
    "literal-type": {sequence: [{token: "{"}, {zeroOrOne: "literal-type-list"}, {token: "}"}], inline: false, left: 1, right: 1},
    "literal-type-list": {list: "literal-type-entry", separator: {token: ","}},
    "literal-type-entry": {sequence: ["literal-object-key", {token: ":"}, "literal-type-value"], railroad: false},
    "literal-type-value": {choice: ["expression", "type-expression", "literal-type"], railroad: false}
  },
  variations: {
    full: {
      "operator-binary": null,
      "parameter": null
    },
    concise: {
      "expression": {sequence: [
        {zeroOrOne: {sequence: ["expression", {token: "?"}, "expression", {token: ":"}]}},
        "expression-binary"
      ], inline: false, right: 1},
      "expression-ternary": null,
      "expression-binary": {sequence: [
        {zeroOrOne: {sequence: ["expression-binary", "operator-binary"]}},
        "expression-unary"
      ], inline: false, right: 1},
      "expression-logical-or": null,
      "expression-logical-and": null,
      "expression-inclusive-or": null,
      "expression-exclusive-or": null,
      "expression-and": null,
      "expression-equality": null,
      "expression-relational": null,
      "expression-shift": null,
      "expression-additive": null,
      "expression-multiplicative": null,
      "parameter-list": {list: "parameter", separator: {token: ","}, inline: false, left: 1, right: 1},
      "parameter-positional-list": null,
      "parameter-positional": null,
      "parameter-named-list": null,
      "parameter-named": null
    }
  }
});
