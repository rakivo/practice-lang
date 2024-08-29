#include "file.h"
#include "common.h"

#include <string.h>

str_t
new_str_t(const char *src)
{
	const size_t len = strlen(src);
  ASSERT(len <= 124, "String is too big BRUH");

	char buf[124];
  strncpy((char *) buf, src, sizeof(buf));

	str_t ret = {0};
	ret.len = len;
	strcpy(ret.buf, buf);

	return ret;
}

file_t
new_file_t(str_t file_path)
{
	return (file_t) {
		.file_id = FILES_SIZE++,
		.file_path = file_path
	};
}

DECLARE_STATIC(file, FILE);
