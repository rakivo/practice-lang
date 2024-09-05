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
	tokens = lexer_lex(&lexer);

#ifdef PRINT_TOKENS
	for (size_t i = 0; i < lexer.tokens_count; ++i) printf("%s\n", token_to_str(&tokens[i]));
#endif

	Parser parser = new_parser(tokens, lexer.tokens_count);
	parser_parse(&parser);

#ifdef PRINT_ASTS
	for (ast_id_t i = 0; i < asts_len; ++i) print_ast(&astid(i));
#endif

	if (asts_len == 0) goto ret;

	main_function_check(true, astid(0));
	if(main_function == -1) {
		report_error("%s:0:0: no main at top level function found", file_path);
	}

	if (astid(main_function).func_stmt.ret_type != VALUE_KIND_INTEGER
	|| vec_size(astid(main_function).func_stmt.args) != 0)
	{
		report_error("%s error: main function has wrong signature. \n"
								 "  NOTE: expected signature: func int do <...> end",
								 loc_to_str(&locid(astid(main_function).loc_id)));
	}

	Consteval consteval = new_consteval(&const_map);
	ast_t ast = astid(0);
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

	Compiler compiler = new_compiler(main_function, const_map, var_map);
	compiler_compile(&compiler);

	Nob_Cmd cmd = {0};
	nob_cmd_append(&cmd, PATH_TO_ASM_EXECUTABLE, X86_64_OUTPUT, ASM_FLAGS);
	nob_cmd_run_sync(cmd, false);
	cmd.count = 0;

#ifndef DEBUG
	nob_rename(EXECUTABLE_OUTPUT".tmp", OBJECT_OUTPUT, false);
#endif

	nob_cmd_append(&cmd, PATH_TO_LD_EXECUTABLE, OBJECT_OUTPUT, LD_OUTPUT_FLAGS);
	nob_cmd_run_sync(cmd, false);

ret:
	nob_cmd_free(cmd);
	main_deinit();
	return 0;
}

/* TODO:
	#2. Implement proper system for error-reporting
*/
