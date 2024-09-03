#ifndef FILE_H_
#define FILE_H_

#include "common.h"

typedef struct {
	u32 len;
	char buf[124];
} str_t;

str_t
new_str_t(const char *src);

typedef u32 file_id_t;

typedef struct {
	file_id_t file_id;
	str_t file_path;
} file_t;

file_t
new_file_t(str_t file_path);

#define FILES_CAP 1024
extern file_id_t FILES_SIZE;
extern file_t FILES[FILES_CAP];

#define fileid(id) (FILES[id])
#define files_len (FILES_SIZE)
#define last_file (FILES[FILES_SIZE - 1])
#define append_file(file_) (FILES[file_.file_id] = file_)

#endif // FILE_H_
