#pragma once

#include "common.h"

struct str_t {
  u32 len;
  char buf[124];

  str_t(void);
  str_t(const char *src);
};

typedef u16 file_id_t;

struct file_t {
  file_id_t file_id;
  str_t file_path;

  file_t(void) = default;
  file_t(str_t file_path);
};

DECLARE_EXTERN(file, FILE);

#define fileid(id) (FILES[id])
#define files_len (FILES_SIZE)
#define last_file (FILES[FILES_SIZE - 1])
#define append_file(file_) (FILES[file_.file_id] = file_)
