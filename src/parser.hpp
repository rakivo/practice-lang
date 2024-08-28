#pragma once

#include <vector>

#include "lexer.hpp"

struct Parser {
  const tokens_t ts;

  Parser(const tokens_t ts) : ts(std::move(ts)) {};
  ~Parser() = default;

  /* -- parser methods -- */

  void
  parse(void);

  void
  ast_token(const token_t &token, size_t &i, ast_id_t &next);
};
