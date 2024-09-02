#ifndef COMPILER_H_
#define COMPILER_H_

#include "ast.h"

#define MAX_STACK_TYPES_CAP 1024
#define PROC_CTX_MAX_STACK_TYPES_CAP 512
#define FUNC_CTX_MAX_STACK_TYPES_CAP 512

typedef struct {
	const proc_stmt_t *stmt;

	bool called_funcptr;

	size_t stack_size;
	value_kind_t stack_types[PROC_CTX_MAX_STACK_TYPES_CAP];
} proc_ctx_t;

typedef struct {
	const func_stmt_t *stmt;

	bool called_funcptr;

	size_t stack_size;
	value_kind_t stack_types[FUNC_CTX_MAX_STACK_TYPES_CAP];
} func_ctx_t;

typedef struct {
	bool is_used;
	ast_id_t ast_id;
	ast_kind_t ast_kind;
} value_t;

typedef struct {
	ast_id_t ast_cur;
	const char *output_file_path;

	struct {
		const char *key;
		value_t value;
	} *values_map;

	proc_ctx_t proc_ctx;
	func_ctx_t func_ctx;
} Compiler;

Compiler
new_compiler(ast_id_t ast_cur);

void
compiler_compile(Compiler *compiler);

#endif // COMPILER_H_
