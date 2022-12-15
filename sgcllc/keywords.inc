// enum value, string presentation, type options
// b b b b b b b b
//             | |-- is type?
//             |---- is type specifier?

keyword(KW_VOID, "void", 0b01)
keyword(KW_BOOL, "bool", 0b01)
keyword(KW_I8, "i8", 0b01)
keyword(KW_I16, "i16", 0b01)
keyword(KW_I32, "i32", 0b01)
keyword(KW_I64, "i64", 0b01)
keyword(KW_F32, "f32", 0b01)
keyword(KW_F64, "f64", 0b01)
keyword(KW_LET, "let", 0b01)
keyword(KW_STRING, "string", 0b01)
keyword(KW_RETURN, "return", 0b00)
keyword(KW_IMPORT, "import", 0b00)
keyword(KW_PRIVATE, "private", 0b10)
keyword(KW_PUBLIC, "public", 0b10)
keyword(KW_PROTECTED, "protected", 0b10)
keyword(KW_UNSIGNED, "unsigned", 0b10)

// operators
keyword(OP_ASSIGN_ADD, "+=", 0b00)
keyword(OP_ASSIGN_SUB, "-=", 0b00)
keyword(OP_ASSIGN_MUL, "*=", 0b00)
keyword(OP_ASSIGN_DIV, "/=", 0b00)
keyword(OP_ASSIGN_MOD, "%=", 0b00)