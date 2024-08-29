#pragma once

#include "ast.hpp"
#include "file.hpp"

struct Compiler {
  ast_id_t ast_cur;
  str_t output_file_path;

  Compiler(ast_id_t ast_cur)
    : ast_cur(ast_cur),
      output_file_path(str_t { "out.asm" })
  {};

  void
  compile(void);
};
