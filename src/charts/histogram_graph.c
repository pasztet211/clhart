#include "histogram_graph.h"
#include "parser_internal.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool parse_parameter_value(Parser *p, double *value) {
    skip_whitespace(p);

    char *end;
    double parsed = strtod(p->source + p->position, &end);

    if (end == p->source + p->position || !isfinite(parsed)) {
        return false;
    }

    p->position = (size_t)(end - p->source);
    *value = parsed;

    return true;
}

static bool parse_fuzzy_parameter(
    Parser *p,
    HistogramGraph *histogram
) {
    FuzzyParameter parameter = {0};

    parameter.name = parse_name(p);

    if (!parameter.name) {
        return false;
    }

    if (!expect_operator(p, "---")) {
        goto failure;
    }

    if (!parse_parameter_value(p, &parameter.a) ||
        !expect(p, ';') ||
        !parse_parameter_value(p, &parameter.b) ||
        !expect(p, ';') ||
        !parse_parameter_value(p, &parameter.c))
        goto failure;

    // Check for an optional fourth value.
    if (expect(p, ';')) {
        if (!parse_parameter_value(p, &parameter.d))
            goto failure;

        parameter.trapezoidal = true;

        if (parameter.a > parameter.b ||
            parameter.b > parameter.c ||
            parameter.c > parameter.d) {
            fprintf(stderr, "invalid trapezoidal parameter ordering\n");
            goto failure;
        }
    } else {
        if (parameter.a > parameter.b ||
            parameter.b > parameter.c) {
            fprintf(stderr, "invalid triangular parameter ordering\n");
            goto failure;
        }
    }

    skip_whitespace(p);

    if (expect(p, ':')) {
        if (!expect(p, '#'))
            goto failure;

        for (int i = 0; i < 6; i++) {
            char c = p->source[p->position++];

            if (!isxdigit((unsigned char)c))
                goto failure;

            parameter.color[i] = c;
        }

        parameter.color[6] = '\0';

        if (!expect(p, ':'))
            goto failure;

        parameter.has_color = true;
    }

    FuzzyParameter *new_parameters = realloc(
        histogram->data.fuzzy.parameters,
        sizeof(FuzzyParameter) * (histogram->data.fuzzy.parameter_count + 1)
    );

    if (!new_parameters) {
        goto failure;
    }

    histogram->data.fuzzy.parameters = new_parameters;
    histogram->data.fuzzy.parameters[histogram->data.fuzzy.parameter_count++] = parameter;

    return true;

failure:
    free(parameter.name);
    return false;
}

static bool parse_distribution_parameter(
    Parser *p,
    HistogramGraph *histogram
) {
    DistributionParameter parameter = {0};

    parameter.name = parse_name(p);

    if (!parameter.name) {
        return false;
    }

    if (!expect_operator(p, "---")) {
        goto failure;
    }

    if (!parse_parameter_value(p, &parameter.min) ||
        !expect(p, ';') ||
        !parse_parameter_value(p, &parameter.max) ||
        !expect(p, '|') ||
        !parse_parameter_value(p, &parameter.amount))
        goto failure;

    if (parameter.min >= parameter.max || parameter.amount < 0) {
        fprintf(stderr, "invalid distribution parameter\n");
        goto failure;
    }

    skip_whitespace(p);

    if (expect(p, ':')) {
        if (!expect(p, '#'))
            goto failure;

        for (int i = 0; i < 6; i++) {
            char c = p->source[p->position++];

            if (!isxdigit((unsigned char)c))
                goto failure;

            parameter.color[i] = c;
        }

        parameter.color[6] = '\0';

        if (!expect(p, ':'))
            goto failure;

        parameter.has_color = true;
    }

    DistributionParameter *new_parameters = realloc(
        histogram->data.distribution.parameters,
        sizeof(DistributionParameter) *
        (histogram->data.distribution.parameter_count + 1)
    );

    if (!new_parameters) {
        goto failure;
    }

    histogram->data.distribution.parameters = new_parameters;
    histogram->data.distribution.parameters[
        histogram->data.distribution.parameter_count++
    ] = parameter;

    return true;

failure:
    free(parameter.name);
    return false;
}

bool histogram_graph_parse(
    Parser *p,
    Graph *graph,
    ChartConfig *config
) {
    HistogramGraph *histogram = &graph->data.histogram;
    HistogramConfig *settings = &config->specific.histogram;

    *histogram = (HistogramGraph){0};

    bool distribution = settings->type == HISTOGRAM_DISTRIBUTION;

    if (!expect_word(p, "parameters") ||
        !expect(p, '{')) {
        return false;
    }

    while (true) {
        skip_whitespace(p);

        if (expect(p, '}')) {
            break;
        }

        bool success = distribution
            ? parse_distribution_parameter(p, histogram)
            : parse_fuzzy_parameter(p, histogram);

        if (!success) {
            return false;
        }

        skip_whitespace(p);

        if (expect(p, '}')) {
            break;
        }

        if (!expect(p, ',')) {
            fprintf(stderr, "expected ',' or '}' after histogram parameter\n");
            return false;
        }
    }

    int count = distribution
        ? histogram->data.distribution.parameter_count
        : histogram->data.fuzzy.parameter_count;

    if (count == 0) {
        fprintf(stderr, "histogram must contain at least one parameter\n");
        return false;
    }

    double x_min;
    double x_max;
    double y_max = 0.0;

    if (distribution) {
        DistributionParameter *first =
            &histogram->data.distribution.parameters[0];

        x_min = first->min;
        x_max = first->max;
        y_max = first->amount;

        for (int i = 1; i < count; i++) {
            DistributionParameter *parameter =
                &histogram->data.distribution.parameters[i];

            if (parameter->min < x_min)
                x_min = parameter->min;

            if (parameter->max > x_max)
                x_max = parameter->max;

            if (parameter->amount > y_max)
                y_max = parameter->amount;
        }
    } else {
        FuzzyParameter *first =
            &histogram->data.fuzzy.parameters[0];

        x_min = first->a;
        x_max = first->trapezoidal ? first->d : first->c;

        for (int i = 1; i < count; i++) {
            FuzzyParameter *parameter =
                &histogram->data.fuzzy.parameters[i];

            if (parameter->a < x_min)
                x_min = parameter->a;

            double parameter_max =
                parameter->trapezoidal ? parameter->d : parameter->c;

            if (parameter_max > x_max)
                x_max = parameter_max;
        }
    }

    if (!settings->has_x_range) {
        settings->x_min = x_min;
        settings->x_max = x_max + 0.5;
    }

    if (!settings->has_y_range) {
        settings->y_min = 0.0;
        settings->y_max = distribution
            ? (y_max > 0.0 ? y_max * 1.1 : 1.0)
            : 1.0;
    }

    return true;
}

static float map_x(double value, double min, double max,
                   float left, float width) {
    return left + (float)((value - min) / (max - min)) * width;
}

static float map_y(
    double value,
    double min,
    double max,
    float top,
    float height
) {
    return top + height -
        (float)((value - min) / (max - min)) * height;
}

static void draw_thick_line(
    SDL_Renderer *renderer,
    float x1, float y1,
    float x2, float y2,
    int thickness
) {
    float dx = x2 - x1;
    float dy = y2 - y1;
    float length = sqrtf(dx * dx + dy * dy);

    if (length == 0.0f)
        return;

    for (int i = 0; i < thickness; i++) {
        float offset = i - (thickness - 1) / 2.0f;

        float ox = -dy / length * offset;
        float oy = dx / length * offset;

        SDL_RenderDrawLineF(
            renderer,
            x1 + ox, y1 + oy,
            x2 + ox, y2 + oy
        );
    }
}

static void draw_membership(
    SDL_Renderer *renderer,
    const FuzzyParameter *parameter,
    double x_min,
    double x_max,
    double y_min,
    double y_max,
    float left,
    float top,
    float plot_width,
    float plot_height
) {
    double a = parameter->a;
    double b = parameter->b;
    double c = parameter->c;
    double d = parameter->d;

    double points_x[4] = {a, b, c, 0.0};
    double points_y[4] = {0.0, 1.0, 0.0, 0.0};

    int segment_count = 2;

    if (parameter->trapezoidal) {
        points_x[2] = parameter->c;
        points_x[3] = parameter->d;

        points_y[2] = 1.0;
        points_y[3] = 0.0;

        segment_count = 3;
    }

    if (parameter->has_color) {
        unsigned int color = (unsigned int)strtoul(
            parameter->color, NULL, 16
        );

        SDL_SetRenderDrawColor(
            renderer,
            (color >> 16) & 0xFF,
            (color >> 8) & 0xFF,
            color & 0xFF,
            255
        );
    } else {
        SDL_SetRenderDrawColor(renderer, 50, 120, 255, 255);
    }

    for (int i = 0; i < segment_count; i++) {
        double x1 = points_x[i];
        double y1 = points_y[i];
        double x2 = points_x[i + 1];
        double y2 = points_y[i + 1];

        // Clip the segment horizontally to the visible range.
        if (x1 < x_min) {
            if (x2 <= x_min) continue;

            double t = (x_min - x1) / (x2 - x1);
            y1 += t * (y2 - y1);
            x1 = x_min;
        }

        if (x2 > x_max) {
            if (x1 >= x_max) continue;

            double t = (x_max - x1) / (x2 - x1);
            y2 = y1 + t * (y2 - y1);
            x2 = x_max;
        }

        draw_thick_line(
            renderer,
            map_x(x1, x_min, x_max, left, plot_width),
            map_y(y1, y_min, y_max, top, plot_height),
            map_x(x2, x_min, x_max, left, plot_width),
            map_y(y2, y_min, y_max, top, plot_height),
            3
        );
    }
}

static double calculate_grid_step(double min, double max) {
    double step = 10.0;

    while (step > (max - min) / 2.0) {
        step /= 2.0;
    }

    return step;
}

static void draw_grid_level(
    SDL_Renderer *renderer,
    bool vertical,
    double min,
    double max,
    double step,
    int level,
    int thickness,
    float left,
    float top,
    float plot_width,
    float plot_height
) {
    long long first = (long long)ceil(min / step - 1e-9);
    long long last = (long long)floor(max / step + 1e-9);

    for (long long k = first; k <= last; k++) {
        // Skip lines already drawn by a stronger grid level.
        if (level < 2 && k % 2 == 0)
            continue;

        double value = k * step;

        if (vertical) {
            float x = map_x(value, min, max, left, plot_width);

            draw_thick_line(
                renderer, x, top,
                x, top + plot_height,
                thickness
            );
        } else {
            float y = map_y(value, min, max, top, plot_height);

            draw_thick_line(
                renderer, left, y,
                left + plot_width, y,
                thickness
            );
        }
    }
}

static void draw_histogram_text(
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

void histogram_graph_draw(
    SDL_Renderer *renderer,
    TTF_Font *font,
    const Graph *graph,
    const ChartConfig *config,
    int width,
    int height
) {
    (void)font;

    const HistogramGraph *histogram = &graph->data.histogram;
    const HistogramConfig *settings = &config->specific.histogram;

    int parameter_count = settings->type == HISTOGRAM_DISTRIBUTION
        ? histogram->data.distribution.parameter_count
        : histogram->data.fuzzy.parameter_count;

    if (parameter_count == 0)
        return;

    double scale = fmin(width / 1000.0, height / 800.0);

    // Scale plot margins with the chart cell so combination charts keep a useful plot area.
    float left = 90 * scale * 1.15;
    float top = 80 * scale * 1.15;
    float right = width - 70 * scale * 1.15;
    float bottom = height - 90 * scale * 1.15;

    float plot_width = right - left;
    float plot_height = bottom - top;

    double x_min = settings->x_min;
    double x_max = settings->x_max;
    double y_min = settings->y_min;
    double y_max = settings->y_max;

    if (x_min >= x_max || y_min >= y_max)
        return;

    // Grid
    if (settings->show_grid) {
        double x_step = calculate_grid_step(x_min, x_max);
        double y_step = calculate_grid_step(y_min, y_max);

        for (int level = 0; level < 3; level++) {
            int divisions = level == 0 ? 4 : level == 1 ? 2 : 1;

            double x_level_step = x_step / divisions;
            double y_level_step = y_step / divisions;

            if (level == 0) {
                // Minor: every 2.5 when major is 10.
                SDL_SetRenderDrawColor(renderer, 38, 41, 49, 255);
            } else if (level == 1) {
                // Medium: every 5.
                SDL_SetRenderDrawColor(renderer, 50, 53, 62, 255);
            } else {
                // Major: every 10.
                SDL_SetRenderDrawColor(renderer, 75, 78, 90, 255);
            }

            int thickness = level == 2 ? 2 : 1;

            draw_grid_level(
                renderer, true,
                x_min, x_max, x_level_step,
                level, thickness,
                left, top, plot_width, plot_height
            );

            draw_grid_level(
                renderer, false,
                y_min, y_max, y_level_step,
                level, thickness,
                left, top, plot_width, plot_height
            );
        }
    }
    // Axes
    SDL_SetRenderDrawColor(renderer, 220, 220, 220, 255);

    // Y axis
    draw_thick_line(
        renderer,
        left, top,
        left, bottom,
        3
    );

    // X axis
    draw_thick_line(
        renderer,
        left, bottom,
        right, bottom,
        3
    );

    if (settings->show_axis_numbers) {
        SDL_Color text_color = {220, 220, 220, 255};

        double x_step = calculate_grid_step(x_min, x_max);
        double y_step = calculate_grid_step(y_min, y_max);

        // X-axis numbers
        for (double value = ceil(x_min / x_step) * x_step;
            value <= x_max;
            value += x_step) {

            char label[32];
            snprintf(label, sizeof(label), "%.4g", value);

            int x = (int)map_x(
                value, x_min, x_max, left, plot_width
            );

            int text_width, text_height;

            if (TTF_SizeUTF8(font, label, &text_width, &text_height) == 0) {
                draw_histogram_text(
                    renderer, font, label,
                    x - text_width / 2,
                    (int)bottom + 10,
                    text_color
                );
            }
        }

        // Y-axis numbers
        for (double value = ceil(y_min / y_step) * y_step;
            value <= y_max;
            value += y_step) {

            char label[32];
            snprintf(label, sizeof(label), "%.4g", value);

            int y = (int)map_y(
                value, y_min, y_max, top, plot_height
            );

            int text_width, text_height;

            if (TTF_SizeUTF8(font, label, &text_width, &text_height) == 0) {
                draw_histogram_text(
                    renderer, font, label,
                    (int)left - text_width - 10,
                    y - text_height / 2,
                    text_color
                );
            }
        }
    }

    if (settings->type == HISTOGRAM_DISTRIBUTION) {
        SDL_Rect clip = {
            (int)left,
            (int)top,
            (int)plot_width,
            (int)plot_height
        };

        SDL_RenderSetClipRect(renderer, &clip);

        for (int i = 0; i < histogram->data.distribution.parameter_count; i++) {
            const DistributionParameter *parameter =
                &histogram->data.distribution.parameters[i];

            if (parameter->has_color) {
                unsigned int rgb = (unsigned int)strtoul(
                    parameter->color, NULL, 16
                );

                SDL_SetRenderDrawColor(
                    renderer,
                    (rgb >> 16) & 0xFF,
                    (rgb >> 8) & 0xFF,
                    rgb & 0xFF,
                    255
                );
            } else {
                SDL_SetRenderDrawColor(renderer, 50, 120, 255, 255);
            }

            int x1 = (int)map_x(
                parameter->min, x_min, x_max, left, plot_width
            );

            int x2 = (int)map_x(
                parameter->max, x_min, x_max, left, plot_width
            );

            int y = (int)map_y(
                parameter->amount, y_min, y_max, top, plot_height
            );

            int baseline = (int)map_y(
                y_min, y_min, y_max, top, plot_height
            );

            SDL_Rect column = {
                x1,
                y,
                x2 - x1,
                baseline - y
            };

            SDL_RenderFillRect(renderer, &column);
        }

        SDL_RenderSetClipRect(renderer, NULL);
    } else {
        for (int i = 0; i < histogram->data.fuzzy.parameter_count; i++) {
            draw_membership(
                renderer,
                &histogram->data.fuzzy.parameters[i],
                x_min,
                x_max,
                y_min,
                y_max,
                left,
                top,
                plot_width,
                plot_height
            );
        }
    }
}