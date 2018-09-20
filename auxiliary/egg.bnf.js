// EBNF of the Egg programming language in JSONP form
egg.bnf({
  rules: {
    "module": {oneOrMore: {sequence: [{zeroOrMore:"attribute"}, "statement"]}},
    "attribute": {sequence: [{token: "@"}, "identifier-attribute", {zeroOrOne: {sequence: [{token: "("}, "parameter-list", {token: ")"}]}}]},
    "statement": {choice: [
      {sequence: ["statement-simple", {token: ";"}]},
      "statement-compound",
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
    "statement-call": {sequence: ["expression-postfix", {token: "("}, {zeroOrOne:"parameter-list"}, {token: ")"}]},
    "statement-compound": {sequence: [{token: "{"}, {zeroOrMore: "statement"}, {token: "}"}], collapse: true},
    "statement-flow": {choice: [
      "statement-branch",
      {sequence: ["statement-jump", {token: ";"}]}
    ]},
    "statement-branch": {choice: [
      "statement-if",
      {sequence: [{token: "switch"}, {token: "("}, "expression", {token: ")"}, "statement-compound"]},
      {sequence: [{token: "while"}, {token: "("}, "condition", {token: ")"}, "statement-compound"]},
      {sequence: [{token: "do"}, "statement-compound", {token: "while"}, {token: "("}, "expression", {token: ")"}, {token: ";"}]},
      {sequence: [{token: "for"}, {token: "("}, {zeroOrOne:"statement-simple"}, {token: ";"}, {zeroOrOne:"condition"}, {token: ";"}, {zeroOrOne:"statement-action"}, {token: ")"}, "statement-compound"]},
      {sequence: [{token: "for"}, {token: "("}, "definition-variable-type", "identifier-variable", {token: ":"}, "expression", {token: ")"}, "statement-compound"]},
      {sequence: [{token: "try"}, "statement-compound", {oneOrMore:"statement-catch"}]},
      {sequence: [{token: "try"}, "statement-compound", {zeroOrMore:"statement-catch"}, "statement-finally"]}
    ]},
    "statement-jump": {choice: [
      {token: "continue"},
      {token: "break"},
      {sequence: [{token: "return"}, {zeroOrOne:"expression"}]},
      {sequence: [{token: "yield"}, "expression"]},
      {sequence: [{token: "yield"}, {token: "..."}, "expression"]}
    ]},
    "statement-if": {sequence: [{token: "if"}, {token: "("}, "condition", {token: ")"}, "statement-compound", {zeroOrOne:"statement-else"}]},
    "statement-else": {choice: [
      {sequence: [{token: "else"}, "statement-compound"]},
      {sequence: [{token: "else"}, "statement-if"]}
    ]},
    "statement-catch": {sequence: [{token: "catch"}, {token: "("}, "definition-variable-type", "identifier-variable", {token: ")"}, "statement-compound"]},
    "statement-finally": {sequence: [{token: "finally"}, "statement-compound"]},
    "definition-type": {sequence: [{token: "type"}, "identifier-type", {zeroOrOne: {sequence: [{token: "<"}, "type-list", {token: ">"}]}}, {token: "="}, "type-expression"]},
    "definition-variable": {sequence: ["definition-variable-type", "identifier-variable", {zeroOrOne: {sequence: [{token: "="}, "expression"]}}]},
    "definition-variable-type": {choice: [
      "type-expression",
      {token: "var"}
    ]},
    "definition-function": {sequence: ["type-expression", "identifier-variable", {token: "("}, {zeroOrOne: "definition-function-parameter-list"}, {token: ")"}, "statement-compound"]},
    "definition-function-parameter-list": {choice: [
      "definition-function-parameter",
      {sequence: ["definition-function-parameter-list", {token: ","}, "definition-function-parameter"]}
    ]},
    "definition-function-parameter": {choice: [
      {sequence: [{zeroOrMore: "attribute"}, "type-expression", "identifier-variable", {zeroOrMore: {sequence: [{token: "="}, {token: "null"}]}}]},
      {sequence: [{zeroOrMore: "attribute"}, {token: "..."}, "type-expression", {token: "["}, {token: "]"}, "identifier-variable"]}
    ]},
    "type-list": {choice: [
      "type-expression",
      {sequence: ["type-list", {token: ","}, "type-expression"]}
    ]},
    "type-expression": {choice: ["type-expression-union"]},
    "type-expression-union": {choice: [
      "type-expression-postfix",
      {sequence: ["type-expression-union", {token: "|" }, "type-expression-postfix"]}
    ]},
    "type-expression-postfix": {choice: [
      "type-expression-primary",
      {sequence: ["type-expression-postfix", {token: "?"}]},
      {sequence: ["type-expression-postfix", {token: "!"}]},
      {sequence: ["type-expression-postfix", {token: "*"}]},
      {sequence: ["type-expression-postfix", {token: "["}, {zeroOrOne: "type-expression"}, {token: "]"}]},
      {sequence: ["type-expression-postfix", {token: "("}, {zeroOrOne: "definition-function-parameter-list"}, {token: ")"}]}
    ]},
    "type-expression-primary": {choice: [
      "identifier-type",
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
      {sequence: ["expression-binary", {token: "?"}, "expression", {token: ":"}, "expression-ternary"]}
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
      {sequence: [{token: "&"}, "identifier-variable"]},
      {sequence: ["operator-unary", "expression-unary"]},
    ]},
    "expression-postfix": {choice: [
      "expression-primary",
      {sequence: ["expression-postfix", {token: "["}, "expression", {token: "]"}]},
      {sequence: ["expression-postfix", {token: "("}, {zeroOrOne: "parameter-list"}, {token: ")"}]},
      {sequence: ["expression-postfix", {token: "."}, "identifier-property"]},
      {sequence: ["expression-postfix", {token: "?."}, "identifier-property"]}
    ]},
    "expression-primary": {choice: [
      "identifier-variable",
      "literal",
      //"lambda-value",
      {sequence: [{token: "("}, "expression", {token: ")"}]}
    ]},

    "operator-unary": {tokens: ["*", "-", "~", "!"]},
    "operator-binary": {tokens: ["??", "||", "&&", "|", "^", "&", "==", "!=", "<", ">", "<=", ">=", "<<", ">>", ">>>", "+", "-", "*", "/", "%"]},
    "operator-assignment": {tokens: ["=", "+=", "-=", "*=", "/=", "%=", "<<=", ">>=", ">>>=", "&=", "|=", "^=", "&&=", "||=", "??="]},
    "operator-mutation": {tokens: ["++", "--"]},

    "condition": {choice: [
      "expression",
      {sequence: ["condition-variable-type", "identifier-variable", {token: "="}, "expression"]}
    ]},
    "condition-variable-type": {choice: [
      "type-expression",
      {token: "var"},
      {sequence: [{token: "var"}, {token: "?"}]}
    ]},

    "assignment-target": {choice: [
      "identifier-variable",
      {sequence: [{oneOrMore: {token: "*"}}, "expression"]},
      {sequence: ["expression", {token: "["}, "expression", {token: "]"}]},
      {sequence: ["expression", {token: "."}, "identifier-property"]}
    ]},

    "parameter-list": {choice: [
      "positional-parameter-list",
      {sequence: ["positional-parameter-list", {token: ","}, "named-parameter-list"]},
      "named-parameter-list"
    ]},
    "positional-parameter-list": {choice: [
      "positional-parameter",
      {sequence: ["positional-parameter-list", {token: ","}, "positional-parameter"]}
    ]},
    "positional-parameter": {choice: [
      "expression",
      {sequence: [{token: "..."}, "expression"]}
    ]},
    "named-parameter-list": {choice: [
      "named-parameter",
      {sequence: ["named-parameter-list", {token: ","}, "named-parameter"]}
    ]},
    "named-parameter": {sequence: ["identifier-variable", {token: ":"}, "expression"]},

    "identifier-attribute": {terminal: "identifier", collapse: true},
    "identifier-property": {terminal: "identifier", collapse: true},
    "identifier-type": {terminal: "identifier", collapse: true},
    "identifier-variable": {terminal: "identifier", collapse: true},

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
    "literal-int": {terminal: "integer", collapse: false},
    "literal-float": {terminal: "floating-point", collapse: false},
    "literal-string": {sequence: [{token: "\""}, {terminal: "string"}, {token: "\""}]},
    "literal-object": {sequence: [{token: "{"}, {zeroOrOne: "literal-object-list"}, {token: "}"}]},
    "literal-object-list": {choice: [
      "literal-object-entry",
      {sequence: ["literal-object-list", {zeroOrOne: {token: ","}}, "literal-object-entry"]}
    ]},
    "literal-object-entry": {choice: [
      {sequence: ["literal-object-key", {token: ":"}, "expression"]},
      {sequence: [{token: "..."}, "expression"]}
    ]},
    "literal-object-key": {choice: [
      "identifier-property",
      "literal-string"
    ]},
    "literal-array": {sequence: [{token: "["}, {zeroOrOne: "literal-array-list"}, {token: "]"}], collapse: false},
    "literal-array-list": {choice: [
      "literal-array-entry",
      {sequence: ["literal-array-list", {zeroOrOne: {token: ","}}, "literal-array-entry"]}
    ]},
    "literal-array-entry": {choice: [
      "expression",
      {sequence: [{token: "..."}, "expression"]}
    ]}
  },
  variations: {
    full: {
      "operator-binary": null
    },
    concise: {
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
      "expression-multiplicative": null
    }
  }
});

/*
*/