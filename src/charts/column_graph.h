#ifndef COLUMN_GRAPH_H
#define COLUMN_GRAPH_H

#include "parser.h"
#include "parser_internal.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

void column_graph_draw(
    SDL_Renderer *renderer,
    TTF_Font *font,
    const Graph *graph, 
    const ChartConfig *config,
    int width,
    int height
);

#endif