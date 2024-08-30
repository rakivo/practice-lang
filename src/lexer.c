#include "lexer.h"
#include "common.h"
#include "lib.h"

#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>

const char *KEYWORDS[KEYWORDS_SIZE] = {
	[TOKEN_IF] = "if",
	[TOKEN_END] = "end"
};

const char *
token_kind_to_str(const token_kind_t token_kind)
{
	switch (token_kind) {
	case TOKEN_INTEGER:        return "TOKEN_INTEGER";        break;
	case TOKEN_LITERAL:        return "TOKEN_LITERAL";        break;
	case TOKEN_STRING_LITERAL: return "TOKEN_STRING_LITERAL"; break;
	case TOKEN_PLUS:           return "TOKEN_PLUS";           break;
	case TOKEN_KEYWORDS_END:   return "TOKEN_KEYWORDS_END";   break;
	case TOKEN_EQUAL:          return "TOKEN_EQUAL";          break;
	case TOKEN_DOT:            return "TOKEN_DOT";            break;
	case TOKEN_IF:             return "TOKEN_IF";             break;
	case TOKEN_END:            return "TOKEN_END";            break;
	}
}

const char *
loc_to_str(const loc_t *loc)
{
	const str_t file_path = fileid(loc->file_id).file_path;
	const size_t len = file_path.len
		+ 32 * 2 // two 32bit integers
		+ 3;     // 3 colons

	char *ret = calloc_string(len + 1);
	snprintf(ret, len,
					 "%s:%d:%d:",
					 file_path.buf,
					 loc->row + 1,
					 loc->col + 1);

	return ret;
}

const char *
token_to_str(const token_t *token)
{
	const char *token_loc = loc_to_str(&token->loc);
	const char *token_kind = token_kind_to_str(token->kind);
	const size_t len = strlen(token_loc) + strlen(token_kind) + strlen(token->str)
		+ 1  // space
		+ 2  // two quotes
		+ 1  // colon
		+ 1  // space
		+ 1; // \0

	char *ret = calloc_string(len);
	snprintf(ret, len, "%s '%s': %s",
					 token_loc,
					 token->str,
					 token_kind);

	return ret;
}

Lexer
new_lexer(file_id_t file_id, sss2D_t lines)
{
	return (Lexer) {
		.row = 0,
		.lines = lines,
		.file_id = file_id
	};
}

tokens_t
lexer_lex(Lexer *lexer)
{
	tokens_t ret = NULL;

	FOREACH(sss_t, line, lexer->lines) {
		lexer_lex_line(lexer, line, &ret);
		lexer->row++;
	}

	return ret;
}

INLINE i32
check_for_keywords(const char *str)
{
	for (size_t i = 0; i < KEYWORDS_SIZE; ++i) {
		if (0 == strcmp(str, KEYWORDS[i])) return i;
	}

	return -1;
}

token_kind_t
type_token(const char *str)
{
	const i32 keyword_idx = check_for_keywords(str);
	if (keyword_idx >= 0) return (token_kind_t) keyword_idx;

	const char first = str[0];

	if (isdigit(first)) return TOKEN_INTEGER;
	if (isalpha(first)) return TOKEN_LITERAL;

	switch (first) {
	case '.': return TOKEN_DOT;            break;
	case '+': return TOKEN_PLUS;           break;
	case '=': return TOKEN_EQUAL;          break;
	case '"': return TOKEN_STRING_LITERAL; break;
	}

	UNREACHABLE;
}

void
lexer_lex_line(Lexer *lexer, sss_t line, tokens_t *ret)
{
	FOREACH(ss_t, ss, line) {
		const token_kind_t kind = type_token(ss.str);

		token_t token = {
			.loc = {
				.row = lexer->row,
				.col = ss.col,
				.file_id = lexer->file_id
			},
			.kind = kind,
			.str = ss.str,
		};

		vec_add(*ret, token);
	}
}

static size_t utf8_char_len(unsigned char c)
{
	if ((u32) c < 0x80)    return 1;
	if ((u32) c < 0x800)   return 2;
	if ((u32) c < 0x10000) return 3;
	return 4;
}

sss_t
split(const char *input, char delim)
{
	const size_t len = strlen(input);
	sss_t ret = NULL;

	size_t s = 0;
	size_t e = 0;

	bool in_single_quote = false;
	bool in_double_quote = false;

	while (e < len) {
		char c = input[e];

		if (c == '\'' && !in_double_quote) {
			in_single_quote = !in_single_quote;
		} else if (c == '"' && !in_single_quote) {
			in_double_quote = !in_double_quote;
		} else if (c == delim && !in_single_quote && !in_double_quote) {
			if (s != e) {
				scratch_buffer_clear();
				for (size_t i = s; i < e - s; ++i) {
					scratch_buffer_append_char(input[i]);
				}
				const ss_t ss = (ss_t) {
					.col = s,
					.str = scratch_buffer_copy(),
				};
				vec_add(ret, ss);
			}
			s = e + 1;
		}

		e += utf8_char_len(c);
	}

	if (s < e) {
		scratch_buffer_clear();
		for (size_t i = s; i < e; ++i) {
			scratch_buffer_append_char(input[i]);
		}

		const ss_t ss = (ss_t) {
			.col = s,
			.str = scratch_buffer_copy(),
		};
		vec_add(ret, ss);
	}

	return ret;
}
