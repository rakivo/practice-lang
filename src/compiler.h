#ifndef COMPILER_H_
#define COMPILER_H_

#include "ast.h"

typedef struct {
	ast_id_t ast_cur;
	const char *output_file_path;
} Compiler;

Compiler
new_compiler(ast_id_t ast_cur);

void
compiler_compile(Compiler *compiler);

#endif // COMPILER_H_
