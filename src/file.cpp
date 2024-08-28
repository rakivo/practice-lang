#include "file.hpp"
#include "common.hpp"

#include <cstring>

str_t::str_t(void)
  : len(0)
{
  buf[0] = '\0';
}

str_t::str_t(const char *src)
  : len(strlen(src))
{
  ASSERT(strlen(src) <= 124);
  strncpy((char *) this->buf, src, sizeof(this->buf));
}

file_t::file_t(str_t file_path)
  : file_id(FILES_SIZE++),
    file_path(file_path) {};

file_t FILES[128];
file_id_t FILES_SIZE = 0;

#define fileid(id) (FILES[id])
#define append_file(file_) (FILES[file_.file_id] = file_)
