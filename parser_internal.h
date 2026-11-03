#ifndef PARSER_INTERNAL_H
#define PARSER_INTERNAL_H

#include "parser.h"

#include <stddef.h>

typedef struct {
    const char *source;
    size_t position;
} Parser;

void skip_whitespace(Parser *parser);
bool expect_word(Parser *parser, const char *word);
bool expect(Parser *parser, char c);
bool expect_operator(Parser *parser, const char *op);
char *parse_name(Parser *parser);
bool node_graph_parse(Parser *parser, Graph *graph);
bool histogram_graph_parse(
    Parser *parser,
    Graph *graph,
    ChartConfig *config
);
bool pie_graph_parse(Parser *parser, Graph *graph);
bool bar_graph_parse(Parser *p, Graph *graph);

#endif
