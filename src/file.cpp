#include "file.hpp"
#include "common.h"

#include <cstring>

str_t::str_t(void)
  : len(0)
{
  buf[0] = '\0';
}

str_t::str_t(const char *src)
  : len(strlen(src))
{
  ASSERT(strlen(src) <= 124, "String is too big BRUH");
  strncpy((char *) this->buf, src, sizeof(this->buf));
}

file_t::file_t(str_t file_path)
  : file_id(FILES_SIZE++),
    file_path(file_path) {};

DECLARE_STATIC(file, FILE);
