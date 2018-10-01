// EBNF of the Egg programming language in JSONP form
egg.bnf({
  rules: {
    "module": {oneOrMore: {sequence: [{zeroOrMore:"attribute"}, "statement"]}, collapse: false},
    "attribute": {sequence: [{token: "@"}, "identifier-attribute", {zeroOrOne: {sequence: [{token: "("}, "parameter-list", {token: ")"}]}}], collapse: false},
    "statement": {choice: [
      "statement-compound",
      {sequence: ["statement-simple", {token: ";"}]},
      "statement-flow",
      "definition-function"
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
    "statement-call": {sequence: ["expression-postfix", {token: "("}, {zeroOrOne: "parameter-list"}, {token: ")"}]},
    "statement-compound": {sequence: [{token: "{"}, {zeroOrMore: "statement"}, {token: "}"}], collapse: true},
    "statement-flow": {choice: [
      "statement-if",
      "statement-switch",
      "statement-while",
      "statement-do",
      "statement-for",
      "statement-foreach",
      "statement-try",
      "statement-try-finally",
      "statement-break",
      "statement-continue",
      "statement-return",
      "statement-yield"
    ]},
    "statement-if": {sequence: [{token: "if"}, {token: "("}, "condition", {token: ")"}, "statement-compound", {zeroOrOne: {sequence: [{token: "else"}, "statement-else"]}}], collapse: false},
    "statement-else": {choice: [
      "statement-compound",
      "statement-if"
    ], railroad: false},
    "statement-switch": {sequence: [{token: "switch"}, {token: "("}, "expression", {token: ")"}, {token: "{"}, {oneOrMore: "statement-case"}, {token: "}"}], collapse: false},
    "statement-case": {sequence: [{oneOrMore: "statement-case-value"}, {oneOrMore: "statement"}]},
    "statement-case-value": {choice: [
      {sequence: [{token: "case"}, "expression", {token: ":"}]},
      {sequence: [{token: "default"}, {token: ":"}]}
    ], railroad: false},
    "statement-while": {sequence: [{token: "while"}, {token: "("}, "condition", {token: ")"}, "statement-compound"], collapse: false},
    "statement-do": {sequence: [{token: "do"}, "statement-compound", {token: "while"}, {token: "("}, "expression", {token: ")"}, {token: ";"}], collapse: false},
    "statement-for": {sequence: [{token: "for"}, {token: "("}, {zeroOrOne:"statement-simple"}, {token: ";"}, {zeroOrOne:"condition"}, {token: ";"}, {zeroOrOne:"statement-action"}, {token: ")"}, "statement-compound"], collapse: false},
    "statement-foreach": {sequence: [{token: "for"}, {token: "("}, "statement-foreach-type", "identifier-variable", {token: ":"}, "expression", {token: ")"}, "statement-compound"], collapse: false},
    "statement-foreach-type": {choice: [
      "type-expression",
      {token: "var"}
    ], railroad: false},
    "statement-try": {sequence: [{token: "try"}, "statement-compound", {oneOrMore:"statement-catch"}], collapse: false},
    "statement-try-finally": {sequence: [{token: "try"}, "statement-compound", { zeroOrMore: "statement-catch" }, {token: "finally"}, "statement-compound"], collapse: false},
    "statement-catch": {sequence: [{token: "catch"}, {token: "("}, "type-expression", "identifier-variable", {token: ")"}, "statement-compound"], collapse: false},
    "statement-break": {sequence: [{token: "break"}, {token: ";"}], collapse: false},
    "statement-continue": {sequence: [{token: "continue"}, {token: ";"}], collapse: false},
    "statement-return": {sequence: [{token: "return"}, {zeroOrOne: "expression"}, {token: ";"}], collapse: false},
    "statement-yield": {sequence: [{token: "yield"}, {zeroOrOne: {token: "..."}}, "expression", {token: ";"}], collapse: false},
    "definition-type": {sequence: [{token: "type"}, "identifier-type", {zeroOrOne: {sequence: [{token: "<"}, "definition-type-list", {token: ">"}]}}, {token: "="}, "type-expression"], collapse: false},
    "definition-type-list": {list: "identifier-type", separator: {token: ","}},
    "definition-variable": {choice: [
      {sequence: ["type-expression", "identifier-variable", {zeroOrOne: {sequence: [{token: "="}, "expression"]}}]},
      {sequence: [{token: "var"}, "identifier-variable", {token: "="}, "expression"]}
    ], collapse: false},
    "definition-function": {sequence: ["type-expression", "identifier-variable", {token: "("}, {zeroOrOne: "definition-function-parameter-list"}, {token: ")"}, "statement-compound"], collapse: false},
    "definition-function-parameter-list": {list: "definition-function-parameter", separator: {token: ","}},
    "definition-function-parameter": {choice: [
      {sequence: [{zeroOrMore: "attribute"}, "type-expression", "identifier-variable", {zeroOrOne: {sequence: [{token: "="}, {token: "null"}]}}]},
      {sequence: [{zeroOrMore: "attribute"}, {token: "..."}, "type-expression", {token: "["}, {token: "]"}, "identifier-variable"]}
    ]},
    "type-list": {list: "type-expression", separator: {token: ","}},
    "type-expression": {list: "type-expression-postfix", separator: {token: "|" }, collapse: false},
    "type-expression-postfix": {sequence: ["type-expression-primary", {zeroOrMore: "type-expression-suffix"}], collapse: false},
    "type-expression-suffix": {choice: [
      {token: "?"},
      {token: "!"},
      {token: "*"},
      {sequence: [{token: "["}, {zeroOrOne: "type-expression"}, {token: "]"}]},
      {sequence: [{token: "("}, {zeroOrOne: "definition-function-parameter-list"}, {token: ")"}]}
    ]},
    "type-expression-primary": {choice: [
      {sequence: ["identifier-type", {zeroOrOne: {sequence: [{token: "<"}, "type-list", {token: ">"}]}}]},
      {token: "any"},
      {token: "void"},
      {token: "null"},
      {token: "bool"},
      {token: "int"},
      {token: "float"},
      {token: "string"},
      {token: "object"},
      {sequence: [{token: "type"}, "literal-object"]},
      {sequence: [{token: "("}, "type-expression", {token: ")"}]}
    ]},
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
    "expression-unary": {choice: [
      "expression-postfix",
      {sequence: ["operator-unary", "expression-unary"]},
      {sequence: [{token: "&"}, "identifier-variable"]}
    ]},
    "expression-postfix": {sequence: ["expression-primary", {zeroOrMore: "expression-suffix"}], collapse: false},
    "expression-suffix": {choice: [
      {sequence: [{token: "["}, "expression", {token: "]"}]},
      {sequence: [{token: "("}, {zeroOrOne: "parameter-list"}, {token: ")"}]},
      {sequence: [{token: "."}, "identifier-property"]},
      {sequence: [{token: "?."}, "identifier-property"]}
    ]},
    "expression-primary": {choice: [
      "identifier-variable",
      "literal",
      //"lambda-value",
      {sequence: [{token: "("}, "expression", {token: ")"}]}
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
      "type-expression",
      {token: "var"},
      {sequence: [{token: "var"}, {token: "?"}]}
    ], railroad: false},

    "assignment-target": {choice: [
      "identifier-variable",
      {sequence: [{token: "*"}, "expression-unary"]},
      {sequence: ["expression-postfix", {token: "["}, "expression", {token: "]"}]},
      {sequence: ["expression-postfix", {token: "."}, "identifier-property"]}
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

    "identifier-attribute": {terminal: "identifier", collapse: false},
    "identifier-property": {terminal: "identifier", collapse: false},
    "identifier-type": {terminal: "identifier", collapse: false},
    "identifier-variable": {terminal: "identifier", collapse: false},

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
    "literal-int": {terminal: "integer", collapse: false, railroad: false},
    "literal-float": {terminal: "floating-point", collapse: false, railroad: false},
    "literal-string": {sequence: [{token: "\""}, {terminal: "string"}, {token: "\""}]},
    "literal-object": {sequence: [{token: "{"}, {zeroOrOne: "literal-object-list"}, {token: "}"}], collapse: false},
    "literal-object-list": {list: "literal-object-entry", separator: {token: ","}},
    "literal-object-entry": {choice: [
      {sequence: ["literal-object-key", {token: ":"}, "expression"]},
      {sequence: [{token: "..."}, "expression"]}
    ], railroad: false},
    "literal-object-key": {choice: [
      "identifier-property",
      "literal-string"
    ], railroad: false},
    "literal-array": {sequence: [{token: "["}, {zeroOrOne: "literal-array-list"}, {token: "]"}], collapse: false},
    "literal-array-list": {list: "literal-array-entry", separator: {token: ","}},
    "literal-array-entry": {choice: [
      "expression",
      {sequence: [{token: "..."}, "expression"]}
    ], railroad: false}
  },
  variations: {
    full: {
      rules: {
        "operator-binary": null,
        "parameter": null
      },
      collapse: false
    },
    concise: {
      rules: {
        "expression-binary": {choice: [
          "expression-unary",
          {sequence: ["expression-binary", "operator-binary", "expression-unary"]}
        ]},
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
        "parameter-list": {list: "parameter", separator: {token: ","}, collapse: false},
        "parameter-positional-list": null,
        "parameter-positional": null,
        "parameter-named-list": null,
        "parameter-named": null
      },
      collapse: true
    }
  }
});
