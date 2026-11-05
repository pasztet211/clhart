#ifndef BAR_GRAPH_H
#define BAR_GRAPH_H

#include "parser.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

void bar_graph_draw(
    SDL_Renderer *renderer,
    TTF_Font *font,
    const Graph *graph,
    const ChartConfig *config,
    int width,
    int height
);

#endif