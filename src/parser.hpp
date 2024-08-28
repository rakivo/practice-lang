#pragma once

#include <vector>

#include "ast.hpp"
#include "lexer.hpp"

struct Parser {
  const tokens_t ts;

  Parser(const tokens_t ts) : ts(std::move(ts)) {};
  ~Parser() = default;

  /* -- parser methods -- */

  void
  parse(void);

  ast_t
  ast_token(const token_t &token);
};
