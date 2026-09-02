#ifndef LILY_TYPES_BUILTINS_H
#define LILY_TYPES_BUILTINS_H

#define BUILTIN_TYPES(X)                                            \
    X(variadic,  "...",     0,               0)                     \
    X(void,      "void",    0,               0)                     \
    X(bool,      "bool",    1,               1)                     \
    /*X(char,      "char",    1,               1)                 */\
                                                                    \
    X(u8,        "u8",      1,               _Alignof(u8))          \
    X(u16,       "u16",     2,               _Alignof(u16))         \
    X(u32,       "u32",     4,               _Alignof(u32))         \
    X(u64,       "u64",     8,               _Alignof(u64))         \
    X(usize,     "usize",   sizeof(void*),   _Alignof(void*))       \
                                                                    \
    X(i8,        "i8",      1,               _Alignof(i8))          \
    X(i16,       "i16",     2,               _Alignof(i16))         \
    X(i32,       "i32",     4,               _Alignof(i32))         \
    X(i64,       "i64",     8,               _Alignof(i64))         \
    X(isize,     "isize",   sizeof(void*),   _Alignof(void*))       \
                                                                    \
    X(f32,       "f32",     4,               _Alignof(f32))         \
    X(f64,       "f64",     8,               _Alignof(f64))         \
                                                                    \
    X(va_list,   "va_list", sizeof(va_list), _Alignof(va_list))

void types_register_builtins(void);

#endif // !LILY_TYPES_BUILTINS_H
