#include "common.h"

#include "lib.h"
#include "ast.h"
#include "file.h"
#include "lexer.h"
#include "parser.h"
#include "compiler.h"

#include <stdio.h>
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
		eprintf("ERROR: Failed to open file: %s", file_path);
		return EXIT_FAILURE;
	}

	memory_init(1);

	char line[1024];
	sss2D_t lines = NULL;
	while (fgets(line, sizeof(line), file) != NULL) {
		vec_add(lines, split(line, ' '));
	}

	Lexer lexer = new_lexer(file_.file_id, lines);
	tokens_t tokens = lexer_lex(&lexer);

	// FOREACH(token_t, token, tokens) printf("%s\n", token_to_str(&token));

	Parser parser = { tokens };
	parser_parse(&parser);

	// for (size_t i = 0; i < (size_t) ASTS_SIZE; ++i) print_ast(&astid(i));

	Compiler compiler = new_compiler(0);
	compiler_compile(&compiler);

	fclose(file);
	memory_release();
	return 0;
}

/* TODO:
	#1. Add support for nested if-elses
	#2. Implement proper system for error-reporting
*/
