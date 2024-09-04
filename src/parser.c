#include "lib.h"
#include "ast.h"
#include "lexer.h"
#include "parser.h"
#include "common.h"

#include <errno.h>
#include <limits.h>
#include <stdlib.h>

#undef report_error
#define report_error(fmt, ...) do { \
	report_error_noexit(__func__, __FILE__, __LINE__, fmt, __VA_ARGS__); \
	main_deinit(); \
	exit(EXIT_FAILURE); \
} while (0)

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

Parser
new_parser(const tokens_t ts, size_t tc)
{
	return (Parser) {
		.ts = ts,
		.tc = tc,
	};
}

static ast_id_t next = 0;
static size_t token_idx = 0;

void
parser_parse(Parser *parser)
{
	while (token_idx < parser->tc) {
		const ast_t ast = ast_token(parser, &parser->ts[token_idx], false);
		append_ast(ast);
		token_idx++;
	}
}

static bool cond_is_not_empty = false;

static size_t if_count = 0;
static size_t else_count = 0;

ast_t
ast_token(Parser *parser, const token_t *token, bool rec)
{
	switch (token->kind) {
	case TOKEN_VAR:
	case TOKEN_CONST: {
		// Skip `const` keyword
		token_idx++;

		if (token_idx > parser->tc || parser->ts[token_idx].kind == TOKEN_END) {
			report_error("%s error: %s without a name",
									 loc_to_str(&locid(token->loc_id)),
									 token->kind == TOKEN_VAR ? "var" : "const");
		}

		if (parser->ts[token_idx].kind != TOKEN_LITERAL) {
			report_error("%s error: expected name of the %s to be non-keyword `literal`, but got: `%s`",
									 loc_to_str(&locid(parser->ts[token_idx].loc_id)),
									 token_kind_to_str_pretty(parser->ts[token_idx].kind),
									 token->kind == TOKEN_VAR ? "variable" : "constant");
		}

		ast_t ast = make_ast(0, token->loc_id, ++next,
												 token->kind == TOKEN_VAR ? AST_VAR : AST_CONST,
			.const_stmt = {
				.name = &parser->ts[token_idx++],
				.body = -1,
				.constexpr = false,
			}
		);

		bool done = false;
		size_t token_count = 0;
		while (token_idx < parser->tc) {
			const token_t token_ = parser->ts[token_idx];
			if (token_.kind == TOKEN_END) {
				done = true;
				break;
			} else if (token_count == 0) {
				if (token->kind == TOKEN_VAR) {
					ast.var_stmt.body = next;
				} else {
					ast.const_stmt.body = next;
				}
			}

			ast_t ast = ast_token(parser, &parser->ts[token_idx], true);
			token_idx++;

			if (parser->ts[token_idx].kind == TOKEN_END) {
				ast.next = -1;
			}

			append_ast(ast);
			token_count++;
		}

		if (!done) {
			report_error("%s error: no closing end found", loc_to_str(&locid(token->loc_id)));
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

	case TOKEN_PROC: {
		// Skip `proc` keyword
		token_idx++;

		if (token_idx > parser->tc || parser->ts[token_idx].kind == TOKEN_DO) {
			report_error("%s error: proc without a name", loc_to_str(&locid(token->loc_id)));
		}

		ast_t ast = make_ast(0, token->loc_id, ++next, AST_PROC,
			.proc_stmt = {
				.name = &parser->ts[token_idx++],
				.args = NULL,
				.body = -1,
			}
		);

		while (token_idx < parser->tc) {
			const token_t token_ = parser->ts[token_idx++];

			if (token_.kind == TOKEN_DO) {
				break;
			}

			const i32 kind_ = value_kind_try_from_str(token_.str);
			if (kind_ < 0) {
				report_error("%s error: invalid type: %s", loc_to_str(&locid(token_.loc_id)), token_.str);
			}

			const value_kind_t kind = (value_kind_t) kind_;

			if (token_idx > parser->tc || parser->ts[token_idx].kind == TOKEN_DO) {
				report_error("%s error: expected a name after the type", loc_to_str(&locid(token_.loc_id)));
			}

			const arg_t proc_arg = {
				.kind = kind,
				.loc_id = token_.loc_id,
				.name = parser->ts[token_idx++].str
			};

			vec_add(ast.proc_stmt.args, proc_arg);
		}

		bool done = false;
		size_t token_count = 0;
		while (token_idx < parser->tc) {
			const token_t token_ = parser->ts[token_idx];
			if (token_.kind == TOKEN_END) {
				done = true;
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

		if (!done) {
			report_error("%s error: no closing end found", loc_to_str(&locid(token->loc_id)));
		}

		if (last_ast.next > 0) {
			ast.next = last_ast.next;
		} else {
			ast.next = next;
		}

		if (token_count > 0) {
			// indicate the end of the body
			last_ast.next = -1;
		}

		return ast;
	} break;

	case TOKEN_FUNC: {
		// Skip `proc` keyword
		token_idx++;

		if ((token_idx > parser->tc
			|| parser->ts[token_idx].kind != TOKEN_LITERAL)
				|| (parser->ts[token_idx].kind == TOKEN_LITERAL
					&& value_kind_try_from_str(parser->ts[token_idx].str) != -1))
		{
			report_error("%s error: func without a name", loc_to_str(&locid(token->loc_id)));
		}

		ast_t ast = make_ast(0, token->loc_id, ++next, AST_FUNC,
			.func_stmt = {
				.name = &parser->ts[token_idx++],
				.args = NULL,
				.body = -1,
				.ret_type = VALUE_KIND_POISONED
			}
		);

		const token_t ret_type_token = parser->ts[token_idx++];
		const i32 ret_type_ = value_kind_try_from_str(ret_type_token.str);
		if (ret_type_ < 0) {
			report_error("%s error: expected return type after `func` keyword, but got: %s", loc_to_str(&locid(ret_type_token.loc_id)), ret_type_token.str);
		}

		ast.func_stmt.ret_type = (value_kind_t) ret_type_;

		while (token_idx < parser->tc) {
			const token_t token_ = parser->ts[token_idx++];

			if (token_.kind == TOKEN_DO) {
				break;
			}

			const i32 kind_ = value_kind_try_from_str(token_.str);
			if (kind_ < 0) {
				eprintf("%s error: invalid type: %s\n", loc_to_str(&locid(token_.loc_id)), token_.str);
				report_error("  NOTE: You might have forgot to specify return type of the function.\n"
										 "    For example: 'func %s int <...> do <...> end'",
										 ast.func_stmt.name->str);
			}

			const value_kind_t kind = (value_kind_t) kind_;

			if (token_idx > parser->tc || parser->ts[token_idx].kind == TOKEN_DO) {
				report_error("%s error: expected a name after the type", loc_to_str(&locid(token_.loc_id)));
			}

			const arg_t proc_arg = {
				.kind = kind,
				.loc_id = token_.loc_id,
				.name = parser->ts[token_idx++].str
			};

			vec_add(ast.proc_stmt.args, proc_arg);
		}

		bool done = false;
		size_t token_count = 0;
		while (token_idx < parser->tc) {
			const token_t token_ = parser->ts[token_idx];
			if (token_.kind == TOKEN_END) {
				done = true;
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

		if (!done) {
			report_error("%s error: no closing end found", loc_to_str(&locid(token->loc_id)));
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

		while (token_idx < parser->tc) {
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

		bool done = false;
		while (token_idx < parser->tc) {
			const token_t token_ = parser->ts[token_idx];
			if (token_.kind == TOKEN_END) {
				done = true;
				break;
			} else if (token_count == 0) {
				ast.while_stmt.body = next;
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

		if (!done) {
			report_error("%s error: no closing end found", loc_to_str(&locid(token->loc_id)));
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

		bool done = false;
		bool is_else = false;
		size_t token_count = 0;
		while (token_idx < parser->tc) {
			const token_t token_ = parser->ts[token_idx];
			if (token_.kind == TOKEN_END) {
				done = true;
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

			ast_t ast = ast_token(parser, &parser->ts[token_idx], true);

			if (token_.kind != TOKEN_ELSE) token_idx++;
			append_ast(ast);
			token_count++;
		}

		if (!done) {
			report_error("%s error: no closing end found", loc_to_str(&locid(token->loc_id)));
		}

		if (token_idx + 1 < parser->tc && parser->ts[token_idx + 1].kind == TOKEN_END) {
			ast.next = -1;
		} else if (last_ast.next > 0) {
			ast.next = last_ast.next;
		} else {
			ast.next = next;
		}

		// indicate the end of the body
		last_ast.next = -1;

		return ast;
	} break;

	case TOKEN_DO:		report_error("%s error: do without while, func or proc context", loc_to_str(&locid(token->loc_id)));
	case TOKEN_ELSE:	report_error("%s error: else outside of if scope", loc_to_str(&locid(token->loc_id)));
	case TOKEN_END:		report_error("%s error: no matching if, proc, const, func or do found", loc_to_str(&locid(token->loc_id)));

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

	case TOKEN_WRITE: {
		if (token->str + 1 == NULL || *(token->str + 1) == '\0') {
			report_error("%s error: invalid write statement", loc_to_str(&locid(token->loc_id)));
		}
		return (ast_t) make_ast(0, token->loc_id, ++next, AST_WRITE, .write_stmt = {token});
	} break;

	case TOKEN_DROP:			return (ast_t) make_ast(0, token->loc_id, ++next, AST_DROP,			.drop_stmt		= {0});
	case TOKEN_DUP:				return (ast_t) make_ast(0, token->loc_id, ++next, AST_DUP,			.dup_stmt			= {0});
	case TOKEN_EQUAL:			return (ast_t) make_ast(0, token->loc_id, ++next, AST_EQUAL,		.equal_stmt		= {0});
	case TOKEN_GREATER:		return (ast_t) make_ast(0, token->loc_id, ++next, AST_GREATER,	.greater_stmt = {0});
	case TOKEN_LESS:			return (ast_t) make_ast(0, token->loc_id, ++next, AST_LESS,			.less_stmt		= {0});
	case TOKEN_SYSCALL:		return (ast_t) make_ast(0, token->loc_id, ++next, AST_SYSCALL,	.syscall			= {0});
	case TOKEN_SYSCALL1:	return (ast_t) make_ast(0, token->loc_id, ++next, AST_SYSCALL,	.syscall			= {1});
	case TOKEN_SYSCALL2:	return (ast_t) make_ast(0, token->loc_id, ++next, AST_SYSCALL,	.syscall			= {2});
	case TOKEN_SYSCALL3:	return (ast_t) make_ast(0, token->loc_id, ++next, AST_SYSCALL,	.syscall			= {3});
	case TOKEN_SYSCALL4:	return (ast_t) make_ast(0, token->loc_id, ++next, AST_SYSCALL,	.syscall			= {4});
	case TOKEN_SYSCALL5:	return (ast_t) make_ast(0, token->loc_id, ++next, AST_SYSCALL,	.syscall			= {5});
	case TOKEN_SYSCALL6:	return (ast_t) make_ast(0, token->loc_id, ++next, AST_SYSCALL,	.syscall			= {6});
	case TOKEN_BOR:				return (ast_t) make_ast(0, token->loc_id, ++next, AST_BOR,			.bor_stmt			= {0});
	case TOKEN_MUL:				return (ast_t) make_ast(0, token->loc_id, ++next, AST_MUL,			.mul_stmt			= {0});
	case TOKEN_DIV:				return (ast_t) make_ast(0, token->loc_id, ++next, AST_DIV,			.div_stmt			= {0});
	case TOKEN_MINUS:			return (ast_t) make_ast(0, token->loc_id, ++next, AST_MINUS,		.minus_stmt		= {0});
	case TOKEN_PLUS:			return (ast_t) make_ast(0, token->loc_id, ++next, AST_PLUS,			.plus_stmt		= {0});
	case TOKEN_MOD:				return (ast_t) make_ast(0, token->loc_id, ++next, AST_MOD,			.mod_stmt			= {0});
	case TOKEN_DOT:				return (ast_t) make_ast(0, token->loc_id, ++next, AST_DOT,			.dot_stmt			= {0});
	}
	UNREACHABLE;
}
