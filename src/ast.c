#include "ast.h"
#include "common.h"
#include "lexer.h"
#include "lib.h"

#include <string.h>

DECLARE_STATIC(ast, AST);

ast_id_t main_function = -1;

i32
value_kind_try_from_str(const char *str)
{
	for (i32 i = (i32) VALUE_KIND_POISONED + 1; i < (i32) VALUE_KIND_LAST; ++i) {
		if (0 == strcmp(str, value_kind_to_str_pretty((value_kind_t) i))) {
			return i;
		}
	}

	return -1;
}

const char *
value_kind_to_str_pretty(value_kind_t kind)
{
	switch (kind) {
	case VALUE_KIND_BYTE:							return "byte";
	case VALUE_KIND_FUNCTION_POINTER: return "funcptr";
	case VALUE_KIND_POISONED:					return "poisoned";
	case VALUE_KIND_STRING:						return "str";
	case VALUE_KIND_INTEGER:					return "int";
	case VALUE_KIND_LAST:							return "last";
	}
}

const char *
arg_to_str(const arg_t *arg)
{
	const char *kind = value_kind_to_str_pretty(arg->kind);
	const size_t len = 0
		+ strlen(kind)					 // value_kind
		+ 1											 // space between
		+ strlen(arg->name) // name
		+ 1;										 // \0

	char *ret = calloc_string(len);
	snprintf(ret, len,
					 "%s %s",
					 kind,
					 arg->name);

	return ret;
}

void
print_ast(const ast_t *ast)
{
	printf("--------------\n");

	printf("file_id: %d\n", ast->file_id);
	printf("ast_id: %d\n", ast->ast_id);
	printf("next: %d\n", ast->next);
	printf("ast_kind: ");

	switch (ast->ast_kind) {
	case AST_WRITE: {
		printf("%s\n", ast_kind_to_str(ast->ast_kind));
		printf("token: %s\n", token_to_str(ast->write_stmt.token));
	} break;

	case AST_VAR: {
		printf("%s\n", ast_kind_to_str(ast->ast_kind));
		printf("constexpr: %b\n", ast->var_stmt.constexpr);
		printf("body: %d\n", ast->var_stmt.body);
		printf("name: %s\n", ast->var_stmt.name->str);
	} break;

	case AST_CONST: {
		printf("%s\n", ast_kind_to_str(ast->ast_kind));
		printf("constexpr: %b\n", ast->const_stmt.constexpr);
		printf("body: %d\n", ast->const_stmt.body);
		printf("name: %s\n", ast->const_stmt.name->str);
	} break;

	case AST_LITERAL: {
		printf("%s\n", ast_kind_to_str(ast->ast_kind));
		printf("str: %s\n", ast->literal.str);
	} break;

	case AST_FUNC: {
		printf("%s\n", ast_kind_to_str(ast->ast_kind));
		printf("name: %s\n", ast->func_stmt.name->str);
		FOREACH(arg_t, func_arg, ast->func_stmt.args) {
			printf("arg: %s\n", arg_to_str(&func_arg));
		}
		printf("body: %d\n", ast->func_stmt.body);
		FOREACH(value_kind_t, func_ret_type, ast->func_stmt.ret_types) {
			printf("ret_type: %s\n", value_kind_to_str_pretty(func_ret_type));
		}
		printf("inlin: %d\n", ast->proc_stmt.inlin);
	} break;

	case AST_PROC: {
		printf("%s\n", ast_kind_to_str(ast->ast_kind));
		printf("name: %s\n", ast->proc_stmt.name->str);
		FOREACH(arg_t, arg, ast->proc_stmt.args) {
			printf("arg: %s\n", arg_to_str(&arg));
		}
		printf("body: %d\n", ast->proc_stmt.body);
		printf("inlin: %d\n", ast->proc_stmt.inlin);
	} break;

	case AST_PUSH: {
		printf("%s\n", ast_kind_to_str(ast->ast_kind));
		printf("value_kind: %s\n", value_kind_to_str_pretty(ast->push_stmt.value_kind));
		if (ast->push_stmt.value_kind == VALUE_KIND_INTEGER) printf("integer: %ld\n", ast->push_stmt.integer);
		else printf("str: %s\n", ast->push_stmt.str ? ast->push_stmt.str : "NULL");
	} break;

	case AST_IF: {
		printf("%s\n", ast_kind_to_str(ast->ast_kind));
		printf("then_body: %d\n", ast->if_stmt.then_body);
		printf("else_body: %d\n", ast->if_stmt.else_body);
	} break;

	case AST_WHILE: {
		printf("%s\n", ast_kind_to_str(ast->ast_kind));
		printf("cond: %d\n", ast->while_stmt.cond);
		printf("body: %d\n", ast->while_stmt.body);
	} break;

	case AST_DUP:						printf("%s\n", ast_kind_to_str(ast->ast_kind)); break;
	case AST_SYSCALL:				printf("%s\n", ast_kind_to_str(ast->ast_kind)); break;
	case AST_DOT:						printf("%s\n", ast_kind_to_str(ast->ast_kind)); break;
	case AST_DROP:					printf("%s\n", ast_kind_to_str(ast->ast_kind)); break;
	case AST_PLUS:					printf("%s\n", ast_kind_to_str(ast->ast_kind)); break;
	case AST_MOD:						printf("%s\n", ast_kind_to_str(ast->ast_kind)); break;
	case AST_MINUS:					printf("%s\n", ast_kind_to_str(ast->ast_kind)); break;
	case AST_DIV:						printf("%s\n", ast_kind_to_str(ast->ast_kind)); break;
	case AST_MUL:						printf("%s\n", ast_kind_to_str(ast->ast_kind)); break;
	case AST_EQUAL:					printf("%s\n", ast_kind_to_str(ast->ast_kind)); break;
	case AST_LESS:					printf("%s\n", ast_kind_to_str(ast->ast_kind)); break;
	case AST_CALL:					printf("%s\n", ast_kind_to_str(ast->ast_kind)); break;
	case AST_BOR:						printf("%s\n", ast_kind_to_str(ast->ast_kind)); break;
	case AST_BNOT:					printf("%s\n", ast_kind_to_str(ast->ast_kind)); break;
	case AST_GREATER_EQUAL: printf("%s\n", ast_kind_to_str(ast->ast_kind)); break;
	case AST_LESS_EQUAL:		printf("%s\n", ast_kind_to_str(ast->ast_kind)); break;
	case AST_GREATER:				printf("%s\n", ast_kind_to_str(ast->ast_kind)); break;
	case AST_POISONED:			printf("%s\n", ast_kind_to_str(ast->ast_kind)); break;
	}
}

const char *
ast_kind_to_str(const ast_kind_t ast_kind)
{
	switch (ast_kind) {
	case AST_CALL:					return "AST_CALL";
	case AST_BNOT:					return "AST_BNOT";
	case AST_WRITE:					return "AST_WRITE";
	case AST_VAR:						return "AST_VAR";
	case AST_BOR:						return "AST_BOR";
	case AST_MOD:						return "AST_MOD";
	case AST_SYSCALL:				return "AST_SYSCALL";
	case AST_FUNC:					return "AST_FUNC";
	case AST_DUP:						return "AST_DUP";
	case AST_LITERAL:				return "AST_LITERAL";
	case AST_PROC:					return "AST_PROC";
	case AST_DROP:					return "AST_DROP";
	case AST_IF:						return "AST_IF";
	case AST_POISONED:			return "AST_POISONED";
	case AST_WHILE:					return "AST_WHILE";
	case AST_PUSH:					return "AST_PUSH";
	case AST_PLUS:					return "AST_PLUS";
	case AST_DIV:						return "AST_DIV";
	case AST_MUL:						return "AST_MUL";
	case AST_MINUS:					return "AST_MINUS";
	case AST_DOT:						return "AST_DOT";
	case AST_EQUAL:					return "AST_EQUAL";
	case AST_LESS:					return "AST_LESS";
	case AST_GREATER:				return "AST_GREATER";
	case AST_GREATER_EQUAL:	return "AST_GREATER_EQUAL";
	case AST_LESS_EQUAL:		return "AST_LESS_EQUAL";
	case AST_CONST:					return "AST_CONST";
	}
}

const char *
ast_kind_to_str_pretty(const ast_kind_t ast_kind)
{
	switch (ast_kind) {
	case AST_CALL:					return "call";
	case AST_BNOT:					return "bnot";
	case AST_WRITE:					return "!";
	case AST_VAR:						return "var";
	case AST_BOR:						return "bor";
	case AST_MOD:						return "%";
	case AST_SYSCALL:				return "syscall";
	case AST_FUNC:					return "func";
	case AST_DUP:						return "dup";
	case AST_LITERAL:				return "literal";
	case AST_PROC:					return "proc";
	case AST_DROP:					return "drop";
	case AST_IF:						return "if";
	case AST_POISONED:			return "poisoned";
	case AST_WHILE:					return "while";
	case AST_PUSH:					return "push";
	case AST_PLUS:					return "+";
	case AST_DIV:						return "/";
	case AST_MUL:						return "*";
	case AST_MINUS:					return "-";
	case AST_DOT:						return ".";
	case AST_EQUAL:					return "=";
	case AST_LESS:					return "<";
	case AST_GREATER:				return ">";
	case AST_GREATER_EQUAL:	return ">=";
	case AST_LESS_EQUAL:		return "<=";
	case AST_CONST:					return "const";
	}
}

void
main_function_check(bool at_top_level, ast_t ast)
{
	if (ast.ast_kind == AST_FUNC
	&& 0 == strcmp(MAIN_FUNCTION, ast.func_stmt.name->str))
	{
		main_function = ast.ast_id;
		return;
	} else {
		switch (ast.ast_kind) {
		case AST_FUNC: break;
		case AST_PROC: break;
		case AST_VAR: break;
		case AST_CONST: break;

		case AST_POISONED:
		case AST_IF:
		case AST_WHILE:
		case AST_DOT:
		case AST_DUP:
		case AST_BNOT:
		case AST_BOR:
		case AST_MOD:
		case AST_PUSH:
		case AST_MUL:
		case AST_DIV:
		case AST_MINUS:
		case AST_PLUS:
		case AST_LESS:
		case AST_EQUAL:
		case AST_CALL:
		case AST_WRITE:
		case AST_DROP:
		case AST_GREATER:
		case AST_GREATER_EQUAL:
		case AST_LESS_EQUAL:
		case AST_SYSCALL:
		case AST_LITERAL: {
			report_error("%s error: no %ss are allowed at the top level",
									 loc_to_str(&locid(ast.loc_id)),
									 ast_kind_to_str_pretty(ast.ast_kind));
		} break;
		}
	}

	if (ast.next < 0 || ast.ast_kind == AST_POISONED) return;
	main_function_check(at_top_level, astid(ast.next));
}
