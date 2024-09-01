#ifndef AST_H_
#define AST_H_

#include "file.h"
#include "lexer.h"
#include "common.h"

#include <stdio.h>

#define astid(id) (ASTS[id])
#define asts_len (ASTS_SIZE)
#define last_ast (ASTS[ASTS_SIZE - 1])
#define append_ast(ast_) (ASTS[ast_.ast_id] = ast_)

// Last argument represents stmt
#define make_ast(file_id_, loc_id_, next_, ast_kind_, ...) {	\
	.file_id = file_id_, \
	.loc_id = loc_id_, \
	.ast_id = ASTS_SIZE++, \
	.next = next_, \
	.ast_kind = ast_kind_, \
	__VA_ARGS__ \
}

typedef i16 ast_id_t;

typedef enum {
	// Reserved first variant
	VALUE_KIND_POISONED,

	VALUE_KIND_INTEGER,
	VALUE_KIND_STRING,
	VALUE_KIND_FUNCTION_POINTER,

	// Reserved last variant
	VALUE_KIND_LAST,
} value_kind_t;

i32
value_kind_try_from_str(const char *str);

const char *
value_kind_to_str_pretty(value_kind_t kind);

typedef struct {} plus_stmt_t;
typedef struct {} minus_stmt_t;
typedef struct {} div_stmt_t;
typedef struct {} mul_stmt_t;
typedef struct {} dot_stmt_t;
typedef struct {} less_stmt_t;
typedef struct {} greater_stmt_t;
typedef struct {} drop_stmt_t;
typedef struct {} dup_stmt_t;
typedef struct {} equal_stmt_t;

typedef struct {
	value_kind_t kind;
	loc_id_t loc_id;
	const char *name;
} proc_arg_t;

const char *
proc_arg_to_str(const proc_arg_t *proc_arg);

typedef struct {
	token_t name;
	proc_arg_t *args;
	ast_id_t body;
} proc_stmt_t;

typedef struct {
	const char *str;
} literal_t;

typedef struct {
	const char *str;
} call_t;

typedef struct {
	value_kind_t value_kind;

	union {
		i64 integer;
		const char *str;
	};
} push_stmt_t;

typedef struct {
	ast_id_t then_body;
	ast_id_t else_body;
} if_stmt_t;

typedef struct {
	ast_id_t cond;
	ast_id_t body;
} while_stmt_t;

typedef enum {
	AST_POISONED,
	AST_IF,
	AST_PROC,
	AST_WHILE,
	AST_DOT,
	AST_DUP,
	AST_PUSH,
	AST_MUL,
	AST_DIV,
	AST_MINUS,
	AST_PLUS,
	AST_LESS,
	AST_EQUAL,
	AST_CALL,
	AST_DROP,
	AST_GREATER,
	AST_LITERAL,
} ast_kind_t;

typedef struct {
	file_id_t file_id;
	loc_id_t loc_id;
	ast_id_t ast_id;
	ast_id_t next;
	ast_kind_t ast_kind;

	union {
		if_stmt_t if_stmt;
		dot_stmt_t dot_stmt;
		plus_stmt_t plus_stmt;
		push_stmt_t push_stmt;
		minus_stmt_t minus_stmt;
		div_stmt_t div_stmt;
		mul_stmt_t mul_stmt;
		less_stmt_t less_stmt;
		equal_stmt_t equal_stmt;
		greater_stmt_t greater_stmt;
		while_stmt_t while_stmt;
		drop_stmt_t drop_stmt;
		dup_stmt_t dup_stmt;
		proc_stmt_t proc_stmt;
		call_t call;
		literal_t literal;
	};
} ast_t;

#define ASTS_CAP 1024
extern ast_id_t ASTS_SIZE;
extern ast_t ASTS[ASTS_CAP];

const char *
ast_kind_to_str(const ast_kind_t ast_kind);

void
print_ast(const ast_t *ast);

#endif // AST_H_
