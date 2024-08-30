#include "lib.h"
#include "ast.h"
#include "common.h"
#include "compiler.h"

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

#define FASM

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
static size_t stack_size = 0;
static size_t label_counter = 0;
static size_t string_literal_counter = 0;
static size_t equal_jne_label_counter = 0;
static size_t last_integer_push_value = 0;
static value_kind_t last_value_kind = VALUE_KIND_POISONED;

static void
compile_ast(const ast_t *ast);

INLINE void scratch_buffer_genstr(void)
{
	scratch_buffer_clear();
	scratch_buffer_printf("__str_%zu__", string_literal_counter);
}

INLINE void scratch_buffer_genstr_offset(i32 offset)
{
	string_literal_counter += offset;
	scratch_buffer_genstr();
	string_literal_counter -= offset;
}

INLINE void scratch_buffer_genstrlen(void)
{
	scratch_buffer_clear();
	scratch_buffer_printf("__str_%zu_len__", string_literal_counter);
}

INLINE void scratch_buffer_genstrlen_offset(i32 offset)
{
	string_literal_counter += offset;
	scratch_buffer_genstrlen();
	string_literal_counter -= offset;
}

// Compile an ast till the `ast.next` is greater than or equal to `0`.
// Every non-empty block should be with an `ast.next` = -1 at the end.
INLINE void compile_block(ast_t ast)
{
	while (ast.ast_id < asts_len) {
		compile_ast(&ast);
		ast = astid(ast.next);
		if (ast.next < 0) {
			compile_ast(&ast);
			break;
		}
	}
}

INLINE void
print_last_two(const char *r1, const char *r2)
{
	wtprintln("mov %s, qword [r15 - WORD_SIZE]", r1);
	wtprintln("mov %s, qword [r15 - WORD_SIZE * 2]", r2);
}

// Perform binary operation with rax, rbx
#define print_binop_and_mov_to_stack(op) \
	print_last_two("rbx", "rax"); \
	wtln(op); \
	wtln("mov [r15 - WORD_SIZE * 2], rax");

static void
compile_ast(const ast_t *ast)
{
	wprintln("; -- %s --", ast_kind_to_str(ast->ast_kind));
	switch (ast->ast_kind) {
	case AST_IF: {
		if (stack_size < 1) {
			report_error("%s error: `if` with empty stack", loc_to_str(&locid(ast->loc_id)));
		}

		// If statement is empty
		if (ast->if_stmt.then_body < 0 && ast->if_stmt.else_body < 0) return;

		size_t curr_label = label_counter++;
		size_t else_label = curr_label;
		size_t done_label = curr_label + 1;

		wtln("sub r15, WORD_SIZE");
		wtln("mov rax, [r15]");

		wtln("test rax, rax");
		wtprintln("jz ._else_%zu", else_label);

		ast_t if_ast = astid(ast->if_stmt.then_body);
		if (ast->if_stmt.then_body >= 0) {
			compile_block(if_ast);
			wtprintln("jmp ._edon_%zu", done_label);
		}

		wprintln("._else_%zu:", else_label);

		if (ast->if_stmt.else_body >= 0) {
			if_ast = astid(ast->if_stmt.else_body);
			compile_block(if_ast);
		}

		wprintln("._edon_%zu:", done_label);
		label_counter++;
	} break;

	case AST_PUSH: {
		switch (ast->push_stmt.value_kind) {
		case VALUE_KIND_INTEGER: {
			last_value_kind = VALUE_KIND_INTEGER;
			last_integer_push_value = ast->push_stmt.integer;
			wtprintln("mov qword [r15], 0x%lX", ast->push_stmt.integer);
		} break;

		case VALUE_KIND_STRING: {
			last_value_kind = VALUE_KIND_STRING;
			scratch_buffer_genstrlen();
			wtprintln("mov qword [r15], %s", scratch_buffer_to_string());
			string_literal_counter++;
		} break;

		case VALUE_KIND_POISONED: UNREACHABLE;
		}

		stack_size++;
		wtln("add r15, WORD_SIZE");
	} break;

	case AST_PLUS: {
		print_binop_and_mov_to_stack("add rax, rbx");
		wtln("sub r15, WORD_SIZE");
	} break;

	case AST_MINUS: {
		print_binop_and_mov_to_stack("sub rax, rbx");
		wtln("sub r15, WORD_SIZE");
	} break;

	case AST_DIV: {
		wtln("xor edx, edx");
		print_binop_and_mov_to_stack("div rbx");
		wtln("sub r15, WORD_SIZE");
	} break;

	case AST_MUL: {
		wtln("xor edx, edx");
		print_binop_and_mov_to_stack("mul rbx");
		wtln("sub r15, WORD_SIZE");
	} break;

	case AST_EQUAL: {
		wtln("mov rax, qword [r15 - WORD_SIZE]");
		wtln("mov rbx, qword [r15 - WORD_SIZE * 2]");
		wtln("cmp rax, rbx");

		wtprintln("jne ._jne_%zu", equal_jne_label_counter);
		wtln("mov qword [r15], 0x1");
		wtprintln("jmp ._done_%zu", equal_jne_label_counter);
		wprintln("._jne_%zu:", equal_jne_label_counter);
		wtln("mov qword [r15], 0x0");
		wprintln("._done_%zu:", equal_jne_label_counter);
		wtln("add r15, WORD_SIZE");

		equal_jne_label_counter++;
	} break;

	case AST_DOT: {
		switch (last_value_kind) {
		case VALUE_KIND_INTEGER: {
			wtln("mov rax, qword [r15 - WORD_SIZE]");
			wtln("mov r14, 0x1"); // mov 1 to r14 to print newline
			wtln("call dmp_i64");
		} break;

		case VALUE_KIND_STRING: {
			wtln("mov rax, SYS_WRITE");
			wtln("mov rdi, SYS_STDOUT");

			scratch_buffer_genstr_offset(-1);
			wtprintln("mov rsi, %s", scratch_buffer_to_string());

			scratch_buffer_genstrlen_offset(-1);
			wtprintln("mov rdx, %s", scratch_buffer_to_string());
			wtln("syscall");
		} break;

		case VALUE_KIND_POISONED: UNREACHABLE; break;
		}
	} break;

	case AST_POISONED: UNREACHABLE;
	}
}

static void
print_defines(void)
{
	wln(DEFINE " MEMORY_CAP 8 * 1024");
	wln(DEFINE " STACK_CAP  1024");
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
print_exit(u8 code)
{
	wtln("mov rax, SYS_EXIT");

	if (0 == code)
		wtln("xor rdi, rdi");
	else
		wtprintln("mov rdi, %X", code);

	wtln("syscall");
}

static void
print_data_section(void)
{
	wln(SECTION_DATA_WRITEABLE);
	wln("stack_ptr dq stack_buf");
}

static void
print_bss_section(void)
{
	wln(SECTION_BSS_WRITEABLE);
	wln("stack_buf " RESERVE_QUAD " STACK_CAP");
}

Compiler
new_compiler(ast_id_t ast_cur)
{
	return (Compiler) {
		.ast_cur = ast_cur,
		.output_file_path = "out.asm",
	};
}

void
compiler_compile(Compiler *compiler)
{
	stream = fopen(compiler->output_file_path, "w");
	if (stream == NULL) {
		eprintf("ERROR: Failed to open file: %s", compiler->output_file_path);
		exit(EXIT_FAILURE);
	}

	wln(FORMAT_64BIT);

	print_defines();
	wln(SECTION_TEXT_EXECUTABLE);
	print_dmp_i64();

	wln(GLOBAL " _start");
	wln("_start:");

	wtln("mov r15, qword [stack_ptr]");

	ast_t ast = astid(compiler->ast_cur);
	while (ast.next && ast.next <= ASTS_SIZE) {
		compile_ast(&ast);
		ast = astid(ast.next);
	}

	print_exit(EXIT_SUCCESS);
	print_data_section();

	string_literal_counter = 0;

	ast = astid(compiler->ast_cur);
	while (ast.next && ast.next <= ASTS_SIZE) {
		if (ast.ast_kind == AST_PUSH && ast.push_stmt.value_kind == VALUE_KIND_STRING) {
			scratch_buffer_clear();

			scratch_buffer_genstr();
			char *strdb = scratch_buffer_copy();

			size_t len = strlen(ast.push_stmt.str);
			if (len > 2 && ast.push_stmt.str[len - 3] == '\\' && ast.push_stmt.str[len - 2] == 'n') {
				scratch_buffer_clear();
				scratch_buffer_append(ast.push_stmt.str);

				scratch_buffer.str[len - 3] = '"';
				scratch_buffer.str[len - 2] = '\0';

				wprintln("%s db %s, NEW_LINE", strdb, scratch_buffer_to_string());
			} else {
				wprintln("%s db %s", strdb, ast.push_stmt.str);
			}

			scratch_buffer_genstrlen();
			wprintln("%s " COMPTIME_EQU " $ - %s", scratch_buffer_to_string(), strdb);
			string_literal_counter++;
		}

		ast = astid(ast.next);
	}

	print_bss_section();

	fclose(stream);
}
