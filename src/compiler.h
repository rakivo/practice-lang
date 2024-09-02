#ifndef COMPILER_H_
#define COMPILER_H_

#include "ast.h"

#define MAX_STACK_TYPES_CAP 1024
#define PROC_CTX_MAX_STACK_TYPES_CAP 512

typedef struct {
	proc_stmt_t *stmt;
	size_t stack_size;
	bool called_funcptr;
	value_kind_t stack_types[PROC_CTX_MAX_STACK_TYPES_CAP];
} proc_ctx_t;

typedef struct {
	ast_id_t ast_cur;
	const char *output_file_path;

	struct {
		const char *key;
		ast_id_t value;
	} *values_map;

	proc_ctx_t proc_ctx;
} Compiler;

Compiler
new_compiler(ast_id_t ast_cur);

void
compiler_compile(Compiler *compiler);

#endif // COMPILER_H_
