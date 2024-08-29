#include "common.h"
#include "lexer.hpp"

#include <cctype>
#include <string>
#include <vector>
#include <cstring>
#include <cstdlib>
#include <utility>
#include <iostream>

const char *KEYWORDS[KEYWORDS_SIZE] = {
  [TOKEN_IF] = "if",
  [TOKEN_END] = "end"
};

std::ostream
&operator<<(std::ostream &os, const token_kind_t &token_kind)
{
  switch (token_kind) {
  case TOKEN_INTEGER:        os << "TOKEN_INTEGER";        break;
  case TOKEN_LITERAL:        os << "TOKEN_LITERAL";        break;
  case TOKEN_STRING_LITERAL: os << "TOKEN_STRING_LITERAL"; break;
  case TOKEN_PLUS:           os << "TOKEN_PLUS";           break;
  case TOKEN_KEYWORDS_END:   os << "TOKEN_KEYWORDS_END";   break;
  case TOKEN_EQUAL:          os << "TOKEN_EQUAL";          break;
  case TOKEN_DOT:            os << "TOKEN_DOT";            break;
  case TOKEN_IF:             os << "TOKEN_IF";             break;
  case TOKEN_END:            os << "TOKEN_END";            break;
  }
  return os;
}

std::ostream
&operator<<(std::ostream &os, const loc_t &loc)
{
  os << fileid(loc.file_id).file_path.buf << ':' << loc.row + 1 << ':' << loc.col + 1 << ':';
  return os;
}

std::ostream
&operator<<(std::ostream &os, const token_t &token)
{
  os << token.loc << " '" << token.str << "': " << token.type;
  return os;
}

tokens_t
Lexer::lex(void)
{
  tokens_t ret = {};
  ret.reserve(this->lines.size());

  for (const auto &line: this->lines) {
    this->lex_line(line, ret);
    this->row++;
  }

  return ret;
}

INLINE i32
check_for_keywords(const char *str)
{
  for (size_t i = 0; i < KEYWORDS_SIZE; ++i) {
    if (0 == strcmp(str, KEYWORDS[i])) return i;
  }

  return -1;
}

token_kind_t
Lexer::type_token(const std::string &str)
{
  const i32 keyword_idx = check_for_keywords(str.data());
  if (keyword_idx >= 0) return (token_kind_t) keyword_idx;

  const char first = str[0];

  if (std::isdigit(first)) return TOKEN_INTEGER;
  if (std::isalpha(first)) return TOKEN_LITERAL;

  switch (first) {
  case '.': return TOKEN_DOT;            break;
  case '+': return TOKEN_PLUS;           break;
  case '=': return TOKEN_EQUAL;          break;
  case '"': return TOKEN_STRING_LITERAL; break;
  }

  UNREACHABLE;
}

void
Lexer::lex_line(const sss_t &line, tokens_t &ret)
{
  for (const auto &ss: line) {
    const auto type = type_token(ss.first);

    if (type != TOKEN_STRING_LITERAL) {
      token_t token = {
        .loc = {
          .row = this->row,
          .col = ss.second,
          .file_id = this->file_id
        },
        .type = type,
        .str = ss.first.data(),
      };

      ret.emplace_back(std::move(token));
      continue;
    }

    token_t token = {
      .loc = {
        .row = this->row,
        .col = ss.second,
        .file_id = this->file_id
      },
      .type = type,
      .str = ss.first.data(),
    };

    ret.emplace_back(std::move(token));
  }
}

static size_t utf8_char_len(unsigned char c)
{
  if ((u32) c < 0x80)    return 1;
  if ((u32) c < 0x800)   return 2;
  if ((u32) c < 0x10000) return 3;
  return 4;
}

sss_t
Lexer::split(const std::string &input, char delim)
{
  sss_t ret;
  ret.reserve(input.size());

  size_t s = 0;
  size_t e = 0;

  bool in_single_quote = false;
  bool in_double_quote = false;

  while (e < input.size()) {
    char c = input[e];

    if (c == '\'' && !in_double_quote) {
      in_single_quote = !in_single_quote;
    } else if (c == '"' && !in_single_quote) {
      in_double_quote = !in_double_quote;
    } else if (c == delim && !in_single_quote && !in_double_quote) {
      if (s != e) ret.emplace_back(input.substr(s, e - s), s);
      s = e + 1;
    }

    e += utf8_char_len(c);
  }

  if (s < e) ret.emplace_back(input.substr(s, e - s), s);

  return ret;
}
