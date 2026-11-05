#include "combination_graph_render.h"

#include "node_graph.h"
#include "histogram_graph.h"
#include "pie_graph.h"
#include "bar_graph.h"
#include "renderer.h"
#include "column_graph.h"

#include <math.h>
#include <stdio.h>
#include <stdbool.h>

static int child_font_size(int width, int height) {
    double scale = fmin(
        width / (double)RENDERER_BASE_WIDTH,
        height / (double)RENDERER_BASE_HEIGHT
    );

    int size = (int)lround(RENDERER_BASE_FONT_SIZE * scale);
    return size > 0 ? size : 1;
}

static bool draw_child_chart(
    SDL_Renderer *renderer,
    TTF_Font *font,
    const CombinationChart *chart,
    const ChartConfig *parent_config,
    int width,
    int height,
    int parent_width,
    int parent_height
) {
    int font_size = child_font_size(width, height);
    int parent_font_size = child_font_size(parent_width, parent_height);

    if (TTF_SetFontSize(font, font_size) != 0) {
        fprintf(
            stderr,
            "could not resize child chart font: %s\n",
            TTF_GetError()
        );
        return false;
    }

    bool draw_succeeded = true;

    switch (chart->config->type) {
        case CHART_NODES:
            node_graph_draw(renderer, font, chart->graph, chart->config, width, height);
            break;

        case CHART_HISTOGRAM:
            histogram_graph_draw(renderer, font, chart->graph, chart->config, width, height);
            break;

        case CHART_PIE:
            pie_graph_draw(renderer, font, chart->graph, chart->config, width, height);
            break;

        case CHART_BAR:
            bar_graph_draw(renderer, font, chart->graph, chart->config, width, height);
            break;

        case CHART_COMBINATION:
            draw_succeeded = combination_graph_draw(
                renderer, font, chart->graph, chart->config, width, height
            );
            break;

        case CHART_COLUMN:
            column_graph_draw(
                renderer,
                font,
                chart->graph,
                chart->config,
                width,
                height
            );
            break;

        default:
            fprintf(stderr, "cannot render child chart with unknown type\n");
            draw_succeeded = false;
            break;
    }

    if (!draw_succeeded) {
        if (TTF_SetFontSize(font, parent_font_size) != 0) {
            fprintf(stderr, "could not restore parent chart font: %s\n", TTF_GetError());
        }
        return false;
    }

    if (TTF_SetFontSize(font, RENDERER_BASE_FONT_SIZE) != 0) {
        fprintf(
            stderr,
            "could not set fixed title and legend font: %s\n",
            TTF_GetError()
        );
        if (TTF_SetFontSize(font, parent_font_size) != 0) {
            fprintf(stderr, "could not restore parent chart font: %s\n", TTF_GetError());
        }
        return false;
    }

    if (chart->config->global.legend) {
        draw_legend(renderer, font, chart->graph, chart->config, width);
    }

    if (parent_config->specific.combination.show_sub_titles) {
        draw_title(renderer, font, chart->config->title, width);
    }

    if (TTF_SetFontSize(font, parent_font_size) != 0) {
        fprintf(stderr, "could not restore parent chart font: %s\n", TTF_GetError());
        return false;
    }

    return true;
}

bool combination_graph_draw(
    SDL_Renderer *renderer,
    TTF_Font *font,
    const Graph *graph,
    const ChartConfig *config,
    int width,
    int height
) {
    const CombinationGraph *combination =
        &graph->data.combination;

    int count = combination->chart_count;

    if (count <= 0) {
        fprintf(stderr, "cannot render an empty combination chart\n");
        return false;
    }

    int columns = (int)ceil(sqrt(count));
    int rows = (count + columns - 1) / columns;

    const int gap = 12;

    int cell_width = (width - gap * (columns + 1)) / columns;
    int cell_height = (height - gap * (rows + 1)) / rows;

    if (cell_width <= 0 || cell_height <= 0) {
        fprintf(stderr, "combination chart cells are too small\n");
        return false;
    }

    SDL_Rect parent_viewport;
    SDL_RenderGetViewport(renderer, &parent_viewport);

    for (int i = 0; i < count; i++) {
        int column = i % columns;
        int row = i / columns;

        SDL_Rect viewport = {
            parent_viewport.x + gap + column * (cell_width + gap),
            parent_viewport.y + gap + row * (cell_height + gap),
            cell_width,
            cell_height
        };

        if (SDL_RenderSetViewport(renderer, &viewport) != 0) {
            fprintf(
                stderr,
                "could not set combination chart viewport: %s\n",
                SDL_GetError()
            );

            if (SDL_RenderSetViewport(renderer, &parent_viewport) != 0) {
                fprintf(
                    stderr,
                    "could not restore combination chart viewport: %s\n",
                    SDL_GetError()
                );
            }
            return false;
        }

        if (!draw_child_chart(
            renderer,
            font,
            &combination->charts[i],
            config,
            cell_width,
            cell_height,
            width,
            height
        )) {
            if (SDL_RenderSetViewport(renderer, &parent_viewport) != 0) {
                fprintf(
                    stderr,
                    "could not restore combination chart viewport: %s\n",
                    SDL_GetError()
                );
            }
            return false;
        }
    }

    if (SDL_RenderSetViewport(renderer, &parent_viewport) != 0) {
        fprintf(
            stderr,
            "could not restore combination chart viewport: %s\n",
            SDL_GetError()
        );
        return false;
    }

    return true;
}