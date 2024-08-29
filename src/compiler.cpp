#include "ast.hpp"
#include "compiler.hpp"

#include <cstdio>
#include <cstdlib>
#include <fstream>

/*
  In r15 you always have an updated pointer to the top of the stack.
*/

#define TAB "  "

#define wln(...) (stream << __VA_ARGS__ << std::endl)
#define wtln(...) wln(TAB << __VA_ARGS__)

static size_t jump_label_counter = 0;
static size_t if_else_label_counter = 0;

static void
compile_ast(const ast_t &ast, std::ofstream &stream)
{
  wtln("; -- " << ast.ast_kind << " --");
  switch (ast.ast_kind) {
  case AST_IF: {
    wtln("sub r15, WORD_SIZE");
    wtln("mov rax, [r15]");

    if (!ast.if_stmt.then_body) return;

    wtln("test rax, rax");
    wtln("jz ._else_" << if_else_label_counter);

    auto if_ast = astid(ast.if_stmt.then_body);
    while (if_ast.next && if_ast.next < ASTS_SIZE) {
      compile_ast(if_ast, stream);
      if_ast = astid(if_ast.next);
    }

    wln("._else_" << if_else_label_counter << ":");

    if_else_label_counter++;
  } break;

  case AST_PUSH: {
    wtln("mov qword [r15], 0x" << std::hex << ast.push_stmt.integer);
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

    wtln("jne ._jne_" << jump_label_counter);
    wtln("mov qword [r15], 0x1");
    wtln("jmp ._done_" << jump_label_counter);
    wln("._jne_" << jump_label_counter <<  ":");
    wtln("mov qword [r15], 0x0");
    wln("._done_" << jump_label_counter << ":");
    wtln("add r15, WORD_SIZE");

    jump_label_counter++;
  } break;

  case AST_DOT: {
    wtln("mov rax, qword [r15 - WORD_SIZE]");
    wtln("mov r14, 0x1"); // mov 1 to r15 to print newline
    wtln("call dmp_i64");
  } break;
  }
}

static void
print_defines(std::ofstream &stream)
{
  wln("%define MEMORY_CAP 8 * 1024");
  wln("%define STACK_CAP  1024");
  wln("%define WORD_SIZE  8");
  wln("%define SYS_WRITE  1");
  wln("%define SYS_STDOUT 1");
  wln("%define SYS_EXIT   60");
}

static void
print_dmp_i64(std::ofstream &stream)
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
print_exit(std::ofstream &stream, u8 code)
{
  wtln("mov rax, SYS_EXIT");

  if (0 == code)
    wtln("xor rdi, rdi");
  else
    wtln("mov rdi, " << std::hex << code);

  wtln("syscall");
}

static void
print_data_section(std::ofstream &stream)
{
  wln("section .data");
  wln("stack_ptr dq stack_buf");
  wln("memory_ptr dq memory_buf");
}

static void
print_bss_section(std::ofstream &stream)
{
  wln("section .bss");
  wln("memory_buf resb MEMORY_CAP");
  wln("stack_buf resq STACK_CAP");
}

void
Compiler::compile(void)
{
  std::ofstream stream(this->output_file_path.buf);
  if (!stream) {
    eprintf("Failed to create file: %s", this->output_file_path.buf);
    exit(1);
  }

  wln("BITS 64");
  print_defines(stream);
  wln("section .text");
  print_dmp_i64(stream);
  wln("global _start");
  wln("_start:");

  wtln("mov r15, qword [stack_ptr]");

  ast_t ast = astid(this->ast_cur);
  while (ast.next && ast.next <= ASTS_SIZE) {
    compile_ast(ast, stream);
    ast = astid(ast.next);
  }

  print_exit(stream, 0);
  print_data_section(stream);
  print_bss_section(stream);

  stream.close();
}
