#ifndef CONSTEVAL_H_
#define CONSTEVAL_H_

#include "ast.h"
#include "common.h"
#include "compiler.h"

#define CONSTEVAL_SIMULATION_STACK_CAP 1024

typedef struct {
	consteval_value_t stack[CONSTEVAL_SIMULATION_STACK_CAP];
	size_t stack_size;

	const_map_t **const_map;
} Consteval;

Consteval
new_consteval(const_map_t **const_map);

consteval_value_t
consteval_eval(Consteval *consteval, const ast_t *const_ast, bool is_var);

#endif // CONSTEVAL_H_
