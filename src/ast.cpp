#include "ast.hpp"

ast_t ASTS[1024];
ast_id_t ASTS_SIZE = 0;

std::ostream
&operator<<(std::ostream &os, const ast_kind_t &ast_kind)
{
  switch (ast_kind) {
  case AST_IF:    os << "AST_IF";    break;
  case AST_PUSH:  os << "AST_PUSH";  break;
  case AST_PLUS:  os << "AST_PLUS";  break;
  case AST_DOT:   os << "AST_DOT";   break;
  case AST_EQUAL: os << "AST_EQUAL"; break;
  default: UNREACHABLE;
  }
  return os;
}
