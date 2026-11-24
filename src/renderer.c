#include "renderer.h"
#include "node_graph.h"
#include "histogram_graph.h"
#include "pie_graph.h"
#include "bar_graph.h"
#include "combination_graph_render.h"
#include "column_graph.h"
#include "line_graph.h"
#include "display_graph.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../stb_image_write.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    FILE *file;
    bool failed;
} PngWriteContext;

#define WINDOW_WIDTH RENDERER_BASE_WIDTH
#define WINDOW_HEIGHT RENDERER_BASE_HEIGHT
#define EXPORT_SCALE 2

static int scale_legend_dimension(int dimension, double scale) {
    int scaled = (int)lround(dimension * scale);
    return scaled > 0 ? scaled : 1;
}

void draw_image_fit(
    SDL_Renderer *renderer,
    SDL_Texture *image,
    SDL_Rect bounds
) {
    if (!renderer || !image || bounds.w <= 0 || bounds.h <= 0) {
        return;
    }

    int image_width;
    int image_height;

    if (SDL_QueryTexture(
            image,
            NULL,
            NULL,
            &image_width,
            &image_height
        ) != 0) {
        fprintf(stderr, "could not query image dimensions: %s\n", SDL_GetError());
        return;
    }

    if (image_width <= 0 || image_height <= 0) {
        fprintf(stderr, "image has invalid dimensions\n");
        return;
    }

    double scale = fmin(
        (double)bounds.w / image_width,
        (double)bounds.h / image_height
    );

    if (scale > 1.0) {
        scale = 1.0;
    }

    int scaled_width = (int)lround(image_width * scale);
    int scaled_height = (int)lround(image_height * scale);

    if (scaled_width <= 0)
        scaled_width = 1;
    if (scaled_height <= 0)
        scaled_height = 1;

    SDL_Rect destination = {
        bounds.x + (bounds.w - scaled_width) / 2,
        bounds.y + (bounds.h - scaled_height) / 2,
        scaled_width,
        scaled_height
    };

    if (SDL_RenderCopy(renderer, image, NULL, &destination) != 0) {
        fprintf(stderr, "could not render image: %s\n", SDL_GetError());
    }
}

static void set_color(SDL_Renderer *renderer, int r, int g, int b) {
    SDL_SetRenderDrawColor(renderer, r, g, b, 255);
}

static void draw_chart_frame(
    SDL_Renderer *renderer,
    TTF_Font *font,
    const Graph *graph,
    const ChartConfig *config
) {
    set_color(renderer, 24, 26, 32);
    SDL_RenderClear(renderer);

    switch (config->type) {
        case CHART_NODES:
            node_graph_draw(renderer, font, graph, config, WINDOW_WIDTH, WINDOW_HEIGHT);
            break;
        case CHART_HISTOGRAM:
            histogram_graph_draw(renderer, font, graph, config, WINDOW_WIDTH, WINDOW_HEIGHT);
            break;
        case CHART_PIE:
            pie_graph_draw(renderer, font, graph, config, WINDOW_WIDTH, WINDOW_HEIGHT);
            break;
        case CHART_BAR:
            bar_graph_draw(renderer, font, graph, config, WINDOW_WIDTH, WINDOW_HEIGHT);
            break;
        case CHART_COMBINATION:
            combination_graph_draw(renderer, font, graph, config, WINDOW_WIDTH, WINDOW_HEIGHT);
            break;
        case CHART_COLUMN:
            column_graph_draw(renderer, font, graph, config, WINDOW_WIDTH, WINDOW_HEIGHT);
            break;
        case CHART_LINE:
            line_graph_draw(renderer, font, graph, config, WINDOW_WIDTH, WINDOW_HEIGHT);
            break;
        case CHART_DISPLAY:
            display_graph_draw(renderer, graph, config, WINDOW_WIDTH, WINDOW_HEIGHT);
            break;
    }

    if (config->type != CHART_COMBINATION) {
        draw_legend(renderer, font, graph, config, WINDOW_WIDTH, 1.0);
    }

    draw_title(renderer, font, config->title, WINDOW_WIDTH);
}

static void write_png_data(void *context, void *data, int size) {
    PngWriteContext *write_context = context;

    if (size < 0 ||
        fwrite(data, 1, (size_t)size, write_context->file) != (size_t)size) {
        write_context->failed = true;
    }
}

static bool is_reserved_filename(const char *name) {
    size_t stem_length = strcspn(name, ".");

    if (stem_length == 3) {
        char first = (char)toupper((unsigned char)name[0]);
        char second = (char)toupper((unsigned char)name[1]);
        char third = (char)toupper((unsigned char)name[2]);

        if ((first == 'C' && second == 'O' && third == 'N') ||
            (first == 'P' && second == 'R' && third == 'N') ||
            (first == 'A' && second == 'U' && third == 'X') ||
            (first == 'N' && second == 'U' && third == 'L')) {
            return true;
        }
    }

    if (stem_length == 4) {
        char first = (char)toupper((unsigned char)name[0]);
        char second = (char)toupper((unsigned char)name[1]);
        char third = (char)toupper((unsigned char)name[2]);
        char fourth = name[3];

        return (first == 'C' && second == 'O' && third == 'M' &&
                fourth >= '1' && fourth <= '9') ||
            (first == 'L' && second == 'P' && third == 'T' &&
                fourth >= '1' && fourth <= '9');
    }

    return false;
}

static char *make_safe_filename(const char *name, size_t length) {
    char *safe_name = malloc(length + 2);

    if (!safe_name) {
        return NULL;
    }

    size_t safe_length = 0;

    for (size_t i = 0; i < length; i++) {
        unsigned char character = (unsigned char)name[i];

        if (character < 32 || strchr("<>:\"/\\|?*", character)) {
            safe_name[safe_length++] = '_';
        } else {
            safe_name[safe_length++] = (char)character;
        }
    }

    while (safe_length > 0 &&
           (safe_name[safe_length - 1] == '.' ||
            safe_name[safe_length - 1] == ' ')) {
        safe_length--;
    }

    if (safe_length == 0) {
        free(safe_name);
        return NULL;
    }

    safe_name[safe_length] = '\0';

    if (is_reserved_filename(safe_name)) {
        memmove(safe_name + 1, safe_name, safe_length + 1);
        safe_name[0] = '_';
    }

    return safe_name;
}

static char *make_png_path(
    const char *chart_path,
    const char *base_name,
    unsigned int collision
) {
    char *absolute_chart_path = _fullpath(NULL, chart_path, 0);

    if (!absolute_chart_path) {
        fprintf(stderr, "could not resolve chart path '%s'\n", chart_path);
        return NULL;
    }

    const char *last_separator = strrchr(absolute_chart_path, '\\');
    const char *forward_separator = strrchr(absolute_chart_path, '/');

    if (!last_separator || (forward_separator && forward_separator > last_separator)) {
        last_separator = forward_separator;
    }

    size_t directory_length = last_separator
        ? (size_t)(last_separator - absolute_chart_path + 1)
        : 0;
    size_t base_length = strlen(base_name);
    char collision_suffix[32] = "";

    if (collision > 0) {
        snprintf(collision_suffix, sizeof(collision_suffix), " (%u)", collision);
    }

    size_t suffix_length = strlen(collision_suffix);

    if (directory_length > SIZE_MAX - base_length ||
        directory_length + base_length > SIZE_MAX - suffix_length ||
        directory_length + base_length + suffix_length >
            SIZE_MAX - sizeof(".png")) {
        free(absolute_chart_path);
        return NULL;
    }

    size_t output_size =
        directory_length + base_length + suffix_length + sizeof(".png");
    char *output_path = malloc(output_size);

    if (!output_path) {
        free(absolute_chart_path);
        return NULL;
    }

    char *cursor = output_path;
    memcpy(cursor, absolute_chart_path, directory_length);
    cursor += directory_length;
    memcpy(cursor, base_name, base_length);
    cursor += base_length;
    memcpy(cursor, collision_suffix, suffix_length);
    cursor += suffix_length;
    memcpy(cursor, ".png", sizeof(".png"));

    free(absolute_chart_path);
    return output_path;
}

static char *get_output_base_name(
    const char *chart_path,
    const char *title
) {
    const char *name = title;
    size_t name_length = title ? strlen(title) : 0;

    if (name_length == 0) {
        const char *last_separator = strrchr(chart_path, '\\');
        const char *forward_separator = strrchr(chart_path, '/');

        if (!last_separator ||
            (forward_separator && forward_separator > last_separator)) {
            last_separator = forward_separator;
        }

        name = last_separator ? last_separator + 1 : chart_path;
        name_length = strlen(name);

        const char *extension = strrchr(name, '.');
        if (extension) {
            name_length = (size_t)(extension - name);
        }
    }

    char *base_name = make_safe_filename(name, name_length);

    if (!base_name && title && title[0] != '\0') {
        return get_output_base_name(chart_path, NULL);
    }

    return base_name;
}

static bool save_chart_png(
    SDL_Renderer *renderer,
    TTF_Font *font,
    const Graph *graph,
    const ChartConfig *config,
    const char *chart_path
) {
    char *base_name = get_output_base_name(chart_path, config->title);

    if (!base_name) {
        fprintf(stderr, "could not create a valid PNG filename\n");
        return false;
    }

    int window_width;
    int window_height;

    if (SDL_GetRendererOutputSize(renderer, &window_width, &window_height) != 0 ||
        window_width <= 0 || window_height <= 0 ||
        window_width > INT_MAX / (EXPORT_SCALE * 4) ||
        window_height > INT_MAX / EXPORT_SCALE) {
        fprintf(stderr, "could not determine chart image dimensions: %s\n", SDL_GetError());
        free(base_name);
        return false;
    }

    int width = window_width * EXPORT_SCALE;
    int height = window_height * EXPORT_SCALE;

    if ((size_t)width > SIZE_MAX / 4 / (size_t)height) {
        fprintf(stderr, "chart image dimensions are too large to export\n");
        free(base_name);
        return false;
    }

    int stride = width * 4;
    size_t pixel_size = (size_t)stride * (size_t)height;
    unsigned char *pixels = malloc(pixel_size);

    if (!pixels) {
        fprintf(stderr, "could not allocate chart image buffer\n");
        free(base_name);
        return false;
    }

    if (!SDL_RenderTargetSupported(renderer)) {
        fprintf(stderr, "renderer does not support high-resolution image export\n");
        free(pixels);
        free(base_name);
        return false;
    }

    SDL_Texture *export_target = SDL_CreateTexture(
        renderer,
        SDL_PIXELFORMAT_RGBA32,
        SDL_TEXTUREACCESS_TARGET,
        width,
        height
    );

    if (!export_target) {
        fprintf(stderr, "could not create high-resolution image target: %s\n", SDL_GetError());
        free(pixels);
        free(base_name);
        return false;
    }

    SDL_Texture *previous_target = SDL_GetRenderTarget(renderer);
    float previous_scale_x;
    float previous_scale_y;
    SDL_RenderGetScale(renderer, &previous_scale_x, &previous_scale_y);

    if (SDL_SetRenderTarget(renderer, export_target) != 0) {
        fprintf(stderr, "could not select high-resolution image target: %s\n", SDL_GetError());
        SDL_DestroyTexture(export_target);
        free(pixels);
        free(base_name);
        return false;
    }

    bool capture_success = true;

    if (SDL_RenderSetScale(renderer, (float)EXPORT_SCALE, (float)EXPORT_SCALE) != 0) {
        fprintf(stderr, "could not set high-resolution render scale: %s\n", SDL_GetError());
        capture_success = false;
    } else {
        draw_chart_frame(renderer, font, graph, config);

        if (SDL_RenderReadPixels(
                renderer,
                NULL,
                SDL_PIXELFORMAT_RGBA32,
                pixels,
                stride
            ) != 0) {
            fprintf(stderr, "could not read high-resolution chart pixels: %s\n", SDL_GetError());
            capture_success = false;
        }
    }

    if (SDL_SetRenderTarget(renderer, previous_target) != 0) {
        fprintf(stderr, "could not restore render target: %s\n", SDL_GetError());
        capture_success = false;
    }

    if (SDL_RenderSetScale(renderer, previous_scale_x, previous_scale_y) != 0) {
        fprintf(stderr, "could not restore render scale: %s\n", SDL_GetError());
        capture_success = false;
    }

    SDL_DestroyTexture(export_target);

    if (!capture_success) {
        free(pixels);
        free(base_name);
        return false;
    }

    unsigned int collision = 0;
    char *output_path = NULL;
    FILE *file = NULL;

    while (true) {
        free(output_path);
        output_path = make_png_path(chart_path, base_name, collision);

        if (!output_path) {
            fprintf(stderr, "could not create chart image path\n");
            break;
        }

        file = fopen(output_path, "wbx");

        if (file) {
            break;
        }

        if (errno != EEXIST) {
            fprintf(stderr, "could not create '%s': %s\n", output_path, strerror(errno));
            break;
        }

        if (collision == UINT_MAX) {
            fprintf(stderr, "could not find an unused chart image filename\n");
            break;
        }

        collision++;
    }

    bool saved = false;

    if (file) {
        PngWriteContext write_context = {
            .file = file,
            .failed = false
        };
        int write_result = stbi_write_png_to_func(
            write_png_data,
            &write_context,
            width,
            height,
            4,
            pixels,
            stride
        );

        saved = write_result != 0 && !write_context.failed;

        if (fflush(file) != 0) {
            saved = false;
        }

        if (fclose(file) != 0) {
            saved = false;
        }

        if (saved) {
            printf("saved chart image to '%s'\n", output_path);
        } else {
            fprintf(stderr, "could not write chart image to '%s'\n", output_path);
            remove(output_path);
        }
    }

    free(output_path);
    free(pixels);
    free(base_name);
    return saved;
}

bool renderer_create(SDL_Window **window, SDL_Renderer **renderer) {
    if (!window || !renderer) {
        return false;
    }

    *window = NULL;
    *renderer = NULL;

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "SDL initialization failed: %s\n", SDL_GetError());
        return false;
    }

    *window = SDL_CreateWindow(
        "Clhart",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        WINDOW_WIDTH,
        WINDOW_HEIGHT,
        SDL_WINDOW_SHOWN
    );

    if (!*window) {
        fprintf(stderr, "window creation failed: %s\n", SDL_GetError());
        renderer_destroy(*window, *renderer);
        *window = NULL;
        return false;
    }

    *renderer = SDL_CreateRenderer(
        *window,
        -1,
        SDL_RENDERER_ACCELERATED |
        SDL_RENDERER_PRESENTVSYNC |
        SDL_RENDERER_TARGETTEXTURE
    );

    if (!*renderer) {
        fprintf(stderr, "renderer creation failed: %s\n", SDL_GetError());
        renderer_destroy(*window, *renderer);
        *window = NULL;
        return false;
    }

    return true;
}

void renderer_destroy(SDL_Window *window, SDL_Renderer *renderer) {
    if (renderer) {
        SDL_DestroyRenderer(renderer);
    }

    if (window) {
        SDL_DestroyWindow(window);
    }

    SDL_Quit();
}

void draw_title(
    SDL_Renderer *renderer,
    TTF_Font *font,
    const char *title,
    int width
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
            (width - surface->w) / 2,
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

static void draw_axis_label(
    SDL_Renderer *renderer,
    TTF_Font *font,
    const char *label,
    int center_x,
    int center_y,
    bool vertical
) {
    if (!label || label[0] == '\0') {
        return;
    }

    SDL_Color color = {220, 220, 220, 255};
    SDL_Surface *surface = TTF_RenderUTF8_Blended(font, label, color);
    if (!surface) {
        fprintf(stderr, "could not render axis label: %s\n", TTF_GetError());
        return;
    }

    SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surface);
    if (!texture) {
        fprintf(
            stderr,
            "could not create axis label texture: %s\n",
            SDL_GetError()
        );
        SDL_FreeSurface(surface);
        return;
    }

    SDL_Rect destination = {
        center_x - surface->w / 2,
        center_y - surface->h / 2,
        surface->w,
        surface->h
    };

    if (SDL_RenderCopyEx(
        renderer,
        texture,
        NULL,
        &destination,
        vertical ? 270.0 : 0.0,
        NULL,
        SDL_FLIP_NONE
    ) != 0) {
        fprintf(stderr, "could not draw axis label: %s\n", SDL_GetError());
    }

    SDL_DestroyTexture(texture);
    SDL_FreeSurface(surface);
}

void draw_axis_labels(
    SDL_Renderer *renderer,
    TTF_Font *font,
    const ChartConfig *config,
    int left,
    int top,
    int right,
    int bottom,
    int height
) {
    if (!config) {
        return;
    }

    int label_height = TTF_FontHeight(font);
    draw_axis_label(
        renderer,
        font,
        config->global.xlabel,
        (left + right) / 2,
        height - label_height / 2 - 8,
        false
    );
    draw_axis_label(
        renderer,
        font,
        config->global.ylabel,
        left / 2 - label_height - 6,
        (top + bottom) / 2,
        true
    );
}

void draw_legend(
    SDL_Renderer *renderer,
    TTF_Font *font,
    const Graph *graph,
    const ChartConfig *config,
    int width,
    double scale
) {
    if (!config->global.legend)
        return;
        
    int count = 0;

    switch (config->type) {
        case CHART_COMBINATION:
        case CHART_DISPLAY:
            return;

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
            count = graph->data.bar.bar_count;
            break;
        
        case CHART_COLUMN:
        case CHART_LINE:
            count = graph->data.column.series_count;
            break;
    }

    int max_text_width = 0;

    for (int i = 0; i < count; i++) {
        const char *name;

        if (config->type == CHART_NODES) {
            name = graph->data.nodes.nodes[i].name;
        } else if (config->type == CHART_PIE) {
            name = graph->data.pie.slices[i].name;
        } else if (config->type == CHART_BAR) {
            name = graph->data.bar.bars[i].name;
        } else if (config->type == CHART_COLUMN) {
            name = graph->data.column.series[i].name;
        } else if (config->type == CHART_LINE) {
            name = graph->data.line.series[i].name;
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

    const int padding = scale_legend_dimension(10, scale);
    const int swatch_width = scale_legend_dimension(16, scale);
    const int gap = scale_legend_dimension(8, scale);
    const int row_height = scale_legend_dimension(30, scale);
    const int swatch_height = scale_legend_dimension(16, scale);
    const int right_margin = scale_legend_dimension(20, scale);
    const int top_margin = scale_legend_dimension(20, scale);
    const int text_offset = scale_legend_dimension(5, scale);

    int legend_width =
        padding * 2 + swatch_width + gap + max_text_width;

    int legend_x = width - legend_width - right_margin;
    int legend_y = top_margin;

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

            case CHART_BAR: {
                const Bar *object = &graph->data.bar.bars[i];

                name = object->name;
                color = object->color;
                has_color = object->has_color;
                break;
            }

            case CHART_COLUMN: {
                const ColumnSeries *object =
                    &graph->data.column.series[i];

                name = object->name;
                color = object->color;
                has_color = object->has_color;
                break;
            }

            case CHART_LINE: {
                const ColumnSeries *object =
                    &graph->data.line.series[i];

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
            swatch_height
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
                    y - text_offset,
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
    const char *chart_path,
    const ChartConfig *config,
    SDL_Renderer *renderer
) {
    if (!graph || !chart_path || !config || !renderer) {
        fprintf(stderr, "cannot render without a graph, chart path, configuration, and renderer\n");
        return false;
    }

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
            if (graph->data.bar.bar_count <= 0) {
                fprintf(stderr, "cannot render an empty bar chart\n");
                return false;
            }
            break;
        
        case CHART_COMBINATION:
            if (graph->data.combination.chart_count <= 0) {
                fprintf(stderr, "cannot render an empty combination chart\n");
                return false;
            }
            break;

        case CHART_COLUMN:
            if (graph->data.column.category_count <= 0 ||
                graph->data.column.series_count <= 0) {
                fprintf(stderr, "cannot render an empty column chart\n");
                return false;
            }
            break;

        case CHART_LINE:
            if (graph->data.line.category_count <= 0 ||
                graph->data.line.series_count <= 0) {
                fprintf(stderr, "cannot render an empty line chart\n");
                return false;
            }
            break;

        case CHART_DISPLAY:
            if (!graph->data.display.image) {
                fprintf(stderr, "cannot render a display without an image\n");
                return false;
            }
            break;
    }

    if (TTF_Init() != 0) {
        fprintf(stderr, "SDL_ttf initialization failed: %s\n", TTF_GetError());
        return false;
    }

    TTF_Font *font = TTF_OpenFont(font_path, RENDERER_BASE_FONT_SIZE);

    if (!font) {
        fprintf(stderr, "could not load font: %s\n", TTF_GetError());
        TTF_Quit();
        return false;
    }

    bool running = true;

    while (running) {
        SDL_Event event;
        bool save_requested = false;

        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = false;
            } else if (event.type == SDL_KEYDOWN &&
                       event.key.repeat == 0 &&
                       event.key.keysym.sym == SDLK_s &&
                       (event.key.keysym.mod & KMOD_CTRL) != 0) {
                save_requested = true;
            }
        }

        draw_chart_frame(renderer, font, graph, config);
        SDL_RenderPresent(renderer);
        if (save_requested) {
            save_chart_png(renderer, font, graph, config, chart_path);
        }
    }

    TTF_CloseFont(font);
    TTF_Quit();

    return true;
}