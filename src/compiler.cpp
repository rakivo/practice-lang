#include "ast.hpp"
#include "compiler.hpp"

#include <fstream>
#include <cstdio>
#include <cstdlib>

#define wln(...) (stream << __VA_ARGS__ << std::endl)
#define wtln(...) (stream << "    " << __VA_ARGS__ << std::endl)

static void
compile_ast(const ast_t &ast, std::ofstream &stream)
{
  wtln("; -- " << ast.ast_kind << " --");
  switch (ast.ast_kind) {
  case AST_IF: {
    wtln("mov r15, qword [stack_ptr]");
    wtln("mov rax, [r15]");
    wtln("sub qword [stack_ptr], WORD_SIZE");

    if (!ast.if_stmt.then_body) return;

    wtln("test rax, rax");
    wtln("jz _else");

    auto if_ast = astid(ast.if_stmt.then_body);
    while (if_ast.next && if_ast.next < ASTS_SIZE) {
      compile_ast(if_ast, stream);
      if_ast = astid(if_ast.next);
    }

    wln("_else:");
  } break;

  case AST_PUSH: {
    wtln("mov r15, qword [stack_ptr]");
    wtln("mov qword [r15], " << ast.push_stmt.integer);
    wtln("add qword [stack_ptr], WORD_SIZE");
  } break;

  case AST_PLUS: {
    wtln("mov r15, qword [stack_ptr]");
    wtln("mov rax, qword [r15 - WORD_SIZE]");
    wtln("mov rbx, qword [r15 - WORD_SIZE * 2]");
    wtln("add rax, rbx");
    wtln("mov [r15 - WORD_SIZE * 2], rax");
    wtln("sub qword [stack_ptr], WORD_SIZE");
  } break;

  case AST_EQUAL: {
    wtln("mov r15, qword [stack_ptr]");
    wtln("mov rax, qword [r15 - WORD_SIZE]");
    wtln("mov rbx, qword [r15 - WORD_SIZE * 2]");
    wtln("cmp rax, rbx");
    wtln("jne ._jne_");
    wtln("mov qword [r15 - WORD_SIZE * 2], 0x1");
    wtln("jmp ._done_");
    wln("._jne_:");
    wtln("mov qword [r15 - WORD_SIZE * 2], 0x0");
    wln("._done_:");
    wtln("sub qword [stack_ptr], WORD_SIZE");
  } break;

  case AST_DOT: {
    wtln("mov r15, qword [stack_ptr]");
    wtln("mov rax, qword [r15 - WORD_SIZE]");
    wtln("mov r15, 1"); // mov 1 to r15 to print newline
    wtln("call dmp_i64");
  } break;

  default: UNREACHABLE;
  }
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
  wln("%define MEMORY_CAP 8 * 1024");
  wln("%define STACK_CAP  1024");
  wln("%define WORD_SIZE  8");
  wln("%define SYS_WRITE  1");
  wln("%define SYS_STDOUT 1");
  wln("%define SYS_EXIT   60");
  wln("section .text");

  wln("; r15b: newline");
  wln("dmp_i64:");
  wln("    push    rbp");
  wln("    mov     rbp, rsp");
  wln("    sub     rsp, 64");
  wln("    mov     qword [rbp - 8],  rax");
  wln("    mov     dword [rbp - 36], 0");
  wln("    mov     dword [rbp - 40], 0");
  wln("    cmp     qword [rbp - 8],  0");
  wln("    jge     .LBB0_2");
  wln("    mov     dword [rbp - 40], 1");
  wln("    xor     eax, eax");
  wln("    sub     rax, qword [rbp - 8]");
  wln("    mov     qword [rbp - 8], rax");
  wln(".LBB0_2:");
  wln("    cmp     qword [rbp - 8], 0");
  wln("    jne     .LBB0_4");
  wln("    mov     eax, dword [rbp - 36]");
  wln("    mov     ecx, eax");
  wln("    add     ecx, 1");
  wln("    mov     dword [rbp - 36], ecx");
  wln("    cdqe");
  wln("    mov     byte [rbp + rax - 32], 48");
  wln("    jmp     .LBB0_8");
  wln(".LBB0_4:");
  wln("    jmp     .LBB0_5");
  wln(".LBB0_5:");
  wln("    cmp     qword [rbp - 8], 0");
  wln("    jle     .LBB0_7");
  wln("    mov     rax, qword [rbp - 8]");
  wln("    mov     ecx, 10");
  wln("    cqo");
  wln("    idiv    rcx");
  wln("    mov     eax, edx");
  wln("    mov     dword [rbp - 44], eax");
  wln("    mov     eax, dword [rbp - 44]");
  wln("    add     eax, 48");
  wln("    mov     cl, al");
  wln("    mov     eax, dword [rbp - 36]");
  wln("    mov     edx, eax");
  wln("    add     edx, 1");
  wln("    mov     dword [rbp - 36], edx");
  wln("    cdqe");
  wln("    mov     byte [rbp + rax - 32], cl");
  wln("    mov     rax, qword [rbp - 8]");
  wln("    mov     ecx, 10");
  wln("    cqo");
  wln("    idiv    rcx");
  wln("    mov     qword [rbp - 8], rax");
  wln("    jmp     .LBB0_5");
  wln(".LBB0_7:");
  wln("    jmp     .LBB0_8");
  wln(".LBB0_8:");
  wln("    cmp     dword [rbp - 40], 0");
  wln("    je      .LBB0_10");
  wln("    mov     eax, dword [rbp - 36]");
  wln("    mov     ecx, eax");
  wln("    add     ecx, 1");
  wln("    mov     dword [rbp - 36], ecx");
  wln("    cdqe");
  wln("    mov     byte [rbp + rax - 32], 45");
  wln(".LBB0_10:");
  wln("    movsxd  rax, dword [rbp - 36]");
  wln("    mov     byte [rbp + rax - 32], 0");
  wln("    mov     dword [rbp - 48], 0");
  wln("    mov     eax, dword [rbp - 36]");
  wln("    sub     eax, 1");
  wln("    mov     dword [rbp - 52], eax");
  wln(".LBB0_11:");
  wln("    mov     eax, dword [rbp - 48]");
  wln("    cmp     eax, dword [rbp - 52]");
  wln("    jge     .LBB0_13");
  wln("    movsxd  rax, dword [rbp - 48]");
  wln("    mov     al, byte [rbp + rax - 32]");
  wln("    mov     byte [rbp - 53], al");
  wln("    movsxd  rax, dword [rbp - 52]");
  wln("    mov     cl, byte [rbp + rax - 32]");
  wln("    movsxd  rax, dword [rbp - 48]");
  wln("    mov     byte [rbp + rax - 32], cl");
  wln("    mov     cl, byte [rbp - 53]");
  wln("    movsxd  rax, dword [rbp - 52]");
  wln("    mov     byte [rbp + rax - 32], cl");
  wln("    mov     eax, dword [rbp - 48]");
  wln("    add     eax, 1");
  wln("    mov     dword [rbp - 48], eax");
  wln("    mov     eax, dword [rbp - 52]");
  wln("    add     eax, -1");
  wln("    mov     dword [rbp - 52], eax");
  wln("    jmp     .LBB0_11");
  wln(".LBB0_13:");
  wln("    mov     eax, dword [rbp - 36]");
  wln("    mov     ecx, eax");
  wln("    add     ecx, 1");
  wln("    mov     dword [rbp - 36], ecx");
  wln("    cdqe");
  wln("    mov     byte [rbp + rax - 32], 10");
  wln("    lea     rsi, [rbp - 32]");
  wln("    movsxd  rdx, dword [rbp - 36]");
  wln("    test    r15b, r15b");
  wln("    jz      .not_newline");
  wln("    jmp     .write");
  wln(".not_newline:");
  wln("    dec rdx");
  wln(".write:");
  wln("    mov     eax, SYS_WRITE");
  wln("    mov     edi, SYS_STDOUT");
  wln("    syscall");
  wln("    add     rsp, 64");
  wln("    pop     rbp");
  wln("    ret");

  wln("global _start");
  wln("_start:");

  ast_t ast = astid(this->ast_cur);
  while (ast.next && ast.next < ASTS_SIZE) {
    compile_ast(ast, stream);
    ast = astid(ast.next);
  }

  wtln("mov rax, 60");
  wtln("xor rdi, rdi");
  wtln("syscall");

  wln("section .data");
  wln("stack_ptr dq stack_buf");
  wln("memory_ptr dq memory_buf");
  wln("section .bss");
  wln("memory_buf resb MEMORY_CAP");
  wln("stack_buf resq STACK_CAP");

  stream.close();
}
