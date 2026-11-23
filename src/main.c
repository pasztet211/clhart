#include <stdio.h>
#include <stdlib.h>

#include "parser.h"
#include "renderer.h"

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("usage: clhart <file>\n");
        return 1;
    }

    Graph *graph = NULL;
    ChartConfig *config = NULL;
    SDL_Window *window = NULL;
    SDL_Renderer *renderer = NULL;

    if (!renderer_create(&window, &renderer)) {
        return 1;
    }

    if (!parse_file(argv[1], &graph, &config, renderer)) {
        fprintf(stderr, "failed to parse graph\n");
        renderer_destroy(window, renderer);
        return 1;
    }

    bool rendered = render_graph(
        graph,
        "C:/Windows/Fonts/arial.ttf",
        argv[1],
        config,
        renderer
    );

    free_graph(graph, config->type);
    free_chart_config(config);
    renderer_destroy(window, renderer);
    return rendered ? 0 : 1;
}
//     printf("successfully parsed graph!\n\n");

//     printf("nodes (%d):\n", graph.node_count);

//     for (int i = 0; i < graph.node_count; i++) {
//         Node *node = &graph.nodes[i];

//         printf("  [%d] %s", i, node->name);

//         if (node->identifier) {
//             printf(" (identifier: %s)", node->identifier);
//         }

//         printf("\n");
//     }

//     printf("\nconnections (%d):\n", graph.connection_count);

//     for (int i = 0; i < graph.connection_count; i++) {
//         Connection *connection = &graph.connections[i];

//         printf(
//             "  %s <-> %s\n",
//             graph.nodes[connection->from].name,
//             graph.nodes[connection->to].name
//         );
//     }

//     free_graph(&graph);

//     return 0;
// }
