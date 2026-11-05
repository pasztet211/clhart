#include "combination_graph.h"

#include "parser_internal.h"

#include <stdio.h>
#include <stdlib.h>

bool combination_graph_parse(
    Parser *p,
    Graph *graph,
    ChartConfig *config
) {
    if (!expect_word(p, "charts")) {
        fprintf(stderr, "expected 'charts' block\n");
        return false;
    }

    if (!expect(p, '{')) {
        fprintf(stderr, "expected '{' after charts\n");
        return false;
    }

    CombinationGraph *combination = &graph->data.combination;

    while (true) {
        skip_whitespace(p);

        // End of the charts block.
        if (p->source[p->position] == '}') {
            if (combination->chart_count == 0) {
                fprintf(stderr, "combination must contain at least one chart\n");
                return false;
            }

            p->position++;
            return true;
        }

        // Every child chart must be enclosed in braces.
        if (!expect(p, '{')) {
            fprintf(stderr, "expected '{' before child chart\n");
            return false;
        }

        Graph *child_graph = calloc(1, sizeof(Graph));
        ChartConfig *child_config = calloc(1, sizeof(ChartConfig));

        if (!child_graph || !child_config) {
            fprintf(stderr, "could not allocate child chart\n");

            free(child_graph);
            free(child_config);
            return false;
        }

        // Parse the child using the normal chart parser.
        if (!parse_graph(p, child_graph, child_config)) {
            free_graph(child_graph, child_config->type);
            free_chart_config(child_config);
            return false;
        }

        // Consume the child's outer closing brace.
        if (!expect(p, '}')) {
            fprintf(stderr, "expected '}' after child chart\n");

            free_graph(child_graph, child_config->type);
            free_chart_config(child_config);
            return false;
        }

        // Append the successfully parsed chart.
        CombinationChart *charts = realloc(
            combination->charts,
            sizeof(CombinationChart) * (combination->chart_count + 1)
        );

        if (!charts) {
            fprintf(stderr, "could not allocate combination chart array\n");

            free_graph(child_graph, child_config->type);
            free_chart_config(child_config);
            return false;
        }

        combination->charts = charts;

        combination->charts[combination->chart_count++] = (CombinationChart) {
            .graph = child_graph,
            .config = child_config
        };

        skip_whitespace(p);

        if (p->source[p->position] == ',') {
            p->position++;
            continue;
        }

        if (p->source[p->position] == '}') {
            p->position++;
            return true;
        }

        fprintf(stderr, "expected ',' or '}' after child chart\n");
        return false;
    }
}