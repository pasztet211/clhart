#ifndef RENDERER_H
#define RENDERER_H

#include <stdbool.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

#include "parser.h"

#define RENDERER_BASE_WIDTH 1000
#define RENDERER_BASE_HEIGHT 800
#define RENDERER_BASE_FONT_SIZE 20

bool renderer_create(SDL_Window **window, SDL_Renderer **renderer);
void renderer_destroy(SDL_Window *window, SDL_Renderer *renderer);
void draw_image_fit(
    SDL_Renderer *renderer,
    SDL_Texture *image,
    SDL_Rect bounds
);
bool render_graph(
    const Graph *graph,
    const char *font_path,
    const char *chart_path,
    const ChartConfig *config,
    SDL_Renderer *renderer
);
void draw_legend(SDL_Renderer *renderer,
    TTF_Font *font,
    const Graph *graph,
    const ChartConfig *config,
    int width,
    double scale
);
void draw_title(
    SDL_Renderer *renderer,
    TTF_Font *font,
    const char *title,
    int width
);

#endif