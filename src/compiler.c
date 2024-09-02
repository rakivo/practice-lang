#include "lib.h"
#include "ast.h"
#include "common.h"
#include "compiler.h"

#define STB_DS_IMPLEMENTATION
#include "stb_ds.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/*
	In r15 you always have an updated pointer to the top of the stack.
*/

#define TAB "  "

#define wln(...) fprintf(stream, __VA_ARGS__ "\n")
#define wtln(...) wln(TAB __VA_ARGS__)
#define wprintln(fmt, ...) fprintf(stream, fmt "\n", __VA_ARGS__)
#define wtprintln(fmt, ...) fprintf(stream, TAB fmt "\n", __VA_ARGS__)

#ifndef DEBUG
	#define FASM
#endif

#ifdef FASM
	#define DEFINE "define"
	#define GLOBAL "public"
	#define COMPTIME_EQU "="
	#define RESERVE_QUAD "rq"
	#define FORMAT_64BIT "format ELF64"
	#define SECTION_BSS_WRITEABLE "section '.bss' writeable"
	#define SECTION_DATA_WRITEABLE "section '.data' writeable"
	#define SECTION_TEXT_EXECUTABLE "section '.text' executable"
#else
	#define DEFINE "%%define"
	#define GLOBAL "global"
	#define COMPTIME_EQU "equ"
	#define RESERVE_QUAD "resq"
	#define FORMAT_64BIT "BITS 64"
	#define SECTION_BSS_WRITEABLE "section .bss"
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

static void
compile_ast(Compiler *ctx, const ast_t *ast);

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
INLINE void
compile_block(Compiler *ctx, ast_t ast)
{
	while (ast.ast_id < asts_len) {
		compile_ast(ctx, &ast);
		if (ast.next < 0) break;
		else ast = astid(ast.next);
	}
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
	else
		stack_size--;
}

INLINE const value_kind_t *
stack_at(const Compiler *ctx, size_t idx)
{
	return ctx->proc_ctx.stmt == NULL ? &stack_types[idx] : &ctx->proc_ctx.stack_types[idx];
}

UNUSED INLINE value_kind_t *
stack_at_mut(Compiler *ctx, size_t idx)
{
	return ctx->proc_ctx.stmt == NULL ? &stack_types[idx] : &ctx->proc_ctx.stack_types[idx];
}

INLINE size_t
get_stack_size(const Compiler *ctx)
{
	return ctx->proc_ctx.stmt == NULL ? stack_size : ctx->proc_ctx.stack_size;
}

INLINE const value_kind_t *
get_type_from_end(const Compiler *ctx, size_t idx)
{
	const i32 size = get_stack_size(ctx) - idx - 1;
	if (size < 0) return NULL;
	return stack_at(ctx, size);
}

// Check the stack size and types of values on the stack before performing a binop.
INLINE void
check_for_two_integers_on_the_stack(const Compiler *ctx, const char *op, const ast_t *ast)
{
	const value_kind_t *first_type = get_type_from_end(ctx, 1);
	const value_kind_t *second_type = get_type_from_end(ctx, 0);

	if (second_type == NULL || first_type == NULL) {
		report_error("%s error: `%s` stack underflow, bruv", loc_to_str(&locid(ast->loc_id)), op);
	} else if (*first_type != VALUE_KIND_INTEGER
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
#define print_binop(...) \
	wtln("pop rbx"); \
	wtln("mov rax, qword [rsp]"); \
	wtln(__VA_ARGS__); \
	wtln("mov [rsp], rax");

static void
compile_ast(Compiler *ctx, const ast_t *ast)
{
#ifdef DEBUG
	if (ast->ast_kind != AST_PROC) {
		wprintln("; -- %s --", ast_kind_to_str(ast->ast_kind));
	}
#endif

	switch (ast->ast_kind) {
	case AST_PROC: {} break;

	case AST_IF: {
		check_for_integer_on_the_stack(ctx, "if",
																	 "expected last value on "
																	 "the stack to be integer",
																	 ast);

		// If statement is empty
		if (ast->if_stmt.then_body < 0 && ast->if_stmt.else_body < 0) return;

		const size_t curr_label = label_counter++;

		wtln("pop rax");
		wtln("test rax, rax");
		wtprintln("jz ._else_%zu", curr_label);
		stack_pop(ctx);
		if (ctx->proc_ctx.stmt != NULL) ctx->proc_ctx.stack_size--;

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

	case AST_WHILE: {
		const size_t curr_label = while_label_counter++;

		wprintln("._while_%zu:", curr_label);

#ifdef DEBUG
		wln("; -- COND --");
#endif

		ast_t while_ast;

		if (ast->while_stmt.cond < 0) goto compile_loop;

		while_ast = astid(ast->while_stmt.cond);
		if (ast->while_stmt.cond >= 0) {
			compile_block(ctx, while_ast);
		}

#ifdef DEBUG
		wln("; -- COND END --");
#endif

		check_for_integer_on_the_stack(ctx, "while",
																	 "expected last value after performing `while` "
																	 "condition on the stack to be integer",
																	 ast);

		wtln("pop rax");
		wtln("test rax, rax");

		stack_pop(ctx);

		wtprintln("jz ._wdon_%zu", curr_label);

compile_loop:

		if (ast->while_stmt.body >= 0) {
			while_ast = astid(ast->while_stmt.body);
			compile_block(ctx, while_ast);
		}

		wtprintln("jmp ._while_%zu", curr_label);
		wprintln("._wdon_%zu:", curr_label);
	} break;

	case AST_LITERAL: {
		if (ctx->proc_ctx.stmt != NULL) {
			size_t arg_idx = 0;
			proc_arg_t *arg = NULL;
			FOREACH_IDX(idx, proc_arg_t, proc_arg, ctx->proc_ctx.stmt->args) {
				if (0 == strcmp(proc_arg.name, ast->literal.str)) {
					arg = &proc_arg;
					arg_idx = idx;
					break;
				}
			}

			if (arg == NULL) {
				report_error("%s error: undefined symbol: `%s`",
										 loc_to_str(&locid(ast->loc_id)),
										 ast->literal.str);
			}

			// Compute the index of the value from the end of the stack
			const size_t stack_idx = vec_size(ctx->proc_ctx.stmt->args) - arg_idx + ctx->proc_ctx.stack_size + 1;
			// const size_t stack_idx = vec_size(ctx->proc_ctx.stmt->args) - arg_idx;

			wtprintln("mov rax, [rsp + %zu * WORD_SIZE]", stack_idx);
			wtln("push rax");

			stack_add_type(ctx, arg->kind);
		} else {
			const i32 value_idx = shgeti(ctx->values_map, ast->literal.str);
			if (value_idx == -1) {
				report_error("%s error: undefined symbol: `%s`",
										 loc_to_str(&locid(ast->loc_id)),
										 ast->literal.str);
			}
			wtprintln("push __%s__", astid(ctx->values_map[value_idx].value).proc_stmt.name.str);
			stack_add_type(ctx, VALUE_KIND_FUNCTION_POINTER);
		}
	} break;

	case AST_CALL: {
		if (ctx->proc_ctx.stmt != NULL) {
			size_t arg_idx = 0;
			proc_arg_t *arg = NULL;
			FOREACH_IDX(idx, proc_arg_t, proc_arg, ctx->proc_ctx.stmt->args) {
				if (0 == strcmp(proc_arg.name, ast->literal.str)) {
					arg = &proc_arg;
					arg_idx = idx;
					break;
				}
			}

			if (arg == NULL) {
				report_error("%s error: undefined symbol: `%s`",
										 loc_to_str(&locid(ast->loc_id)),
										 ast->literal.str);
			}

			// printf("%zu\n", ctx->proc_ctx.stack_size);
			// Compute the index of the value from the end of the stack
			const size_t stack_idx = vec_size(ctx->proc_ctx.stmt->args) - arg_idx + ctx->proc_ctx.stack_size + 1;
			wtprintln("call [rsp + %zu * WORD_SIZE]", stack_idx);
		} else {
			const i32 value_idx = shgeti(ctx->values_map, ast->call.str);
			if (value_idx == -1) {
				report_error("%s error: undefined symbol: `%s`",
										 loc_to_str(&locid(ast->loc_id)),
										 ast->call.str);
			}
			wtprintln("call __%s__", astid(ctx->values_map[value_idx].value).proc_stmt.name.str);
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

		case VALUE_KIND_FUNCTION_POINTER: UNREACHABLE;
		case VALUE_KIND_POISONED:					UNREACHABLE;
		case VALUE_KIND_LAST:							UNREACHABLE;
		}
	} break;

	case AST_PLUS: {
		check_for_two_integers_on_the_stack(ctx, "+", ast);
		print_binop("add rax, rbx");
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
	} break;

	case AST_MUL: {
		check_for_two_integers_on_the_stack(ctx, "*", ast);
		wtln("xor edx, edx");
		print_binop("mul rbx");
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
		stack_types[stack_size - 1] = VALUE_KIND_INTEGER;
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
		stack_types[stack_size - 1] = VALUE_KIND_INTEGER;
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
		stack_types[stack_size - 1] = VALUE_KIND_INTEGER;
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
		} break;

		case VALUE_KIND_STRING: {
			wtln("mov rdi, [rsp]");
			wtln("call strlen");
			wtln("mov rdx, rax");
			wtln("mov rsi, rdi");
			wtln("mov rax, SYS_WRITE");
			wtln("mov rdi, SYS_STDOUT");
			wtln("syscall");
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
	wln(DEFINE " WORD_SIZE  8");
	wln(DEFINE " NEW_LINE   10");
	wln(DEFINE " SYS_WRITE  1");
	wln(DEFINE " SYS_STDOUT 1");
	wln(DEFINE " SYS_EXIT   60");
}

static void
print_dmp_i64(void)
{
	wln("; r14b: newline");
	wln("; rax: number to print");
	wln("dmp_i64:");
	wtln("push    rbp");
	wtln("mov     rbp, rsp");
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
	wtln("pop     rbp");
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
print_exit(u8 code)
{
	wtln("mov rax, SYS_EXIT");

	if (0 == code) wtln("xor rdi, rdi");
	else wtprintln("mov rdi, %X", code);

	wtln("syscall");
}

INLINE void
print_data_section(void)
{
	wln(SECTION_DATA_WRITEABLE);
}

INLINE void
print_bss_section(void)
{
	wln(SECTION_BSS_WRITEABLE);
}

Compiler
new_compiler(ast_id_t ast_cur)
{
	return (Compiler) {
		.ast_cur = ast_cur,
		.output_file_path = "out.asm",
		.values_map = NULL,
		.proc_ctx = {
			.stmt = NULL,
			.stack_size = 0,
		},
	};
}

void
compiler_compile(Compiler *compiler)
{
	stream = fopen(compiler->output_file_path, "w");
	if (stream == NULL) {
		eprintf("error: Failed to open file: %s\n", compiler->output_file_path);
		exit(EXIT_FAILURE);
	}

	wln(FORMAT_64BIT);

	print_defines();
	wln(SECTION_TEXT_EXECUTABLE);
	print_dmp_i64();
	print_strlen();

	ast_t ast = astid(compiler->ast_cur);
	while (ast.next && ast.next <= ASTS_SIZE) {
		if (ast.ast_kind == AST_PROC) {
#ifdef DEBUG
			FOREACH(proc_arg_t, proc_arg, ast.proc_stmt.args) {
				wprintln("; %s", proc_arg_to_str(&proc_arg));
			}
#endif

			wprintln("__%s__:", ast.proc_stmt.name.str);

			wtln("push rbp");
			wtln("mov rbp, rsp");

			shput(compiler->values_map, ast.proc_stmt.name.str, ast.ast_id);

			compiler->proc_ctx.stmt = &ast.proc_stmt;

			ast_t proc_ast = astid(ast.proc_stmt.body);
			if (ast.proc_stmt.body >= 0) {
				compile_block(compiler, proc_ast);
			}

			// Clean the stack
			while (compiler->proc_ctx.stack_size > 0) {
				wtln("pop rax");
				compiler->proc_ctx.stack_size--;
			}

			wtln("mov rsp, rbp");
			wtln("pop rbp");
			wtln("ret");

			compiler->proc_ctx.stmt = NULL;
			compiler->proc_ctx.stack_size = 0;
		}

		ast = astid(ast.next);
	}

	wln(GLOBAL " _start");
	wln("_start:");

	ast = astid(compiler->ast_cur);
	while (ast.next && ast.next <= ASTS_SIZE) {
		compile_ast(compiler, &ast);
		ast = astid(ast.next);
	}

	print_exit(EXIT_SUCCESS);
	print_data_section();

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

			wprintln("%s db %s, NEW_LINE, 0", strdb, scratch_buffer_to_string());
		} else {
			wprintln("%s db %s, 0", strdb, str);
		}

		scratch_buffer_genstrlen();
		wprintln("%s " COMPTIME_EQU " $ - %s", scratch_buffer_to_string(), strdb);
		string_literal_counter++;
	}

	print_bss_section();

	fclose(stream);
}
