#include "diagnostics/diagnostics.h"
#include "files/files.h"
#include "lexer/lexer.h"
#include "files/types.h"
#include "lexer/types.h"
#include "token/token.h"
#include "token/types.h"

#include <assert.h>
#include <string.h>
#include <strings.h>

#define IS_DIGIT(c)         (CHAR_MAP[(unsigned char)(c)] & 1)
#define IS_ALPHA(c)         (CHAR_MAP[(unsigned char)(c)] & 2)
#define IS_OPERATOR(c)      (CHAR_MAP[(unsigned char)(c)] & 4)
#define IS_DELIMITER(c)     (CHAR_MAP[(unsigned char)(c)] & 8)
#define IS_WHITESPACE(c)    (CHAR_MAP[(unsigned char)(c)] & 16)
#define IS_CHAR_DELIM(c)    (CHAR_MAP[(unsigned char)(c)] & 32)
#define IS_STRING_DELIM(c)  (CHAR_MAP[(unsigned char)(c)] & 64)
#define IS_ALPHA_NUMERIC(c) (CHAR_MAP[(unsigned char)(c)] & 3)

static void delimiter_match(File* file, Token* token);
static void delimiter_stack_push(u32 index);

static const char* lex_whitespace(File* file, const char* cursor);
static const char* lex_word(File* file, const char* cursor);
static const char* lex_number(File* file, const char* cursor);
static const char* lex_operator(File* file, const char* cursor);
static const char* lex_delimiter(File* file, const char* cursor);
static const char* lex_char_lit(File* file, const char* cursor);
static const char* lex_string_lit(File* file, const char* cursor);
static const char* lex_invalid(File* file, const char* cursor);

typedef const char* (*LexFn)(File*, const char*);

static const LexFn LEXER_DISPATCH[] = {
    ['0' ... '9'] = lex_number,

    ['a' ... 'z'] = lex_word,
    ['A' ... 'Z'] = lex_word,
    ['_']         = lex_word,
    
    ['@']         = lex_operator,
    ['#']         = lex_operator,
    ['$']         = lex_operator,
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

static DelimiterStack delimiter_stack = {
    .top = 0,
    .items = {0}
};

void lex_file(FileId id) {
    File* file = file_lookup_id(id);

    assert(file -> stage == FILE_ALLOCATED);

    str8 buffer = file -> buffer;

    assert(buffer.len != 0);

    const char* buffer_start = buffer.ptr;
    const char* buffer_end   = buffer_start + buffer.len;

    const char* cursor = buffer_start;

    delimiter_stack = (DelimiterStack) {
        .top = 0,
        .items = {0}
    };

    while (cursor < buffer_end) {
        LexFn fn = LEXER_DISPATCH[(unsigned char) *cursor];
        cursor   = fn ? fn(file, cursor) : lex_invalid(file, cursor);
    }

    if (delimiter_stack.top != 0) {
        u32 index = delimiter_stack.items[delimiter_stack.top - 1];

        Token* delim_tok = &file -> tokens.items[index];

        diagnostic_add_token(
            id,
            DIAG_ERROR,
            delim_tok,
            DIAG_LOC_WHOLE_TOK,
            "unclosed delimiter",
            "add closing delimiter"
        );

        file -> stage = FILE_ERROR;
    } else {
        file -> stage = FILE_LEXED;
    }
}

static const char* lex_whitespace(File* file, const char* cursor) {
    const char* end = file -> buffer.ptr + file -> buffer.len;

    while (cursor < end && IS_WHITESPACE(*cursor)) {
        cursor++;
    }

    return cursor;
}

static const char* lex_word(File* file, const char* cursor) {
    Token* token = tokens_get_new_token(&file -> tokens);

    const char* start = cursor;

    while (IS_ALPHA_NUMERIC(*cursor)) {
        cursor++;
    }

    u32 length = cursor - start;

    token -> start  = start - file -> buffer.ptr;
    token -> length = length;

    switch (length) {
        case 2: {
            if (start[0] == 'f' && start[1] == 'n') { token -> kind = TOK_KW_FN; break; }
            if (start[0] == 'i' && start[1] == 'f') { token -> kind = TOK_KW_IF; break; }
            token -> kind = TOK_IDENT;
        } break;

        case 3: {
            switch (start[0]) {
                case 'f': 
                    token -> kind = (start[1]=='o' && start[2]=='r') ? TOK_KW_FOR : TOK_IDENT; 
                    break;

                default:  
                    token -> kind = TOK_IDENT; 
            }
        } break;

        case 4: {
            switch (start[0]) {
                case 'c': 
                    if (start[1] == 'a' && start[2] == 'a' && start[3] == 's') { token -> kind = TOK_KW_CASE; break; }
                    token -> kind = TOK_IDENT;
                    break;

                case 'e':
                    if (start[1] == 'l' && start[2] == 's' && start[3] == 'e') { token -> kind = TOK_KW_ELSE; break; }
                    if (start[1] == 'n' && start[2] == 'u' && start[3] == 'm') { token -> kind = TOK_KW_ENUM; break; }
                    token -> kind = TOK_IDENT;
                    break;

                case 'n': 
                    if (start[1] == 'u' && start[2] == 'l' && start[3] == 'l') { token -> kind = TOK_KW_NULL; break; }
                    token -> kind = TOK_IDENT;
                    break;

                case 't':
                    if (start[1] == 'r' && start[2] == 'u' && start[3] == 'e') { token -> kind = TOK_KW_TRUE; break; }
                    token -> kind = TOK_IDENT;
                    break;

                default:  
                    token -> kind = TOK_IDENT;
            }
        } break;

        case 5: {
            switch (start[0]) {
                case 'b': token -> kind = (memcmp(start, "break", 5) == 0) ? TOK_KW_BREAK : TOK_IDENT; break;
                case 'c': token -> kind = (memcmp(start, "const", 5) == 0) ? TOK_KW_CONST : TOK_IDENT; break;
                case 'd': token -> kind = (memcmp(start, "defer", 5) == 0) ? TOK_KW_DEFER : TOK_IDENT; break;
                case 'f': token -> kind = (memcmp(start, "false", 5) == 0) ? TOK_KW_FALSE : TOK_IDENT; break;
                case 'm': token -> kind = (memcmp(start, "macro", 5) == 0) ? TOK_KW_MACRO : TOK_IDENT; break;
                case 'u': token -> kind = (memcmp(start, "union", 5) == 0) ? TOK_KW_UNION : TOK_IDENT; break;
                case 'w': token -> kind = (memcmp(start, "while", 5) == 0) ? TOK_KW_WHILE : TOK_IDENT; break;
                default:  token -> kind = TOK_IDENT;
            }
        } break;

        case 6: {
            switch (start[0]) {
                case 'i': token -> kind = (memcmp(start, "import", 6) == 0) ? TOK_KW_IMPORT : TOK_IDENT; break;
                case 'm': token -> kind = (memcmp(start, "module", 6) == 0) ? TOK_KW_MODULE : TOK_IDENT; break;
                case 'r': token -> kind = (memcmp(start, "return", 6) == 0) ? TOK_KW_RETURN : TOK_IDENT; break;
                case 's': 
                    token -> kind = (memcmp(start, "struct", 6) == 0) ? TOK_KW_STRUCT : TOK_IDENT; break;
                    token -> kind = (memcmp(start, "switch", 6) == 0) ? TOK_KW_SWITCH : TOK_IDENT; break;
                default:  token -> kind = TOK_IDENT;
            }
        } break;

        case 7: {
            switch (start[0]) {
                case 'd': token -> kind = (memcmp(start, "default", 7) == 0) ? TOK_KW_DEFAULT : TOK_IDENT; break;
                default:  token -> kind = TOK_IDENT;
            }
        } break;

        case 8: {
            switch (start[0]) {
                case 'c': token -> kind = (memcmp(start, "continue", 8) == 0) ? TOK_KW_CONTINUE : TOK_IDENT; break;
                case 'e': token -> kind = (memcmp(start, "external", 8) == 0) ? TOK_KW_EXTERNAL : TOK_IDENT; break;
                default:  token -> kind = TOK_IDENT;
            }
        } break;

        default: {
            token -> kind = TOK_IDENT;
        } break;
    }

    return cursor;
}

static const char* lex_number(File* file, const char* cursor) {
    Token* token = tokens_get_new_token(&file -> tokens);

    bool is_floating_point = false;

    const char* start = cursor;

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

    token -> kind = is_floating_point ? TOK_FLOAT_LIT : TOK_INTEGER_LIT;
    token -> start  = start - file -> buffer.ptr;
    token -> length = cursor - start;

    return cursor;
}

static const char* lex_operator(File* file, const char* cursor) {
    Token* token = tokens_get_new_token(&file -> tokens);

    const char* start = cursor++;

    switch (*start) {
        case '@': {
            token -> kind = TOK_AT;
        } break;

        case '#': {
            token -> kind = TOK_HASHTAG;
        } break;

        case '$': {
            token -> kind = TOK_DOLLAR;
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
                file -> tokens.count--;

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

            if (*cursor == '<') {
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

    token -> start  = start - file -> buffer.ptr;
    token -> length = cursor - start;

    return cursor;
}

static const char* lex_delimiter(File* file, const char* cursor) {
    Token* token = tokens_get_new_token(&file -> tokens);

    const char* start = cursor++;
    
    token -> start = start - file -> buffer.ptr;
    token -> length = cursor - start;

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
                token -> length += 1;

                cursor++;
            }
        } break;

        case '(': {
            token -> kind = TOK_L_PAREN;

            delimiter_stack_push(file -> tokens.count - 1);
        } break;

        case ')': {
            token -> kind = TOK_R_PAREN;

            delimiter_match(file, token);
        } break;

        case '[': {
            token -> kind = TOK_L_BRACKET;

            delimiter_stack_push(file -> tokens.count - 1);
        } break;

        case ']': {
            token -> kind = TOK_R_BRACKET;

            delimiter_match(file, token);
        } break;

        case '{': {
            token -> kind = TOK_L_BRACE;

            delimiter_stack_push(file -> tokens.count - 1);
        } break;

        case '}': {
            token -> kind = TOK_R_BRACE;

            delimiter_match(file, token);
        } break;

        case '\0': {
            token -> kind = TOK_EOF;
        } break;
    }

    return cursor;
}

static const char* lex_char_lit(File* file, const char* cursor) {
    Token* token = tokens_get_new_token(&file -> tokens);

    token -> kind = TOK_CHAR_LIT;
    token -> start = cursor - file -> buffer.ptr; 

    const char* start = cursor++;

    // empty char literal
    if (*cursor == '\'') {
        token -> kind = TOK_ERROR;
        token -> length = cursor - start;

        file -> stage = FILE_ERROR;

        diagnostic_add_token(
            file -> id,
            DIAG_ERROR,
            token,
            DIAG_LOC_WHOLE_TOK,
            "empty char literal",
            "add a char to this literal"
        );

        return cursor;
    }

    if (*cursor == '\\') {
        cursor++;
    }

    cursor++;

    // unterminated char literal
    if (*cursor != '\'') {
        token -> kind = TOK_ERROR;
        token -> length = cursor - start;

        file -> stage = FILE_ERROR;

        diagnostic_add_token(
            file -> id,
            DIAG_ERROR,
            token,
            DIAG_LOC_WHOLE_TOK,
            "unterminated char literal",
            "add the closing delimiter to this char literal"
        );

        return cursor + 1;
    }

    token -> start = start - file -> buffer.ptr;
    token -> length = ++cursor - start; 

    return cursor;
}

static const char* lex_string_lit(File* file, const char* cursor) {
    Token* token = tokens_get_new_token(&file -> tokens);

    const char* start = cursor;

    token -> kind = TOK_STRING_LIT;
    token -> start = start - file -> buffer.ptr; 

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
        token -> length = cursor - start;

        file -> stage = FILE_ERROR;

        diagnostic_add_token(
            file -> id,
            DIAG_ERROR,
            token,
            DIAG_LOC_WHOLE_TOK,
            "unterminated string literal",
            "add the closing delimiter to this string literal"
        );
    }

    cursor++;

    token -> length = cursor - start;

    return cursor;
}

static const char* lex_invalid(File* file, const char* cursor) {
    file -> stage = FILE_ERROR;

    Token* token = tokens_get_new_token(&file -> tokens);

    const char* start = cursor;

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
    token -> start = start - file -> buffer.ptr;
    token -> length = cursor - start;

    diagnostic_add_token(
        file -> id,
        DIAG_ERROR,
        token,
        DIAG_LOC_WHOLE_TOK,
        "unknown token",
        null 
    );

    return cursor;
}

static bool delimiter_matches(TokenKind open, TokenKind close) {
    switch (open) {
        case TOK_L_PAREN:   return close == TOK_R_PAREN;
        case TOK_L_BRACKET: return close == TOK_R_BRACKET;
        case TOK_L_BRACE:   return close == TOK_R_BRACE;
        default:            return false;
    }
}

static void delimiter_match(File* file, Token* token) {
    if (delimiter_stack.top == 0) {
        diagnostic_add_token(
            file -> id,
            DIAG_ERROR,
            token,
            DIAG_LOC_WHOLE_TOK,
            "unopened delimiter",
            null
        );

        file -> stage = FILE_ERROR;
        return;
    }

    Token* open = &file  ->  tokens.items[delimiter_stack.items[delimiter_stack.top - 1]];

    if (!delimiter_matches(open -> kind, token -> kind)) {
        diagnostic_add_token(
            file -> id,
            DIAG_ERROR,
            token,
            DIAG_LOC_WHOLE_TOK,
            "mismatched delimiters",
            null
        );

        file -> stage = FILE_ERROR;
        return;
    }

    delimiter_stack.top--;
}

static void delimiter_stack_push(u32 index) {
    assert(delimiter_stack.top < DELIMITER_STACK_MAX_DEPTH && "Max delimiter stack depth");
    delimiter_stack.items[delimiter_stack.top++] = index;
}
