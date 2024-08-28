#define FLAG_IMPLEMENTATION
#include "flag.hpp"
#include "common.hpp"

#include "ast.hpp"
#include "file.hpp"
#include "lexer.hpp"
#include "parser.hpp"
#include "compiler.hpp"

#include <cstdio>
#include <cstdlib>
#include <fstream>

int
main(int argc, const char *argv[])
{
  if (argc < 2) {
    eprintf("Usage: %s <file_path>\n", argv[0]);
    exit(1);
  }

  const auto file_path = argv[1];

  const file_t file_ = { str_t(file_path) };
  append_file(file_);

  std::ifstream file(file_path);
  if (!file.is_open()) {
    eprintf("ERROR: Failed to open file: %s", file_path);
    exit(1);
  }

  sss2D_t lines = {};
  lines.reserve(128);

  std::string line = {};
  while (std::getline(file, line)) {
    lines.emplace_back(Lexer::split(line, ' '));
  }

  file.close();

  Lexer lexer(file_.file_id, lines);
  tokens_t tokens = lexer.lex();

  Parser parser(tokens);
  parser.parse();

  // for (size_t i = 0; i < (size_t) ASTS_SIZE; ++i) print_ast(astid(i));

  Compiler compiler(0);
  compiler.compile();

  return 0;
}
