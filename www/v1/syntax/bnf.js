// EBNF of the Egg programming language in JSONP form
egg.bnf({
  rules: {
    "module": {zeroOrMore: "module-entry", left: 1, right: 1},
    "module-entry": {choice: [
       {sequence: [{zeroOrMore:"attribute"}, "statement"]},
       {sequence: [{token: "@"}, "attribute"]}
     ], railroad: false},
    "attribute": {sequence: [{token: "@"}, {sequence: ["attribute-name", {zeroOrOne: {sequence: [{token: "("}, "parameter-list", {token: ")"}]}}]}], inline: false, left: 1, right: 1},
    "attribute-name": {list: "identifier-attribute", separator: {token: "."}},
    "statement": {choice: [
      "statement-compound",
      {sequence: ["statement-simple", {token: ";"}]},
      "definition-function",
      "definition-type",
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
    "definition-type": {sequence: [{token: "type"}, "identifier-type", {zeroOrOne: {sequence: [{token: "<"}, "definition-type-list", {token: ">"}]}}, {zeroOrMore: "type-constraint"}, "definition-type-value"], inline: false, left: 1, right: 1},
    "definition-type-list": {list: "definition-type-entry", separator: {token: ","}},
    "definition-type-entry": {sequence: ["identifier-type", {zeroOrMore: "type-constraint" }, {zeroOrOne: {sequence: [{token: "="}, "type-expression"]}}]},
    "definition-type-value": {choice: [
      {sequence: [{token: "="}, "type-expression", {token: ";"}]},
      "literal-type"
    ], railroad: false},
    "definition-variable": {choice: [
      {sequence: [{ token: "var" }, {zeroOrOne: {token: "?"}}, "identifier-variable", {token: "="}, "expression"]},
      {sequence: ["type-expression", "identifier-variable", {zeroOrOne: {sequence: [{token: "="}, "expression"]}}]}
    ], inline: false},
    "definition-function": {sequence: ["function-signature", "statement-compound"], inline: false, left: 1, right: 1},
    "function-signature": {sequence: ["type-expression", "identifier-function", "function-parameter-list"]},
    "function-parameter-list": {sequence: [{token: "("}, {zeroOrOne: "function-parameters"}, {token: ")"}]},
    "function-parameters": {list: "function-parameter", separator: {token: ","}},
    "function-parameter": {sequence: [{zeroOrMore: "attribute"}, "function-parameter-declaration"], inline: false, left: 1},
    "function-parameter-declaration": {choice: [
      {sequence: ["type-expression", "identifier-variable", {zeroOrOne: {sequence: [{token: "="}, {token: "null"}]}}]},
      {sequence: [{token: "..."}, "type-expression", "identifier-variable"]},
      {sequence: [{token: "..."}, "type-expression", {token: "["}, {token: "]"}, "identifier-variable"]}
    ], railroad: false},
    "type-list": {list: "type-expression", separator: {token: ","}},
    "type-constraint": { sequence: [{token: ":"}, "type-expression"], inline: false },
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
      {sequence: [{token: "type"}, {zeroOrMore: "type-constraint"}, "literal-type"]},
      {sequence: [{token: "("}, "type-expression", {token: ")"}]}
    ]},
    "type-expression-suffix": {choice: [
      {token: "?"},
      {token: "!"},
      {token: "*"},
      {sequence: [{token: "["}, {zeroOrOne: "type-expression"}, {token: "]"}]},
      "function-parameter-list"
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
      "expression-type-instantiation",
      {sequence: [{token: "("}, "expression", {token: ")"}]}
    ], railroad: false},
    "expression-suffix": {choice: [
      {sequence: [{token: "["}, "expression", {token: "]"}]},
      {sequence: [{token: "."}, "identifier-property"]},
      {sequence: [{token: "("}, {zeroOrOne: "parameter-list"}, {token: ")"}]}
    ], railroad: false},
    "expression-function": {sequence: ["type-expression", "function-parameter-list", "statement-compound"], inline: false, left: 1, right: 1},
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
    "expression-type-instantiation": {sequence: ["identifier-type", {zeroOrOne: {sequence: [{token: "<"}, "expression-type-instantiation-list", {token: ">"}]}}, {zeroOrMore: "type-constraint"}, "type-instantiation"], inline: false, left: 1, right: 1},
    "expression-type-instantiation-list": {list: "type-expression", separator: {token: ","}},
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
    "identifier-function": { alias: "identifier" },
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
    "literal-exponent": {tokens: ["E", "e"], railroad: false},
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
    "literal-type": {sequence: [{token: "{"}, {zeroOrMore: "literal-type-entry"}, {token: "}"}], inline: false, left: 1, right: 1},
    "literal-type-entry": {choice: [
      "literal-type-iterate",
      "literal-type-call",
      "literal-type-property",
      "literal-type-function",
      "literal-type-static-property",
      "literal-type-static-function",
      "literal-type-static-type"
    ], railroad: false},
    "literal-type-iterate": {sequence: ["type-expression", {token: "..."}, {token: ";"}]},
    "literal-type-call": {sequence: ["type-expression", "function-parameter-list", {token: ";"}]},
    "literal-type-property": {sequence: ["type-expression", "literal-type-property-signature", "literal-type-access"]},
    "literal-type-property-signature": {choice: ["identifier-property", {sequence: [{token: "["}, {zeroOrOne: "type-expression"}, {token: "]"}]}], railroad: false},
    "literal-type-function": {sequence: ["function-signature", {token: ";"}]},
    "literal-type-static-property": {sequence: [{token: "static"}, {sequence: ["type-expression", "identifier-property", {token: "="}, "expression", {token: ";"}]}]},
    "literal-type-static-function": {sequence: [{token: "static"}, "definition-function"]},
    "literal-type-static-type": {sequence: [{token: "static"}, "definition-type"]},
    "literal-type-access": {choice: [
      {token: ";"},
      {sequence: [{token: "{"}, {oneOrMore: {sequence: [
        "literal-type-delegate-name",
        {token: ";"}
      ]}}, {token: "}"}]}
    ], railroad: false},
    "literal-type-delegate-name": {tokens: ["get", "set", "mut", "del"]},
    "type-instantiation": {sequence: [{ token: "{"}, {zeroOrMore: "type-instantiation-entry"}, {token: "}"}], inline: false, left: 1, right: 1},
    "type-instantiation-entry": {choice: [
      "type-instantiation-property",
      "type-instantiation-function",
      "type-instantiation-iterable",
      "type-instantiation-callable",
      "type-instantiation-indexable",
      "definition-type"
    ], railroad: false},
    "type-instantiation-property": {sequence: ["type-expression", "identifier-property", "type-instantiation-property-body"], inline: false, left: 1},
    "type-instantiation-property-body": {choice: [
      {sequence: [{token:"=" }, "expression", {token: ";"}]},
      {sequence: [{token:"=>" }, "expression-lambda-body", {token: ";"}]},
      {sequence: [{token: "{"}, {oneOrMore: "type-instantiation-delegate"}, {token: "}"}]}
    ], railroad: false},
    "type-instantiation-delegate": {sequence: ["type-expression", "literal-type-delegate-name", "function-parameter-list", "statement-compound"], inline: false},
    "type-instantiation-function": {sequence: ["type-expression", "identifier-property", "function-parameter-list", "statement-compound"], inline: false, left: 1, right: 1},
    "type-instantiation-function-body": {choice: [
      {sequence: [{token:"=>" }, "expression", {token: ";"}]},
      "statement-compound"
    ], railroad: false},
    "type-instantiation-iterable": {sequence: ["type-expression", {token: "..."}, "statement-compound"], inline: false, left: 1, right: 1},
    "type-instantiation-callable": {sequence: ["type-expression", "function-parameter-list", "statement-compound"], inline: false, left: 1, right: 1},
    "type-instantiation-indexable": {sequence: ["type-expression", {token: "["}, {zeroOrOne: "type-expression"}, {token: "]"}, {token: "{"}, {oneOrMore: "type-instantiation-delegate"}, {token: "}"}], inline: false, left: 1, right: 1}
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
