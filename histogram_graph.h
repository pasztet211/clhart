#ifndef HISTOGRAM_GRAPH_H
#define HISTOGRAM_GRAPH_H

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

#include "parser.h"
#include "parser_internal.h"

void histogram_graph_draw(
    SDL_Renderer *renderer,
    TTF_Font *font,
    const Graph *histogram,
    const ChartConfig *config,
    int width,
    int height
);

#endif