#include "script_tokenizer.h"

#include <stdlib.h>
#include <string.h>
#include <ctype.h>

typedef struct ScriptTokenizer {
    const char *start;
    const char *current;
    unsigned start_line;
    unsigned start_column;
    unsigned line;
    unsigned column;
    ScriptTokenizerResult result;
} ScriptTokenizer;

static int is_alpha(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

static int is_digit(char c) {
    return c >= '0' && c <= '9';
}

static int is_alnum(char c) {
    return is_alpha(c) || is_digit(c);
}

static int at_end(const ScriptTokenizer *tokenizer) {
    return *tokenizer->current == '\0';
}

static char advance(ScriptTokenizer *tokenizer) {
    char c = *tokenizer->current++;
    if (c == '\n') {
        tokenizer->line++;
        tokenizer->column = 1;
    } else {
        tokenizer->column++;
    }
    return c;
}

static char peek(const ScriptTokenizer *tokenizer) {
    return *tokenizer->current;
}

static char peek_next(const ScriptTokenizer *tokenizer) {
    if (at_end(tokenizer)) {
        return '\0';
    }
    return tokenizer->current[1];
}

static int match(ScriptTokenizer *tokenizer, char expected) {
    if (at_end(tokenizer) || *tokenizer->current != expected) {
        return 0;
    }
    advance(tokenizer);
    return 1;
}

static int reserve_tokens(ScriptTokenList *list, size_t needed) {
    size_t capacity;
    ScriptToken *items;

    if (list->capacity >= needed) {
        return 1;
    }
    capacity = list->capacity == 0 ? 32 : list->capacity * 2;
    while (capacity < needed) {
        capacity *= 2;
    }
    items = (ScriptToken *)realloc(list->items, capacity * sizeof(*items));
    if (items == NULL) {
        return 0;
    }
    list->items = items;
    list->capacity = capacity;
    return 1;
}

static int reserve_diagnostics(ScriptDiagnosticList *list, size_t needed) {
    size_t capacity;
    ScriptDiagnostic *items;

    if (list->capacity >= needed) {
        return 1;
    }
    capacity = list->capacity == 0 ? 8 : list->capacity * 2;
    while (capacity < needed) {
        capacity *= 2;
    }
    items = (ScriptDiagnostic *)realloc(list->items, capacity * sizeof(*items));
    if (items == NULL) {
        return 0;
    }
    list->items = items;
    list->capacity = capacity;
    return 1;
}

static void add_token(ScriptTokenizer *tokenizer, ScriptTokenType type) {
    ScriptToken *token;

    if (!reserve_tokens(&tokenizer->result.tokens, tokenizer->result.tokens.count + 1)) {
        return;
    }
    token = &tokenizer->result.tokens.items[tokenizer->result.tokens.count++];
    token->type = type;
    token->lexeme = tokenizer->start;
    token->length = (size_t)(tokenizer->current - tokenizer->start);
    token->line = tokenizer->start_line;
    token->column = tokenizer->start_column;
}

static void add_sized_token(ScriptTokenizer *tokenizer, ScriptTokenType type, const char *lexeme, size_t length) {
    ScriptToken *token;

    if (!reserve_tokens(&tokenizer->result.tokens, tokenizer->result.tokens.count + 1)) {
        return;
    }
    token = &tokenizer->result.tokens.items[tokenizer->result.tokens.count++];
    token->type = type;
    token->lexeme = lexeme;
    token->length = length;
    token->line = tokenizer->start_line;
    token->column = tokenizer->start_column;
}

static void add_diagnostic(ScriptTokenizer *tokenizer, const char *message) {
    ScriptDiagnostic *diagnostic;

    if (!reserve_diagnostics(&tokenizer->result.diagnostics, tokenizer->result.diagnostics.count + 1)) {
        return;
    }
    diagnostic = &tokenizer->result.diagnostics.items[tokenizer->result.diagnostics.count++];
    diagnostic->severity = SCRIPT_DIAGNOSTIC_ERROR;
    diagnostic->message = message;
    diagnostic->line = tokenizer->start_line;
    diagnostic->column = tokenizer->start_column;
}

static ScriptTokenType identifier_type(const char *text, size_t length) {
#define KEYWORD(name, token) if (length == sizeof(name) - 1 && memcmp(text, name, sizeof(name) - 1) == 0) return token
    KEYWORD("room", SCRIPT_TOKEN_KEYWORD_ROOM);
    KEYWORD("object", SCRIPT_TOKEN_KEYWORD_OBJECT);
    KEYWORD("hotspot", SCRIPT_TOKEN_KEYWORD_HOTSPOT);
    KEYWORD("npc", SCRIPT_TOKEN_KEYWORD_NPC);
    KEYWORD("var", SCRIPT_TOKEN_KEYWORD_VAR);
    KEYWORD("on", SCRIPT_TOKEN_KEYWORD_ON);
    KEYWORD("if", SCRIPT_TOKEN_KEYWORD_IF);
    KEYWORD("else", SCRIPT_TOKEN_KEYWORD_ELSE);
    KEYWORD("true", SCRIPT_TOKEN_KEYWORD_TRUE);
    KEYWORD("false", SCRIPT_TOKEN_KEYWORD_FALSE);
    KEYWORD("event", SCRIPT_TOKEN_KEYWORD_EVENT);
#undef KEYWORD
    return SCRIPT_TOKEN_IDENTIFIER;
}

static void scan_identifier(ScriptTokenizer *tokenizer) {
    while (is_alnum(peek(tokenizer))) {
        advance(tokenizer);
    }
    add_token(tokenizer, identifier_type(tokenizer->start, (size_t)(tokenizer->current - tokenizer->start)));
}

static void scan_number(ScriptTokenizer *tokenizer) {
    while (is_digit(peek(tokenizer))) {
        advance(tokenizer);
    }
    add_token(tokenizer, SCRIPT_TOKEN_INTEGER);
}

static void scan_string(ScriptTokenizer *tokenizer) {
    const char *content = tokenizer->current;
    size_t length;

    while (peek(tokenizer) != '"' && !at_end(tokenizer)) {
        advance(tokenizer);
    }

    if (at_end(tokenizer)) {
        add_diagnostic(tokenizer, "unterminated string");
        return;
    }

    length = (size_t)(tokenizer->current - content);
    advance(tokenizer);
    add_sized_token(tokenizer, SCRIPT_TOKEN_STRING, content, length);
}

static void skip_comment(ScriptTokenizer *tokenizer) {
    while (peek(tokenizer) == ' ' || peek(tokenizer) == '\t') {
        advance(tokenizer);
    }
    if (peek(tokenizer) == '@') {
        const char *label;
        size_t length;
        advance(tokenizer);
        label = tokenizer->current;
        while (peek(tokenizer) != '\n' && !at_end(tokenizer) &&
               !isspace((unsigned char)peek(tokenizer))) {
            advance(tokenizer);
        }
        length = (size_t)(tokenizer->current - label);
        if (length > 0) {
            add_sized_token(tokenizer, SCRIPT_TOKEN_STRING_ID, label, length);
        }
    }
    while (peek(tokenizer) != '\n' && !at_end(tokenizer)) {
        advance(tokenizer);
    }
}

static void scan_token(ScriptTokenizer *tokenizer) {
    char c = advance(tokenizer);
    switch (c) {
    case ' ':
    case '\r':
    case '\t':
    case '\n':
        break;
    case '#':
        skip_comment(tokenizer);
        break;
    case '{':
        add_token(tokenizer, SCRIPT_TOKEN_LEFT_BRACE);
        break;
    case '}':
        add_token(tokenizer, SCRIPT_TOKEN_RIGHT_BRACE);
        break;
    case '(':
        add_token(tokenizer, SCRIPT_TOKEN_LEFT_PAREN);
        break;
    case ')':
        add_token(tokenizer, SCRIPT_TOKEN_RIGHT_PAREN);
        break;
    case '[':
        add_token(tokenizer, SCRIPT_TOKEN_LEFT_BRACKET);
        break;
    case ']':
        add_token(tokenizer, SCRIPT_TOKEN_RIGHT_BRACKET);
        break;
    case ',':
        add_token(tokenizer, SCRIPT_TOKEN_COMMA);
        break;
    case ':':
        add_token(tokenizer, SCRIPT_TOKEN_COLON);
        break;
    case '=':
        add_token(tokenizer, match(tokenizer, '=') ? SCRIPT_TOKEN_EQUAL_EQUAL : SCRIPT_TOKEN_EQUAL);
        break;
    case '!':
        if (match(tokenizer, '=')) add_token(tokenizer, SCRIPT_TOKEN_BANG_EQUAL);
        else add_diagnostic(tokenizer, "unexpected character");
        break;
    case '<':
        add_token(tokenizer, match(tokenizer, '=') ? SCRIPT_TOKEN_LESS_EQUAL : SCRIPT_TOKEN_LESS);
        break;
    case '>':
        add_token(tokenizer, match(tokenizer, '=') ? SCRIPT_TOKEN_GREATER_EQUAL : SCRIPT_TOKEN_GREATER);
        break;
    case '"':
        scan_string(tokenizer);
        break;
    default:
        if (is_alpha(c)) {
            scan_identifier(tokenizer);
        } else if (is_digit(c)) {
            scan_number(tokenizer);
        } else {
            (void)peek_next(tokenizer);
            add_diagnostic(tokenizer, "unexpected character");
        }
        break;
    }
}

ScriptTokenizerResult script_tokenize(const char *source) {
    ScriptTokenizer tokenizer;
    memset(&tokenizer, 0, sizeof(tokenizer));
    tokenizer.start = source;
    tokenizer.current = source;
    tokenizer.start_line = 1;
    tokenizer.start_column = 1;
    tokenizer.line = 1;
    tokenizer.column = 1;

    while (!at_end(&tokenizer)) {
        tokenizer.start = tokenizer.current;
        tokenizer.start_line = tokenizer.line;
        tokenizer.start_column = tokenizer.column;
        scan_token(&tokenizer);
    }

    tokenizer.start = tokenizer.current;
    tokenizer.start_line = tokenizer.line;
    tokenizer.start_column = tokenizer.column;
    add_token(&tokenizer, SCRIPT_TOKEN_EOF);
    return tokenizer.result;
}

void script_tokenizer_result_free(ScriptTokenizerResult *result) {
    if (result == NULL) {
        return;
    }
    free(result->tokens.items);
    free(result->diagnostics.items);
    memset(result, 0, sizeof(*result));
}

const char *script_token_type_name(ScriptTokenType type) {
    switch (type) {
    case SCRIPT_TOKEN_IDENTIFIER: return "identifier";
    case SCRIPT_TOKEN_STRING: return "string";
    case SCRIPT_TOKEN_INTEGER: return "integer";
    case SCRIPT_TOKEN_LEFT_BRACE: return "{";
    case SCRIPT_TOKEN_RIGHT_BRACE: return "}";
    case SCRIPT_TOKEN_LEFT_PAREN: return "(";
    case SCRIPT_TOKEN_RIGHT_PAREN: return ")";
    case SCRIPT_TOKEN_LEFT_BRACKET: return "[";
    case SCRIPT_TOKEN_RIGHT_BRACKET: return "]";
    case SCRIPT_TOKEN_COMMA: return ",";
    case SCRIPT_TOKEN_COLON: return ":";
    case SCRIPT_TOKEN_EQUAL: return "=";
    case SCRIPT_TOKEN_EQUAL_EQUAL: return "==";
    case SCRIPT_TOKEN_BANG_EQUAL: return "!=";
    case SCRIPT_TOKEN_LESS: return "<";
    case SCRIPT_TOKEN_LESS_EQUAL: return "<=";
    case SCRIPT_TOKEN_GREATER: return ">";
    case SCRIPT_TOKEN_GREATER_EQUAL: return ">=";
    case SCRIPT_TOKEN_KEYWORD_ROOM: return "room";
    case SCRIPT_TOKEN_KEYWORD_OBJECT: return "object";
    case SCRIPT_TOKEN_KEYWORD_HOTSPOT: return "hotspot";
    case SCRIPT_TOKEN_KEYWORD_NPC: return "npc";
    case SCRIPT_TOKEN_KEYWORD_VAR: return "var";
    case SCRIPT_TOKEN_KEYWORD_ON: return "on";
    case SCRIPT_TOKEN_KEYWORD_IF: return "if";
    case SCRIPT_TOKEN_KEYWORD_ELSE: return "else";
    case SCRIPT_TOKEN_KEYWORD_TRUE: return "true";
    case SCRIPT_TOKEN_KEYWORD_FALSE: return "false";
    case SCRIPT_TOKEN_KEYWORD_EVENT: return "event";
    case SCRIPT_TOKEN_STRING_ID: return "string-id";
    case SCRIPT_TOKEN_EOF: return "EOF";
    }
    return "unknown";
}
