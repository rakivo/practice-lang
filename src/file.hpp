#pragma once

#include "common.hpp"

struct str_t {
  u32 len;
  char buf[124];

  str_t(void);
  str_t(const char *src);
};

static_assert(sizeof(str_t) == 128);

typedef u16 file_id_t;

struct file_t {
  file_id_t file_id;
  str_t file_path;

  file_t(void) = default;
  file_t(str_t file_path);
};

extern file_t FILES[128];
extern file_id_t FILES_SIZE;

#define fileid(id) (FILES[id])
#define append_file(file_) (FILES[file_.file_id] = file_)
