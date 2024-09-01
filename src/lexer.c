#include "lexer.h"
#include "common.h"
#include "lib.h"

#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>

DECLARE_STATIC(loc, LOC);

const char *KEYWORDS[KEYWORDS_SIZE] = {
	[TOKEN_IF]		= "if",
	[TOKEN_ELSE]	= "else",
	[TOKEN_CALL]  = "call",
	[TOKEN_WHILE] = "while",
	[TOKEN_DROP]	= "drop",
	[TOKEN_DUP]   = "dup",
	[TOKEN_DO]		= "do",
	[TOKEN_PROC]  = "proc",
	[TOKEN_END]		= "end"
};

const char *
token_kind_to_str(const token_kind_t token_kind)
{
	switch (token_kind) {
	case TOKEN_CALL:						return "TOKEN_CALL";						break;
	case TOKEN_PROC:						return "TOKEN_PROC";						break;
	case TOKEN_DUP:							return "TOKEN_DUP";							break;
	case TOKEN_DROP:						return "TOKEN_DROP";						break;
	case TOKEN_INTEGER:					return "TOKEN_INTEGER";					break;
	case TOKEN_LITERAL:					return "TOKEN_LITERAL";					break;
	case TOKEN_STRING_LITERAL:	return "TOKEN_STRING_LITERAL";	break;
	case TOKEN_PLUS:						return "TOKEN_PLUS";						break;
	case TOKEN_KEYWORDS_END:		return "TOKEN_KEYWORDS_END";		break;
	case TOKEN_EQUAL:						return "TOKEN_EQUAL";						break;
	case TOKEN_MINUS:						return "TOKEN_MINUS";						break;
	case TOKEN_DIV:							return "TOKEN_DIV";							break;
	case TOKEN_MUL:							return "TOKEN_MUL";							break;
	case TOKEN_DOT:							return "TOKEN_DOT";							break;
	case TOKEN_GREATER:					return "TOKEN_GREATER";					break;
	case TOKEN_LESS:						return "TOKEN_LESS";						break;
	case TOKEN_WHILE:						return "TOKEN_WHILE";						break;
	case TOKEN_DO:							return "TOKEN_DO";							break;
	case TOKEN_IF:							return "TOKEN_IF";							break;
	case TOKEN_ELSE:						return "TOKEN_ELSE";						break;
	case TOKEN_END:							return "TOKEN_END";							break;
	}
}

const char *
loc_to_str(const loc_t *loc)
{
	const str_t file_path = fileid(loc->file_id).file_path;
	const size_t len = file_path.len
		+ 11 * 2 // two 32bit integers
		+ 3			 // 3 colons
		+ 1;		 // \0

	char *ret = calloc_string(len);
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
	const char *token_loc = loc_to_str(&locid(token->loc_id));
	const char *token_kind = token_kind_to_str(token->kind);
	const size_t len = strlen(token_loc) + strlen(token_kind) + strlen(token->str)
		+ 2  // 2 spaces
		+ 2  // two quotes
		+ 1  // colon
		+ 1; // \0

	char *ret = calloc_string(len);
	snprintf(ret, len, "%s '%s': %s",
					 token_loc,
					 token->str,
					 token_kind);

	return ret;
}

Lexer
new_lexer(file_id_t file_id, sss2D_t lines, u32 lines_skipped)
{
	return (Lexer) {
		.row = 0,
		.lines = lines,
		.file_id = file_id,
		.lines_skipped = lines_skipped,
	};
}

token_t
new_token(loc_t loc, token_kind_t token_kind, const char *str)
{
	append_loc(loc);
	return (token_t) {
		.loc_id = locs_len - 1,
		.kind = token_kind,
		.str = str,
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
type_token(const char *str, const loc_t *loc)
{
	const i32 keyword_idx = check_for_keywords(str);
	if (keyword_idx >= 0) return (token_kind_t) keyword_idx;

	const char first = str[0];

	switch (first) {
	case NUMBER_CHAR_CASE: return TOKEN_INTEGER; break;

	case LOWER_CHAR_CASE:
	case UPPER_CHAR_CASE: return TOKEN_LITERAL; break;

	case '.': return TOKEN_DOT;							break;
	case '>': return TOKEN_GREATER;					break;
	case '<': return TOKEN_LESS;						break;
	case '+': return TOKEN_PLUS;						break;
	case '-': return TOKEN_MINUS;						break;
	case '/': return TOKEN_DIV;							break;
	case '*': return TOKEN_MUL;							break;
	case '=': return TOKEN_EQUAL;						break;
	case '"': return TOKEN_STRING_LITERAL;	break;
	}

	report_error("%s error: unexpected literal: '%s'", loc_to_str(loc), str);
}

void
lexer_lex_line(Lexer *lexer, sss_t line, tokens_t *ret)
{
	FOREACH(ss_t, ss, line) {
		if (ss.str == NULL || *ss.str == '\0') continue;

		const loc_t loc = (loc_t) {
			.row = lexer->row + lexer->lines_skipped,
			.col = ss.col,
			.file_id = lexer->file_id
		};
		const token_kind_t kind = type_token(ss.str, &loc);
		token_t token = new_token(loc, kind, ss.str);
		vec_add(*ret, token);
	}
}

INLINE size_t utf8_char_len(unsigned char c)
{
	if ((u32) c < 0x80)    return 1;
	if ((u32) c < 0x800)   return 2;
	if ((u32) c < 0x10000) return 3;
	return 4;
}

sss_t
split(char *input, char delim)
{
	// trim newline from `fgets`
	size_t len_ = strlen(input);
	if (len_ > 0 && input[len_ - 1] == '\n') {
		input[--len_] = '\0';
	}

	for (size_t i = 0; i < len_; ++i) {
		if (input[i] == '#') {
			input[i] = '\0';
			len_ = i - 1;
			break;
		}
	}

	// do rtrim
	while (isspace(input[len_])) {
		input[len_] = '\0';
		len_ -= 1;
	}

	// do ltrim
	size_t lspace_count = 0;
	while (isspace(*input)) {
		input++;
		lspace_count++;
	}

	const size_t len = strlen(input);

	sss_t ret = NULL;

	size_t s = 0;
	size_t e = 0;
	size_t i = 0;

	bool in_single_quote = false;
	bool in_double_quote = false;

	for (; i < len; ++i) {
		char c = input[i];

		if (c == '\'' && !in_double_quote) {
			in_single_quote = !in_single_quote;
		} else if (c == '"' && !in_single_quote) {
			in_double_quote = !in_double_quote;
		} else if (c == delim && !in_single_quote && !in_double_quote) {
			if (s < i) {
				scratch_buffer_clear();
				for (size_t i_ = s; i_ < i; ++i_) {
					scratch_buffer_append_char(input[i_]);
				}
				vec_add(ret, (ss_t) {
					.col = s + lspace_count,
					.str = scratch_buffer_copy(),
				});

				s = i + utf8_char_len(c);

				// Skip consecutive delimiters
				while (s < len && input[s] == delim) s++;
				i = s - 1;
			} else {
				e = i + utf8_char_len(c);
			}
		}
	}

	if (s + e == 0 && i > 0) {
		scratch_buffer_clear();
		scratch_buffer_append(input);
		vec_add(ret, (ss_t) {
			.col = s + lspace_count,
			.str = scratch_buffer_copy(),
		});
	} else if (s != e) {
		scratch_buffer_clear();
		for (size_t i = s; i < len; ++i) {
			scratch_buffer_append_char(input[i]);
		}
		vec_add(ret, (ss_t) {
			.col = s + lspace_count,
			.str = scratch_buffer_copy(),
		});
	}

	return ret;
}
