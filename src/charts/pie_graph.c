#include "pie_graph.h"
#include "parser_internal.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PI 3.14159265358979323846

static bool parse_slice(
    Parser *p,
    PieGraph *pie,
    ChartConfig *config
) {
    PieSlice slice = {0};

    slice.name = parse_name(p, config);

    if (!slice.name)
        return false;

    if (!expect_operator(p, "---"))
        goto failure;

    skip_whitespace(p);

    char *end;
    slice.value = strtod(p->source + p->position, &end);

    if (end == p->source + p->position ||
        !isfinite(slice.value) ||
        slice.value < 0.0) {
        goto failure;
    }

    p->position = (size_t)(end - p->source);

    bool image_seen = false;
    bool color_seen = false;

    while (true) {
        skip_whitespace(p);

        if (!image_seen &&
            strncmp(p->source + p->position, "\"*)", 3) == 0) {
            slice.image = parse_image_reference(p);

            if (!slice.image)
                goto failure;

            image_seen = true;
        } else if (!color_seen && p->source[p->position] == ':') {
            p->position++;

            if (!expect(p, '#'))
                goto failure;

            for (int i = 0; i < 6; i++) {
                char c = p->source[p->position++];

                if (!isxdigit((unsigned char)c))
                    goto failure;

                slice.color[i] = c;
            }

            slice.color[6] = '\0';

            if (!expect(p, ':'))
                goto failure;

            slice.has_color = true;
            color_seen = true;
        } else {
            break;
        }
    }

    PieSlice *new_slices = realloc(
        pie->slices,
        sizeof(PieSlice) * (pie->slice_count + 1)
    );

    if (!new_slices)
        goto failure;

    pie->slices = new_slices;
    pie->slices[pie->slice_count++] = slice;

    return true;

failure:
    free(slice.name);
    return false;
}

bool pie_graph_parse(
    Parser *p,
    Graph *graph,
    ChartConfig *config
) {
    PieGraph *pie = &graph->data.pie;

    *pie = (PieGraph){0};

    if (!expect_word(p, "slices") ||
        !expect(p, '{')) {
        fprintf(stderr, "expected 'slices { ... }'\n");
        return false;
    }

    while (true) {
        skip_whitespace(p);

        if (expect(p, '}'))
            break;

        if (!parse_slice(p, pie, config)) {
            fprintf(stderr, "invalid pie slice\n");
            return false;
        }

        skip_whitespace(p);

        if (expect(p, '}'))
            break;

        if (!expect(p, ',')) {
            fprintf(stderr, "expected ',' or '}' after pie slice\n");
            return false;
        }
    }

    if (pie->slice_count == 0) {
        fprintf(stderr, "pie chart must contain at least one slice\n");
        return false;
    }

    double total = 0.0;

    for (int i = 0; i < pie->slice_count; i++) {
        total += pie->slices[i].value;

        if (!isfinite(total)) {
            fprintf(stderr, "pie chart total is too large\n");
            return false;
        }
    }

    if (total <= 0.0) {
        fprintf(stderr, "pie chart total must be greater than zero\n");
        return false;
    }

    return true;
}

static const SDL_Color default_colors[] = {
    {50, 120, 255, 255},
    {255, 90, 90, 255},
    {70, 200, 130, 255},
    {255, 190, 60, 255},
    {180, 100, 255, 255},
    {50, 210, 210, 255},
    {255, 120, 200, 255},
    {150, 160, 170, 255}
};

static SDL_Color get_slice_color(const PieSlice *slice, int index) {
    if (slice->has_color) {
        unsigned int rgb = (unsigned int)strtoul(
            slice->color, NULL, 16
        );

        return (SDL_Color) {
            (rgb >> 16) & 0xFF,
            (rgb >> 8) & 0xFF,
            rgb & 0xFF,
            255
        };
    }

    int count = sizeof(default_colors) / sizeof(default_colors[0]);
    return default_colors[index % count];
}

static void draw_slice(
    SDL_Renderer *renderer,
    float cx,
    float cy,
    float radius,
    double start_angle,
    double advance_angle,
    SDL_Color color,
    float squash
) {
    if (advance_angle <= 0.0)
        return;

    int segments = (int)ceil(advance_angle * radius / 3.0);

    if (segments < 1)
        segments = 1;

    SDL_Vertex *vertices = malloc(
        sizeof(SDL_Vertex) * (segments + 2)
    );

    int *indices = malloc(
        sizeof(int) * segments * 3
    );

    if (!vertices || !indices) {
        free(vertices);
        free(indices);
        return;
    }

    vertices[0] = (SDL_Vertex) {
        {cx, cy},
        color,
        {0.0f, 0.0f}
    };

    for (int i = 0; i <= segments; i++) {
        double angle = start_angle +
            advance_angle * i / segments;

        vertices[i + 1] = (SDL_Vertex) {
            {
                cx + (float)(cos(angle) * radius),
                cy + (float)(sin(angle) * radius * squash)
            },
            color,
            {0.0f, 0.0f}
        };
    }

    for (int i = 0; i < segments; i++) {
        indices[i * 3] = 0;
        indices[i * 3 + 1] = i + 1;
        indices[i * 3 + 2] = i + 2;
    }

    if (SDL_RenderGeometry(
            renderer,
            NULL,
            vertices,
            segments + 2,
            indices,
            segments * 3
        ) != 0) {
        fprintf(stderr, "could not render pie slice: %s\n", SDL_GetError());
    }

    free(vertices);
    free(indices);
}

static void draw_slice_image(
    SDL_Renderer *renderer,
    SDL_Texture *image,
    float cx,
    float cy,
    float radius,
    double start_angle,
    double advance_angle,
    float squash
) {
    int texture_width;
    int texture_height;

    if (SDL_QueryTexture(
            image,
            NULL,
            NULL,
            &texture_width,
            &texture_height
        ) != 0) {
        fprintf(stderr, "could not query pie slice image: %s\n", SDL_GetError());
        return;
    }

    if (texture_width <= 0 || texture_height <= 0 || squash <= 0.0f) {
        fprintf(stderr, "pie slice image has invalid dimensions\n");
        return;
    }

    double center_radius;
    double available_radius;

    if (advance_angle >= 2.0 * PI - 1e-6) {
        center_radius = 0.0;
        available_radius = radius;
    } else {
        center_radius = radius * 0.6;
        double minor_angle = fmin(advance_angle, 2.0 * PI - advance_angle);
        double radial_edge_distance = center_radius *
            sin(minor_angle / 2.0);

        available_radius = fmin(
            fmin(center_radius, radius - center_radius),
            radial_edge_distance
        );
    }

    if (available_radius <= 0.0) {
        return;
    }

    double scaled_texture_height = (double)texture_height / squash;
    double scale = 2.0 * available_radius / sqrt(
        (double)texture_width * texture_width +
        scaled_texture_height * scaled_texture_height
    );
    int image_width = (int)lround(texture_width * scale);
    int image_height = (int)lround(texture_height * scale);

    if (image_width <= 0 || image_height <= 0) {
        return;
    }

    double middle_angle = start_angle + advance_angle / 2.0;
    int image_center_x = (int)lround(
        cx + cos(middle_angle) * center_radius
    );
    int image_center_y = (int)lround(
        cy + sin(middle_angle) * center_radius * squash
    );
    SDL_Rect destination = {
        image_center_x - image_width / 2,
        image_center_y - image_height / 2,
        image_width,
        image_height
    };

    if (SDL_RenderCopy(renderer, image, NULL, &destination) != 0) {
        fprintf(stderr, "could not render pie slice image: %s\n", SDL_GetError());
    }
}

static void draw_percentage(
    SDL_Renderer *renderer,
    TTF_Font *font,
    double percentage,
    float x,
    float y
) {
    if (!font)
        return;

    char text[16];
    snprintf(text, sizeof(text), "%.1f%%", percentage);

    SDL_Color text_color = {0, 0, 0, 255};

    SDL_Surface *surface = TTF_RenderUTF8_Blended(
        font, text, text_color
    );

    if (!surface)
        return;

    SDL_Texture *texture = SDL_CreateTextureFromSurface(
        renderer, surface
    );

    if (texture) {
        SDL_Rect destination = {
            (int)(x - surface->w / 2.0f),
            (int)(y - surface->h / 2.0f),
            surface->w,
            surface->h
        };

        SDL_RenderCopy(renderer, texture, NULL, &destination);
        SDL_DestroyTexture(texture);
    }

    SDL_FreeSurface(surface);
}

static void draw_slice_sides(
    SDL_Renderer *renderer,
    float cx,
    float cy,
    float radius,
    double start_angle,
    double advance_angle,
    SDL_Color color,
    float squash,
    float depth
) {
    if (advance_angle <= 0.0 || depth <= 0.0f)
        return;

    int segments = (int)ceil(advance_angle * radius / 3.0);

    if (segments < 1)
        segments = 1;

    // Darken the sides.
    color.r = (Uint8)(color.r * 0.65f);
    color.g = (Uint8)(color.g * 0.65f);
    color.b = (Uint8)(color.b * 0.65f);

    for (int i = 0; i < segments; i++) {
        double a1 = start_angle +
            advance_angle * i / segments;

        double a2 = start_angle +
            advance_angle * (i + 1) / segments;

        double middle = (a1 + a2) / 2.0;

        // Only draw the front half of the pie.
        if (sin(middle) <= 0.0)
            continue;

        float x1 = cx + (float)(cos(a1) * radius);
        float y1 = cy + (float)(sin(a1) * radius * squash);

        float x2 = cx + (float)(cos(a2) * radius);
        float y2 = cy + (float)(sin(a2) * radius * squash);

        SDL_Vertex vertices[] = {
            {{x1, y1}, color, {0, 0}},
            {{x2, y2}, color, {0, 0}},
            {{x2, y2 + depth}, color, {0, 0}},
            {{x1, y1 + depth}, color, {0, 0}}
        };

        int indices[] = {
            0, 1, 2,
            0, 2, 3
        };

        SDL_RenderGeometry(
            renderer,
            NULL,
            vertices,
            4,
            indices,
            6
        );
    }
}

static float deterministic_scale(int index) {
    int tier = index % 3; // Cycles 0, 1, 2, 0, 1, 2...
    return 0.50f + 0.12f * (float)tier;
}

void pie_graph_draw(
    SDL_Renderer *renderer,
    TTF_Font *font,
    const Graph *graph,
    const ChartConfig *config,
    int width,
    int height
) {
    const PieGraph *pie = &graph->data.pie;
    const PieConfig *settings = &config->specific.pie;

    if (pie->slice_count == 0)
        return;

    double total = 0.0;

    for (int i = 0; i < pie->slice_count; i++)
        total += pie->slices[i].value;

    if (total <= 0.0 || !isfinite(total))
        return;

    float cx = width / 2.0f;
    float cy = height / 2.0f;

    float radius = fminf(width, height) *
        (settings->three_dimensional ? 0.44f : 0.42f);

    float squash = settings->three_dimensional ? 0.4f : 1.0f;
    float depth = settings->three_dimensional ? radius * 0.125f : 0.0f;

    double angle = -PI / 2.0;

    for (int i = 0; i < pie->slice_count; i++) {
        const PieSlice *slice = &pie->slices[i];

        double percentage = slice->value / total * 100.0;
        double advance = percentage / 100.0 * 2.0 * PI;

        SDL_Color color = get_slice_color(slice, i);

        if (settings->three_dimensional) {
            draw_slice_sides(
                renderer,
                cx,
                cy,
                radius,
                angle,
                advance,
                color,
                squash,
                depth
            );
        }

        draw_slice(
            renderer,
            cx,
            cy,
            radius,
            angle,
            advance,
            color,
            squash
        );

        if (slice->image) {
            draw_slice_image(
                renderer,
                slice->image,
                cx,
                cy,
                radius,
                angle,
                advance,
                squash
            );
        }

        angle += advance;
    }

    if (settings->show_percentages) {
        for (int i = 0; i < pie->slice_count; i++) {
            const PieSlice *slice = &pie->slices[i];

            double percentage = slice->value / total * 100.0;
            double advance = percentage / 100.0 * 2.0 * PI;
            if (slice->value > 0.0) {
                double middle_angle = angle + advance / 2.0;

                float label_radius = radius * deterministic_scale(i);

                float label_x = cx +
                    (float)(cos(middle_angle) * label_radius);

                float label_y = cy +
                    (float)(sin(middle_angle) * label_radius * squash);

                draw_percentage(
                    renderer,
                    font,
                    percentage,
                    label_x,
                    label_y
                );
            }

            angle += advance;
        }
    }
}