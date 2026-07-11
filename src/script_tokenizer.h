#ifndef YAAT_SCRIPT_TOKENIZER_H
#define YAAT_SCRIPT_TOKENIZER_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum ScriptTokenType {
    SCRIPT_TOKEN_IDENTIFIER,
    SCRIPT_TOKEN_STRING,
    SCRIPT_TOKEN_INTEGER,

    SCRIPT_TOKEN_LEFT_BRACE,
    SCRIPT_TOKEN_RIGHT_BRACE,
    SCRIPT_TOKEN_LEFT_PAREN,
    SCRIPT_TOKEN_RIGHT_PAREN,
    SCRIPT_TOKEN_LEFT_BRACKET,
    SCRIPT_TOKEN_RIGHT_BRACKET,
    SCRIPT_TOKEN_COMMA,
    SCRIPT_TOKEN_COLON,
    SCRIPT_TOKEN_EQUAL,
    SCRIPT_TOKEN_EQUAL_EQUAL,
    SCRIPT_TOKEN_BANG_EQUAL,
    SCRIPT_TOKEN_LESS,
    SCRIPT_TOKEN_LESS_EQUAL,
    SCRIPT_TOKEN_GREATER,
    SCRIPT_TOKEN_GREATER_EQUAL,

    SCRIPT_TOKEN_KEYWORD_ROOM,
    SCRIPT_TOKEN_KEYWORD_OBJECT,
    SCRIPT_TOKEN_KEYWORD_HOTSPOT,
    SCRIPT_TOKEN_KEYWORD_NPC,
    SCRIPT_TOKEN_KEYWORD_VAR,
    SCRIPT_TOKEN_KEYWORD_ON,
    SCRIPT_TOKEN_KEYWORD_IF,
    SCRIPT_TOKEN_KEYWORD_ELSE,
    SCRIPT_TOKEN_KEYWORD_TRUE,
    SCRIPT_TOKEN_KEYWORD_FALSE,
    SCRIPT_TOKEN_KEYWORD_EVENT,
    SCRIPT_TOKEN_STRING_ID,

    SCRIPT_TOKEN_EOF
} ScriptTokenType;

typedef enum ScriptDiagnosticSeverity {
    SCRIPT_DIAGNOSTIC_ERROR,
    SCRIPT_DIAGNOSTIC_WARNING
} ScriptDiagnosticSeverity;

typedef struct ScriptToken {
    ScriptTokenType type;
    const char *lexeme;
    size_t length;
    unsigned line;
    unsigned column;
} ScriptToken;

typedef struct ScriptDiagnostic {
    ScriptDiagnosticSeverity severity;
    const char *message;
    unsigned line;
    unsigned column;
} ScriptDiagnostic;

typedef struct ScriptTokenList {
    ScriptToken *items;
    size_t count;
    size_t capacity;
} ScriptTokenList;

typedef struct ScriptDiagnosticList {
    ScriptDiagnostic *items;
    size_t count;
    size_t capacity;
} ScriptDiagnosticList;

typedef struct ScriptTokenizerResult {
    ScriptTokenList tokens;
    ScriptDiagnosticList diagnostics;
} ScriptTokenizerResult;

/*
 * Tokenizes a complete YAAT script source buffer.
 *
 * Token lexemes are non-owning slices into source. The source buffer must remain
 * alive as long as token lexemes are read. String token lexemes point to the
 * string contents between the opening and closing double quotes. All other
 * token lexemes point to the exact source characters that formed the token.
 *
 * The returned token and diagnostic arrays are owned by the caller and must be
 * released with script_tokenizer_result_free(). Diagnostic message strings are
 * static and must not be freed separately.
 */
ScriptTokenizerResult script_tokenize(const char *source);
void script_tokenizer_result_free(ScriptTokenizerResult *result);

const char *script_token_type_name(ScriptTokenType type);

#ifdef __cplusplus
}
#endif

#endif /* YAAT_SCRIPT_TOKENIZER_H */
