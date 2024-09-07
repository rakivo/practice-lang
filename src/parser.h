#ifndef PARSER_H_
#define PARSER_H_

#include "ast.h"
#include "lexer.h"

typedef struct {
	const tokens_t ts;
} Parser;

void
parser_parse(Parser *parser);

Parser
new_parser(const tokens_t ts);

ast_t
ast_token(Parser *parser, const token_t *token, bool rec);

#endif // PARSER_H_
