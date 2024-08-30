#include "ast.h"
#include "common.h"

DECLARE_STATIC(ast, AST);

void
print_ast(const ast_t *ast)
{
	printf("--------------\n");

	printf("file_id: %d\n", ast->file_id);
	printf("ast_id: %d\n", ast->ast_id);
	printf("next: %d\n", ast->next);
	printf("ast_kind: ");

	switch (ast->ast_kind) {
	case AST_PUSH: {
		printf("AST_PUSH\n");
		printf("value_kind: %s\n", ast->push_stmt.value_kind == VALUE_KIND_INTEGER ? "int" : "str");
		if (ast->push_stmt.value_kind == VALUE_KIND_INTEGER)
			printf("integer: %ld\n", ast->push_stmt.integer);
		else
			printf("str: %s\n", ast->push_stmt.str ? ast->push_stmt.str : "NULL");
	} break;

	case AST_IF: {
		printf("AST_IF\n");
		printf("then_body: %d\n", ast->if_stmt.then_body);
		printf("else_body: %d\n", ast->if_stmt.else_body);
	} break;

	case AST_DOT:      printf("AST_DOT\n");      break;
	case AST_PLUS:     printf("AST_PLUS\n");     break;
	case AST_EQUAL:    printf("AST_EQUAL\n");    break;
	case AST_POISONED: printf("AST_POISONED\n"); break;
	}
}

const char *
ast_kind_to_str(const ast_kind_t ast_kind)
{
	switch (ast_kind) {
	case AST_IF:       return "AST_IF";       break;
	case AST_POISONED: return "AST_POISONED"; break;
	case AST_PUSH:     return "AST_PUSH";     break;
	case AST_PLUS:     return "AST_PLUS";     break;
	case AST_DOT:      return "AST_DOT";      break;
	case AST_EQUAL:    return "AST_EQUAL";    break;
	}
}
