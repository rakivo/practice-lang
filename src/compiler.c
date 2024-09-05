#include "lib.h"
#include "ast.h"
#include "common.h"
#include "compiler.h"

#include "nob.h"
#include "stb_ds.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#undef report_error
#define report_error(fmt, ...) do { \
	report_error_noexit(__func__, __FILE__, __LINE__, fmt, __VA_ARGS__); \
	compiler_emergency_clean(); \
	exit(EXIT_FAILURE); \
} while (0)

#define TAB "\t"

#define wln(...) fprintf(stream, __VA_ARGS__ "\n")
#define wtln(...) wln(TAB __VA_ARGS__)
#define wprintln(fmt, ...) fprintf(stream, fmt "\n", __VA_ARGS__)
#define wtprintln(fmt, ...) wprintln(TAB fmt, __VA_ARGS__)

#ifndef DEBUG
	#define FASM
#endif

#define WORD_SIZE 8

#define OBJECT_OUTPUT "out.o"
#define X86_64_OUTPUT "out.asm"
#define EXECUTABLE_OUTPUT "out"

#define ASM_OUTPUT_FLAGS "-o", OBJECT_OUTPUT
#define LD_OUTPUT_FLAGS "-o", EXECUTABLE_OUTPUT

#define PATH_TO_LD_EXECUTABLE "/usr/bin/ld"

#ifdef FASM
	#define DEFINE "define"
	#define GLOBAL "public"
	#define ASM_FLAGS
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

static FILE *stream = NULL;

static size_t label_counter = 0;
static size_t while_label_counter = 0;
static size_t string_literal_counter = 0;

static size_t stack_size = 0;
static value_kind_t stack_types[MAX_STACK_TYPES_CAP];

static const char **strs = NULL;

static bool used_strlen = false;
static bool used_dmp_i64 = false;

static const char *X86_64_LINUX_CONVENTION_REGISTERS[6] = {
	"rdi", "rsi", "rdx", "r10", "r8", "r9"
};

typedef struct {
	bool is_used;
	ast_id_t ast_id;
	ast_kind_t ast_kind;
} value_t;

struct {
	const char *key;
	value_t value;
} *values_map = NULL;

static void
compile_ast(Compiler *ctx, const ast_t *ast);

static void
compiler_deinit(void);

static void
compiler_emergency_clean(void);

INLINE void
scratch_buffer_genstr(void)
{
	scratch_buffer_clear();
	scratch_buffer_printf("__str_%zu__", string_literal_counter);
}

INLINE void
scratch_buffer_genstrlen(void)
{
	scratch_buffer_clear();
	scratch_buffer_printf("__str_%zu_len__", string_literal_counter);
}

// Compile an ast till the `ast.next` is greater than or equal to `0`.
// Every non-empty block should be with an `ast.next` = -1 at the end.
INLINE ast_id_t
compile_block(Compiler *ctx, ast_t ast)
{
	while (ast.ast_id < asts_len) {
		compile_ast(ctx, &ast);
		if (ast.next < 0) break;
		else ast = astid(ast.next);
	}
	return ast.ast_id;
}

INLINE
void stack_add_type(Compiler *ctx, value_kind_t type)
{
	if (ctx->proc_ctx.stmt != NULL) {
		if (unlikely(ctx->proc_ctx.stack_size + 1 >= MAX_STACK_TYPES_CAP)) {
			eprintf("EMERGENCY CRASH, STACK IS TOO BIG\n");
			exit(1);
		}
		ctx->proc_ctx.stack_types[ctx->proc_ctx.stack_size++] = type;
	} else if (ctx->func_ctx.stmt != NULL) {
		if (unlikely(ctx->func_ctx.stack_size + 1 >= MAX_STACK_TYPES_CAP)) {
			eprintf("EMERGENCY CRASH, STACK IS TOO BIG\n");
			exit(1);
		}
		ctx->func_ctx.stack_types[ctx->func_ctx.stack_size++] = type;
	} else {
		if (unlikely(stack_size + 1 >= MAX_STACK_TYPES_CAP)) {
			eprintf("EMERGENCY CRASH, STACK IS TOO BIG\n");
			exit(1);
		}
		stack_types[stack_size++] = type;
	}
}

INLINE
void stack_pop(Compiler *ctx)
{
	if (ctx->proc_ctx.stmt != NULL)
		ctx->proc_ctx.stack_size--;
	else if (ctx->func_ctx.stmt != NULL)
		ctx->func_ctx.stack_size--;
	else
		stack_size--;
}

INLINE const value_kind_t *
stack_at(const Compiler *ctx, size_t idx)
{
	if (ctx->proc_ctx.stmt != NULL) return &ctx->proc_ctx.stack_types[idx];
	else if (ctx->func_ctx.stmt != NULL) return &ctx->func_ctx.stack_types[idx];
	else return &stack_types[idx];
}

UNUSED INLINE value_kind_t *
stack_at_mut(Compiler *ctx, size_t idx)
{
	if (ctx->proc_ctx.stmt != NULL) return &ctx->proc_ctx.stack_types[idx];
	else if (ctx->func_ctx.stmt != NULL) return &ctx->func_ctx.stack_types[idx];
	else return &stack_types[idx];
}

INLINE size_t
get_stack_size(const Compiler *ctx)
{
	if (ctx->proc_ctx.stmt != NULL) return ctx->proc_ctx.stack_size;
	else if (ctx->func_ctx.stmt != NULL) return ctx->func_ctx.stack_size;
	else return stack_size;
}

void
stack_dump(const Compiler *ctx)
{
	const size_t size = get_stack_size(ctx);
	for (int i = size - 1, j = 0; i > 0; --i, ++j) {
		printf("  %d: %s\n", j, value_kind_to_str_pretty(*stack_at(ctx, i)));
	}
}

INLINE const value_kind_t *
get_type_from_end(const Compiler *ctx, size_t idx)
{
	const i32 idx_ = get_stack_size(ctx) - idx - 1;
	if (idx_ < 0) return NULL;
	return stack_at(ctx, idx_);
}

// Check the stack size and types of values on the stack before performing a binop.
INLINE void
check_for_two_integers_on_the_stack(const Compiler *ctx, const char *op, const ast_t *ast)
{
	const value_kind_t *first_type = get_type_from_end(ctx, 1);
	const value_kind_t *second_type = get_type_from_end(ctx, 0);

	if (second_type == NULL || first_type == NULL) {
		report_error("%s error: `%s` stack underflow, bruv", loc_to_str(&locid(ast->loc_id)), op);
	} else if (*first_type  != VALUE_KIND_INTEGER
				 ||  *second_type != VALUE_KIND_INTEGER)
	{
		report_error("%s error: expected two integers on the stack, but got: %s and %s",
								 loc_to_str(&locid(ast->loc_id)),
								 value_kind_to_str_pretty(*second_type),
								 value_kind_to_str_pretty(*first_type));
	}
}

INLINE void
check_for_integer_on_the_stack(const Compiler *ctx, const char *op, const char *msg, const ast_t *ast)
{
	const value_kind_t *type = get_type_from_end(ctx, 0);

	if (type == NULL) {
		report_error("%s error: `%s` with an empty stack", loc_to_str(&locid(ast->loc_id)), op);
	} else if (*type != VALUE_KIND_INTEGER) {
		report_error("%s error: %s, but got: %s",
								 loc_to_str(&locid(ast->loc_id)),
								 msg,
								 value_kind_to_str_pretty(*type));
	}
}

INLINE value_kind_t
check_stack_for_last(const Compiler *ctx, const char *op, const ast_t *ast)
{
	const value_kind_t *type = get_type_from_end(ctx, 0);
	if (type == NULL) report_error("%s error: `%s` with an empty stack", loc_to_str(&locid(ast->loc_id)), op);
	return *type;
}

// Perform binary operation with rax, rbx
#define print_binop(...) do { \
	wtln("pop rbx"); \
	wtln("mov rax, qword [rsp]"); \
	wtln(__VA_ARGS__); \
	wtln("mov [rsp], rax"); \
} while (0)

static size_t arg_idx = 0;

static const arg_t *
check_for_arg(Compiler *ctx, const char *str)
{
	arg_idx = 0;
	arg_t *arg = NULL;

	if (ctx->proc_ctx.stmt != NULL) {
		FOREACH_IDX(idx, arg_t, arg_, ctx->proc_ctx.stmt->args) {
			if (0 == strcmp(arg_.name, str)) {
				arg = &arg_;
				arg_idx = idx;
				break;
			}
		}
	} else if (ctx->func_ctx.stmt != NULL) {
		FOREACH_IDX(idx, arg_t, arg_, ctx->func_ctx.stmt->args) {
			if (0 == strcmp(arg_.name, str)) {
				arg = &arg_;
				arg_idx = idx;
				break;
			}
		}
	}

	return arg;
}

static void
compile_ast(Compiler *ctx, const ast_t *ast)
{
#ifdef DEBUG
	if (ast->ast_kind != AST_PROC && ast->ast_kind != AST_FUNC) {
		wprintln("; -- %s --", ast_kind_to_str(ast->ast_kind));

#ifdef PRINT_STACK
		printf("%s:\n", ast_kind_to_str(ast->ast_kind));
		stack_dump(ctx);
#endif
	}
#endif

	switch (ast->ast_kind) {
	// These are handled in different place
	case AST_CONST:
	case AST_VAR:
	case AST_FUNC:
	case AST_PROC: {} break;

	case AST_IF: {
		check_for_integer_on_the_stack(ctx, "if",
																	 "expected last value on "
																	 "the stack to be integer",
																	 ast);

		wtln("pop rax");
		stack_pop(ctx);

		// If statement is empty
		if (ast->if_stmt.then_body < 0 && ast->if_stmt.else_body < 0) return;

		const size_t curr_label = label_counter++;

		wtln("test rax, rax");
		wtprintln("jz ._else_%zu", curr_label);

		ast_t if_ast = astid(ast->if_stmt.then_body);
		if (ast->if_stmt.then_body >= 0) {
			compile_block(ctx, if_ast);
			wtprintln("jmp ._edon_%zu", curr_label);
		}

		wprintln("._else_%zu:", curr_label);
		if (ast->if_stmt.else_body >= 0) {
			if_ast = astid(ast->if_stmt.else_body);
			compile_block(ctx, if_ast);
		}

		wprintln("._edon_%zu:", curr_label);
	} break;

	case AST_WRITE: {
		value_kind_t value_kind = check_stack_for_last(ctx, "write", ast);
		const char *str = ast->write_stmt.token->str + 1;
		const i32 var_idx = shgeti(ctx->var_map, str);
		if (var_idx == -1) {
			report_error("%s error: undefined symbol: `%s`",
									 loc_to_str(&locid(ast->loc_id)),
									 str);
		}

		const consteval_value_t value = ctx->var_map[var_idx].value;

		if (value_kind != value.kind) {
			report_error("%s error: write a value of type `%s` into the variable of type `%s`",
									 loc_to_str(&locid(ast->loc_id)),
									 value_kind_to_str_pretty(value_kind),
									 value_kind_to_str_pretty(value.kind));
		}

		wtln("pop rax");
		wtprintln("mov [%s], rax", str);
		stack_pop(ctx);
	} break;

	case AST_SYSCALL: {
		const size_t stack_size = get_stack_size(ctx);
		if (stack_size < ast->syscall.args_count + 1) {
			report_error("%s error: too few arguments to call: `syscall%d`",
									 loc_to_str(&locid(ast->loc_id)),
									 ast->syscall.args_count);
		}

		// Pop syscall number
		wtln("pop rax");

		// Pop all the shit in the reversed order
		for (u8 i = 0; i < ast->syscall.args_count; ++i) {
			wtprintln("pop %s", X86_64_LINUX_CONVENTION_REGISTERS[ast->syscall.args_count - 1 - i]);
		}

		wtln("syscall");
	} break;

	case AST_WHILE: {
		const size_t curr_label = while_label_counter++;

		wprintln("._while_%zu:", curr_label);

#ifdef DEBUG
		wln("; -- COND --");
#endif

		ast_t while_ast;
		ast_id_t last_ast_id_in_body;

		const size_t start_stack_size = get_stack_size(ctx);

		if (ast->while_stmt.cond < 0) goto compile_loop;

		while_ast = astid(ast->while_stmt.cond);
		if (ast->while_stmt.cond >= 0) {
			last_ast_id_in_body = compile_block(ctx, while_ast);
		}

#ifdef DEBUG
		wln("; -- COND END --");
#endif

		check_for_integer_on_the_stack(ctx, "while",
																	 "expected last value after performing `while` "
																	 "condition on the stack to be integer",
																	 ast);

		wtln("pop rax");
		stack_pop(ctx);
		wtln("test rax, rax");

		wtprintln("jz ._wdon_%zu", curr_label);

compile_loop:

		if (ast->while_stmt.body >= 0) {
			while_ast = astid(ast->while_stmt.body);
			last_ast_id_in_body = compile_block(ctx, while_ast);
		}

		const size_t end_stack_size = get_stack_size(ctx);

		/*
			We do check here only if:
				While statement is not in procedure OR while statement is in procedure/function, but you didn't call a function pointer in it,
				because, if you did, we won't be able to keep track of the stack.
		*/
		if (start_stack_size != end_stack_size
			&& !((ctx->proc_ctx.stmt == NULL || !ctx->proc_ctx.called_funcptr)
				|| (ctx->func_ctx.stmt == NULL || !ctx->func_ctx.called_funcptr)))
		{
			eprintf("%s error: The amount of elements at the start of the `while` statement "
							"should be equal to the amount of elements at the end of the statement\n",
							loc_to_str(&locid(ast->loc_id)));

			eprintf("  NOTE: expected size: %zu, but got: %zu. Perhaps, %s\n",
							start_stack_size,
							end_stack_size,
							end_stack_size > start_stack_size ?
							"you can drop some elements" : "you lost the counter"
			);

			report_error("%s end of the statement", loc_to_str(&locid(astid(last_ast_id_in_body).loc_id)));
		}

		wtprintln("jmp ._while_%zu", curr_label);
		wprintln("._wdon_%zu:", curr_label);
	} break;

	case AST_LITERAL: {
		const i32 value_idx = shgeti(values_map, ast->literal.str);
		const i32 const_idx = shgeti(ctx->const_map, ast->literal.str);
		const i32 var_idx   = shgeti(ctx->var_map, ast->literal.str);
		if (const_idx != -1) {
			const consteval_value_t value = ctx->const_map[const_idx].value;
			wtprintln("push 0x%lX", value.value);
			stack_add_type(ctx, value.kind);
		} else if (var_idx != -1) {
			wtprintln("push [%s]", ast->literal.str);
			stack_add_type(ctx, ctx->const_map[const_idx].value.kind);
		} else if (value_idx != -1) {
			const value_t value = values_map[value_idx].value;
			if (value.ast_kind == AST_PROC) {
				wtprintln("push __%s__", astid(value.ast_id).proc_stmt.name->str);
			} else if (value.ast_kind == AST_FUNC) {
				wtprintln("push __%s__", astid(value.ast_id).func_stmt.name->str);
			} else {
				UNREACHABLE;
			}

			const value_t value_ = {
				.ast_id = value.ast_id,
				.ast_kind = value.ast_kind,
				.is_used = true
			};
			shput(values_map, ast->call.str, value_);
			stack_add_type(ctx, VALUE_KIND_FUNCTION_POINTER);
		} else {
				if (ctx->proc_ctx.stmt != NULL || ctx->func_ctx.stmt != NULL) {
					const arg_t *arg = check_for_arg(ctx, ast->literal.str);
					if (arg == NULL) {
						report_error("%s error: undefined symbol: `%s`",
												 loc_to_str(&locid(ast->loc_id)),
												 ast->literal.str);
					}

					// Compute the index of the value from the end of the stack
					size_t stack_idx = 0;
					if (ctx->proc_ctx.stmt != NULL) {
						stack_idx = vec_size(ctx->proc_ctx.stmt->args) - arg_idx + ctx->proc_ctx.stack_size;
					} else {
						stack_idx = vec_size(ctx->func_ctx.stmt->args) - arg_idx + ctx->func_ctx.stack_size;
					}

					wtprintln("mov rax, [rsp + %zu]", stack_idx * WORD_SIZE);
					wtln("push rax");
					stack_add_type(ctx, arg->kind);
			} else {
				if (value_idx == -1) {
					report_error("%s error: undefined symbol: `%s`",
											 loc_to_str(&locid(ast->loc_id)),
											 ast->call.str);
				}
			}
		}
	} break;

	case AST_CALL: {
		const i32 value_idx = shgeti(values_map, ast->call.str);
		const i32 var_idx = shgeti(ctx->var_map, ast->call.str);
		if (value_idx != -1) {
			const size_t stack_size = get_stack_size(ctx);

			size_t args_count_required = 0;
			const value_t value = values_map[value_idx].value;
			const ast_t *decl_ast = &astid(values_map[value_idx].value.ast_id);
			if (value.ast_kind == AST_PROC) {
				args_count_required = vec_size(decl_ast->proc_stmt.args);
			} else if (value.ast_kind == AST_FUNC) {
				args_count_required = vec_size(decl_ast->func_stmt.args);
			} else {
				UNREACHABLE;
			}

			if (stack_size < args_count_required) {
				eprintf("%s error: stack underflow trying to call: `%s`\n",
								loc_to_str(&locid(ast->loc_id)),
								ast->call.str);

				report_error("  NOTE: expected amount of values on "
										 "the stack: %zu, the actual stack size: %zu",
										 args_count_required, stack_size);
			}

			for (size_t i = 0; i < args_count_required; ++i) {
				stack_pop(ctx);
			}

			const value_t value_ = {
				.ast_id = value.ast_id,
				.ast_kind = value.ast_kind,
				.is_used = true
			};

			if (value.ast_kind == AST_PROC) {
				wtprintln("call __%s__", decl_ast->proc_stmt.name->str);
			} else {
				wtprintln("call __%s__", decl_ast->func_stmt.name->str);
				stack_add_type(ctx, decl_ast->func_stmt.ret_type);
			}

			shput(values_map, ast->call.str, value_);
		} else if (var_idx != -1) {
			wtprintln("mov rax, [%s]", ctx->var_map[var_idx].key);
			wtln("push rax");
			stack_add_type(ctx, ctx->var_map[var_idx].value.kind);
		} else {
			if (ctx->proc_ctx.stmt != NULL || ctx->func_ctx.stmt != NULL) {
				const arg_t *arg = check_for_arg(ctx, ast->literal.str);
				if (arg == NULL) {
					report_error("%s error: undefined symbol: `%s`",
											 loc_to_str(&locid(ast->loc_id)),
											 ast->literal.str);
				}

				// Compute the index of the value from the end of the stack
				size_t stack_idx = 0;
				if (ctx->proc_ctx.stmt != NULL) {
					ctx->proc_ctx.called_funcptr = true;
					stack_idx = vec_size(ctx->proc_ctx.stmt->args) - arg_idx + ctx->proc_ctx.stack_size + 1;
				} else {
					ctx->func_ctx.called_funcptr = true;
					stack_idx = vec_size(ctx->func_ctx.stmt->args) - arg_idx + ctx->func_ctx.stack_size + 1;
				}

				wtprintln("call qword [rsp + %zu]", stack_idx * WORD_SIZE);
			} else {
				if (value_idx == -1) {
					report_error("%s error: undefined symbol: `%s`",
											 loc_to_str(&locid(ast->loc_id)),
											 ast->call.str);
				}
			}
		}
	} break;

	case AST_PUSH: {
		switch (ast->push_stmt.value_kind) {
		case VALUE_KIND_INTEGER: {
			wtprintln("mov rax, 0x%lX", ast->push_stmt.integer);
			wtln("push rax");
			stack_add_type(ctx, VALUE_KIND_INTEGER);
		} break;

		case VALUE_KIND_STRING: {
			scratch_buffer_genstr();
			wtprintln("mov rax, %s", scratch_buffer_to_string());
			wtln("push rax");
			stack_add_type(ctx, VALUE_KIND_STRING);
			string_literal_counter++;
			vec_add(strs, ast->push_stmt.str);
		} break;

		case VALUE_KIND_FUNCTION_POINTER:
		case VALUE_KIND_POISONED:
		case VALUE_KIND_LAST:	UNREACHABLE; break;
		}
	} break;

	case AST_PLUS: {
		check_for_two_integers_on_the_stack(ctx, "+", ast);
		print_binop("add rax, rbx");
		stack_pop(ctx);
	} break;

	case AST_BOR: {
		check_for_two_integers_on_the_stack(ctx, "|", ast);
		print_binop("or rax, rbx");
		stack_pop(ctx);
	} break;

	case AST_MINUS: {
		check_for_two_integers_on_the_stack(ctx, "-", ast);
		print_binop("sub rax, rbx");
		stack_pop(ctx);
	} break;

	case AST_DIV: {
		check_for_two_integers_on_the_stack(ctx, "/", ast);
		wtln("xor edx, edx");
		print_binop("div rbx");
		stack_pop(ctx);
	} break;

	case AST_MOD: {
		check_for_two_integers_on_the_stack(ctx, "%", ast);
		wtln("xor edx, edx");
		wtln("pop rbx");
		wtln("mov rax, qword [rsp]");
		wtln("div rbx");
		wtln("mov [rsp], rdx");
		stack_pop(ctx);
	} break;

	case AST_MUL: {
		check_for_two_integers_on_the_stack(ctx, "*", ast);
		wtln("xor edx, edx");
		print_binop("mul rbx");
		stack_pop(ctx);
	} break;

	case AST_EQUAL: {
		check_for_two_integers_on_the_stack(ctx, "=", ast);
		wtln("pop rax");
		wtln("mov rbx, qword [rsp]");
		wtln("cmp rax, rbx");
		wtln("sete al");
		wtln("movzx rax, al");
		wtln("mov [rsp], rax");
		stack_pop(ctx);
		*stack_at_mut(ctx, get_stack_size(ctx) - 1) = VALUE_KIND_INTEGER;
	} break;

	case AST_LESS: {
		check_for_two_integers_on_the_stack(ctx, "<", ast);
		wtln("pop rax");
		wtln("mov rbx, qword [rsp]");
		wtln("cmp rbx, rax");
		wtln("setb al");
		wtln("movzx rax, al");
		wtln("mov [rsp], rax");
		stack_pop(ctx);
		*stack_at_mut(ctx, get_stack_size(ctx) - 1) = VALUE_KIND_INTEGER;
	} break;

	case AST_GREATER: {
		check_for_two_integers_on_the_stack(ctx, ">", ast);
		wtln("pop rax");
		wtln("mov rbx, qword [rsp]");
		wtln("cmp rbx, rax");
		wtln("setg al");
		wtln("movzx rax, al");
		wtln("mov [rsp], rax");
		stack_pop(ctx);
		*stack_at_mut(ctx, get_stack_size(ctx) - 1) = VALUE_KIND_INTEGER;
	} break;

	case AST_DROP: {
		check_stack_for_last(ctx, "drop", ast);
		wtln("pop rax");
		stack_pop(ctx);
	} break;

	case AST_DUP: {
		value_kind_t last_type = check_stack_for_last(ctx, "dup", ast);
		wtln("mov rax, [rsp]");
		wtln("push rax");
		stack_add_type(ctx, last_type);
	} break;

	case AST_DOT: {
		value_kind_t last_type = check_stack_for_last(ctx, ".", ast);
		switch (last_type) {
		case VALUE_KIND_FUNCTION_POINTER:
		case VALUE_KIND_INTEGER: {
			wtln("mov rax, qword [rsp]");
			wtln("mov r14, 0x1"); // mov 1 to r14 to print newline
			wtln("call dmp_i64");
			used_dmp_i64 = true;
		} break;

		case VALUE_KIND_STRING: {
			wtln("mov rdi, [rsp]");
			wtln("call strlen");
			wtln("mov rdx, rax");
			wtln("mov rsi, rdi");
			wtln("mov rax, SYS_WRITE");
			wtln("mov rdi, SYS_STDOUT");
			wtln("syscall");
			used_strlen = true;
		} break;

		case VALUE_KIND_POISONED: UNREACHABLE; break;
		case VALUE_KIND_LAST:			UNREACHABLE; break;
		}
	} break;

	case AST_POISONED: UNREACHABLE;
	}
}

INLINE void
print_defines(void)
{
	wln(DEFINE " SYS_WRITE  1");
	wln(DEFINE " SYS_STDOUT 1");
	wln(DEFINE " WORD_SIZE  8");
	wln(DEFINE " SYS_EXIT   60");
}

static void
print_dmp_i64(void)
{
#ifdef DEBUG
	wln("; r14b: newline");
	wln("; rax: number to print");
#endif
	wln("dmp_i64:");
	wtln("enter   0, 0");
	wtln("sub     rsp, 64");
	wtln("mov     qword [rbp - 8],  rax");
	wtln("mov     dword [rbp - 36], 0");
	wtln("mov     dword [rbp - 40], 0");
	wtln("cmp     qword [rbp - 8],  0");
	wtln("jge     .LBB0_2");
	wtln("mov     dword [rbp - 40], 1");
	wtln("xor     eax, eax");
	wtln("sub     rax, qword [rbp - 8]");
	wtln("mov     qword [rbp - 8], rax");
	wln(".LBB0_2:");
	wtln("cmp     qword [rbp - 8], 0");
	wtln("jne     .LBB0_4");
	wtln("mov     eax, dword [rbp - 36]");
	wtln("mov     ecx, eax");
	wtln("add     ecx, 1");
	wtln("mov     dword [rbp - 36], ecx");
	wtln("cdqe");
	wtln("mov     byte [rbp + rax - 32], 48");
	wtln("jmp     .LBB0_8");
	wln(".LBB0_4:");
	wtln("jmp     .LBB0_5");
	wln(".LBB0_5:");
	wtln("cmp     qword [rbp - 8], 0");
	wtln("jle     .LBB0_7");
	wtln("mov     rax, qword [rbp - 8]");
	wtln("mov     ecx, 10");
	wtln("cqo");
	wtln("idiv    rcx");
	wtln("mov     eax, edx");
	wtln("mov     dword [rbp - 44], eax");
	wtln("mov     eax, dword [rbp - 44]");
	wtln("add     eax, 48");
	wtln("mov     cl, al");
	wtln("mov     eax, dword [rbp - 36]");
	wtln("mov     edx, eax");
	wtln("add     edx, 1");
	wtln("mov     dword [rbp - 36], edx");
	wtln("cdqe");
	wtln("mov     byte [rbp + rax - 32], cl");
	wtln("mov     rax, qword [rbp - 8]");
	wtln("mov     ecx, 10");
	wtln("cqo");
	wtln("idiv    rcx");
	wtln("mov     qword [rbp - 8], rax");
	wtln("jmp     .LBB0_5");
	wln(".LBB0_7:");
	wtln("jmp     .LBB0_8");
	wln(".LBB0_8:");
	wtln("cmp     dword [rbp - 40], 0");
	wtln("je      .LBB0_10");
	wtln("mov     eax, dword [rbp - 36]");
	wtln("mov     ecx, eax");
	wtln("add     ecx, 1");
	wtln("mov     dword [rbp - 36], ecx");
	wtln("cdqe");
	wtln("mov     byte [rbp + rax - 32], 45");
	wln(".LBB0_10:");
	wtln("movsxd  rax, dword [rbp - 36]");
	wtln("mov     byte [rbp + rax - 32], 0");
	wtln("mov     dword [rbp - 48], 0");
	wtln("mov     eax, dword [rbp - 36]");
	wtln("sub     eax, 1");
	wtln("mov     dword [rbp - 52], eax");
	wln(".LBB0_11:");
	wtln("mov     eax, dword [rbp - 48]");
	wtln("cmp     eax, dword [rbp - 52]");
	wtln("jge     .LBB0_13");
	wtln("movsxd  rax, dword [rbp - 48]");
	wtln("mov     al, byte [rbp + rax - 32]");
	wtln("mov     byte [rbp - 53], al");
	wtln("movsxd  rax, dword [rbp - 52]");
	wtln("mov     cl, byte [rbp + rax - 32]");
	wtln("movsxd  rax, dword [rbp - 48]");
	wtln("mov     byte [rbp + rax - 32], cl");
	wtln("mov     cl, byte [rbp - 53]");
	wtln("movsxd  rax, dword [rbp - 52]");
	wtln("mov     byte [rbp + rax - 32], cl");
	wtln("mov     eax, dword [rbp - 48]");
	wtln("add     eax, 1");
	wtln("mov     dword [rbp - 48], eax");
	wtln("mov     eax, dword [rbp - 52]");
	wtln("add     eax, -1");
	wtln("mov     dword [rbp - 52], eax");
	wtln("jmp     .LBB0_11");
	wln(".LBB0_13:");
	wtln("mov     eax, dword [rbp - 36]");
	wtln("mov     ecx, eax");
	wtln("add     ecx, 1");
	wtln("mov     dword [rbp - 36], ecx");
	wtln("cdqe");
	wtln("mov     byte [rbp + rax - 32], 10");
	wtln("lea     rsi, [rbp - 32]");
	wtln("movsxd  rdx, dword [rbp - 36]");
	wtln("test    r14b, r14b");
	wtln("jz      .not_newline");
	wtln("jmp     .write");
	wln(".not_newline:");
	wtln("dec rdx");
	wln(".write:");
	wtln("mov     eax, SYS_WRITE");
	wtln("mov     edi, SYS_STDOUT");
	wtln("syscall");
	wtln("add     rsp, 64");
	wtln("leave");
	wtln("ret");
}

static void
print_strlen(void)
{
	wln("strlen:");
	wtln("xor rax, rax");
	wln(".loop:");
	wtln("cmp byte [rdi + rax], 0");
	wtln("jz .done");
	wtln("inc rax");
	wtln("jmp .loop");
	wln(".done:");
	wtln("ret");
}

INLINE void
print_exit(void)
{
	wtln("mov rax, SYS_EXIT");
	wtln("mov rdi, [ret_code]");
	wtln("syscall");
}

INLINE void
print_data_section(void)
{
	wln(SECTION_BSS_WRITEABLE);
	wln("rsp_stack: " RESERVE_QUAD_WORD " 1024");
	wln(SECTION_DATA_WRITEABLE);
	wln("ret_code dq 0x0");
	wln("rsp_stack_ptr: dq rsp_stack");
}

Compiler
new_compiler(ast_id_t ast_cur,
						 const_map_t *const_map,
						 var_map_t *var_map)
{
	return (Compiler) {
		.ast_cur = ast_cur,
		.proc_ctx = {
			.stmt = NULL,
			.stack_size = 0,
			.called_funcptr = false
		},
		.var_map = var_map,
		.const_map = const_map,
	};
}

static void
compiler_deinit(void)
{
	shfree(values_map);
	fclose(stream);
}

static void
compiler_emergency_clean(void)
{
	compiler_deinit();
	main_deinit();
	remove(X86_64_OUTPUT);
}

static void
rsp_stack_mov_rsp(void)
{
	wtln("mov rax, qword [rsp_stack_ptr]");
	wtln("mov qword [rax], rsp");
	wtln("add qword [rsp_stack_ptr], WORD_SIZE");
}

static void
rsp_stack_mov_to_rsp(void)
{
	wtln("sub qword [rsp_stack_ptr], WORD_SIZE");
	wtln("mov rax, qword [rsp_stack_ptr]");
	wtln("mov rsp, qword [rax]");
}

static void
compile_proc(Compiler *ctx, const ast_t *ast)
{
#ifdef DEBUG
	FOREACH(arg_t, arg, ast->proc_stmt.args) {
		wprintln("; %s", arg_to_str(&arg));
	}
#endif

	wprintln("__%s__:", ast->proc_stmt.name->str);
	rsp_stack_mov_rsp();

	ctx->proc_ctx.stmt = &ast->proc_stmt;

	ast_t proc_ast = astid(ast->proc_stmt.body);
	if (ast->proc_stmt.body >= 0) {
		compile_block(ctx, proc_ast);
	}

	rsp_stack_mov_to_rsp();
	wtln("ret");

	ctx->proc_ctx.stmt = NULL;
	ctx->proc_ctx.stack_size = 0;
	ctx->proc_ctx.called_funcptr = false;
}

static void compile_func(Compiler *ctx, const ast_t *ast)
{
#ifdef DEBUG
	FOREACH(arg_t, arg, ast->func_stmt.args) {
		wprintln("; %s", arg_to_str(&arg));
	}
#endif

	wprintln("__%s__:", ast->func_stmt.name->str);
	rsp_stack_mov_rsp();

	ctx->func_ctx.stmt = &ast->func_stmt;

	ast_id_t last_ast_in_body = ast->ast_id;
	ast_t func_ast = astid(ast->func_stmt.body);
	if (ast->func_stmt.body >= 0) {
		last_ast_in_body = compile_block(ctx, func_ast);
	}

	if (ctx->func_ctx.stack_size < 1) {
		report_error("%s error: expected to have element on the stack "
								 "to perform implicit return from function, but don't have any",
								 loc_to_str(&locid(astid(last_ast_in_body).loc_id)));
	} else if (*get_type_from_end(ctx, 0) != ast->func_stmt.ret_type) {
		report_error("%s error: expected last element on the stack to be: `%s`, but got: `%s`",
								 loc_to_str(&locid(astid(last_ast_in_body).loc_id)),
								 value_kind_to_str_pretty(ast->func_stmt.ret_type),
								 value_kind_to_str_pretty(*get_type_from_end(ctx, 0)));
	}

	wtln("pop rdi");
	if (0 == strcmp(MAIN_FUNCTION, ctx->func_ctx.stmt->name->str)) {
		wtln("mov [ret_code], rdi");
	}

	rsp_stack_mov_to_rsp();
	wtln("pop rax");
	wtln("push rdi");
	wtln("push rax");
	wtln("ret");

	ctx->func_ctx.stmt = NULL;
	ctx->func_ctx.stack_size = 0;
	ctx->func_ctx.called_funcptr = false;
}

void
compiler_compile(Compiler *ctx)
{
	stream = fopen(X86_64_OUTPUT, "w");
	if (stream == NULL) {
		eprintf("error: Failed to open file: %s\n", X86_64_OUTPUT);
		exit(EXIT_FAILURE);
	}

	wln(FORMAT_64BIT);

	print_defines();
	wln(SECTION_TEXT_EXECUTABLE);

	ast_t ast = astid(0);
	while (ast.next && ast.next <= ASTS_SIZE) {
		const bool is_proc = ast.ast_kind == AST_PROC;
		const bool is_func = ast.ast_kind == AST_FUNC;
		if (is_proc || is_func) {
			const value_t value = {
				.ast_id = ast.ast_id,
				.ast_kind = ast.ast_kind
			};
			shput(values_map, is_proc ? ast.proc_stmt.name->str : ast.func_stmt.name->str, value);
		}
		ast = astid(ast.next);
	}

	ast = astid(ctx->ast_cur);
	compile_func(ctx, &ast);

	// TODO: Properly check if procedure/function is used or not
	// Compile only used procs/funcs
	for (ptrdiff_t i = 0; i < shlen(values_map); ++i) {
		const value_t value = values_map[i].value;
		if (value.ast_kind == AST_PROC // && value.is_used
		)
		{
			compile_proc(ctx, &astid(value.ast_id));
		} else if (value.ast_kind == AST_FUNC // && value.is_used
					 && 0 != strcmp(MAIN_FUNCTION, astid(value.ast_id).func_stmt.name->str))
		{
			compile_func(ctx, &astid(value.ast_id));
		}
	}

	wln(GLOBAL " _start");
	wln("_start:");

	wtln("call __" MAIN_FUNCTION "__");

	print_exit();

	if (used_dmp_i64) print_dmp_i64();
	if (used_strlen) print_strlen();

	print_data_section();

	for (ptrdiff_t i = 0; i < shlen(ctx->var_map); ++i) {
		wprintln("%s dq 0x%lX", ctx->var_map[i].key, ctx->var_map[i].value.value);
	}

	string_literal_counter = 0;
	FOREACH(const char *, str, strs) {
		scratch_buffer_clear();
		scratch_buffer_genstr();
		char *strdb = scratch_buffer_copy();
		size_t len = strlen(str);
		if (len > 2 && str[len - 3] == '\\' && str[len - 2] == 'n') {
			scratch_buffer_clear();
			scratch_buffer_append(str);

			scratch_buffer.str[len - 3] = '"';
			scratch_buffer.str[len - 2] = '\0';

			wprintln("%s db %s, 0xA, 0x0", strdb, scratch_buffer_to_string());
		} else {
			wprintln("%s db %s, 0x0", strdb, str);
		}

		scratch_buffer_genstrlen();
		wprintln("%s " COMPTIME_EQU " $ - %s", scratch_buffer_to_string(), strdb);
		string_literal_counter++;
	}

	compiler_deinit();

	Nob_Cmd cmd = {0};
	nob_cmd_append(&cmd, PATH_TO_ASM_EXECUTABLE, X86_64_OUTPUT, ASM_FLAGS, ASM_OUTPUT_FLAGS);
	nob_cmd_run_sync(cmd, false);

	cmd.count = 0;
	nob_cmd_append(&cmd, PATH_TO_LD_EXECUTABLE, OBJECT_OUTPUT, LD_OUTPUT_FLAGS);
	nob_cmd_run_sync(cmd, false);

	nob_cmd_free(cmd);
}

/* TODO:
	#3. Distinguish between compile-time strings and runtime strings, to reduce the amount of calls to strlen
	#4. Introduce let-binding notion
	#5. Introduce `elif` keyword
*/
