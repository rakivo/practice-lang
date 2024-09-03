#ifndef CONSTEVAL_H_
#define CONSTEVAL_H_

#include "ast.h"
#include "common.h"

#define CONSTEVAL_SIMULATION_STACK_CAP 1024

typedef struct {
	i64 value;
	value_kind_t kind;
} consteval_value_t;

typedef struct {
	consteval_value_t stack[CONSTEVAL_SIMULATION_STACK_CAP];

	size_t stack_size;
} Consteval;

Consteval
new_consteval(void);

// Only for integers right now
consteval_value_t
consteval_eval(Consteval *consteval, const ast_t *const_ast);

#endif // CONSTEVAL_H_
