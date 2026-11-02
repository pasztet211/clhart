#ifndef NODE_GRAPH_H
#define NODE_GRAPH_H

#include "parser.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

void node_graph_draw(
    SDL_Renderer *renderer,
    TTF_Font *font,
    const Graph *graph,
    const ChartConfig *config,
    int width,
    int height
);

#endif
