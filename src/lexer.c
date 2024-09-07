#include "lib.h"
#include "lexer.h"
#include "common.h"

#include <time.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>

DECLARE_STATIC(loc, LOC);

const char *KEYWORDS[KEYWORDS_SIZE] = {
	[TOKEN_IF]				= "if",
	[TOKEN_INLINE]		= "inline",
	[TOKEN_ELSE]			= "else",
	[TOKEN_WHILE]			= "while",
	[TOKEN_FUNC]			= "func",
	[TOKEN_CONST]			= "const",
	[TOKEN_DROP]			= "drop",
	[TOKEN_DUP]				= "dup",
	[TOKEN_DO]				= "do",
	[TOKEN_PROC]			= "proc",
	[TOKEN_VAR]				= "var",
	[TOKEN_BNOT]			= "bnot",
	[TOKEN_SYSCALL]		= "syscall",
	[TOKEN_SYSCALL1]	= "syscall1",
	[TOKEN_SYSCALL2]	= "syscall2",
	[TOKEN_SYSCALL3]	= "syscall3",
	[TOKEN_SYSCALL4]	= "syscall4",
	[TOKEN_SYSCALL5]	= "syscall5",
	[TOKEN_SYSCALL6]	= "syscall6",
	[TOKEN_END]				= "end"
};

const char *
token_kind_to_str(const token_kind_t token_kind)
{
	switch (token_kind) {
	case TOKEN_INLINE:					return "TOKEN_INLINE";
	case TOKEN_WRITE:						return "TOKEN_WRITE";
	case TOKEN_BNOT:						return "TOKEN_BNOT";
	case TOKEN_SYSCALL:					return "TOKEN_SYSCALL";
	case TOKEN_SYSCALL1:				return "TOKEN_SYSCALL1";
	case TOKEN_SYSCALL2:				return "TOKEN_SYSCALL2";
	case TOKEN_SYSCALL3:				return "TOKEN_SYSCALL3";
	case TOKEN_SYSCALL4:				return "TOKEN_SYSCALL4";
	case TOKEN_SYSCALL5:				return "TOKEN_SYSCALL5";
	case TOKEN_SYSCALL6:				return "TOKEN_SYSCALL6";
	case TOKEN_MOD:							return "TOKEN_MOD";
	case TOKEN_CONST:						return "TOKEN_CONST";
	case TOKEN_PROC:						return "TOKEN_PROC";
	case TOKEN_FUNC:						return "TOKEN_FUNC";
	case TOKEN_VAR:							return "TOKEN_VAR";
	case TOKEN_DUP:							return "TOKEN_DUP";
	case TOKEN_BOR:							return "TOKEN_BOR";
	case TOKEN_DROP:						return "TOKEN_DROP";
	case TOKEN_INTEGER:					return "TOKEN_INTEGER";
	case TOKEN_LITERAL:					return "TOKEN_LITERAL";
	case TOKEN_STRING_LITERAL:	return "TOKEN_STRING_LITERAL";
	case TOKEN_PLUS:						return "TOKEN_PLUS";
	case TOKEN_KEYWORDS_END:		return "TOKEN_KEYWORDS_END";
	case TOKEN_EQUAL:						return "TOKEN_EQUAL";
	case TOKEN_MINUS:						return "TOKEN_MINUS";
	case TOKEN_DIV:							return "TOKEN_DIV";
	case TOKEN_MUL:							return "TOKEN_MUL";
	case TOKEN_DOT:							return "TOKEN_DOT";
	case TOKEN_GREATER:					return "TOKEN_GREATER";
	case TOKEN_LESS:						return "TOKEN_LESS";
	case TOKEN_WHILE:						return "TOKEN_WHILE";
	case TOKEN_DO:							return "TOKEN_DO";
	case TOKEN_IF:							return "TOKEN_IF";
	case TOKEN_ELSE:						return "TOKEN_ELSE";
	case TOKEN_END:							return "TOKEN_END";
	}
}

const char *
token_kind_to_str_pretty(const token_kind_t token_kind)
{
	switch (token_kind) {
	case TOKEN_INLINE:					return "inline";
	case TOKEN_CONST:						return "const";
	case TOKEN_VAR:							return "var";
	case TOKEN_BNOT:						return "bnot";
	case TOKEN_SYSCALL:					return "syscall";
	case TOKEN_SYSCALL1:				return "syscall1";
	case TOKEN_SYSCALL2:				return "syscall2";
	case TOKEN_SYSCALL3:				return "syscall3";
	case TOKEN_SYSCALL4:				return "syscall4";
	case TOKEN_SYSCALL5:				return "syscall5";
	case TOKEN_SYSCALL6:				return "syscall6";
	case TOKEN_PROC:						return "proc";
	case TOKEN_FUNC:						return "func";
	case TOKEN_DUP:							return "dup";
	case TOKEN_DROP:						return "drop";
	case TOKEN_INTEGER:					return "integer";
	case TOKEN_LITERAL:					return "literal";
	case TOKEN_STRING_LITERAL:	return "string literal";
	case TOKEN_KEYWORDS_END:		return "keywords_end";
	case TOKEN_PLUS:						return "+";
	case TOKEN_MOD:							return "%";
	case TOKEN_BOR:							return "|";
	case TOKEN_WRITE:						return "!";
	case TOKEN_EQUAL:						return "=";
	case TOKEN_MINUS:						return "-";
	case TOKEN_DIV:							return "/";
	case TOKEN_MUL:							return "*";
	case TOKEN_DOT:							return ".";
	case TOKEN_GREATER:					return ">";
	case TOKEN_LESS:						return "<";
	case TOKEN_WHILE:						return "while";
	case TOKEN_DO:							return "do";
	case TOKEN_IF:							return "if";
	case TOKEN_ELSE:						return "else";
	case TOKEN_END:							return "end";
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
new_lexer(file_id_t file_id, lines_t lines, size_t lines_count)
{
	return (Lexer) {
		.row = 0,
		.lines = lines,
		.tokens_count = 0,
		.file_id = file_id,
		.lines_count = lines_count,
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
#ifdef DEBUG
	set_time;
#endif
	tokens_t ret = (tokens_t) malloc(sizeof(token_t)
																	 * MAXIMUM_TOKENS_AMOUNT_PER_LINE
																	 * lexer->lines_count);
#ifdef DEBUG
	dbg_time("allocation in `lexer_lex()`");
	set_time;
#endif
	for (size_t i = 0; i < lexer->lines_count; ++i) {
		lexer_lex_line(lexer, lexer->lines[i], &ret);
		lexer->row++;
	}
#ifdef DEBUG
	dbg_time("lexing_lines in `lexer_lex()`");
#endif
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

	case '.': return TOKEN_DOT;
	case '!': return TOKEN_WRITE;
	case '>': return TOKEN_GREATER;
	case '<': return TOKEN_LESS;
	case '+': return TOKEN_PLUS;
	case '-': return TOKEN_MINUS;
	case '/': return TOKEN_DIV;
	case '*': return TOKEN_MUL;
	case '%': return TOKEN_MOD;
	case '|': return TOKEN_BOR;
	case '=': return TOKEN_EQUAL;
	case '"': return TOKEN_STRING_LITERAL;
	}

	report_error("%s error: unexpected literal: '%s'", loc_to_str(loc), str);
}

void
lexer_lex_line(Lexer *lexer, line_t line, tokens_t *ret)
{
	for (size_t i = 0; i < line.count; ++i) {
		const ss_t ss = line.items[i];
		if (ss.str == NULL || *ss.str == '\0' || *ss.str == '#') continue;

		const loc_t loc = (loc_t) {
			.row = lexer->row,
			.col = ss.col,
			.file_id = lexer->file_id
		};

		const token_kind_t kind = type_token(ss.str, &loc);
		token_t token = new_token(loc, kind, ss.str);
		(*ret)[lexer->tokens_count++] = token;
	}
}

INLINE size_t utf8_char_len(unsigned char c)
{
	if ((u32) c < 0x80)    return 1;
	if ((u32) c < 0x800)   return 2;
	if ((u32) c < 0x10000) return 3;
	return 4;
}

line_t
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

	line_t ret = {
		.count = 0,
		.items = (sss_t) malloc(sizeof(ss_t) * 1024)
	};

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

				ret.items[ret.count++] = (ss_t) {
					.col = s + lspace_count,
					.str = scratch_buffer_copy(),
				};

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
		ret.items[ret.count++] = (ss_t) {
			.col = s + lspace_count,
			.str = scratch_buffer_copy(),
		};
	} else if (s != e) {
		scratch_buffer_clear();
		for (size_t i = s; i < len; ++i) {
			scratch_buffer_append_char(input[i]);
		}
		ret.items[ret.count++] = (ss_t) {
			.col = s + lspace_count,
			.str = scratch_buffer_copy(),
		};
	}

	return ret;
}
