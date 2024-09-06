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

	const file_t file_ = new_file_t(new_str_t(file_path));
	append_file(file_);

	FILE *file = fopen(file_path, "r");
	if (file == NULL) {
		eprintf("error: failed to open file: %s\n", file_path);
		return EXIT_FAILURE;
	}

	memory_init(3);

	char line[LINE_CAP];
	lines = (lines_t) malloc(sizeof(lines_t *) * LINES_CAP);
	while (fgets(line, sizeof(line), file) != NULL) {
		lines[lines_count++] = split(line, ' ');
	}

	fclose(file);

	Lexer lexer = new_lexer(file_.file_id, lines, lines_count);

#ifdef DEBUG
	const clock_t lex_start = clock();
#endif
	tokens = lexer_lex(&lexer);
#ifdef DEBUG
	const clock_t lex_end = clock();
	dbg_time(lex, "lexing");
#endif

#ifdef PRINT_TOKENS
	for (size_t i = 0; i < lexer.tokens_count; ++i) printf("%s\n", token_to_str(&tokens[i]));
#endif

	Parser parser = new_parser(tokens, lexer.tokens_count);

#ifdef DEBUG
	const clock_t parser_start = clock();
#endif
	parser_parse(&parser);
#ifdef DEBUG
	const clock_t parser_end = clock();
	dbg_time(parser, "parsing");
#endif

#ifdef PRINT_ASTS
	for (ast_id_t i = 0; i < asts_len; ++i) print_ast(&astid(i));
#endif

	if (asts_len == 0) goto ret;

	main_function_check(true, astid(0));
	if(main_function == -1) {
		report_error("%s:0:0: no main at top level function found", file_path);
	}

	if (*astid(main_function).func_stmt.ret_types != VALUE_KIND_INTEGER
	|| vec_size(astid(main_function).func_stmt.args) != 0)
	{
		report_error("%s error: main function has wrong signature. \n"
								 "  NOTE: expected signature: func int do <...> end",
								 loc_to_str(&locid(astid(main_function).loc_id)));
	}

	ast_t ast = astid(0);
	Consteval consteval = new_consteval(&const_map);
#ifdef DEBUG
	const clock_t consteval_start = clock();
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
	const clock_t consteval_end = clock();
	dbg_time(consteval, "constevaling");
#endif

	Compiler compiler = new_compiler(main_function, const_map, var_map);

#ifdef DEBUG
	const clock_t compile_start = clock();
#endif
	compiler_compile(&compiler);

#ifdef DEBUG
	const clock_t compile_end = clock();
	dbg_time(compile, "compiling");
	print_elapsed(lex_start, compile_end, "everything");
#endif

#ifdef DEBUG
	const clock_t compile_asm_start = clock();
#endif

	Nob_Cmd cmd = {0};
	nob_cmd_append(&cmd, PATH_TO_ASM_EXECUTABLE, X86_64_OUTPUT, ASM_FLAGS);
	nob_cmd_run_sync(cmd, dbg_echo);
	cmd.count = 0;

#ifndef DEBUG
	nob_rename(EXECUTABLE_OUTPUT".tmp", OBJECT_OUTPUT, dbg_echo);
#endif

	nob_cmd_append(&cmd, PATH_TO_LD_EXECUTABLE, OBJECT_OUTPUT, LD_OUTPUT_FLAGS);
	nob_cmd_run_sync(cmd, dbg_echo);

#ifdef DEBUG
	const clock_t compile_asm_end = clock();
	dbg_time(compile_asm, "compiling asm");
#endif

ret:
	nob_cmd_free(cmd);
	main_deinit();
	return 0;
}

/* TODO:
	#2. Implement proper system for error-reporting
*/
