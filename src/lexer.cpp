#include "lexer.hpp"
#include "common.hpp"

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
  case TOKEN_INTEGER:      os << "TOKEN_INTEGER";      break;
  case TOKEN_LITERAL:      os << "TOKEN_LITERAL";      break;
  case TOKEN_PLUS:         os << "TOKEN_PLUS";         break;
  case TOKEN_KEYWORDS_END: os << "TOKEN_KEYWORDS_END"; break;
  case TOKEN_EQUAL:        os << "TOKEN_EQUAL";        break;
  case TOKEN_DOT:          os << "TOKEN_DOT";          break;
  case TOKEN_IF:           os << "TOKEN_IF";           break;
  case TOKEN_END:          os << "TOKEN_END";          break;
  default: UNREACHABLE;
  }
  return os;
}

std::ostream
&operator<<(std::ostream &os, const loc_t &loc)
{
  os << fileid(loc.file_id).file_path.buf << ':' << loc.row + 1 << ':' << loc.col + 1;
  return os;
}

std::ostream
&operator<<(std::ostream &os, const token_t &token)
{
  os << token.loc << ": '" << token.str << "': " << token.type;
  return os;
}

tokens_t
Lexer::lex(void)
{
  tokens_t ret = {};
  ret.reserve(this->lines.size());

  while (this->row < this->lines.size()) {
    const auto line = this->lines[this->row];
    this->lex_line(line, ret);
  }

  return ret;
}

static i32
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
  case '.': return TOKEN_DOT;   break;
  case '+': return TOKEN_PLUS;  break;
  case '=': return TOKEN_EQUAL; break;
  default:
    eprintf("UNEXPECTED CHARACTER: %c\n", first);
    TODO("Create other token types");
  }
}

void
Lexer::lex_line(const sss_t &line, tokens_t &ret)
{
  for (const auto &ss: line) {
    const auto type = type_token(ss.first);

    token_t token = {
      .loc = {
        .row = this->row,
        .col = ss.second,
        .file_id = this->file_id
      },
      .type = type,
      .str = std::move(ss.first.data()),
    };

    ret.emplace_back(std::move(token));
  }

  this->row++;
}

sss_t
Lexer::split(const std::string &input, char delim)
{
  sss_t ret = {};
  ret.reserve(input.size() / 2);

  size_t s = 0;
  size_t e = 0;

  while (e < input.size()) {
    while (e < input.size() && input[e] == delim) ++e;
    s = e;
    while (e < input.size() && input[e] != delim) ++e;
    if (s < e) ret.emplace_back(input.substr(s, e - s), s);
  }

  return ret;
}
