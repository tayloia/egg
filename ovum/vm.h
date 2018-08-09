// Macro to define module sections: X(section, byte)
// 0xA3 is "POUND SIGN" in Latin-1 and not valid UTF-8
#define EGG_VM_SECTIONS(X) \
  X(SECTION_MAGIC, 0xA3) \
  X(SECTION_POSINTS, 0x01) \
  X(SECTION_NEGINTS, 0x02) \
  X(SECTION_FLOATS, 0x03) \
  X(SECTION_STRINGS, 0x04) \
  X(SECTION_CODE, 0xFF)

// Macro to define initial magic section: X(byte)
//  The first magic section is expected to be 0xA3,0x67,0x67,0x56,0x4D,0x00 like "EggVM\0"
#define EGG_VM_MAGIC(X) \
  X(0xA3) \
  X(0x67) \
  X(0x67) \
  X(0x56) \
  X(0x4D) \
  X(0x00)

// Macro to define module opcodes: X(opcode, text, row, minargs, maxargs)
#define EGG_VM_OPCODES(X) \
  X(OPCODE_ANY, "any", 1, 0, 0) \
  X(OPCODE_ANYQ, "anyq", 2, 0, 0) \
  X(OPCODE_ASSERT, "assert", 1, 1, 1) \
  X(OPCODE_ASSIGN, "assign", 1, 2, 2) \
  X(OPCODE_ATTRIBUTE, "attribute", 24, 1, N) \
  X(OPCODE_AVALUE, "avalue", 35, 0, N) \
  X(OPCODE_BINARY, "binary", 0, 3, 3) \
  X(OPCODE_BLOCK, "block", 25, 1, N) \
  X(OPCODE_BOOL, "bool", 16, 0, 1) \
  X(OPCODE_BREAK, "break", 3, 0, 0) \
  X(OPCODE_BYNAME, "byname", 2, 2, 2) \
  X(OPCODE_CALL, "call", 26, 1, N) \
  X(OPCODE_CALLABLE, "callable", 27, 1, N) \
  X(OPCODE_CASE, "case", 23, 3, N) \
  X(OPCODE_CATCH, "catch", 1, 3, 3) \
  X(OPCODE_CHOICE, "choice", 28, 1, N) \
  X(OPCODE_COMPARE, "compare", 3, 2, 2) \
  X(OPCODE_CONTINUE, "continue", 4, 0, 0) \
  X(OPCODE_DECREMENT, "decrement", 2, 1, 1) \
  X(OPCODE_DEFAULT, "default", 18, 2, N) \
  X(OPCODE_DO, "do", 4, 2, 2) \
  X(OPCODE_ELLIPSIS, "ellipsis", 3, 1, 1) \
  X(OPCODE_END, "end", 0, 0, 0) \
  X(OPCODE_EXTENSIBLE, "extensible", 29, 1, N) \
  X(OPCODE_FALSE, "false", 5, 0, 0) \
  X(OPCODE_FINALLY, "finally", 4, 1, 1) \
  X(OPCODE_FINITE, "finite", 6, 0, 0) \
  X(OPCODE_FLOAT, "float", 36, 0, N) \
  X(OPCODE_FOR, "for", 1, 4, 4) \
  X(OPCODE_FOREACH, "foreach", 2, 3, 3) \
  X(OPCODE_FUNCTION, "function", 15, 2, 3) \
  X(OPCODE_FVALUE, "fvalue", 5, 1, 1) \
  X(OPCODE_GENERATOR, "generator", 16, 2, 3) \
  X(OPCODE_GUARD, "guard", 3, 3, 3) \
  X(OPCODE_HAS, "has", 5, 2, 2) \
  X(OPCODE_HASQ, "hasq", 6, 2, 2) \
  X(OPCODE_IDENTIFIER, "identifier", 6, 1, 1) \
  X(OPCODE_IF, "if", 17, 2, 3) \
  X(OPCODE_INCREMENT, "increment", 7, 1, 1) \
  X(OPCODE_INDEX, "index", 7, 2, 2) \
  X(OPCODE_INDEXABLE, "indexable", 2, 4, 4) \
  X(OPCODE_INFERRED, "inferred", 7, 0, 0) \
  X(OPCODE_INT, "int", 37, 0, N) \
  X(OPCODE_ITERABLE, "iterable", 8, 1, 1) \
  X(OPCODE_IVALUE, "ivalue", 9, 1, 1) \
  X(OPCODE_LAMBDA, "lambda", 30, 1, N) \
  X(OPCODE_LENGTH, "length", 31, 1, N) \
  X(OPCODE_META, "meta", 8, 2, 2) \
  X(OPCODE_MODULE, "module", 42, 1, 3) \
  X(OPCODE_MUTATE, "mutate", 4, 3, 3) \
  X(OPCODE_NAMED, "named", 9, 2, 2) \
  X(OPCODE_NOOP, "noop", 8, 0, 0) \
  X(OPCODE_NOT, "not", 0, 1, 1) \
  X(OPCODE_NULL, "null", 9, 0, 0) \
  X(OPCODE_OBJECT, "object", 38, 0, N) \
  X(OPCODE_OPTIONAL, "optional", 22, 1, 2) \
  X(OPCODE_OVALUE, "ovalue", 39, 0, N) \
  X(OPCODE_POINTEE, "pointee", 10, 1, 1) \
  X(OPCODE_POINTER, "pointer", 11, 1, 1) \
  X(OPCODE_PROPERTY, "property", 10, 2, 2) \
  X(OPCODE_PROPERTYQ, "propertyq", 11, 2, 2) \
  X(OPCODE_REGEX, "regex", 12, 1, 1) \
  X(OPCODE_REQUIRED, "required", 23, 1, 2) \
  X(OPCODE_RETURN, "return", 17, 0, 1) \
  X(OPCODE_STRING, "string", 40, 0, N) \
  X(OPCODE_SVALUE, "svalue", 13, 1, 1) \
  X(OPCODE_SWITCH, "switch", 19, 2, N) \
  X(OPCODE_TERNARY, "ternary", 0, 4, 4) \
  X(OPCODE_THROW, "throw", 18, 0, 1) \
  X(OPCODE_TRUE, "true", 10, 0, 0) \
  X(OPCODE_TRY, "try", 32, 1, N) \
  X(OPCODE_TYPE, "type", 41, 0, N) \
  X(OPCODE_UNARY, "unary", 0, 2, 2) \
  X(OPCODE_UNION, "union", 33, 1, N) \
  X(OPCODE_VARARGS, "varargs", 20, 2, N) \
  X(OPCODE_VOID, "void", 11, 0, 0) \
  X(OPCODE_WHILE, "while", 12, 2, 2) \
  X(OPCODE_YIELD, "yield", 19, 0, 1)
