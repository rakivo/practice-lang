#ifndef LEXER_H_
#define LEXER_H_

#include "lib.h"
#include "file.h"
#include "common.h"

#include <stdlib.h>
#include <stdint.h>

#define LINE_CAP (1024 * 5)
#define LINES_CAP (1024 * 500)
#define TOKENS_LINE_CAP 1024
#define TOKENS_CAP (1024 * 500)

// NOTE: If you added a new keyword, update `KEYWORDS` array at the top of the `lexer.c` file.
typedef enum {
	TOKEN_IF,
	TOKEN_WHILE,
	TOKEN_DO,
	TOKEN_ELSE,
	TOKEN_DUP,
	TOKEN_FUNC,
	TOKEN_CONST,
	TOKEN_DROP,
	TOKEN_PROC,
	TOKEN_SYSCALL,
	TOKEN_SYSCALL1,
	TOKEN_SYSCALL2,
	TOKEN_SYSCALL3,
	TOKEN_SYSCALL4,
	TOKEN_SYSCALL5,
	TOKEN_SYSCALL6,
	TOKEN_END,

	// Separator of keywords and other tokens, which is also the size of `KEYWORDS` array.
	TOKEN_KEYWORDS_END,

	TOKEN_GREATER,
	TOKEN_LESS,
	TOKEN_EQUAL,

	TOKEN_PLUS,
	TOKEN_MINUS,
	TOKEN_DIV,
	TOKEN_BOR,
	TOKEN_MOD,
	TOKEN_MUL,

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

typedef u32 loc_id_t;

typedef struct {
	loc_id_t loc_id;
	token_kind_t kind;
	const char *str;
} token_t;

token_t
new_token(loc_t loc, token_kind_t token_kind, const char *str);

const char *
token_kind_to_str(token_kind_t token_kind);

const char *
token_kind_to_str_pretty(const token_kind_t token_kind);

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
	sss_t items;
	size_t count;
} line_t;

typedef line_t *lines_t;

typedef struct {
	u32 row;
	lines_t lines;
	file_id_t file_id;
	size_t lines_count;
	size_t tokens_count;
} Lexer;

Lexer
new_lexer(file_id_t file_id, lines_t lines, size_t lines_count);

tokens_t
lexer_lex(Lexer *lexer);

line_t
split(char *input, char delim);

token_kind_t
type_token(const char *str, const loc_t *loc);

void
lexer_lex_line(Lexer *lexer, line_t line, tokens_t *ret);

#define LOCS_CAP (1024 * 500)
extern loc_id_t LOCS_SIZE;
extern loc_t LOCS[LOCS_CAP];

#define locid(id) (LOCS[id])
#define locs_len (LOCS_SIZE)
#define last_loc (LOCS[LOCS_SIZE - 1])
#define append_loc(loc_) (LOCS[LOCS_SIZE++] = loc_)

#endif // LEXER_H_
