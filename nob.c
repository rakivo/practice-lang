#define NOB_IMPLEMENTATION
#include "src/nob.h"
#include "src/common.h"

#include <string.h>
#include <limits.h>

// #define DEBUG

#define ALWAYS_MAKE_FLAG "-B"
#define RELEASE_FLAG "release"

#ifndef CC
	#define CC "clang"
#endif

#ifndef DEBUG
	#define CFLAGS "-std=c11", "-g"
#else
	#define CFLAGS "-std=c11", "-g", "-DDEBUG"
#endif

#define C_EXT "c"
#define SRC_DIR "src"
#define SRC_DIR_LEN 3
#define BUILD_DIR "build"
#define BIN_FILE (BUILD_DIR"/pracc")
#define ROOT_FILE_NOEXT "main.c"
#define ROOT_FILE (SRC_DIR"/main.c")
#define COMMON_H (SRC_DIR"/common.h")
#define WFLAGS "-Wall", "-Wextra", "-Wpedantic", "-Wswitch-enum", "-Wno-gnu-zero-variadic-macro-arguments", "-Wno-gnu-folding-constant", "-Wno-gnu-empty-struct", "-Wno-excess-initializers", "-Wno-unsequenced"

#define nob_da_free_items(what) do { \
	for (size_t i = 0; i < what.count; ++i) free(what.items[i]); \
} while (0)

static bool always_make = false;
static bool release_mode = false;

void
parse_flags(int argc, const char *argv[])
{
	for (size_t i = 1; i < argc; ++i) {
		if (0 == strcmp(argv[i], ALWAYS_MAKE_FLAG)) {
			always_make = true;
		} else if (0 == strcmp(argv[i], RELEASE_FLAG)) {
			release_mode = true;
		}
	}
}

int
main(int argc, const char *argv[])
{
	NOB_GO_REBUILD_URSELF(argc, argv);
	parse_flags(argc, argv);

	Nob_File_Paths src_files_ = {0};
	nob_read_entire_dir(SRC_DIR, &src_files_);

	// Filter out `.`, `..` and `main.c`
	Nob_File_Paths src_files = {0};
	for (size_t i = 0; i < src_files_.count; ++i) {
		if (*src_files_.items[i] != '.' && 0 != strcmp(src_files_.items[i], ROOT_FILE_NOEXT)) {
			nob_da_append(&src_files, src_files_.items[i]);
		}
	}

	// Collect c_files and obj_files
	Nob_File_Paths c_files = {0};
	Nob_File_Paths obj_files = {0};
	for (size_t i = 0; i < src_files.count; ++i) {
		char file[PATH_MAX];
		const size_t file_len = strlen(src_files.items[i]);
		strncpy(file, src_files.items[i], PATH_MAX);

		Nob_String_View sv = nob_sv_from_cstr(file);
		nob_sv_chop_by_delim(&sv, '.');

		if (sv.count > 0 && 0 == strncmp(sv.data, C_EXT, sv.count)) {
			Nob_String_Builder sb = {0};
			nob_sb_append_cstr(&sb, SRC_DIR);
			nob_sb_append_cstr(&sb, "/");
			nob_sb_append_cstr(&sb, file);
			nob_sb_append_null(&sb);
			nob_da_append(&c_files, sb.items);
		} else continue;

		for (size_t i = 0; i < file_len; ++i) {
			if (file[i] == '.') {
				file[i] = '\0';
			}
		}

		Nob_String_Builder sb = {0};
		nob_sb_append_cstr(&sb, BUILD_DIR);
		nob_sb_append_cstr(&sb, "/");
		nob_sb_append_cstr(&sb, file);
		nob_sb_append_cstr(&sb, ".o");
		nob_sb_append_null(&sb);

		if (sb.items != NULL && *sb.items != '.' && *sb.items != '\0') {
			nob_da_append(&obj_files, sb.items);
		}
	}

	// Prefix src_files with `src/`
	Nob_File_Paths src_files_prefixed = {0};
	for (size_t i = 0; i < src_files.count; ++i) {
		Nob_String_Builder sb = {0};
		nob_sb_append_cstr(&sb, SRC_DIR);
		nob_sb_append_cstr(&sb, "/");
		nob_sb_append_cstr(&sb, src_files.items[i]);
		nob_sb_append_null(&sb);
		nob_da_append(&src_files_prefixed, sb.items);
	}

	// Avoid `[INFO] directory `build` already exists` message.
	if (!nob_file_exists(BUILD_DIR)) {
		nob_mkdir_if_not_exists(BUILD_DIR);
	}

	// Build all the object files
	Nob_Cmd cmd = {0};
	for (size_t i = 0; i < obj_files.count; ++i) {
		if (always_make
		|| nob_needs_rebuild(obj_files.items[i],
												(const char **) src_files_prefixed.items, src_files_prefixed.count))
		{
			cmd.count = 0;
			if (release_mode) {
				nob_cmd_append(&cmd, CC, CFLAGS, "-O3", WFLAGS, "-c", c_files.items[i], "-o", obj_files.items[i]);
			} else {
				nob_cmd_append(&cmd, CC, CFLAGS, "-O0", WFLAGS, "-c", c_files.items[i], "-o", obj_files.items[i]);
			}
#ifdef DEBUG
			nob_cmd_run_sync(cmd, true);
#else
			nob_cmd_run_async(cmd, true);
#endif
		}
	}

	// Append `main.c`
	nob_da_append(&src_files_prefixed, ROOT_FILE);

	// Build `build/pracc` executable
	if (always_make
	|| nob_needs_rebuild(BIN_FILE,
											 (const char **) src_files_prefixed.items, src_files_prefixed.count))
	{
		cmd.count = 0;
		if (release_mode) {
			nob_cmd_append(&cmd, CC, "-o", BIN_FILE, CFLAGS, "-O3", WFLAGS);
		} else {
			nob_cmd_append(&cmd, CC, "-o", BIN_FILE, CFLAGS, "-O0", WFLAGS);
		}
		for (size_t i = 0; i < obj_files.count; ++i) {
			nob_cmd_append(&cmd, obj_files.items[i]);
		}
		nob_cmd_append(&cmd, ROOT_FILE);
		nob_cmd_run_sync(cmd, true);
	}

	nob_cmd_free(cmd);
	for (size_t i = 0; i < src_files_prefixed.count - 1; ++i) free(src_files_prefixed.items[i]);
	nob_da_free_items(c_files);
	nob_da_free_items(obj_files);
	nob_da_free(c_files);
	nob_da_free(obj_files);
	nob_da_free(src_files);
	nob_da_free(src_files_);
	nob_da_free(src_files_prefixed);
	return 0;
}
