#ifndef LEXER_H_
#define LEXER_H_

#include "lib.h"
#include "file.h"
#include "common.h"

#include <stdlib.h>
#include <stdint.h>

typedef enum {
	TOKEN_IF,
	TOKEN_END,

	// Separator of keywords and other tokens, which is also the size of `KEYWORDS` array.
	TOKEN_KEYWORDS_END,

	TOKEN_EQUAL,
	TOKEN_PLUS,
	TOKEN_DOT,
	TOKEN_INTEGER,
	TOKEN_LITERAL,
	TOKEN_STRING_LITERAL,
} token_kind_t;

#define KEYWORDS_SIZE TOKEN_KEYWORDS_END
extern const char *KEYWORDS[KEYWORDS_SIZE];

typedef struct {
	u32 row, col;
	file_id_t file_id;
} loc_t;

typedef struct {
	loc_t loc;
	token_kind_t kind;
	const char *str;
} token_t;

const char *
token_kind_to_str(const token_kind_t token_kind);

const char *
loc_to_str(const loc_t *loc);

const char *
token_to_str(const token_t *token);

typedef struct {
	u32 col;
	const char *str;
} ss_t;

typedef ss_t *sss_t;
typedef ss_t **sss2D_t;

typedef token_t *tokens_t;

typedef struct {
	u32 row;
	sss2D_t lines;
	file_id_t file_id;
} Lexer;

Lexer
new_lexer(file_id_t file_id, sss2D_t lines);

tokens_t
lexer_lex(Lexer *lexer);

sss_t
split(const char *input, char delim);

token_kind_t
type_token(const char *str);

void
lexer_lex_line(Lexer *lexer, sss_t line, tokens_t *ret);

#endif // LEXER_H_
