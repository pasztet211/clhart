#ifndef DISPLAY_GRAPH_H
#define DISPLAY_GRAPH_H

#include "parser.h"

#include <SDL2/SDL.h>

void display_graph_draw(
    SDL_Renderer *renderer,
    const Graph *graph,
    const ChartConfig *config,
    int width,
    int height
);

#endif
