#ifndef COMPILER_H_
#define COMPILER_H_

#include "ast.h"
#include "common.h"

#define WORD_SIZE 8

#define OBJECT_OUTPUT "out.o"
#define X86_64_OUTPUT "out.asm"
#define EXECUTABLE_OUTPUT "out"

#define ASM_OUTPUT_FLAGS "-o", OBJECT_OUTPUT
#define LD_OUTPUT_FLAGS "-o", EXECUTABLE_OUTPUT

#define PATH_TO_LD_EXECUTABLE "/usr/bin/ld"

#ifndef DEBUG
	#define FASM
#endif

#ifdef FASM
	#define DEFINE "define"
	#define GLOBAL "public"
	#define ASM_FLAGS "-m", "524288", EXECUTABLE_OUTPUT".tmp"
	#define COMPTIME_EQU "="
	#define RESERVE_QUAD "rq"
	#define FORMAT_64BIT "format ELF64"
	#define RESERVE_QUAD_WORD "rq"
	#define SECTION_BSS_WRITEABLE "section '.bss' writeable"
	#define PATH_TO_ASM_EXECUTABLE "/usr/bin/fasm"
	#define SECTION_DATA_WRITEABLE "section '.data' writeable"
	#define SECTION_TEXT_EXECUTABLE "section '.text' executable"
#else
	#define DEFINE "%%define"
	#define GLOBAL "global"
	#define COMPTIME_EQU "equ"
	#define RESERVE_QUAD "resq"
	#define FORMAT_64BIT "BITS 64"
	#define RESERVE_QUAD_WORD "resq"
	#define SECTION_BSS_WRITEABLE "section .bss"
	#define ASM_FLAGS "-f", "elf64", "-g", "-F", "dwarf"
	#define PATH_TO_ASM_EXECUTABLE "/usr/bin/nasm"
	#define SECTION_DATA_WRITEABLE "section .data"
	#define SECTION_TEXT_EXECUTABLE "section .text"
#endif // FASM

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

// Consteval only for integers right now
typedef struct {
	i64 value;
	ast_id_t ast_id;
	value_kind_t kind;
} consteval_value_t;

typedef struct {
	const char *key;
	consteval_value_t value;
} const_map_t;

typedef struct {
	const char *key;
	consteval_value_t value;
} var_map_t;

typedef struct {
	ast_id_t ast_cur;
	proc_ctx_t proc_ctx;
	func_ctx_t func_ctx;

	var_map_t *var_map;
	const_map_t *const_map;
} Compiler;

Compiler
new_compiler(ast_id_t ast_cur, const_map_t *const_map, var_map_t *var_map);

void
compiler_compile(Compiler *compiler);

#endif // COMPILER_H_
