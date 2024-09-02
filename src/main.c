#include "common.h"

#include "lib.h"
#include "ast.h"
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

	memory_init(1);

	char line[1024];
	sss2D_t lines = NULL;
	u32 lines_skipped = 0;
	while (fgets(line, sizeof(line), file) != NULL) {
		vec_add(lines, split(line, ' '));
	}

	fclose(file);

	Lexer lexer = new_lexer(file_.file_id, lines, lines_skipped);
	tokens_t tokens = lexer_lex(&lexer);

#ifdef PRINT_TOKENS
	FOREACH(token_t, token, tokens) printf("%s\n", token_to_str(&token));
#endif

	Parser parser = { tokens };
	parser_parse(&parser);

#ifdef PRINT_ASTS
	for (ast_id_t i = 0; i < asts_len; ++i) print_ast(&ast);
#endif

	if (asts_len == 0) goto ret;

	main_function_check(true, astid(0));
	if(main_function == -1) {
		if (main_function_not_at_top_level != -1) {
			report_error("%s error: main function should be at top level",
									 loc_to_str(&locid(astid(main_function_not_at_top_level).loc_id)));
		}
		report_error("%s:0:0: no main function found", file_path);
	}

	Compiler compiler = new_compiler(main_function);
	compiler_compile(&compiler);

ret:
	memory_release();
	return 0;
}

/* TODO:
	#2. Implement proper system for error-reporting
*/
