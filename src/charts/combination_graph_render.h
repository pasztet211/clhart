#ifndef COMBINATION_GRAPH_RENDER_H
#define COMBINATION_GRAPH_RENDER_H

#include "parser.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <stdbool.h>

bool combination_graph_draw(
    SDL_Renderer *renderer,
    TTF_Font *font,
    const Graph *graph,
    const ChartConfig *config,
    int width,
    int height
);

#endif