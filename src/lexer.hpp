#pragma once

#include "common.h"
#include "file.hpp"

#include <vector>
#include <cstdint>
#include <fstream>
#include <utility>

enum token_kind_t {
  TOKEN_IF,
  TOKEN_END,

  // Separator of keywords and other tokens, which is also the size of `KEYWORDS` array.
  TOKEN_KEYWORDS_END,

  TOKEN_EQUAL,
  TOKEN_PLUS,
  TOKEN_DOT,
  TOKEN_INTEGER,
  TOKEN_LITERAL,
  TOKEN_STRING_LITERAL,
};

#define KEYWORDS_SIZE TOKEN_KEYWORDS_END
extern const char *KEYWORDS[KEYWORDS_SIZE];

struct loc_t {
  u32 row, col;
  file_id_t file_id;
};

struct token_t {
  loc_t loc;
  token_kind_t type;
  const char *str;
};

std::ostream
&operator<<(std::ostream &os, const token_kind_t &tt);

std::ostream
&operator<<(std::ostream &os, const loc_t &loc);

std::ostream
&operator<<(std::ostream &os, const token_t &token);

typedef std::pair<std::string, u32> ss_t;
typedef std::vector<ss_t> sss_t;
typedef std::vector<sss_t> sss2D_t;

typedef std::vector<token_t> tokens_t;

struct Lexer {
  u32 row;
  sss2D_t lines;
  file_id_t file_id;

  Lexer(file_id_t file_id, sss2D_t lines)
    : row(0),
      lines(std::move(lines)),
      file_id(file_id)
  {};

  ~Lexer() = default;

  /* -- lexer methods -- */

  tokens_t
  lex(void);

  static sss_t
  split(const std::string &input, char delim);

private:

  static token_kind_t
  type_token(const std::string &str);

  void
  lex_line(const sss_t &line, tokens_t &ret);
};
