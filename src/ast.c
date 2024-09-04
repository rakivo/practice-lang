#include "ast.h"
#include "common.h"
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
	case VALUE_KIND_FUNCTION_POINTER: return "funcptr";		break;
	case VALUE_KIND_POISONED:					return "poisoned";	break;
	case VALUE_KIND_STRING:						return "str";				break;
	case VALUE_KIND_INTEGER:					return "int";				break;
	case VALUE_KIND_LAST:							return "last";			break;
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
		FOREACH(arg_t, proc_arg, ast->func_stmt.args) {
			printf("arg: %s\n", arg_to_str(&proc_arg));
		}
		printf("body: %d\n", ast->func_stmt.body);
		printf("ret_type: %s\n", value_kind_to_str_pretty(ast->func_stmt.ret_type));
	} break;

	case AST_PROC: {
		printf("%s\n", ast_kind_to_str(ast->ast_kind));
		printf("name: %s\n", ast->proc_stmt.name->str);
		FOREACH(arg_t, arg, ast->proc_stmt.args) {
			printf("arg: %s\n", arg_to_str(&arg));
		}
		printf("body: %d\n", ast->proc_stmt.body);
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

	case AST_DUP:			 printf("%s\n", ast_kind_to_str(ast->ast_kind)); break;
	case AST_SYSCALL:	 printf("%s\n", ast_kind_to_str(ast->ast_kind)); break;
	case AST_DOT:      printf("%s\n", ast_kind_to_str(ast->ast_kind)); break;
	case AST_DROP:     printf("%s\n", ast_kind_to_str(ast->ast_kind)); break;
	case AST_PLUS:     printf("%s\n", ast_kind_to_str(ast->ast_kind)); break;
	case AST_MOD:      printf("%s\n", ast_kind_to_str(ast->ast_kind)); break;
	case AST_MINUS:    printf("%s\n", ast_kind_to_str(ast->ast_kind)); break;
	case AST_DIV:      printf("%s\n", ast_kind_to_str(ast->ast_kind)); break;
	case AST_MUL:      printf("%s\n", ast_kind_to_str(ast->ast_kind)); break;
	case AST_EQUAL:    printf("%s\n", ast_kind_to_str(ast->ast_kind)); break;
	case AST_LESS:     printf("%s\n", ast_kind_to_str(ast->ast_kind)); break;
	case AST_CALL:     printf("%s\n", ast_kind_to_str(ast->ast_kind)); break;
	case AST_BOR:      printf("%s\n", ast_kind_to_str(ast->ast_kind)); break;
	case AST_GREATER:  printf("%s\n", ast_kind_to_str(ast->ast_kind)); break;
	case AST_POISONED: printf("%s\n", ast_kind_to_str(ast->ast_kind)); break;
	}
}

const char *
ast_kind_to_str(const ast_kind_t ast_kind)
{
	switch (ast_kind) {
	case AST_CALL:			return "AST_CALL";			break;
	case AST_BOR:				return "AST_BOR";				break;
	case AST_MOD:				return "AST_MOD";				break;
	case AST_SYSCALL:		return "AST_SYSCALL";		break;
	case AST_FUNC:			return "AST_FUNC";			break;
	case AST_DUP:				return "AST_DUP";				break;
	case AST_LITERAL:		return "AST_LITERAL";		break;
	case AST_PROC:			return "AST_PROC";			break;
	case AST_DROP:			return "AST_DROP";			break;
	case AST_IF:				return "AST_IF";				break;
	case AST_POISONED:	return "AST_POISONED";	break;
	case AST_WHILE:			return "AST_WHILE";			break;
	case AST_PUSH:			return "AST_PUSH";			break;
	case AST_PLUS:			return "AST_PLUS";			break;
	case AST_DIV:				return "AST_DIV";				break;
	case AST_MUL:				return "AST_MUL";				break;
	case AST_MINUS:			return "AST_MINUS";			break;
	case AST_DOT:				return "AST_DOT";				break;
	case AST_EQUAL:			return "AST_EQUAL";			break;
	case AST_LESS:			return "AST_LESS";			break;
	case AST_GREATER:		return "AST_GREATER";		break;
	case AST_CONST:			return "AST_CONST";		break;
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
	}

	if (ast.next < 0 || ast.ast_kind == AST_POISONED) return;
	main_function_check(at_top_level, astid(ast.next));
}
