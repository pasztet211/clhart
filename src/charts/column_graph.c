#include "column_graph.h"
#include "parser_internal.h"

#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool parse_column_category(
    Parser *p,
    ColumnGraph *column
) {
    char *category = parse_name(p);

    if (!category)
        return false;

    char **new_categories = realloc(
        column->categories,
        sizeof(char *) * (column->category_count + 1)
    );

    if (!new_categories) {
        free(category);
        return false;
    }

    column->categories = new_categories;
    column->categories[column->category_count++] = category;

    return true;
}

static bool parse_categories(
    Parser *p,
    ColumnGraph *column
) {
    if (!expect_word(p, "categories") ||
        !expect(p, '{')) {
        fprintf(stderr, "expected 'categories { ... }'\n");
        return false;
    }

    while (true) {
        skip_whitespace(p);

        if (expect(p, '}'))
            break;

        if (!parse_column_category(p, column)) {
            fprintf(stderr, "invalid category\n");
            return false;
        }

        skip_whitespace(p);

        if (expect(p, '}'))
            break;

        if (!expect(p, ',')) {
            fprintf(stderr, "expected ',' or '}' after category\n");
            return false;
        }
    }

    if (column->category_count == 0) {
        fprintf(stderr, "categories must contain at least one category\n");
        return false;
    }

    column->has_categories = true;

    return true;
}

static bool append_column_value(
    ColumnSeries *series,
    double value
) {
    double *new_values = realloc(
        series->values,
        sizeof(double) * (series->value_count + 1)
    );

    if (!new_values)
        return false;

    series->values = new_values;
    series->values[series->value_count++] = value;

    return true;
}

static bool parse_column_color(
    Parser *p,
    ColumnSeries *series
) {
    if (!expect(p, ':') || !expect(p, '#'))
        return false;

    for (int i = 0; i < 6; i++) {
        char c = p->source[p->position++];

        if (!isxdigit((unsigned char)c))
            return false;

        series->color[i] = c;
    }

    series->color[6] = '\0';

    if (!expect(p, ':'))
        return false;

    series->has_color = true;

    return true;
}

static bool parse_column_series(
    Parser *p,
    ColumnGraph *column
) {
    ColumnSeries *new_series = realloc(
        column->series,
        sizeof(ColumnSeries) * (column->series_count + 1)
    );

    if (!new_series)
        return false;

    column->series = new_series;

    ColumnSeries *series =
        &column->series[column->series_count++];

    *series = (ColumnSeries){0};

    series->name = parse_name(p);

    if (!series->name)
        return false;

    if (!expect_operator(p, "---"))
        return false;

    while (true) {
        skip_whitespace(p);

        char *end;
        errno = 0;

        double value = strtod(
            p->source + p->position,
            &end
        );

        if (end == p->source + p->position ||
            errno == ERANGE ||
            !isfinite(value)) {
            fprintf(stderr, "invalid column value in series '%s'\n",
                    series->name);
            return false;
        }

        p->position = (size_t)(end - p->source);

        if (!append_column_value(series, value))
            return false;

        skip_whitespace(p);

        if (expect(p, ';'))
            continue;

        break;
    }

    if (series->value_count == 0)
        return false;

    skip_whitespace(p);

    if (p->source[p->position] == ':') {
        if (!parse_column_color(p, series)) {
            fprintf(stderr, "invalid color in series '%s'\n",
                    series->name);
            return false;
        }
    }

    return true;
}

static bool generate_categories(ColumnGraph *column) {
    int category_count = 0;

    for (int i = 0; i < column->series_count; i++) {
        if (column->series[i].value_count > category_count) {
            category_count = column->series[i].value_count;
        }
    }

    if (column->has_categories) {
        for (int i = 0; i < column->series_count; i++) {
            if (column->series[i].value_count > column->category_count) {
                fprintf(
                    stderr,
                    "series '%s' has more values than there are categories\n",
                    column->series[i].name
                );
                return false;
            }
        }
    } else {
        column->category_count = category_count;

        column->categories = calloc(
            (size_t)category_count,
            sizeof(char *)
        );

        if (!column->categories)
            return false;

        for (int i = 0; i < category_count; i++) {
            char label[32];

            snprintf(label, sizeof(label), "%d", i + 1);

            column->categories[i] = malloc(strlen(label) + 1);

            if (!column->categories[i])
                return false;

            strcpy(column->categories[i], label);
        }
    }

    // Pad shorter series with zeroes
    for (int i = 0; i < column->series_count; i++) {
        ColumnSeries *series = &column->series[i];

        if (series->value_count < column->category_count) {
            int old_count = series->value_count;

            double *new_values = realloc(
                series->values,
                sizeof(double) * column->category_count
            );

            if (!new_values)
                return false;

            series->values = new_values;

            for (int j = old_count; j < column->category_count; j++) {
                series->values[j] = 0.0;
            }

            series->value_count = column->category_count;
        }
    }

    return true;
}

bool column_graph_parse(Parser *p, Graph *graph) {
    ColumnGraph *column = &graph->data.column;

    *column = (ColumnGraph){0};

    skip_whitespace(p);

    // Optional categories block
    if (expect_word(p, "categories")) {
        p->position -= strlen("categories");

        if (!parse_categories(p, column))
            return false;
    }

    if (!expect_word(p, "columns") ||
        !expect(p, '{')) {
        fprintf(stderr, "expected 'columns { ... }'\n");
        return false;
    }

    while (true) {
        skip_whitespace(p);

        if (expect(p, '}'))
            break;

        if (!parse_column_series(p, column)) {
            fprintf(stderr, "invalid column series\n");
            return false;
        }

        skip_whitespace(p);

        if (expect(p, '}'))
            break;

        if (!expect(p, ',')) {
            fprintf(stderr, "expected ',' or '}' after column series\n");
            return false;
        }
    }

    if (column->series_count == 0) {
        fprintf(stderr, "column chart must contain at least one series\n");
        return false;
    }

    if (!generate_categories(column))
        return false;

    return true;
}

/* -------------------------------------------------- */
/* Rendering                                           */
/* -------------------------------------------------- */

static void draw_column_text(
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

static void draw_column_text_centered(
    SDL_Renderer *renderer,
    TTF_Font *font,
    const char *text,
    int center_x,
    int y,
    int index,
    SDL_Color color
) {
    int text_width;
    int text_height;

    if (TTF_SizeUTF8(font, text, &text_width, &text_height) != 0)
        return;

    if (index % 2 == 0)
        y += text_height;

    draw_column_text(
        renderer,
        font,
        text,
        center_x - text_width / 2,
        y,
        color
    );
}

static int column_map(
    double value,
    double min,
    double max,
    int top,
    int bottom
) {
    return bottom - (int)(
        (value - min) / (max - min) * (bottom - top)
    );
}

static void set_column_color(
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
        unsigned int rgb = (unsigned int)strtoul(
            series->color,
            NULL,
            16
        );

        SDL_SetRenderDrawColor(
            renderer,
            (rgb >> 16) & 0xFF,
            (rgb >> 8) & 0xFF,
            rgb & 0xFF,
            255
        );
    } else {
        SDL_Color color = default_colors[
            series_index %
            (sizeof(default_colors) / sizeof(default_colors[0]))
        ];

        SDL_SetRenderDrawColor(
            renderer,
            color.r,
            color.g,
            color.b,
            color.a
        );
    }
}

void column_graph_draw(
    SDL_Renderer *renderer,
    TTF_Font *font,
    const Graph *graph,
    const ChartConfig *config,
    int width,
    int height
) {
    const ColumnGraph *column = &graph->data.column;
    const ColumnConfig *settings = &config->specific.column;

    int category_count = column->category_count;
    int series_count = column->series_count;

    if (category_count <= 0 || series_count <= 0)
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
        for (int i = 0; i < series_count; i++) {
            for (int j = 0; j < category_count; j++) {
                double value = column->series[i].values[j];

                if (value < value_min)
                    value_min = value;

                if (value > value_max)
                    value_max = value;
            }
        }
    }

    // Ensure zero is visible
    if (value_min > 0.0)
        value_min = 0.0;

    if (value_max < 0.0)
        value_max = 0.0;

    if (value_max <= value_min) {
        value_min -= 1.0;
        value_max += 1.0;
    }

    double scale = fmin(width / 1000.0, height / 800.0);
    const int left = (int)lround(85 * scale * 1.15);
    const int top = (int)lround(70 * scale * 1.15);
    const int right = width - (int)lround(30 * scale * 1.15);
    const int bottom = height - (int)lround(90 * scale * 1.15);

    int plot_width = right - left;
    int plot_height = bottom - top;

    if (plot_width <= 0 || plot_height <= 0)
        return;

    int zero_y = column_map(
        0.0,
        value_min,
        value_max,
        top,
        bottom
    );

    // Axes
    SDL_SetRenderDrawColor(
        renderer,
        axis_color.r,
        axis_color.g,
        axis_color.b,
        axis_color.a
    );

    SDL_RenderDrawLine(
        renderer,
        left,
        top,
        left,
        bottom
    );

    SDL_RenderDrawLine(
        renderer,
        left,
        zero_y,
        right,
        zero_y
    );

    // Value-axis numbers and grid lines
    if (settings->show_axis_numbers) {
        for (int i = 0; i <= 5; i++) {
            double value = value_min +
                (value_max - value_min) * i / 5.0;

            int y = column_map(
                value,
                value_min,
                value_max,
                top,
                bottom
            );

            char label[32];
            snprintf(label, sizeof(label), "%.4g", value);

            SDL_SetRenderDrawColor(
                renderer,
                grid_color.r,
                grid_color.g,
                grid_color.b,
                grid_color.a
            );

            SDL_RenderDrawLine(
                renderer,
                left,
                y,
                right,
                y
            );

            int text_width;
            int text_height;

            if (TTF_SizeUTF8(
                font,
                label,
                &text_width,
                &text_height
            ) == 0) {
                draw_column_text(
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

    // Clip columns to the plotting area
    SDL_Rect clip = {
        left,
        top,
        plot_width,
        plot_height
    };

    SDL_RenderSetClipRect(renderer, &clip);

    float slot_width = (float)plot_width / category_count;
    float group_width = slot_width * 0.8f;
    float bar_width = group_width / series_count;

    for (int category = 0; category < category_count; category++) {
        float group_start =
            left + slot_width * category +
            (slot_width - group_width) / 2.0f;

        for (int s = 0; s < series_count; s++) {
            const ColumnSeries *series = &column->series[s];

            double value = series->values[category];

            int value_y = column_map(
                value,
                value_min,
                value_max,
                top,
                bottom
            );

            int x = (int)(group_start + s * bar_width);
            int next_x = (int)(group_start + (s + 1) * bar_width);

            SDL_Rect rectangle = {
                x,
                value < 0.0 ? zero_y : value_y,
                next_x - x,
                abs(zero_y - value_y)
            };

            set_column_color(renderer, series, s);
            SDL_RenderFillRect(renderer, &rectangle);
        }
    }

    SDL_RenderSetClipRect(renderer, NULL);

    // Category labels and optional value labels
    for (int category = 0; category < category_count; category++) {
        float group_start =
            left + slot_width * category +
            (slot_width - group_width) / 2.0f;

        int center_x = (int)(
            left + slot_width * (category + 0.5f)
        );

        draw_column_text_centered(
            renderer,
            font,
            column->categories[category],
            center_x,
            bottom + 12,
            category,
            text_color
        );

        if (settings->show_values) {
            for (int s = 0; s < series_count; s++) {
                const ColumnSeries *series = &column->series[s];

                double value = series->values[category];

                int value_y = column_map(
                    value,
                    value_min,
                    value_max,
                    top,
                    bottom
                );

                int x = (int)(
                    group_start + (s + 0.5f) * bar_width
                );

                char label[32];
                snprintf(label, sizeof(label), "%.4g", value);

                int text_height;

                if (TTF_SizeUTF8(
                    font,
                    label,
                    NULL,
                    &text_height
                ) != 0) {
                    continue;
                }

                int label_y = value >= 0.0
                    ? value_y - text_height - 5
                    : value_y + 5;

                draw_column_text_centered(
                    renderer,
                    font,
                    label,
                    x,
                    label_y,
                    category,
                    text_color
                );
            }
        }
    }
}