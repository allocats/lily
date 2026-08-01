// TODO: ADD MISSING OPERATORS

#include "lexer/lexer.h"
#include "files/types.h"
#include "lexer/types.h"

#include "diagnostics/diagnostics.h"
#include "driver/types.h"
#include "string_interner/interner.h"
#include "token/token.h"
#include "token/types.h"

#include <assert.h>
#include <string.h>

#define IS_DIGIT(c)         (CHAR_MAP[(unsigned char)(c)] & 1)
#define IS_ALPHA(c)         (CHAR_MAP[(unsigned char)(c)] & 2)
#define IS_OPERATOR(c)      (CHAR_MAP[(unsigned char)(c)] & 4)
#define IS_DELIMITER(c)     (CHAR_MAP[(unsigned char)(c)] & 8)
#define IS_WHITESPACE(c)    (CHAR_MAP[(unsigned char)(c)] & 16)
#define IS_CHAR_DELIM(c)    (CHAR_MAP[(unsigned char)(c)] & 32)
#define IS_STRING_DELIM(c)  (CHAR_MAP[(unsigned char)(c)] & 64)
#define IS_ALPHA_NUMERIC(c) (CHAR_MAP[(unsigned char)(c)] & 3)

#define matches(str, start, length) \
    ((length) == sizeof(str) - 1 && memcmp((char*)(str), (char*)(start), (length)) == 0)

extern LilyCtx driver_ctx;

static DelimiterStack delimiter_stack = {
    .top = -1,
    .items = {0}
};

void delimiter_match(FileId id, Token* token);
void delimiter_stack_push(Token* token);
Token* delimiter_stack_pop(void); 

char* lex_whitespace(FileId id, char* cursor);
char* lex_word(FileId id, char* cursor);
char* lex_number(FileId id, char* cursor);
char* lex_operator(FileId id, char* cursor);
char* lex_delimiter(FileId id, char* cursor);
char* lex_char_lit(FileId id, char* cursor);
char* lex_string_lit(FileId id, char* cursor);
char* lex_invalid(FileId id, char* cursor);

typedef char* (*LexFn)(FileId, char*);

static const LexFn LEXER_DISPATCH[] = {
    ['0' ... '9'] = lex_number,

    ['a' ... 'z'] = lex_word,
    ['A' ... 'Z'] = lex_word,
    ['_']         = lex_word,
    
    ['@']         = lex_operator,
    ['#']         = lex_operator,
    ['&']         = lex_operator,
    ['|']         = lex_operator,
    ['~']         = lex_operator,
    ['^']         = lex_operator,
    ['-']         = lex_operator,
    ['+']         = lex_operator,
    ['/']         = lex_operator,
    ['*']         = lex_operator,
    ['%']         = lex_operator,
    ['=']         = lex_operator,
    ['!']         = lex_operator,
    ['<']         = lex_operator,
    ['>']         = lex_operator,
    ['.']         = lex_operator,

    [',']         = lex_delimiter,
    ['[']         = lex_delimiter,
    [']']         = lex_delimiter,
    ['(']         = lex_delimiter,
    [')']         = lex_delimiter,
    ['{']         = lex_delimiter,
    ['}']         = lex_delimiter,
    [';']         = lex_delimiter,
    [':']         = lex_delimiter,
    ['\0']        = lex_delimiter,

    [' ']         = lex_whitespace, 
    ['\t']        = lex_whitespace, 
    ['\n']        = lex_whitespace,
    ['\f']        = lex_whitespace,
    ['\r']        = lex_whitespace,

    ['\'']        = lex_char_lit,
    ['\"']        = lex_string_lit,
};

void lexer_tokenize_file(FileId id) {
    driver_ctx.file_registry.entries[id].stage = FILE_LEXING;

    str8 buffer = driver_ctx.file_registry.entries[id].buffer;

    char* buffer_start = buffer.pointer;
    char* buffer_end   = buffer_start + buffer.length;

    char* cursor = buffer_start;

    while (cursor < buffer_end) {
        LexFn fn = LEXER_DISPATCH[(unsigned char) *cursor];
        cursor   = fn ? fn(id, cursor) : lex_invalid(id, cursor);
    }

    driver_ctx.file_registry.entries[id].stage = FILE_LEXED;

    if (delimiter_stack.top != -1) {
        for (i32 i = 0; i < delimiter_stack.top + 1; i++) {
            Token* delim_tok = delimiter_stack.items[i];

            diagnostic_add_token(
                &driver_ctx.diagnostics,
                id,
                DIAG_ERROR,
                delim_tok,
                DIAG_LOC_WHOLE_TOK,
                "unclosed delimiter",
                "add closing delimiter"
            );
        }

        driver_ctx.file_registry.entries[id].stage = FILE_ERROR;
    }
}

char* lex_whitespace(FileId id, char* cursor) {
    str8 buffer = driver_ctx.file_registry.entries[id].buffer;

    char* end = buffer.pointer + buffer.length;

    while (cursor < end && IS_WHITESPACE(*cursor)) {
        cursor++;
    }

    return cursor;
}

char* lex_word(FileId id, char* cursor) {
    TokenArray* tokens = &driver_ctx.file_registry.tokens[id];

    Token* token = tokens_get_new_tok(tokens);

    char* start = cursor;

    while (IS_ALPHA_NUMERIC(*cursor)) {
        cursor++;
    }

    u32 length = cursor - start;

    token -> lexeme.pointer = start;
    token -> lexeme.length = length;

    switch (length) {
        case 2: {
            if (start[0] == 'f' && start[1] == 'n') { token->kind = TOK_FN; break; }
            if (start[0] == 'i' && start[1] == 'f') { token->kind = TOK_IF; break; }
            token->kind = TOK_IDENT;
        } break;

        case 3: {
            switch (start[0]) {
                case 'f': token->kind = (start[1]=='o' && start[2]=='r') ? TOK_FOR : TOK_IDENT; break;
                case 'l': token->kind = (start[1]=='e' && start[2]=='t') ? TOK_LET : TOK_IDENT; break;
                default:  token->kind = TOK_IDENT;
            }
        } break;

        case 4: {
            switch (start[0]) {
                case 'e':
                    if (start[1] == 'l' && start[2] == 's' && start[3] == 'e') { token->kind = TOK_ELSE; break; }
                    if (start[1] == 'n' && start[2] == 'u' && start[3] == 'm') { token->kind = TOK_ENUM; break; }
                    token->kind = TOK_IDENT;
                    break;
                case 'n': 
                    if (start[1] == 'u' && start[2] == 'l' && start[3] == 'l') { token->kind = TOK_NULL; break; }
                    token->kind = TOK_IDENT;
                    break;
                case 't':
                    if (start[1] == 'r' && start[2] == 'u' && start[3] == 'e') { token->kind = TOK_TRUE; break; }
                    token->kind = TOK_IDENT;
                    break;
                default:  token->kind = TOK_IDENT;
            }
        } break;

        case 5: {
            switch (start[0]) {
                case 'b': token->kind = (memcmp(start, "break", 5) == 0) ? TOK_BREAK : TOK_IDENT; break;
                case 'c': token->kind = (memcmp(start, "const", 5) == 0) ? TOK_CONST : TOK_IDENT; break;
                case 'd': token->kind = (memcmp(start, "defer", 5) == 0) ? TOK_DEFER : TOK_IDENT; break;
                case 'f': token->kind = (memcmp(start, "false", 5) == 0) ? TOK_FALSE : TOK_IDENT; break;
                case 'm': token->kind = (memcmp(start, "macro", 5) == 0) ? TOK_MACRO : TOK_IDENT; break;
                case 'u': token->kind = (memcmp(start, "union", 5) == 0) ? TOK_UNION : TOK_IDENT; break;
                case 'w': token->kind = (memcmp(start, "while", 5) == 0) ? TOK_WHILE : TOK_IDENT; break;
                default:  token->kind = TOK_IDENT;
            }
        } break;

        case 6: {
            switch (start[0]) {
                case 'i': token->kind = (memcmp(start, "import", 6) == 0) ? TOK_IMPORT : TOK_IDENT; break;
                case 'm': token->kind = (memcmp(start, "module", 6) == 0) ? TOK_MODULE : TOK_IDENT; break;
                case 'r': token->kind = (memcmp(start, "return", 6) == 0) ? TOK_RETURN : TOK_IDENT; break;
                case 's': token->kind = (memcmp(start, "struct", 6) == 0) ? TOK_STRUCT : TOK_IDENT; break;
                default:  token->kind = TOK_IDENT;
            }
        } break;

        case 8: {
            switch (start[0]) {
                case 'c': token->kind = (memcmp(start, "continue", 8) == 0) ? TOK_CONTINUE : TOK_IDENT; break;
                case 'e': token->kind = (memcmp(start, "external", 8) == 0) ? TOK_EXTERNAL : TOK_IDENT; break;
                default:  token->kind = TOK_IDENT;
            }
        } break;

        default: {
            token->kind = TOK_IDENT;
        } break;
    }

    if (token -> kind == TOK_IDENT) {
        string_intern_str8(token -> lexeme);
    }

    return cursor;
}

char* lex_number(FileId id, char* cursor) {
    TokenArray* tokens = &driver_ctx.file_registry.tokens[id];

    Token* token = tokens_get_new_tok(tokens);

    bool is_floating_point = false;

    char* start = cursor;

    while (IS_DIGIT(*cursor)) {
        cursor++;
    }

    if (*cursor == '.') {
        is_floating_point = true;

        cursor++;

        while (IS_DIGIT(*cursor)) {
            cursor++;
        }
    }

    token -> kind   = is_floating_point ? TOK_FLOAT_LIT : TOK_INTEGER_LIT;
    token -> lexeme.pointer = start;
    token -> lexeme.length = cursor - start;

    return cursor;
}

char* lex_operator(FileId id, char* cursor) {
    TokenArray* tokens = &driver_ctx.file_registry.tokens[id];

    Token* token = tokens_get_new_tok(tokens);

    char* start = cursor++;

    switch (*start) {
        case '#': {
            token -> kind = TOK_HASHTAG;
        } break;

        case '=': {
            if (*cursor == '=') {
                token -> kind = TOK_EQ_EQ;
                cursor++;
                break;
            }

            token -> kind = TOK_EQ;
        } break;

        case '!': {
            if (*cursor == '=') {
                token -> kind = TOK_BANG_EQ;
                cursor++;
                break;
            }

            token -> kind = TOK_BANG;
        } break;

        case '+': {
            if (*cursor == '=') {
                token -> kind = TOK_PLUS_EQ;
                cursor++;
                break;
            }

            token -> kind = TOK_PLUS;
        } break;

        case '-': {
            if (*cursor == '=') {
                token -> kind = TOK_MINUS_EQ;
                cursor++;
                break;
            }

            if (*cursor == '>') {
                token -> kind = TOK_ARROW;
                cursor++;
                break;
            }

            token -> kind = TOK_MINUS;
        } break;

        case '*': {
            if (*cursor == '=') {
                token -> kind = TOK_STAR_EQ;
                cursor++;
                break;
            }

            token -> kind = TOK_STAR;
        } break;

        case '/': {
            if (*cursor == '=') {
                token -> kind = TOK_SLASH_EQ;
                cursor++;
                break;
            }

            if (*cursor == '/') {
                tokens -> count--;

                while (*cursor != 0 && *cursor != '\n') {
                    cursor++;
                }

                if (*cursor != 0) {
                    cursor++;
                }

                break;
            }

            token -> kind = TOK_SLASH;
        } break;

        case '%': {
            if (*cursor == '=') {
                token -> kind = TOK_PERCENT_EQ;
                cursor++;
                break;
            }

            token -> kind = TOK_PERCENT;
        } break;

        case '~': {
            if (*cursor == '=') {
                token -> kind = TOK_TILDE_EQ;
                cursor++;
                break;
            }

            token -> kind = TOK_TILDE;
        } break;

        case '^': {
            if (*cursor == '=') {
                token -> kind = TOK_CARET_EQ;
                cursor++;
                break;
            }

            token -> kind = TOK_CARET;
        } break;

        case '&': {
            if (*cursor == '=') {
                token -> kind = TOK_AMP_EQ;
                cursor++;
                break;
            }

            if (*cursor == '&') {
                token -> kind = TOK_AMP_AMP;
                cursor++;
                break;
            }

            token -> kind = TOK_AMP;
        } break;

        case '|': {
            if (*cursor == '=') {
                token -> kind = TOK_PIPE_EQ;
                cursor++;
                break;
            }

            if (*cursor == '|') {
                token -> kind = TOK_PIPE_PIPE;
                cursor++;
                break;
            }

            token -> kind = TOK_PIPE;
        } break;

        case '>': {
            if (*cursor == '=') {
                token -> kind = TOK_GT_EQ;
                cursor++;
                break;
            }

            if (*cursor == '>') {
                token -> kind = TOK_SHR;
                cursor++;

                if (*cursor == '=') {
                    token -> kind = TOK_SHR_EQ;
                    cursor++;
                }

                break;
            }

            token -> kind = TOK_GT;
        } break;

        case '<': {
            if (*cursor == '=') {
                token -> kind = TOK_LT_EQ;
                cursor++;
                break;
            }

            if (*cursor == '>') {
                token -> kind = TOK_SHL;
                cursor++;

                if (*cursor == '=') {
                    token -> kind = TOK_SHL_EQ;
                    cursor++;
                }

                break;
            }

            token -> kind = TOK_LT;
        } break;

        case '.': {
            if (*cursor == '.') {
                token -> kind = TOK_DOT_DOT;
                cursor++;

                if (*cursor == '.') {
                    token -> kind = TOK_ELLIPSIS;
                    cursor++;
                }

                break;
            }

            token -> kind = TOK_DOT;
        } break;
    }

    token -> lexeme.pointer = start;
    token -> lexeme.length = cursor - start;

    return cursor;
}

char* lex_delimiter(FileId id, char* cursor) {
    TokenArray* tokens = &driver_ctx.file_registry.tokens[id];

    Token* token = tokens_get_new_tok(tokens);

    char* start = cursor++;
    
    token -> lexeme.pointer = start;
    token -> lexeme.length = cursor - start;

    switch (*start) {
        case ',': {
            token -> kind = TOK_COMMA;
        } break;

        case ';': {
            token -> kind = TOK_SEMI;
        } break;

        case ':': {
            token -> kind = TOK_COLON;

            if (*cursor == ':') {
                token -> kind = TOK_COLON_COLON;
                token -> lexeme.length += 1;

                cursor++;
            }
        } break;

        case '(': {
            token -> kind = TOK_LPAREN;

            delimiter_stack_push(token);
        } break;

        case ')': {
            token -> kind = TOK_RPAREN;

            delimiter_match(id, token);
        } break;

        case '[': {
            token -> kind = TOK_LBRACKET;

            delimiter_stack_push(token);
        } break;

        case ']': {
            token -> kind = TOK_RBRACKET;

            delimiter_match(id, token);
        } break;

        case '{': {
            token -> kind = TOK_LBRACE;

            delimiter_stack_push(token);
        } break;

        case '}': {
            token -> kind = TOK_RBRACE;

            delimiter_match(id, token);
        } break;

        case '\0': {
            token -> kind = TOK_EOF;
        } break;
    }

    return cursor;
}

char* lex_char_lit(FileId id, char* cursor) {
    TokenArray* tokens = &driver_ctx.file_registry.tokens[id];

    Token* token = tokens_get_new_tok(tokens);

    token -> kind = TOK_CHAR_LIT;
    token -> lexeme.pointer = cursor; 

    char* start = cursor++;

    // empty char literal
    if (*cursor == '\'') {
        token -> kind = TOK_ERROR;
        token -> lexeme.length = cursor - start;

        driver_ctx.file_registry.entries[id].stage = FILE_ERROR;

        diagnostic_add_token(
            &driver_ctx.diagnostics,
            id,
            DIAG_ERROR,
            token,
            DIAG_LOC_WHOLE_TOK,
            "empty char literal",
            "add a char to this literal"
        );

        return cursor + 1;
    }

    if (*cursor == '\\') {
        cursor++;
    }

    cursor++;

    // unterminated char literal
    if (*cursor != '\'') {
        token -> kind = TOK_ERROR;
        token -> lexeme.length = cursor - start;

        driver_ctx.file_registry.entries[id].stage = FILE_ERROR;

        diagnostic_add_token(
            &driver_ctx.diagnostics,
            id,
            DIAG_ERROR,
            token,
            DIAG_LOC_WHOLE_TOK,
            "unterminated char literal",
            "add the closing delimiter to this char literal"
        );

        return cursor + 1;
    }

    token -> lexeme.pointer = start;
    token -> lexeme.length = ++cursor - start; 

    return cursor;
}

char* lex_string_lit(FileId id, char* cursor) {
    TokenArray* tokens = &driver_ctx.file_registry.tokens[id];

    Token* token = tokens_get_new_tok(tokens);

    char* start = cursor;

    token -> kind = TOK_STRING_LIT;
    token -> lexeme.pointer = start; 

    cursor++;

    while (*cursor != '\"' && *cursor != 0) {
        cursor++;

        if (*cursor == '\\') {
            cursor += 2;
        }
    }

    // unterminated string literal
    if (*cursor != '\"') {
        token -> kind = TOK_ERROR;
        token -> lexeme.length = cursor - start;

        driver_ctx.file_registry.entries[id].stage = FILE_ERROR;

        diagnostic_add_token(
            &driver_ctx.diagnostics,
            id,
            DIAG_ERROR,
            token,
            DIAG_LOC_WHOLE_TOK,
            "unterminated string literal",
            "add the closing delimiter to this string literal"
        );
    }

    cursor++;

    token -> lexeme.length = cursor - start;

    string_intern_str8(token -> lexeme);

    return cursor;
}

char* lex_invalid(FileId id, char* cursor) {
    driver_ctx.file_registry.entries[id].stage = FILE_ERROR;

    TokenArray* tokens = &driver_ctx.file_registry.tokens[id];

    Token* token = tokens_get_new_tok(tokens);

    char* start = cursor;

    while (
        !IS_ALPHA(*cursor)            &&
        !IS_DIGIT(*cursor)            &&
        !IS_OPERATOR(*cursor)         &&
        !IS_CHAR_DELIM(*cursor)       &&
        !IS_STRING_DELIM(*cursor)     &&
        !IS_DELIMITER(*cursor)        &&
        !IS_WHITESPACE(*cursor)
    ) {
        cursor++;
    }

    token -> kind = TOK_ERROR;
    token -> lexeme.pointer = start;
    token -> lexeme.length = cursor - start;

    diagnostic_add_token(
        &driver_ctx.diagnostics,
        id,
        DIAG_ERROR,
        token,
        DIAG_LOC_WHOLE_TOK,
        "unknown token",
        null 
    );

    return cursor;
}

void delimiter_match(FileId id, Token* token) {
    Token* popped = delimiter_stack_pop();

    if (popped == null) {
        diagnostic_add_token(
            &driver_ctx.diagnostics,
            id,
            DIAG_ERROR,
            token,
            DIAG_LOC_WHOLE_TOK,
            "unopened delimiter",
            null
        );

        driver_ctx.file_registry.entries[id].stage = FILE_ERROR;

        return;
    }

    switch (token -> kind) {
        case TOK_RPAREN: {
            if (popped -> kind != TOK_LPAREN) {
                diagnostic_add_token(
                    &driver_ctx.diagnostics,
                    id,
                    DIAG_ERROR,
                    popped,
                    DIAG_LOC_WHOLE_TOK,
                    "mismatched delimiters",
                    null
                );

                driver_ctx.file_registry.entries[id].stage = FILE_ERROR;
            }
        } break;

        case TOK_RBRACE: {
            if (popped -> kind != TOK_LBRACE) {
                diagnostic_add_token(
                    &driver_ctx.diagnostics,
                    id,
                    DIAG_ERROR,
                    popped,
                    DIAG_LOC_WHOLE_TOK,
                    "mismatched delimiters",
                    null
                );

                driver_ctx.file_registry.entries[id].stage = FILE_ERROR;
            }
        } break;

        case TOK_RBRACKET: {
            if (popped -> kind != TOK_LBRACKET) {
                diagnostic_add_token(
                    &driver_ctx.diagnostics,
                    id,
                    DIAG_ERROR,
                    popped,
                    DIAG_LOC_WHOLE_TOK,
                    "mismatched delimiters",
                    null
                );

                driver_ctx.file_registry.entries[id].stage = FILE_ERROR;
            }
        } break;

        default: {
            assert(0 > 1 && "HOW DID WE GET HERE");
        } break;
    }
}

void delimiter_stack_push(Token* token) {
    assert(delimiter_stack.top + 1 < DELIMITER_STACK_MAX_DEPTH && "Max delimiter stack depth");
    delimiter_stack.items[++delimiter_stack.top] = token;
}

Token* delimiter_stack_pop(void) {
    if (delimiter_stack.top == -1) return null;
    return delimiter_stack.items[delimiter_stack.top--];
}
