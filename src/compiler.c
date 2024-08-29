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

#define wtln(...) fprintf(stream, TAB __VA_ARGS__ "\n")
#define wtprintln(fmt, ...) fprintf(stream, TAB fmt "\n", __VA_ARGS__)
#define wprintln(fmt, ...) fprintf(stream, fmt "\n", __VA_ARGS__)
#define wln(...) fprintf(stream, __VA_ARGS__ "\n")

static FILE *stream = NULL;
static size_t jump_label_counter = 0;
static size_t if_else_label_counter = 0;
static size_t string_literal_counter = 0;
static value_kind_t last_value_kind = VALUE_KIND_POISONED;

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

static void
compile_ast(const ast_t *ast)
{
  wprintln("; -- %s --", ast_kind_to_str(ast->ast_kind));
  switch (ast->ast_kind) {
  case AST_IF: {
    wtln("sub r15, WORD_SIZE");
    wtln("mov rax, [r15]");

    if (!ast->if_stmt.then_body) return;

    wtln("test rax, rax");
    wtprintln("jz ._else_%zu", if_else_label_counter);

    ast_t if_ast = astid(ast->if_stmt.then_body);
    while (if_ast.next && if_ast.next < ASTS_SIZE) {
      compile_ast(&if_ast);
      if_ast = astid(if_ast.next);
    }

    wprintln("._else_%zu:", if_else_label_counter);

    if_else_label_counter++;
  } break;

  case AST_PUSH: {
    switch (ast->push_stmt.value_kind) {
    case VALUE_KIND_INTEGER: {
      last_value_kind = VALUE_KIND_INTEGER;
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

    wtln("add r15, WORD_SIZE");
  } break;

  case AST_PLUS: {
    wtln("mov rax, qword [r15 - WORD_SIZE]");
    wtln("mov rbx, qword [r15 - WORD_SIZE * 2]");
    wtln("add rax, rbx");
    wtln("mov [r15 - WORD_SIZE * 2], rax");
    wtln("sub r15, WORD_SIZE");
  } break;

  case AST_EQUAL: {
    wtln("mov rax, qword [r15 - WORD_SIZE]");
    wtln("mov rbx, qword [r15 - WORD_SIZE * 2]");
    wtln("cmp rax, rbx");

    wtprintln("jne ._jne_%zu", jump_label_counter);
    wtln("mov qword [r15], 0x1");
    wtprintln("jmp ._done_%zu", jump_label_counter);
    wprintln("._jne_%zu:", jump_label_counter);
    wtln("mov qword [r15], 0x0");
    wprintln("._done_%zu", jump_label_counter);
    wtln("add r15, WORD_SIZE");

    jump_label_counter++;
  } break;

  case AST_DOT: {
    switch (last_value_kind) {
    case VALUE_KIND_INTEGER: {
      wtln("mov rax, qword [r15 - WORD_SIZE]");
      wtln("mov r14, 0x1"); // mov 1 to r15 to print newline
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
  wln("%%define MEMORY_CAP 8 * 1024");
  wln("%%define STACK_CAP  1024");
  wln("%%define WORD_SIZE  8");
  wln("%%define NEW_LINE   10");
  wln("%%define SYS_WRITE  1");
  wln("%%define SYS_STDOUT 1");
  wln("%%define SYS_EXIT   60");
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
  wln("section .data");
  wln("stack_ptr dq stack_buf");
  wln("memory_ptr dq memory_buf");
}

static void
print_bss_section(void)
{
  wln("section .bss");
  wln("memory_buf resb MEMORY_CAP");
  wln("stack_buf resq STACK_CAP");
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

  wln("BITS 64");
  print_defines();
  wln("section .text");
  print_dmp_i64();
  wln("global _start");
  wln("_start:");

  wtln("mov r15, qword [stack_ptr]");

  ast_t ast = astid(compiler->ast_cur);
  while (ast.next && ast.next <= ASTS_SIZE) {
    compile_ast(&ast);
    ast = astid(ast.next);
  }

  print_exit(0);
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
      wprintln("%s equ $ - %s", scratch_buffer_to_string(), strdb);
      string_literal_counter++;
    }

    ast = astid(ast.next);
  }

  print_bss_section();

	fclose(stream);
}
