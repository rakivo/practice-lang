#pragma once

#include "common.hpp"
#include "file.hpp"

#include <iostream>

typedef i16 ast_id_t;

enum value_kind_t {
  VALUE_KIND_INTEGER,
  VALUE_KIND_STRING,
};

struct dot_stmt_t {};
struct plus_stmt_t {};
struct equal_stmt_t {};

struct push_stmt_t {
  value_kind_t value_kind;

  union {
    i64 integer;
    const char *str;
  };
};

struct if_stmt_t {
  ast_id_t then_body;
  ast_id_t else_body;
};

enum ast_kind_t {
  AST_IF,
  AST_DOT,
  AST_PUSH,
  AST_PLUS,
  AST_EQUAL,
};

std::ostream
&operator<<(std::ostream &os, const ast_kind_t &ast_kind);

struct ast_t {
  file_id_t file_id;
  ast_id_t ast_id;
  ast_id_t next;
  ast_kind_t ast_kind;

  union {
    if_stmt_t if_stmt;
    dot_stmt_t dot_stmt;
    plus_stmt_t plus_stmt;
    push_stmt_t push_stmt;
    equal_stmt_t equal_stmt;
  };
};

void
print_ast(const ast_t &ast);

static_assert(sizeof(ast_t) == 32);

DECLARE_EXTERN(ast, AST);

#define astid(id) (ASTS[id])
#define asts_len (ASTS_SIZE)
#define last_ast (ASTS[ASTS_SIZE - 1])
#define append_ast(ast_) (ASTS[ast_.ast_id] = std::move(ast_))

// Last argument represents stmt
#define make_ast(file_id_, next_, ast_kind_, ...) { \
  .file_id = file_id_, \
  .ast_id = ASTS_SIZE++, \
  .next = next_, \
  .ast_kind = ast_kind_, \
  __VA_ARGS__ \
}
