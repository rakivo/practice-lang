#include "ast.hpp"
#include "common.hpp"
#include "lexer.hpp"
#include "parser.hpp"

#include <cerrno>
#include <climits>
#include <cstdlib>
#include <iostream>

void
print_ast(const ast_t &ast)
{
  std::cout << "AST ID: " << ast.ast_id << std::endl;
  std::cout << "File ID: " << ast.file_id << std::endl;
  std::cout << "Next AST ID: " << ast.next << std::endl;
  std::cout << "AST Kind: ";

  switch (ast.ast_kind) {
  case AST_PUSH: {
    std::cout << "AST_PUSH" << std::endl;
    std::cout << "Value Kind: " << (ast.push_stmt.value_kind == VALUE_KIND_INTEGER ? "Integer" : "String") << std::endl;
    if (ast.push_stmt.value_kind == VALUE_KIND_INTEGER)
      std::cout << "Integer Value: " << ast.push_stmt.integer << std::endl;
    else
      std::cout << "String Value: " << (ast.push_stmt.str ? ast.push_stmt.str : "null") << std::endl;
  } break;

  case AST_IF: {
    std::cout << "AST_IF" << std::endl;
    std::cout << "Then Body ID: " << ast.if_stmt.then_body << std::endl;
    std::cout << "Else Body ID: " << ast.if_stmt.else_body << std::endl;
  } break;

  case AST_PLUS: std::cout << "AST_PLUS" << std::endl; break;
  case AST_EQUAL: std::cout << "AST_EQUAL" << std::endl; break;

  default: UNREACHABLE;
  }

  std::cout << "--------------" << std::endl;
}

void
Parser::parse(void)
{
  ast_id_t next = 0;
  size_t i = 0;
  while (i < this->ts.size()) {
    ast_token(this->ts[i], i, next);
    i++;
  }
}

static i64
parse_int(const char *str)
{
  static_assert(sizeof(long int) == sizeof(i64));

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

void
Parser::ast_token(const token_t &token, size_t &i, ast_id_t &next)
{
  switch (token.type) {
  case TOKEN_IF: {
    i++;
    bool done = false;
    ast_id_t then_body = next + 1;
    while (!done && i < this->ts.size()) {
      const token_t token_ = this->ts[i];
      if (token_.type == TOKEN_END) {
        i++;
        done = true;
        break;
      }

      ast_token(token_, ++i, next);
    }

    if (!done) {
      eprintf("No end");
      exit(1);
    }

    ast_t ast = make_ast(0, ++next, AST_IF,
      .if_stmt = {
        .then_body = then_body,
        .else_body = -1,
      }
    );

    append_ast(ast);
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
    append_ast(ast);
  } break;

  case TOKEN_LITERAL: {
    UNREACHABLE;
  } break;

  case TOKEN_EQUAL: {
    ast_t ast = make_ast(0, ++next, AST_EQUAL, .equal_stmt = {});
    append_ast(ast);
  } break;

  case TOKEN_PLUS: {
    ast_t ast = make_ast(0, ++next, AST_PLUS, .plus_stmt = {});
    append_ast(ast);
  } break;

  case TOKEN_DOT: {
    ast_t ast = make_ast(0, ++next, AST_DOT, .dot_stmt = {});
    append_ast(ast);
  } break;

  default:
    UNREACHABLE;
  }
}
