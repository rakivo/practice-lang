#include "lib.h"
#include "ast.h"
#include "lexer.h"
#include "parser.h"
#include "common.h"

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
		const ast_t ast = ast_token(parser, &parser->ts[token_idx], false);
		append_ast(ast);
		token_idx++;
	}
}

static bool is_else = false;
static bool cond_is_not_empty = false;

static size_t if_count = 0;
static size_t else_count = 0;

ast_t
ast_token(Parser *parser, const token_t *token, bool rec)
{
	switch (token->kind) {
	case TOKEN_PROC: {
		// Skip `proc` keyword
		token_idx++;

		if (token_idx > vec_size(parser->ts) || parser->ts[token_idx].kind == TOKEN_DO) {
			report_error("%s error: proc without a name", loc_to_str(&locid(token->loc_id)));
		}

		ast_t ast = make_ast(0, token->loc_id, ++next, AST_PROC,
			.proc_stmt = {
				.name = parser->ts[token_idx++],
				.args = NULL,
				.body = -1,
			}
		);

		while (token_idx < vec_size(parser->ts)) {
			const token_t token_ = parser->ts[token_idx++];

			if (token_.kind == TOKEN_DO) {
				break;
			}

			const i32 kind_ = value_kind_try_from_str(token_.str);
			if (kind_ < 0) {
				report_error("%s error: invalid type: %s", loc_to_str(&locid(token_.loc_id)), token_.str);
			}

			const value_kind_t kind = (value_kind_t) kind_;

			if (token_idx > vec_size(parser->ts) || parser->ts[token_idx].kind == TOKEN_DO) {
				report_error("%s error: expected a name after the type", loc_to_str(&locid(token_.loc_id)));
			}

			const proc_arg_t proc_arg = {
				.kind = kind,
				.loc_id = token_.loc_id,
				.name = parser->ts[token_idx++].str
			};

			vec_add(ast.proc_stmt.args, proc_arg);
		}

		size_t token_count = 0;
		while (token_idx < vec_size(parser->ts)) {
			const token_t token_ = parser->ts[token_idx];
			if (token_.kind == TOKEN_END) {
				break;
			} else if (token_count == 0) {
				ast.proc_stmt.body = next;
			}

			ast_t ast = ast_token(parser, &parser->ts[token_idx], true);
			token_idx++;

			if (parser->ts[token_idx].kind == TOKEN_END
			&& (ast.ast_kind == AST_IF || ast.ast_kind == AST_WHILE))
			{
				ast.next = -1;
			}

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

		return ast;
	} break;

	case TOKEN_WHILE: {
		if (!rec) {
			cond_is_not_empty = false;
		}

		// Skip `while` keyword
		token_idx++;

		ast_t ast = make_ast(0, token->loc_id, ++next, AST_WHILE,
			.while_stmt = {
				.cond = -1,
				.body = -1,
			}
		);

		size_t token_count = 0;
		size_t start = token_idx;

		while (token_idx < vec_size(parser->ts)) {
			const token_t token_ = parser->ts[token_idx];
			if (token_.kind == TOKEN_DO) {
				if (token_idx > start) {
					cond_is_not_empty = true;
				}
				token_idx++;
				break;
			} else if (token_count == 0) {
				ast.while_stmt.cond = next;
			}

			const ast_t ast = ast_token(parser, &parser->ts[token_idx], true);
			append_ast(ast);
			token_idx++;
			token_count++;
		}

		if (cond_is_not_empty) {
			last_ast.next = -1;
		}

		token_count = 0;

		while (token_idx < vec_size(parser->ts)) {
			const token_t token_ = parser->ts[token_idx];
			if (token_.kind == TOKEN_END) {
				break;
			} else if (token_count == 0) {
				ast.while_stmt.body = next;
			}

			const ast_t ast = ast_token(parser, &parser->ts[token_idx], true);
			append_ast(ast);
			token_idx++;
			token_count++;
		}

		if (last_ast.next > 0) {
			ast.next = last_ast.next;
		} else {
			ast.next = next;
		}

		// indicate the end of the body
		last_ast.next = -1;

		return ast;
	} break;

	case TOKEN_IF: {
		if (!rec) {
			if_count = 0;
			else_count = 0;
			is_else = false;
		}

		if_count++;

		// Skip `if` keyword
		token_idx++;

		ast_t ast = make_ast(0, token->loc_id, ++next, AST_IF,
			.if_stmt = {
				.then_body = -1,
				.else_body = -1,
			}
		);

		size_t token_count = 0;

		while (token_idx < vec_size(parser->ts)) {
			const token_t token_ = parser->ts[token_idx];
			if (token_.kind == TOKEN_END) {
				break;
			} else if (token_.kind == TOKEN_ELSE) {
				if (if_count < ++else_count) {
					report_error("%s error: too many elses in one if", loc_to_str(&locid(token_.loc_id)));
				} else if (asts_len > 0 && token_count > 0) {
					last_ast.next = -1;
				}
				token_idx++;
				is_else = true;
				ast.if_stmt.else_body = next;
				continue;
			} else if (token_count == 0 && !is_else) {
				ast.if_stmt.then_body = next;
			}

			const ast_t ast = ast_token(parser, &parser->ts[token_idx], true);
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

		return ast;
	} break;

	case TOKEN_DO: {
		report_error("%s error: do without while or proc context", loc_to_str(&locid(token->loc_id)));
	} break;

	case TOKEN_ELSE: {
		report_error("%s error: else outside of if scope", loc_to_str(&locid(token->loc_id)));
	} break;

	case TOKEN_END: {
		report_error("%s error: no matching if, proc or do found", loc_to_str(&locid(token->loc_id)));
	} break;

	case TOKEN_DROP: {
		ast_t ast = make_ast(0, token->loc_id, ++next, AST_DROP, .drop_stmt = {0});
		return ast;
	} break;

	case TOKEN_DUP: {
		ast_t ast = make_ast(0, token->loc_id, ++next, AST_DUP, .dup_stmt = {0});
		return ast;
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
		scratch_buffer_clear();
		scratch_buffer_append(token->str);

		bool is_call = false;
		if (scratch_buffer.str[scratch_buffer.len - 1] == '!') {
			scratch_buffer.str[--scratch_buffer.len] = '\0';
			is_call = true;
		}

		if (is_call) {
			return (ast_t) make_ast(0, token->loc_id, ++next, AST_CALL, .call = { .str = scratch_buffer_copy() });
		} else {
			return (ast_t) make_ast(0, token->loc_id, ++next, AST_LITERAL, .literal = { .str = token->str });
		}
	} break;

	case TOKEN_EQUAL: {
		ast_t ast = make_ast(0, token->loc_id, ++next, AST_EQUAL, .equal_stmt = {0});
		return ast;
	} break;

	case TOKEN_GREATER: {
		ast_t ast = make_ast(0, token->loc_id, ++next, AST_GREATER, .greater_stmt = {0});
		return ast;
	} break;

	case TOKEN_LESS: {
		ast_t ast = make_ast(0, token->loc_id, ++next, AST_LESS, .less_stmt = {0});
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
