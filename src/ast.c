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

static void
print_func(const func_stmt_t *func_stmt)
{
	printf("name: %s\n", func_stmt->name->str);
	FOREACH(arg_t, func_arg, func_stmt->args) {
		printf("arg: %s\n", arg_to_str(&func_arg));
	}
	printf("body: %d\n", func_stmt->body);
	FOREACH(value_kind_t, func_ret_type, func_stmt->ret_types) {
		printf("ret_type: %s\n", value_kind_to_str_pretty(func_ret_type));
	}
	printf("inlin: %d\n", func_stmt->inlin);
}

static void
print_proc(const proc_stmt_t *proc_stmt)
{
	printf("name: %s\n", proc_stmt->name->str);
	FOREACH(arg_t, arg, proc_stmt->args) {
		printf("arg: %s\n", arg_to_str(&arg));
	}
	printf("body: %d\n", proc_stmt->body);
	printf("inlin: %d\n", proc_stmt->inlin);
}

void
print_ast(const ast_t *ast)
{
	printf("--------------\n");

	printf("ast_id: %d\n", ast->ast_id);
	printf("next: %d\n", ast->next);
	printf("ast_kind: %s\n", ast_kind_to_str(ast->ast_kind));

	switch (ast->ast_kind) {
	case AST_WRITE: {
		printf("token: %s\n", token_to_str(ast->write_stmt.token));
	} break;

	case AST_EXTERN: {
		printf("%s\n", extern_kind_to_str(ast->extern_decl.kind));
		switch (ast->extern_decl.kind) {
		case EXTERN_FUNC: print_func(&ast->extern_decl.func_stmt); break;
		case EXTERN_PROC: print_proc(&ast->extern_decl.proc_stmt); break;
		}
	} break;

	case AST_VAR: {
		printf("constexpr: %b\n", ast->var_stmt.constexpr);
		printf("body: %d\n", ast->var_stmt.body);
		printf("name: %s\n", ast->var_stmt.name->str);
	} break;

	case AST_CONST: {
		printf("constexpr: %b\n", ast->const_stmt.constexpr);
		printf("body: %d\n", ast->const_stmt.body);
		printf("name: %s\n", ast->const_stmt.name->str);
	} break;

	case AST_LITERAL: {
		printf("str: %s\n", ast->literal.str);
	} break;

	case AST_FUNC: {
		print_func(&ast->func_stmt);
	} break;

	case AST_PROC: {
		print_proc(&ast->proc_stmt);
	} break;

	case AST_PUSH: {
		printf("value_kind: %s\n", value_kind_to_str_pretty(ast->push_stmt.value_kind));
		if (ast->push_stmt.value_kind == VALUE_KIND_INTEGER) printf("integer: %ld\n", ast->push_stmt.integer);
		else printf("str: %s\n", ast->push_stmt.str ? ast->push_stmt.str : "NULL");
	} break;

	case AST_IF: {
		printf("then_body: %d\n", ast->if_stmt.then_body);
		printf("else_body: %d\n", ast->if_stmt.else_body);
	} break;

	case AST_WHILE: {
		printf("cond: %d\n", ast->while_stmt.cond);
		printf("body: %d\n", ast->while_stmt.body);
	} break;

	case AST_DUP:						break;
	case AST_SYSCALL:				break;
	case AST_DOT:						break;
	case AST_DROP:					break;
	case AST_PLUS:					break;
	case AST_MOD:						break;
	case AST_MINUS:					break;
	case AST_DIV:						break;
	case AST_MUL:						break;
	case AST_EQUAL:					break;
	case AST_LESS:					break;
	case AST_CALL:					break;
	case AST_BOR:						break;
	case AST_BNOT:					break;
	case AST_GREATER_EQUAL: break;
	case AST_LESS_EQUAL:		break;
	case AST_GREATER:				break;
	case AST_POISONED:			break;
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
	case AST_EXTERN:				return "extern";
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

const char *
ast_kind_to_str(const ast_kind_t ast_kind)
{
	switch (ast_kind) {
	case AST_CALL:					return "AST_CALL";
	case AST_EXTERN:				return "AST_EXTERN";
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
extern_kind_to_str(const extern_kind_t extern_kind)
{
	switch (extern_kind) {
	case EXTERN_FUNC: return "EXTERN_FUNC";
	case EXTERN_PROC: return "EXTERN_PROC";
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
		case AST_EXTERN: break;
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
