// This is the number of arguments encoded in the machine byte meaning "variable"
#define EGG_VM_NARGS 5

// This is the first machine byte without an integer operand for its opcode
#define EGG_VM_ISTART 30

// This is the integer step between operator classes
#define EGG_VM_OCSTEP 32

// This is the integer step between operator operand counts
#define EGG_VM_OOSTEP 64

// Macro to define module sections: X(section, byte)
// 0xA3 is "POUND SIGN" in Latin-1 and not valid UTF-8
#define EGG_VM_SECTIONS(X) \
  X(SECTION_MAGIC, 0xA3) \
  X(SECTION_POSINTS, 0x01) \
  X(SECTION_NEGINTS, 0x02) \
  X(SECTION_FLOATS, 0x03) \
  X(SECTION_STRINGS, 0x04) \
  X(SECTION_CODE, 0xFE) \
  X(SECTION_SOURCE, 0xFF)

// Macro to define initial magic section: X(byte)
//  The first magic section is expected to be 0xA3,0x67,0x67,0x56,0x4D,0x00 like "EggVM\0" but with a GBP currency sign
#define EGG_VM_MAGIC(X) \
  X(0xA3) \
  X(0x67) \
  X(0x67) \
  X(0x56) \
  X(0x4D) \
  X(0x00)

// Macro to define module opcodes: X(opcode, minbyte, minargs, maxargs, text)
#define EGG_VM_OPCODES(X) \
  X(ANY, 30, 0, 0, "any") \
  X(ANYQ, 36, 0, 0, "anyq") \
  X(ASSERT, 31, 1, 1, "assert") \
  X(ASSIGN, 32, 2, 2, "assign") \
  X(ATTRIBUTE, 151, 1, N, "attribute") \
  X(AVALUE, 210, 0, N, "avalue") \
  X(BINARY, 2, 2, 2, "binary") \
  X(BLOCK, 157, 1, N, "block") \
  X(BOOL, 102, 0, 1, "bool") \
  X(BREAK, 42, 0, 0, "break") \
  X(BYNAME, 38, 2, 2, "byname") \
  X(CALL, 163, 1, N, "call") \
  X(CALLABLE, 169, 1, N, "callable") \
  X(CASE, 128, 2, N, "case") \
  X(CATCH, 33, 3, 3, "catch") \
  X(CHOICE, 175, 1, N, "choice") \
  X(COMPARE, 8, 2, 2, "compare") \
  X(CONTINUE, 48, 0, 0, "continue") \
  X(DECLARE, 92, 2, 3, "declare") \
  X(DECREMENT, 37, 1, 1, "decrement") \
  X(DEFAULT, 181, 1, N, "default") \
  X(DEL, 204, 0, 0, "del") \
  X(DO, 44, 2, 2, "do") \
  X(ELLIPSIS, 43, 1, 1, "ellipsis") \
  X(END, 0, 0, 0, "end") \
  X(EXTENSIBLE, 187, 1, N, "extensible") \
  X(FALSE, 54, 0, 0, "false") \
  X(FINITE, 60, 0, 0, "finite") \
  X(FLOAT, 216, 0, N, "float") \
  X(FOR, 34, 4, 4, "for") \
  X(FOREACH, 39, 3, 3, "foreach") \
  X(FUNCTION, 98, 2, 3, "function") \
  X(FVALUE, 12, 0, 0, "fvalue") \
  X(GENERATOR, 105, 3, 3, "generator") \
  X(GET, 186, 0, 0, "get") \
  X(GUARD, 45, 3, 3, "guard") \
  X(IDENTIFIER, 49, 1, 1, "identifier") \
  X(IF, 110, 2, 3, "if") \
  X(INCREMENT, 55, 1, 1, "increment") \
  X(INDEX, 50, 2, 2, "index") \
  X(INDEXABLE, 86, 2, N, "indexable") \
  X(INFERRED, 66, 0, 0, "inferred") \
  X(INT, 222, 0, N, "int") \
  X(ITERABLE, 61, 1, 1, "iterable") \
  X(IVALUE, 6, 0, 0, "ivalue") \
  X(LAMBDA, 193, 1, N, "lambda") \
  X(LENGTH, 199, 1, N, "length") \
  X(META, 7, 1, 1, "meta") \
  X(MODULE, 253, 1, 3, "module") \
  X(MUT, 198, 0, 0, "mut") \
  X(MUTATE, 14, 2, 2, "mutate") \
  X(NAMED, 62, 2, 2, "named") \
  X(NOOP, 72, 0, 0, "noop") \
  X(NOT, 67, 1, 1, "not") \
  X(NULL_, 78, 0, 0, "null") \
  X(OBJECT, 228, 0, N, "object") \
  X(OPTIONAL, 139, 1, 2, "optional") \
  X(OVALUE, 234, 0, N, "ovalue") \
  X(POINTEE, 73, 1, 1, "pointee") \
  X(POINTER, 79, 1, 1, "pointer") \
  X(PREDICATE, 85, 1, 1, "predicate") \
  X(PROPERTY, 68, 2, 2, "property") \
  X(PROPERTYQ, 74, 2, 2, "propertyq") \
  X(REQUIRED, 145, 1, 2, "required") \
  X(RETURN, 108, 0, 1, "return") \
  X(SET, 192, 0, 0, "set") \
  X(STATIC, 91, 1, 1, "static") \
  X(STRING, 240, 0, N, "string") \
  X(SVALUE, 18, 0, 0, "svalue") \
  X(SWITCH, 122, 2, N, "switch") \
  X(TERNARY, 3, 3, 3, "ternary") \
  X(THROW, 114, 0, 1, "throw") \
  X(TRUE, 84, 0, 0, "true") \
  X(TRY, 116, 2, N, "try") \
  X(TYPEDEF, 19, 1, N, "typedef") \
  X(UNARY, 1, 1, 1, "unary") \
  X(UNION, 205, 1, N, "union") \
  X(VARARGS, 134, 2, N, "varargs") \
  X(VOID, 90, 0, 0, "void") \
  X(WHILE, 80, 2, 2, "while") \
  X(YIELD, 120, 0, 1, "yield")

// Macro to define operator classes: X(opclass, unique, text)
#define EGG_VM_OPCLASSES(X) \
  X(UNARY, 0, "unary") \
  X(BINARY, 2, "binary") \
  X(COMPARE, 3, "compare") \
  X(TERNARY, 4, "ternary")

// Macro to define operators: X(operator, opclass, index, text)
#define EGG_VM_OPERATORS(X) \
  X(ADD, BINARY, 0, "+") \
  X(BITAND, BINARY, 1, "&") \
  X(BITNOT, UNARY, 0, "~") \
  X(BITOR, BINARY, 2, "|") \
  X(BITXOR, BINARY, 3, "^") \
  X(DEREF, UNARY, 1, "*") \
  X(DIV, BINARY, 4, "/") \
  X(EQ, COMPARE, 0, "==") \
  X(GE, COMPARE, 1, ">=") \
  X(GT, COMPARE, 2, ">") \
  X(IFNULL, BINARY, 5, "??") \
  X(LE, COMPARE, 3, "<=") \
  X(LOGAND, BINARY, 6, "&&") \
  X(LOGNOT, UNARY, 2, "!") \
  X(LOGOR, BINARY, 7, "||") \
  X(LT, COMPARE, 4, "<") \
  X(MUL, BINARY, 8, "*") \
  X(NE, COMPARE, 5, "!=") \
  X(NEG, UNARY, 3, "-") \
  X(REF, UNARY, 4, "&") \
  X(REM, BINARY, 9, "%") \
  X(SHIFTL, BINARY, 10, "<<") \
  X(SHIFTR, BINARY, 11, ">>") \
  X(SHIFTU, BINARY, 12, ">>>") \
  X(SUB, BINARY, 13, "-") \
  X(TERNARY, TERNARY, 0, "?:")
