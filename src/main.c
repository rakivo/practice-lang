#include "lib.h"
#include "nob.h"
#include "ast.h"
#include "vmem.h"
#include "file.h"
#include "lexer.h"
#include "parser.h"
#include "common.h"
#include "compiler.h"
#include "consteval.h"

#define STB_DS_IMPLEMENTATION
#include "stb_ds.h"

#include <time.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// global variable to time using `set_time` and `dbg_time` macros from `common.h`
clock_t _time = 0;

static lines_t lines = NULL;
static size_t lines_count = 0;
static tokens_t tokens = NULL;
static var_map_t *var_map = NULL;
static const_map_t *const_map = NULL;

void
main_deinit(void)
{
	for (size_t i = 0; i < lines_count; ++i) free(lines[i].items);
	free(lines);
	free(tokens);
	shfree(var_map);
	shfree(const_map);
	memory_release();
}

static void
check_for_main_function(const char *file_path)
{
	main_function_check(true, astid(0));
	if(main_function == -1) {
		report_error("%s:0:0: no main at top level function found", file_path);
	}

	if (*astid(main_function).func_stmt.ret_types != VALUE_KIND_INTEGER
	|| vec_size(astid(main_function).func_stmt.args) != 0)
	{
		report_error("%s error: main function has wrong signature. \n"
								 "  note: expected signature: func int do <...> end",
								 loc_to_str(&locid(astid(main_function).loc_id)));
	}
}

static void
consteval_step(void)
{
	ast_t ast = astid(0);
	Consteval consteval = new_consteval(&const_map);

#ifdef DEBUG
	set_time;
#endif

	while (ast.next && ast.next <= ASTS_SIZE) {
		if (ast.ast_kind == AST_CONST) {
			const consteval_value_t value = consteval_eval(&consteval, &ast, false);
			shput(const_map, ast.const_stmt.name->str, value);
		} else if (ast.ast_kind == AST_VAR) {
			const consteval_value_t value = consteval_eval(&consteval, &ast, true);
			shput(var_map, ast.var_stmt.name->str, value);
		}
		ast = astid(ast.next);
	}

#ifdef DEBUG
	dbg_time("constevaling");
#endif
}

typedef struct {
	tokens_t tokens;
	size_t tokens_count;
} ttc_t;

static size_t
lex_step(const char *file_path)
{
	const file_t file = new_file_t(new_str_t(file_path));
	append_file(file);

	FILE *stream = fopen(file_path, "r");
	if (stream == NULL) {
		eprintf("error: failed to open file: %s\n", file_path);
		exit(EXIT_FAILURE);
	}

	char line[LINE_CAP];
	lines = (lines_t) malloc(sizeof(lines_t *) * LINES_CAP);
	while (fgets(line, sizeof(line), stream) != NULL) {
		lines[lines_count++] = split(line, ' ');
	}

	fclose(stream);
	Lexer lexer = new_lexer(file.file_id, lines, lines_count);

#ifdef DEBUG
	set_time;
#endif

	tokens = lexer_lex(&lexer);

#ifdef PRINT_TOKENS
	for (size_t i = 0; i < lexer.tokens_count; ++i) printf("%s\n", token_to_str(&tokens[i]));
#endif

#ifdef DEBUG
	dbg_time("lexing");
#endif

	return lexer.tokens_count;
}

static void
parse_step(const tokens_t tokens, size_t tokens_count)
{
	Parser parser = new_parser(tokens, tokens_count);

#ifdef DEBUG
	set_time;
#endif

	parser_parse(&parser);

#ifdef PRINT_ASTS
	for (ast_id_t i = 0; i < asts_len; ++i) print_ast(&astid(i));
#endif

#ifdef DEBUG
	dbg_time("parsing");
#endif
}

static void
compile_step(void)
{
	Compiler compiler = new_compiler(main_function, const_map, var_map);

#ifdef DEBUG
	set_time;
#endif

	compiler_compile(&compiler);

#ifdef DEBUG
	dbg_time("compiling");
#endif
}

static void
compile_asm_step(void)
{
	Nob_Cmd cmd = {0};

#ifdef DEBUG
	set_time;
#endif

	nob_cmd_append(&cmd, PATH_TO_ASM_EXECUTABLE, X86_64_OUTPUT, ASM_FLAGS);
	nob_cmd_run_sync(cmd, dbg_echo);
	cmd.count = 0;

#ifndef DEBUG
	nob_rename(EXECUTABLE_OUTPUT".tmp", OBJECT_OUTPUT, dbg_echo);
#endif

	nob_cmd_append(&cmd, PATH_TO_LD_EXECUTABLE, OBJECT_OUTPUT, LD_OUTPUT_FLAGS);
	nob_cmd_run_sync(cmd, dbg_echo);

#ifdef DEBUG
	dbg_time("compiling asm");
#endif

	nob_cmd_free(cmd);
}

UNUSED
void print_elapsed(clock_t start, clock_t end, const char *what)
{
	double elapsed = (double) (end - start) / CLOCKS_PER_SEC;
	printf("%s took: %.0fus\n", what, elapsed * 1000000);
}

int
main(int argc, const char *argv[])
{
	if (argc < 2) {
		eprintf("Usage: %s <file_path>\n", argv[0]);
		exit(1);
	}

	const char *file_path = argv[1];
	memory_init(3);
	const size_t tokens_count = lex_step(file_path);
	parse_step(tokens, tokens_count);
	if (asts_len == 0) goto ret;
	check_for_main_function(file_path);
	consteval_step();
	compile_step();
	compile_asm_step();

ret:
	main_deinit();
	return 0;
}

/* TODO:
	#2. Implement proper system for error-reporting
*/
