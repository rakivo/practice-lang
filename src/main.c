#include "common.h"

#include "lib.h"
#include "ast.h"
#include "vmem.h"
#include "file.h"
#include "lexer.h"
#include "parser.h"
#include "compiler.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

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

	memory_init(10);

	char line[LINE_CAP];
	size_t lines_count = 0;
	lines_t lines = (lines_t) malloc(sizeof(lines_t *) * LINES_CAP);
	while (fgets(line, sizeof(line), file) != NULL) {
		lines[lines_count++] = split(line, ' ');
	}

	fclose(file);

	Lexer lexer = new_lexer(file_.file_id, lines, lines_count);
	tokens_t tokens = lexer_lex(&lexer);

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

	Compiler compiler = new_compiler(main_function);
	compiler_compile(&compiler);

ret:
	free(lines);
	memory_release();
	return 0;
}

/* TODO:
	#2. Implement proper system for error-reporting
*/
