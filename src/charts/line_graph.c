#include "line_graph.h"
#include "renderer.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

static void draw_line_text(
    SDL_Renderer *renderer,
    TTF_Font *font,
    const char *text,
    int x,
    int y,
    SDL_Color color
) {
    SDL_Surface *surface = TTF_RenderUTF8_Blended(font, text, color);

    if (!surface)
        return;

    SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surface);

    if (texture) {
        SDL_Rect destination = {x, y, surface->w, surface->h};
        SDL_RenderCopy(renderer, texture, NULL, &destination);
        SDL_DestroyTexture(texture);
    }

    SDL_FreeSurface(surface);
}

static int map_line_value(
    double value,
    double min,
    double max,
    int top,
    int bottom
) {
    return bottom - (int)((value - min) / (max - min) * (bottom - top));
}

static void set_line_color(
    SDL_Renderer *renderer,
    const ColumnSeries *series,
    int series_index
) {
    static const SDL_Color default_colors[] = {
        {50, 120, 255, 255},
        {255, 100, 80, 255},
        {70, 200, 130, 255},
        {200, 130, 255, 255},
        {255, 190, 60, 255},
        {60, 200, 210, 255}
    };

    if (series->has_color) {
        unsigned int rgb = (unsigned int)strtoul(series->color, NULL, 16);
        SDL_SetRenderDrawColor(
            renderer,
            (rgb >> 16) & 0xFF,
            (rgb >> 8) & 0xFF,
            rgb & 0xFF,
            255
        );
        return;
    }

    SDL_Color color = default_colors[
        series_index % (sizeof(default_colors) / sizeof(default_colors[0]))
    ];
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
}

static void draw_line_point(
    SDL_Renderer *renderer,
    int center_x,
    int center_y,
    int radius
) {
    for (int y = -radius; y <= radius; y++) {
        int half_width = (int)sqrt(radius * radius - y * y);
        SDL_RenderDrawLine(
            renderer,
            center_x - half_width,
            center_y + y,
            center_x + half_width,
            center_y + y
        );
    }
}

static double get_nice_max(double value_min, double value_max) {
    double range = value_max - value_min;
    if (range <= 1e-12) return value_min + 5.0; // Guard against flat line

    // Find what 1/5th of the range looks like, then snap it to a clean step
    double rough_step = range / 5.0;
    double mag = pow(10.0, floor(log10(rough_step)));
    double norm = rough_step / mag;
    double nice_step;

    if (norm <= 1.0)      nice_step = 1.0 * mag;
    else if (norm <= 2.0) nice_step = 2.0 * mag;
    else if (norm <= 2.5) nice_step = 2.5 * mag;
    else if (norm <= 5.0) nice_step = 5.0 * mag;
    else                  nice_step = 10.0 * mag;

    // Return the exact max needed so (max - min) / 5.0 == nice_step
    return value_min + (nice_step * 5.0);
}

void line_graph_draw(
    SDL_Renderer *renderer,
    TTF_Font *font,
    const Graph *graph,
    const ChartConfig *config,
    int width,
    int height
) {
    const ColumnGraph *line = &graph->data.line;
    const ColumnConfig *settings = &config->specific.line;
    int category_count = line->category_count;
    int series_count = line->series_count;

    if (category_count <= 0 || series_count <= 0)
        return;

    double value_min = settings->has_y_range ? settings->y_min : line->series[0].values[0];
    double value_max = settings->has_y_range ? settings->y_max : line->series[0].values[0];

    if (!settings->has_y_range) {
        for (int i = 0; i < series_count; i++) {
            for (int j = 0; j < category_count; j++) {
                double value = line->series[i].values[j];
                if (value < value_min)
                    value_min = value;
                if (value > value_max)
                    value_max = value;
            }
        }
    }

    if (value_max <= value_min) {
        value_min -= 1.0;
        value_max += 1.0;
    }

    double scale = fmin(width / 1000.0, height / 800.0);
    int left = (int)lround(85 * scale * 1.15);
    int top = (int)lround(70 * scale * 1.15);
    int right = width - (int)lround(30 * scale * 1.15);
    int bottom = height - (int)lround(90 * scale * 1.15);

    if (right <= left || bottom <= top)
        return;

    const SDL_Color text_color = {220, 220, 220, 255};
    const SDL_Color axis_color = {220, 220, 220, 255};
    const SDL_Color grid_color = {75, 78, 90, 255};

    SDL_SetRenderDrawColor(
        renderer,
        axis_color.r,
        axis_color.g,
        axis_color.b,
        axis_color.a
    );
    SDL_RenderDrawLine(renderer, left, top, left, bottom);
    SDL_RenderDrawLine(renderer, left, bottom, right, bottom);

    if (settings->show_axis_numbers) {
        for (int i = 0; i <= 5; i++) {
            value_max = get_nice_max(value_min, value_max);
            double value = value_min + (value_max - value_min) * i / 5.0;
            int y = map_line_value(value, value_min, value_max, top, bottom);
            char label[32];
            int text_width;
            int text_height;
            snprintf(label, sizeof(label), "%.4g", value);

            SDL_SetRenderDrawColor(
                renderer,
                grid_color.r,
                grid_color.g,
                grid_color.b,
                grid_color.a
            );
            SDL_RenderDrawLine(renderer, left, y, right, y);

            if (TTF_SizeUTF8(font, label, &text_width, &text_height) == 0) {
                draw_line_text(
                    renderer,
                    font,
                    label,
                    left - text_width - 12,
                    y - text_height / 2,
                    text_color
                );
            }
        }
    }

    SDL_Rect clip = {left, top, right - left, bottom - top};
    SDL_RenderSetClipRect(renderer, &clip);

    int point_radius = (int)lround(4 * scale);
    if (point_radius < 2)
        point_radius = 2;

    for (int s = 0; s < series_count; s++) {
        const ColumnSeries *series = &line->series[s];
        set_line_color(renderer, series, s);

        int previous_x = 0;
        int previous_y = 0;

        for (int category = 0; category < category_count; category++) {
            int x = category_count == 1
                ? (left + right) / 2
                : left + (int)((double)category * (right - left) /
                               (category_count - 1));
            int y = map_line_value(
                series->values[category],
                value_min,
                value_max,
                top,
                bottom
            );

            if (category > 0)
                SDL_RenderDrawLine(renderer, previous_x, previous_y, x, y);

            draw_line_point(renderer, x, y, point_radius);
            previous_x = x;
            previous_y = y;
        }
    }

    SDL_RenderSetClipRect(renderer, NULL);

    for (int category = 0; category < category_count; category++) {
        int x = category_count == 1
            ? (left + right) / 2
            : left + (int)((double)category * (right - left) /
                           (category_count - 1));
        int text_width;
        int text_height;

        if (TTF_SizeUTF8(
            font,
            line->categories[category],
            &text_width,
            &text_height
        ) == 0) {
            int label_y = bottom + 12 +
                (category % 2 == 0 ? text_height : 0);
            draw_line_text(
                renderer,
                font,
                line->categories[category],
                x - text_width / 2,
                label_y,
                text_color
            );
        }

        if (settings->show_values) {
            for (int s = 0; s < series_count; s++) {
                int y = map_line_value(
                    line->series[s].values[category],
                    value_min,
                    value_max,
                    top,
                    bottom
                );
                char label[32];
                snprintf(label, sizeof(label), "%.4g", line->series[s].values[category]);

                if (TTF_SizeUTF8(font, label, &text_width, &text_height) == 0) {
                    draw_line_text(
                        renderer,
                        font,
                        label,
                        x - text_width / 2,
                        y - text_height - 5 - s * text_height,
                        text_color
                    );
                }
            }
        }
    }
}
