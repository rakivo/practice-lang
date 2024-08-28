#include "ast.hpp"
#include "common.hpp"
#include "lexer.hpp"
#include "parser.hpp"

#include <cerrno>
#include <climits>
#include <cstdlib>
#include <iostream>

INLINE i64
parse_int(const char *str)
{
  errno = 0;
  const i64 ret = strtol(str, NULL, 10);

  if ((errno == ERANGE
  && (ret == LONG_MAX || ret == LONG_MIN)) || (errno != 0 && ret == 0))
  {
    perror("strtol");
    exit(1);
  }

  return ret;
}

size_t token_idx = 0;
static ast_id_t next = 0;

void
Parser::parse(void)
{
  while (token_idx < this->ts.size()) {
    const ast_t ast = ast_token(this->ts[token_idx]);
    append_ast(ast);
    token_idx++;
  }
}

ast_t
Parser::ast_token(const token_t &token)
{
  switch (token.type) {
  case TOKEN_IF: {
    // Skip `if` keyword
    token_idx++;

    ast_t ast = make_ast(0, ++next, AST_IF,
      .if_stmt = {
        .then_body = next,
        .else_body = -1,
      }
    );

    bool done = false;
    while (!done && token_idx < this->ts.size()) {
      const token_t token_ = this->ts[token_idx];
      if (token_.type == TOKEN_END) {
        done = true;
        break;
      }

      token_idx++;
      const ast_t ast = ast_token(token_);
      append_ast(ast);
    }

    ast.next = last_ast.next;
    last_ast.next = -1;

    if (!done) {
      std::cout << token.loc << " error: no end found" << std::endl;
      exit(1);
    }

    return ast;
  } break;

  case TOKEN_END: {
    UNREACHABLE;
  } break;

  case TOKEN_KEYWORDS_END: break;

  case TOKEN_INTEGER: {
    ast_t ast = make_ast(0, ++next, AST_PUSH,
      .push_stmt = {
        .value_kind = VALUE_KIND_INTEGER,
        .integer = parse_int(token.str),
      }
    );
    return ast;
  } break;

  case TOKEN_LITERAL: {
    TODO("Handle literals");
  } break;

  case TOKEN_EQUAL: {
    ast_t ast = make_ast(0, ++next, AST_EQUAL, .equal_stmt = {});
    return ast;
  } break;

  case TOKEN_PLUS: {
    ast_t ast = make_ast(0, ++next, AST_PLUS, .plus_stmt = {});
    return ast;
  } break;

  case TOKEN_DOT: {
    ast_t ast = make_ast(0, ++next, AST_DOT, .dot_stmt = {});
    return ast;
  } break;
  }

  UNREACHABLE;
}
