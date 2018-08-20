// This is the number of arguments encoded in the machine byte meaning "variable"
#define EGG_VM_NARGS 5

// This is the first machine byte without an integer operand for its opcode
#define EGG_VM_ISTART 24

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
//  The first magic section is expected to be 0xA3,0x67,0x67,0x56,0x4D,0x00 like "EggVM\0"
#define EGG_VM_MAGIC(X) \
  X(0xA3) \
  X(0x67) \
  X(0x67) \
  X(0x56) \
  X(0x4D) \
  X(0x00)

// Macro to define module opcodes: X(opcode, minbyte, minargs, maxargs, text)
#define EGG_VM_OPCODES(X) \
  X(OPCODE_ANY, 24, 0, 0, "any") \
  X(OPCODE_ANYQ, 30, 0, 0, "anyq") \
  X(OPCODE_ASSERT, 25, 1, 1, "assert") \
  X(OPCODE_ASSIGN, 26, 2, 2, "assign") \
  X(OPCODE_ATTRIBUTE, 151, 1, N, "attribute") \
  X(OPCODE_AVALUE, 210, 0, N, "avalue") \
  X(OPCODE_BINARY, 2, 2, 2, "binary") \
  X(OPCODE_BLOCK, 157, 1, N, "block") \
  X(OPCODE_BOOL, 108, 0, 1, "bool") \
  X(OPCODE_BREAK, 36, 0, 0, "break") \
  X(OPCODE_BYNAME, 32, 2, 2, "byname") \
  X(OPCODE_CALL, 163, 1, N, "call") \
  X(OPCODE_CALLABLE, 169, 1, N, "callable") \
  X(OPCODE_CASE, 147, 3, N, "case") \
  X(OPCODE_CATCH, 27, 3, 3, "catch") \
  X(OPCODE_CHOICE, 175, 1, N, "choice") \
  X(OPCODE_COMPARE, 7, 1, 1, "compare") \
  X(OPCODE_CONTINUE, 42, 0, 0, "continue") \
  X(OPCODE_DECREMENT, 31, 1, 1, "decrement") \
  X(OPCODE_DEFAULT, 122, 2, N, "default") \
  X(OPCODE_DO, 38, 2, 2, "do") \
  X(OPCODE_ELLIPSIS, 37, 1, 1, "ellipsis") \
  X(OPCODE_END, 0, 0, 0, "end") \
  X(OPCODE_EXTENSIBLE, 181, 1, N, "extensible") \
  X(OPCODE_FALSE, 48, 0, 0, "false") \
  X(OPCODE_FINALLY, 43, 1, 1, "finally") \
  X(OPCODE_FINITE, 54, 0, 0, "finite") \
  X(OPCODE_FLOAT, 216, 0, N, "float") \
  X(OPCODE_FOR, 28, 4, 4, "for") \
  X(OPCODE_FOREACH, 33, 3, 3, "foreach") \
  X(OPCODE_FUNCTION, 104, 2, 3, "function") \
  X(OPCODE_FVALUE, 12, 0, 0, "fvalue") \
  X(OPCODE_GENERATOR, 110, 2, 3, "generator") \
  X(OPCODE_GUARD, 39, 3, 3, "guard") \
  X(OPCODE_HAS, 44, 2, 2, "has") \
  X(OPCODE_HASQ, 50, 2, 2, "hasq") \
  X(OPCODE_IDENTIFIER, 49, 1, 1, "identifier") \
  X(OPCODE_IF, 116, 2, 3, "if") \
  X(OPCODE_INCREMENT, 55, 1, 1, "increment") \
  X(OPCODE_INDEX, 56, 2, 2, "index") \
  X(OPCODE_INDEXABLE, 34, 4, 4, "indexable") \
  X(OPCODE_INFERRED, 60, 0, 0, "inferred") \
  X(OPCODE_INT, 222, 0, N, "int") \
  X(OPCODE_ITERABLE, 61, 1, 1, "iterable") \
  X(OPCODE_IVALUE, 6, 0, 0, "ivalue") \
  X(OPCODE_LAMBDA, 187, 1, N, "lambda") \
  X(OPCODE_LENGTH, 193, 1, N, "length") \
  X(OPCODE_META, 62, 2, 2, "meta") \
  X(OPCODE_MODULE, 253, 1, 3, "module") \
  X(OPCODE_MUTATE, 8, 2, 2, "mutate") \
  X(OPCODE_NAMED, 68, 2, 2, "named") \
  X(OPCODE_NOOP, 66, 0, 0, "noop") \
  X(OPCODE_NOT, 67, 1, 1, "not") \
  X(OPCODE_NULL, 72, 0, 0, "null") \
  X(OPCODE_OBJECT, 228, 0, N, "object") \
  X(OPCODE_OPTIONAL, 139, 1, 2, "optional") \
  X(OPCODE_OVALUE, 234, 0, N, "ovalue") \
  X(OPCODE_POINTEE, 73, 1, 1, "pointee") \
  X(OPCODE_POINTER, 79, 1, 1, "pointer") \
  X(OPCODE_PROPERTY, 74, 2, 2, "property") \
  X(OPCODE_PROPERTYQ, 80, 2, 2, "propertyq") \
  X(OPCODE_REGEX, 85, 1, 1, "regex") \
  X(OPCODE_REQUIRED, 145, 1, 2, "required") \
  X(OPCODE_RETURN, 114, 0, 1, "return") \
  X(OPCODE_STRING, 240, 0, N, "string") \
  X(OPCODE_SVALUE, 18, 0, 0, "svalue") \
  X(OPCODE_SWITCH, 128, 2, N, "switch") \
  X(OPCODE_TERNARY, 3, 3, 3, "ternary") \
  X(OPCODE_THROW, 120, 0, 1, "throw") \
  X(OPCODE_TRUE, 78, 0, 0, "true") \
  X(OPCODE_TRY, 199, 1, N, "try") \
  X(OPCODE_TYPE, 246, 0, N, "type") \
  X(OPCODE_UNARY, 1, 1, 1, "unary") \
  X(OPCODE_UNION, 205, 1, N, "union") \
  X(OPCODE_VARARGS, 134, 2, N, "varargs") \
  X(OPCODE_VOID, 84, 0, 0, "void") \
  X(OPCODE_WHILE, 86, 2, 2, "while") \
  X(OPCODE_YIELD, 126, 0, 1, "yield")

#define EGG_VM_BASAL(X) \
  X(Void) \
  X(Null) \
  X(Bool) \
  X(Int) \
  X(Float) \
  X(String) \
  X(Object)
