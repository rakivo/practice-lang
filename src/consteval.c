#include "ast.h"
#include "lexer.h"
#include "common.h"
#include "consteval.h"

#include "stb_ds.h"

Consteval
new_consteval(const_map_t **const_map)
{
	return (Consteval) {
		.const_map = const_map,
		.stack_size = 0,
	};
}

typedef enum {
	PLUS,
	MINUS,
	DIV,
	MUL,
	LESS,
	GREATER,
	EQUAL
} op_t;

static i64
last_two_binop(Consteval *consteval, op_t op)
{
	consteval->stack_size--;
	const i64 a = consteval->stack[consteval->stack_size].value;
	const i64 b = consteval->stack[consteval->stack_size - 1].value;
	switch (op) {
	case PLUS:		return a + b;
	case MINUS:		return a - b;
	case DIV:			return a / b;
	case MUL:			return a * b;
	case GREATER: return b > a;
	case LESS:		return b < a;
	case EQUAL:		return b == a;
	}
}

static void
simulate_ast(Consteval *consteval, const ast_t *ast)
{
	switch (ast->ast_kind) {
	case AST_IF:
	case AST_DOT:
	case AST_FUNC:
	case AST_PROC:
	case AST_WHILE:
	case AST_CALL:
	case AST_CONST: {
		report_error("%s error: unexpected operation, "
								 "supported operations in constant evaluation:\n"
								 "    `dup`, `push`, `drop`, `*`, `/`, `-`, `+`, `<`, `>`, `=` or another constant literal",
								 loc_to_str(&locid(ast->loc_id)));
	} break;

	case AST_DUP: {
		if (consteval->stack_size < 1) {
			report_error("%s error: stack underflow bruv",
									 loc_to_str(&locid(ast->loc_id)));
		}

		consteval->stack[consteval->stack_size] = consteval->stack[consteval->stack_size - 1];
		consteval->stack_size++;
	} break;

	case AST_PUSH: {
		consteval_value_t value = {
			.value = 0,
			.kind = ast->push_stmt.value_kind
		};

		switch (ast->push_stmt.value_kind) {
		case VALUE_KIND_INTEGER: {
			value.value = ast->push_stmt.integer;
		} break;

		case VALUE_KIND_STRING: {
			value.value = (i64) ast->push_stmt.str;
		} break;

		case VALUE_KIND_FUNCTION_POINTER:
		case VALUE_KIND_POISONED:
		case VALUE_KIND_LAST: UNREACHABLE;
		}

		consteval->stack[consteval->stack_size++] = value;
	} break;

	case AST_MUL: {
		if (consteval->stack_size < 2) {
			report_error("%s error: stack underflow bruv",
									 loc_to_str(&locid(ast->loc_id)));
		}

		consteval->stack[consteval->stack_size - 1].value = last_two_binop(consteval, MUL);
	} break;

	case AST_DIV: {
		if (consteval->stack_size < 2) {
			report_error("%s error: stack underflow bruv",
									 loc_to_str(&locid(ast->loc_id)));
		}

		consteval->stack[consteval->stack_size - 1].value = last_two_binop(consteval, DIV);
	} break;

	case AST_MINUS: {
		if (consteval->stack_size < 2) {
			report_error("%s error: stack underflow bruv",
									 loc_to_str(&locid(ast->loc_id)));
		}

		consteval->stack[consteval->stack_size - 1].value = last_two_binop(consteval, MINUS);
	} break;

	case AST_PLUS: {
		if (consteval->stack_size < 2) {
			report_error("%s error: stack underflow bruv",
									 loc_to_str(&locid(ast->loc_id)));
		}

		consteval->stack[consteval->stack_size - 1].value = last_two_binop(consteval, PLUS);
	} break;

	case AST_LESS: {
		if (consteval->stack_size < 2) {
			report_error("%s error: stack underflow bruv",
									 loc_to_str(&locid(ast->loc_id)));
		}

		consteval->stack[consteval->stack_size - 1].value = last_two_binop(consteval, LESS);
	} break;

	case AST_EQUAL: {
		if (consteval->stack_size < 2) {
			report_error("%s error: stack underflow bruv",
									 loc_to_str(&locid(ast->loc_id)));
		}

		consteval->stack[consteval->stack_size - 1].value = last_two_binop(consteval, EQUAL);
	} break;

	case AST_DROP: {
		consteval->stack_size--;
	} break;

	case AST_GREATER: {
		if (consteval->stack_size < 2) {
			report_error("%s error: stack underflow bruv",
									 loc_to_str(&locid(ast->loc_id)));
		}

		consteval->stack[consteval->stack_size - 1].value = last_two_binop(consteval, GREATER);
	} break;

	case AST_LITERAL: {
		const i32 const_idx = shgeti(*consteval->const_map, ast->literal.str);
		if (const_idx == -1) {
			report_error("%s error: undefined literal: `%s`",
						 loc_to_str(&locid(ast->loc_id)),
						 ast->literal.str);
		}

		consteval->stack[consteval->stack_size++] = (consteval_value_t) {
			.value = (*consteval->const_map)[const_idx].value.value,
			.kind = (*consteval->const_map)[const_idx].value.kind,
		};
	} break;

	case AST_POISONED: UNREACHABLE break;
	}
}

consteval_value_t
consteval_eval(Consteval *consteval, const ast_t *const_ast)
{
	consteval->stack_size = 0;
	ast_t ast = astid(const_ast->const_stmt.body);
	while (ast.next && ast.next <= ASTS_SIZE) {
		simulate_ast(consteval, &ast);
		ast = astid(ast.next);
	}

	if (consteval->stack_size < 1) {
		report_error("%s error: stack underflow, constevaluator needs "
								 "the last value on the stack to set it to constant's name",
								 loc_to_str(&locid(ast.loc_id)));
	}

	return consteval->stack[consteval->stack_size - 1];
}
