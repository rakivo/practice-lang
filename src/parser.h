#ifndef PARSER_H_
#define PARSER_H_

#include "ast.h"
#include "lexer.h"

typedef struct {
	const tokens_t ts;
} Parser;

void
parser_parse(Parser *parser);

ast_t
ast_token(Parser *parser, const token_t *token);

#endif // PARSER_H_
