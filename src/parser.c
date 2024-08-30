#include "ast.h"
#include "common.h"
#include "lexer.h"
#include "lib.h"
#include "parser.h"

#include <errno.h>
#include <limits.h>
#include <stdlib.h>

INLINE i64
parse_int(const char *str)
{
	errno = 0;
	const i64 ret = strtol(str, NULL, 10);

	if ((errno == ERANGE
	&& (ret == LONG_MAX || ret == LONG_MIN)) || (errno != 0 && ret == 0))
	{
		perror("strtol");
		exit(1);
	}

	return ret;
}

static ast_id_t next = 0;
static size_t token_idx = 0;

void
parser_parse(Parser *parser)
{
	while (token_idx < vec_size(parser->ts)) {
		const ast_t ast = ast_token(parser, &parser->ts[token_idx]);
		append_ast(ast);
		token_idx++;
	}
}

static loc_id_t *if_locs = NULL;
static bool done = false;
static bool is_else = false;
static size_t if_count = 1;
static size_t else_count = 0;
static size_t end_count = 0;
static size_t token_count = 0;

ast_t
ast_token(Parser *parser, const token_t *token)
{
	switch (token->kind) {
	case TOKEN_IF: {
		vec_add(if_locs, token->loc_id);
		// Skip `if` keyword
		token_idx++;

		ast_t ast = make_ast(0, token->loc_id, ++next, AST_IF,
			.if_stmt = {
				.then_body = -1,
				.else_body = -1,
			}
		);

		while (!done && token_idx < vec_size(parser->ts)) {
			const token_t token_ = parser->ts[token_idx];
			if (token_.kind == TOKEN_IF) {
				vec_add(if_locs, token_.loc_id);
				token_idx++;
				if_count++;
			} else if (token_.kind == TOKEN_END) {
				token_idx++;
				end_count++;
				if (end_count > if_count) {
					printf("%s error: no matching `if` found\n", loc_to_str(&locid(token_.loc_id)));
					exit(1);
				}
			} else if (token_.kind == TOKEN_ELSE) {
				else_count++;
				if (if_count < else_count) {
					printf("%s error: too many elses in one if\n", loc_to_str(&locid(token_.loc_id)));
					exit(1);
				} else if (asts_len > 0 && token_count > 0) {
					last_ast.next = -1;
				}
				is_else = true;
				token_idx++;
				ast.if_stmt.else_body = next;
				continue;
			} else if (token_count == 0 && !is_else) {
				ast.if_stmt.then_body = next;
			}

			// const ast_t ast = ast_token(parser, &token);
			if (parser->ts[token_idx].kind == TOKEN_END) {
				token_idx++;
				end_count++;
				if (if_count == end_count) {
					done = true;
					break;
				} else if (end_count > if_count) {
					printf("%s error: no matching `if` found\n", loc_to_str(&locid(token_.loc_id)));
					exit(1);
				}
				continue;
			} else if (if_count == end_count) {
				done = true;
				break;
			}

			const ast_t ast = ast_token(parser, &parser->ts[token_idx]);
			if (token_.kind != TOKEN_ELSE) token_idx++;
			append_ast(ast);
			token_count++;
		}

		if (last_ast.next > 0) {
			ast.next = last_ast.next;
		} else {
			ast.next = next;
		}

		// indicate the end of the body
		last_ast.next = -1;

		if (!done) {
			printf("%s error: no matching end found\n", loc_to_str(&locid(if_locs[if_count])));
			exit(1);
		}

		vec_resize(if_locs, 0);

		if_count = 1;
		else_count = 0;
		end_count = 0;
		token_count = 0;
		done = false;
		is_else = false;

		return ast;
	} break;

	case TOKEN_ELSE:
	case TOKEN_END: if (if_count == 1) {
		printf("%s error: no matching if found\n", loc_to_str(&locid(token->loc_id)));
	} else {
		UNREACHABLE;
	} break;

	case TOKEN_KEYWORDS_END: break;

	case TOKEN_INTEGER: {
		ast_t ast = make_ast(0, token->loc_id, ++next, AST_PUSH,
			.push_stmt = {
				.value_kind = VALUE_KIND_INTEGER,
				.integer = parse_int(token->str),
			}
		);
		return ast;
	} break;

	case TOKEN_STRING_LITERAL: {
		ast_t ast = make_ast(0, token->loc_id, ++next, AST_PUSH,
			.push_stmt = {
				.value_kind = VALUE_KIND_STRING,
				.str = token->str,
			}
		);
		return ast;
	} break;

	case TOKEN_LITERAL: {
		printf("%s\n", token->str);
		TODO("Handle literals");
	} break;

	case TOKEN_EQUAL: {
		ast_t ast = make_ast(0, token->loc_id, ++next, AST_EQUAL, .equal_stmt = {0});
		return ast;
	} break;

	case TOKEN_MUL: {
		ast_t ast = make_ast(0, token->loc_id, ++next, AST_MUL, .mul_stmt = {0});
		return ast;
	} break;

	case TOKEN_DIV: {
		ast_t ast = make_ast(0, token->loc_id, ++next, AST_DIV, .div_stmt = {0});
		return ast;
	} break;

	case TOKEN_MINUS: {
		ast_t ast = make_ast(0, token->loc_id, ++next, AST_MINUS, .minus_stmt = {0});
		return ast;
	} break;

	case TOKEN_PLUS: {
		ast_t ast = make_ast(0, token->loc_id, ++next, AST_PLUS, .plus_stmt = {0});
		return ast;
	} break;

	case TOKEN_DOT: {
		ast_t ast = make_ast(0, token->loc_id, ++next, AST_DOT, .dot_stmt = {0});
		return ast;
	} break;
	}

	UNREACHABLE;
}
