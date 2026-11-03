#include "renderer.h"
#include "node_graph.h"
#include "histogram_graph.h"
#include "pie_graph.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

#include <stdio.h>
#include <stdlib.h>

#define WINDOW_WIDTH 1000
#define WINDOW_HEIGHT 800

static void set_color(SDL_Renderer *renderer, int r, int g, int b) {
    SDL_SetRenderDrawColor(renderer, r, g, b, 255);
}

static void draw_title(
    SDL_Renderer *renderer,
    TTF_Font *font,
    const char *title
) {
    if (!title || title[0] == '\0') {
        return;
    }

    SDL_Color color = {230, 230, 230, 255};
    SDL_Surface *surface = TTF_RenderUTF8_Blended(font, title, color);

    if (!surface) {
        fprintf(stderr, "could not render title: %s\n", TTF_GetError());
        return;
    }

    SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surface);

    if (texture) {
        SDL_Rect destination = {
            (WINDOW_WIDTH - surface->w) / 2,
            20,
            surface->w,
            surface->h
        };

        SDL_RenderCopy(renderer, texture, NULL, &destination);
        SDL_DestroyTexture(texture);
    } else {
        fprintf(stderr, "could not create title texture: %s\n", SDL_GetError());
    }

    SDL_FreeSurface(surface);
}

static void draw_legend(
    SDL_Renderer *renderer,
    TTF_Font *font,
    const Graph *graph,
    const ChartConfig *config
) {
    if (!config->global.legend)
        return;
        
    int count = 0;

    switch (config->type) {
        case CHART_NODES:
            count = graph->data.nodes.node_count;
            break;

        case CHART_HISTOGRAM:
            if (config->specific.histogram.type == HISTOGRAM_DISTRIBUTION)
                count = graph->data.histogram.data.distribution.parameter_count;
            else
                count = graph->data.histogram.data.fuzzy.parameter_count;
            break;

        case CHART_PIE:
            count = graph->data.pie.slice_count;
            break;

        case CHART_BAR:
            return;
    }

    int max_text_width = 0;

    for (int i = 0; i < count; i++) {
        const char *name;

        if (config->type == CHART_NODES) {
            name = graph->data.nodes.nodes[i].name;
        } else if (config->type == CHART_PIE) {
            name = graph->data.pie.slices[i].name;
        } else if (config->specific.histogram.type == HISTOGRAM_DISTRIBUTION) {
            name = graph->data.histogram.data.distribution.parameters[i].name;
        } else {
            name = graph->data.histogram.data.fuzzy.parameters[i].name;
        }

        int text_width = 0;

        TTF_SizeUTF8(font, name, &text_width, NULL);

        if (text_width > max_text_width)
            max_text_width = text_width;
    }

    const int padding = 10;
    const int swatch_width = 16;
    const int gap = 8;
    const int row_height = 30;

    int legend_width =
        padding * 2 + swatch_width + gap + max_text_width;

    int legend_height =
        padding * 2 + count * row_height;

    int legend_x = WINDOW_WIDTH - legend_width - 20;
    int legend_y = 20;

    for (int i = 0; i < count; i++) {
        const char *name;
        const char *color;
        bool has_color;

        switch (config->type) {
            case CHART_NODES: {
                const Node *object = &graph->data.nodes.nodes[i];
                name = object->name;
                color = object->color;
                has_color = object->has_color;
                break;
            }

            case CHART_HISTOGRAM: {
                if (config->specific.histogram.type == HISTOGRAM_DISTRIBUTION) {
                    const DistributionParameter *object =
                        &graph->data.histogram.data.distribution.parameters[i];

                    name = object->name;
                    color = object->color;
                    has_color = object->has_color;
                } else {
                    const FuzzyParameter *object =
                        &graph->data.histogram.data.fuzzy.parameters[i];

                    name = object->name;
                    color = object->color;
                    has_color = object->has_color;
                }
                break;
            }

            case CHART_PIE: {
                const PieSlice *object = &graph->data.pie.slices[i];

                name = object->name;
                color = object->color;
                has_color = object->has_color;
                break;
            }

            default:
                return;
        }

        if (has_color) {
            unsigned int rgb = (unsigned int)strtoul(color, NULL, 16);

            SDL_SetRenderDrawColor(
                renderer,
                (rgb >> 16) & 0xFF,
                (rgb >> 8) & 0xFF,
                rgb & 0xFF,
                255
            );
        } else if (config->type == CHART_PIE) {
            static const SDL_Color defaults[] = {
                {50, 120, 255, 255},
                {255, 90, 90, 255},
                {70, 200, 130, 255},
                {255, 190, 60, 255},
                {180, 100, 255, 255},
                {50, 210, 210, 255},
                {255, 120, 200, 255},
                {150, 160, 170, 255}
            };

            SDL_Color c = defaults[i % (sizeof(defaults) / sizeof(defaults[0]))];

            SDL_SetRenderDrawColor(renderer, c.r, c.g, c.b, c.a);
        } else {
            // Default object color
            SDL_SetRenderDrawColor(renderer, 50, 120, 255, 255);
        }

        // Draw color swatch
        int y = legend_y + padding + i * row_height;

        SDL_Rect swatch = {
            legend_x + padding,
            y,
            swatch_width,
            16
        };

        SDL_RenderFillRect(renderer, &swatch);

        // Render display name
        SDL_Color text_color = {240, 240, 240, 255};

        SDL_Surface *surface = TTF_RenderUTF8_Blended(
            font,
            name,
            text_color
        );

        if (surface) {
            SDL_Texture *texture = SDL_CreateTextureFromSurface(
                renderer,
                surface
            );

            if (texture) {
                SDL_Rect text_rect = {
                    legend_x + padding + swatch_width + gap,
                    y - 5,
                    surface->w,
                    surface->h
                };

                SDL_RenderCopy(renderer, texture, NULL, &text_rect);
                SDL_DestroyTexture(texture);
            }

            SDL_FreeSurface(surface);
        }
    }
}

bool render_graph(
    const Graph *graph,
    const char *font_path,
    const ChartConfig *config
) {
    switch (config->type) {
        case CHART_NODES:
            if (graph->data.nodes.node_count <= 0) {
                fprintf(stderr, "cannot render an empty graph\n");
                return false;
            }
            break;

        case CHART_HISTOGRAM:
            if (graph->data.histogram.data.fuzzy.parameter_count <= 0) {
                fprintf(stderr, "cannot render an empty histogram\n");
                return false;
            }
            break;

        case CHART_PIE:
            if (graph->data.pie.slice_count == 0)
                return false;
            break;

        case CHART_BAR:
            break;
    }

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "SDL initialization failed: %s\n", SDL_GetError());
        return false;
    }

    if (TTF_Init() != 0) {
        fprintf(stderr, "SDL_ttf initialization failed: %s\n", TTF_GetError());
        SDL_Quit();
        return false;
    }

    TTF_Font *font = TTF_OpenFont(font_path, 20);

    if (!font) {
        fprintf(stderr, "could not load font: %s\n", TTF_GetError());
        TTF_Quit();
        SDL_Quit();
        return false;
    }

    SDL_Window *window = SDL_CreateWindow(
        "Clhart",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        WINDOW_WIDTH,
        WINDOW_HEIGHT,
        SDL_WINDOW_SHOWN
    );

    if (!window) {
        fprintf(stderr, "window creation failed: %s\n", SDL_GetError());
        TTF_CloseFont(font);
        TTF_Quit();
        SDL_Quit();
        return false;
    }

    SDL_Renderer *renderer = SDL_CreateRenderer(
        window,
        -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC
    );

    if (!renderer) {
        fprintf(stderr, "renderer creation failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        TTF_CloseFont(font);
        TTF_Quit();
        SDL_Quit();
        return false;
    }

    bool running = true;

    while (running) {
        SDL_Event event;

        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = false;
            }
        }

        // Background
        set_color(renderer, 24, 26, 32);
        SDL_RenderClear(renderer);

        switch (config->type) {
            case CHART_NODES:
                node_graph_draw(
                    renderer,
                    font,
                    graph,
                    config,
                    WINDOW_WIDTH,
                    WINDOW_HEIGHT
                );
                break;

            case CHART_HISTOGRAM:
                histogram_graph_draw(
                    renderer,
                    font,
                    graph,
                    config,
                    WINDOW_WIDTH,
                    WINDOW_HEIGHT
                );
                break;

            case CHART_PIE:
                pie_graph_draw(
                    renderer,
                    font,
                    graph,
                    config,
                    WINDOW_WIDTH,
                    WINDOW_HEIGHT
                );
                break;

            case CHART_BAR:
                break;
        }
        draw_legend(renderer, font, graph, config);
        draw_title(renderer, font, config->title);

        SDL_RenderPresent(renderer);
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);

    TTF_CloseFont(font);
    TTF_Quit();
    SDL_Quit();

    return true;
}