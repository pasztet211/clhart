#include "bar_graph.h"
#include "parser_internal.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <string.h>

static bool parse_bar(
    Parser *p,
    BarGraph *bar_graph,
    ChartConfig *config
) {
    Bar bar = {0};

    bar.name = parse_name(p, config);

    if (!bar.name)
        return false;

    if (!expect_operator(p, "---"))
        goto failure;

    skip_whitespace(p);

    char *end;
    bar.value = strtod(p->source + p->position, &end);

    if (end == p->source + p->position ||
        !isfinite(bar.value) ||
        bar.value < 0.0) {
        goto failure;
    }

    p->position = (size_t)(end - p->source);

    skip_whitespace(p);

    if (expect(p, ':')) {
        if (!expect(p, '#'))
            goto failure;

        for (int i = 0; i < 6; i++) {
            char c = p->source[p->position++];

            if (!isxdigit((unsigned char)c))
                goto failure;

            bar.color[i] = c;
        }

        bar.color[6] = '\0';

        if (!expect(p, ':'))
            goto failure;

        bar.has_color = true;
    }

    Bar *new_bars = realloc(
        bar_graph->bars,
        sizeof(Bar) * (bar_graph->bar_count + 1)
    );

    if (!new_bars)
        goto failure;

    bar_graph->bars = new_bars;
    bar_graph->bars[bar_graph->bar_count++] = bar;

    return true;

failure:
    free(bar.name);
    return false;
}

bool bar_graph_parse(Parser *p, Graph *graph, ChartConfig *config) {
    BarGraph *bar_graph = &graph->data.bar;

    *bar_graph = (BarGraph){0};

    if (!expect_word(p, "bars") ||
        !expect(p, '{')) {
        fprintf(stderr, "expected 'bars { ... }'\n");
        return false;
    }

    while (true) {
        skip_whitespace(p);

        if (expect(p, '}'))
            break;

        if (!parse_bar(p, bar_graph, config)) {
            fprintf(stderr, "invalid bar\n");
            return false;
        }

        skip_whitespace(p);

        if (expect(p, '}'))
            break;

        if (!expect(p, ',')) {
            fprintf(stderr, "expected ',' or '}' after bar\n");
            return false;
        }
    }

    if (bar_graph->bar_count == 0) {
        fprintf(stderr, "bar chart must contain at least one bar\n");
        return false;
    }

    return true;
}

static void draw_bar_text(
    SDL_Renderer *renderer,
    TTF_Font *font,
    const char *text,
    int x,
    int y,
    SDL_Color color
) {
    SDL_Surface *surface = TTF_RenderUTF8_Blended(
        font, text, color
    );

    if (!surface)
        return;

    SDL_Texture *texture = SDL_CreateTextureFromSurface(
        renderer, surface
    );

    if (texture) {
        SDL_Rect destination = {
            x, y, surface->w, surface->h
        };

        SDL_RenderCopy(renderer, texture, NULL, &destination);
        SDL_DestroyTexture(texture);
    }

    SDL_FreeSurface(surface);
}

static void draw_bar_text_centered(
    SDL_Renderer *renderer,
    TTF_Font *font,
    const char *text,
    int center_x,
    int y,
    SDL_Color color
) {
    int text_width;
    int text_height;

    if (TTF_SizeUTF8(font, text, &text_width, &text_height) != 0)
        return;

    draw_bar_text(
        renderer, font, text,
        center_x - text_width / 2,
        y,
        color
    );
}

static double bar_map(
    double value,
    double min,
    double max,
    double start,
    double length
) {
    return start + (value - min) / (max - min) * length;
}

void bar_graph_draw(
    SDL_Renderer *renderer,
    TTF_Font *font,
    const Graph *graph,
    const ChartConfig *config,
    int width,
    int height
) {
    const BarGraph *bar_graph = &graph->data.bar;
    const BarConfig *settings = &config->specific.bar;

    int count = bar_graph->bar_count;

    if (count <= 0)
        return;

    const SDL_Color text_color = {220, 220, 220, 255};
    const SDL_Color axis_color = {220, 220, 220, 255};
    const SDL_Color grid_color = {75, 78, 90, 255};

    double value_min = settings->has_y_range
        ? settings->y_min
        : 0.0;

    double value_max = settings->has_y_range
        ? settings->y_max
        : 0.0;

    if (!settings->has_y_range) {
        for (int i = 1; i < count; i++) {
            double value = bar_graph->bars[i].value;

            if (value < value_min)
                value_min = value;

            if (value > value_max)
                value_max = value;
        }
    }

    if (value_max <= value_min)
        value_max = value_min + 1.0;

    double scale = fmin(width / 1000.0, height / 800.0);

    // Scale plot margins with the chart cell so combination charts keep a useful plot area.
    float left = (settings->horizontal ? 180.0f : 100.0f) * scale * 1.15f;
    float top = 80.0f * scale * 1.15f;
    float right = width - 70.0f * scale * 1.15f;
    float bottom = height -
        (settings->horizontal ? 100.0f : 140.0f) * scale * 1.15f;

    float plot_width = right - left;
    float plot_height = bottom - top;

    if (plot_width <= 0 || plot_height <= 0)
        return;

    // Axis
    SDL_SetRenderDrawColor(
        renderer,
        axis_color.r, axis_color.g, axis_color.b, axis_color.a
    );

    if (settings->horizontal) {
        // Value axis
        SDL_RenderDrawLine(
            renderer,
            (int)left, (int)bottom,
            (int)right, (int)bottom
        );

        // Category axis
        SDL_RenderDrawLine(
            renderer,
            (int)left, (int)top,
            (int)left, (int)bottom
        );
    } else {
        // Value axis
        SDL_RenderDrawLine(
            renderer,
            (int)left, (int)top,
            (int)left, (int)bottom
        );

        // Category axis
        SDL_RenderDrawLine(
            renderer,
            (int)left, (int)bottom,
            (int)right, (int)bottom
        );
    }

    // Value-axis numbers and grid lines
    if (settings->show_axis_numbers) {
        for (int i = 0; i <= 5; i++) {
            double value = value_min +
                (value_max - value_min) * i / 5.0;

            char label[32];
            snprintf(label, sizeof(label), "%.4g", value);

            SDL_SetRenderDrawColor(
                renderer,
                grid_color.r, grid_color.g,
                grid_color.b, grid_color.a
            );

            if (settings->horizontal) {
                int x = (int)bar_map(
                    value, value_min, value_max,
                    left, plot_width
                );

                SDL_RenderDrawLine(
                    renderer, x, (int)top,
                    x, (int)bottom
                );

                draw_bar_text_centered(
                    renderer, font, label,
                    x, (int)bottom + 12,
                    text_color
                );
            } else {
                int y = (int)(
                    bottom -
                    (value - value_min) /
                    (value_max - value_min) * plot_height
                );

                SDL_RenderDrawLine(
                    renderer, (int)left, y,
                    (int)right, y
                );

                int text_width;
                int text_height;

                if (TTF_SizeUTF8(
                    font, label, &text_width, &text_height
                ) == 0) {
                    draw_bar_text(
                        renderer, font, label,
                        (int)left - text_width - 12,
                        y - text_height / 2,
                        text_color
                    );
                }
            }
        }
    }

    // Clip bars to the plotting area
    SDL_Rect clip = {
        (int)left,
        (int)top,
        (int)plot_width,
        (int)plot_height
    };

    SDL_RenderSetClipRect(renderer, &clip);

    float slot_size = settings->horizontal
        ? plot_height / count
        : plot_width / count;

    float bar_size = slot_size * 0.65f;

    for (int i = 0; i < count; i++) {
        const Bar *bar = &bar_graph->bars[i];

        // Color
        if (bar->has_color) {
            unsigned int rgb = (unsigned int)strtoul(
                bar->color, NULL, 16
            );

            SDL_SetRenderDrawColor(
                renderer,
                (rgb >> 16) & 0xFF,
                (rgb >> 8) & 0xFF,
                rgb & 0xFF,
                255
            );
        } else {
            SDL_SetRenderDrawColor(
                renderer, 50, 120, 255, 255
            );
        }

        // Shift the value so the minimum becomes zero
        double shifted_value = bar->value - value_min;

        if (shifted_value < 0)
            shifted_value = 0;

        if (shifted_value > value_max - value_min)
            shifted_value = value_max - value_min;

        SDL_Rect rectangle;

        if (settings->horizontal) {
            float center_y = top + slot_size * (i + 0.5f);

            int x_end = (int)bar_map(
                value_min + shifted_value,
                value_min,
                value_max,
                left,
                plot_width
            );

            rectangle = (SDL_Rect) {
                (int)left,
                (int)(center_y - bar_size / 2),
                x_end - (int)left,
                (int)bar_size
            };
        } else {
            float center_x = left + slot_size * (i + 0.5f);

            int y_end = (int)(
                bottom - shifted_value /
                (value_max - value_min) * plot_height
            );

            rectangle = (SDL_Rect) {
                (int)(center_x - bar_size / 2),
                y_end,
                (int)bar_size,
                (int)bottom - y_end
            };
        }

        SDL_RenderFillRect(renderer, &rectangle);
    }

    SDL_RenderSetClipRect(renderer, NULL);

    // Category names and optional value labels
    for (int i = 0; i < count; i++) {
        const Bar *bar = &bar_graph->bars[i];

        char value_label[32];
        snprintf(
            value_label, sizeof(value_label),
            "%.4g", bar->value
        );

        if (settings->horizontal) {
            float center_y = top + slot_size * (i + 0.5f);

            int text_width;
            int text_height;

            if (TTF_SizeUTF8(
                font, bar->name, &text_width, &text_height
            ) == 0) {
                draw_bar_text(
                    renderer, font, bar->name,
                    (int)left - text_width - 12,
                    (int)(center_y - text_height / 2),
                    text_color
                );
            }

            if (settings->show_values) {
                double shifted_value = bar->value - value_min;

                if (shifted_value < 0)
                    shifted_value = 0;

                if (shifted_value > value_max - value_min)
                    shifted_value = value_max - value_min;

                int x = (int)bar_map(
                    value_min + shifted_value,
                    value_min, value_max,
                    left, plot_width
                );

                draw_bar_text(
                    renderer, font, value_label,
                    x + 8,
                    (int)(center_y - 8),
                    text_color
                );
            }
        } else {
            float center_x = left + slot_size * (i + 0.5f);

            draw_bar_text_centered(
                renderer, font, bar->name,
                (int)center_x,
                (int)bottom + 12,
                text_color
            );

            if (settings->show_values) {
                double shifted_value = bar->value - value_min;

                if (shifted_value < 0)
                    shifted_value = 0;

                if (shifted_value > value_max - value_min)
                    shifted_value = value_max - value_min;

                int y = (int)(
                    bottom - shifted_value /
                    (value_max - value_min) * plot_height
                );

                int text_width;
                int text_height;

                if (TTF_SizeUTF8(
                    font, value_label,
                    &text_width, &text_height
                ) == 0) {
                    draw_bar_text(
                        renderer, font, value_label,
                        (int)center_x - text_width / 2,
                        y - text_height - 6,
                        text_color
                    );
                }
            }
        }
    }
}