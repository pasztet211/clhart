#include "parser_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <errno.h>

typedef struct {
    char *name;
    char *value;
} Setting;

typedef struct {
    Setting *items;
    int count;
} Settings;

void skip_whitespace(Parser *p) {
    while (isspace((unsigned char)p->source[p->position])) {
        p->position++;
    }
}

static char *parse_setting_value(Parser *p) {
    skip_whitespace(p);

    size_t start = p->position;

    while (p->source[p->position] &&
           p->source[p->position] != ',' &&
           p->source[p->position] != '}') {
        p->position++;
    }

    size_t end = p->position;

    while (end > start &&
           isspace((unsigned char)p->source[end - 1])) {
        end--;
    }

    if (end == start) {
        return NULL;
    }

    size_t length = end - start;
    char *value = malloc(length + 1);

    if (!value) {
        return NULL;
    }

    memcpy(value, p->source + start, length);
    value[length] = '\0';

    return value;
}

static char *parse_title(Parser *p) {
    if (!expect(p, '{')) {
        fprintf(stderr, "expected '{' after .title\n");
        return NULL;
    }

    size_t start = p->position;
    while (p->source[p->position] &&
           p->source[p->position] != '}') {
        p->position++;
    }

    if (p->source[p->position] != '}') {
        fprintf(stderr, "expected '}' after title\n");
        return NULL;
    }

    size_t end = p->position++;

    while (start < end && isspace((unsigned char)p->source[start])) {
        start++;
    }

    while (end > start && isspace((unsigned char)p->source[end - 1])) {
        end--;
    }

    if (end - start >= 2 &&
        p->source[start] == '"' &&
        p->source[end - 1] == '"') {
        start++;
        end--;
    }

    size_t length = end - start;
    char *title = malloc(length + 1);

    if (!title) {
        return NULL;
    }

    memcpy(title, p->source + start, length);
    title[length] = '\0';
    return title;
}

static bool parse_setup(Parser *p, Settings *settings) {
    if (!expect(p, '{')) {
        fprintf(stderr, "expected '{' after .setup\n");
        return false;
    }

    while (true) {
        skip_whitespace(p);

        if (p->source[p->position] == '}') {
            p->position++;
            return true;
        }

        char *name = parse_name(p);

        if (!name) {
            fprintf(stderr, "expected setting name\n");
            return false;
        }

        if (!expect_operator(p, "---")) {
            fprintf(stderr, "expected '---' after setting name\n");
            free(name);
            return false;
        }

        char *value = parse_setting_value(p);

        if (!value) {
            fprintf(stderr, "expected setting value\n");
            free(name);
            return false;
        }

        for (int i = 0; i < settings->count; i++) {
            if (strcmp(settings->items[i].name, name) == 0) {
                fprintf(stderr, "duplicate setting: %s\n", name);
                free(name);
                free(value);
                return false;
            }
        }

        Setting *items = realloc(
            settings->items,
            sizeof(Setting) * (settings->count + 1)
        );

        if (!items) {
            free(name);
            free(value);
            return false;
        }

        settings->items = items;

        settings->items[settings->count++] = (Setting) {
            .name = name,
            .value = value
        };

        skip_whitespace(p);

        if (p->source[p->position] == ',') {
            p->position++;
        } else if (p->source[p->position] != '}') {
            fprintf(stderr, "expected ',' or '}' in setup\n");
            return false;
        }
    }
}

static bool parse_range(
    const char *value,
    double *min,
    double *max
) {
    char *end;

    *min = strtod(value, &end);
    if (end == value || !isfinite(*min) || *end != ';')
        return false;

    const char *second = end + 1;
    *max = strtod(second, &end);

    if (end == second || !isfinite(*max) || *end != '\0')
        return false;

    if (*min >= *max)
        return false;

    return true;
}

static bool apply_settings(
    const Settings *settings,
    ChartConfig *config
) {
    for (int i = 0; i < settings->count; i++) {
        const char *name = settings->items[i].name;
        const char *value = settings->items[i].value;

        if (strcmp(name, "show_identifiers") == 0) {
            if (strcmp(value, "0") == 0) {
                config->global.show_identifiers = false;
            } else if (strcmp(value, "1") == 0) {
                config->global.show_identifiers = true;
            } else {
                fprintf(stderr, "show_identifiers must be 0 or 1\n");
                return false;
            }
        }

        else if (strcmp(name, "legend") == 0) {
            if (strcmp(value, "0") != 0 && strcmp(value, "1") != 0) {
                fprintf(stderr, "invalid legend value: %s\n", value);
                return false;
            }

            config->global.legend = strcmp(value, "1") == 0;
        }

        else if (config->type == CHART_NODES &&
                 strcmp(name, "start_angle") == 0) {
            char *end;
            errno = 0;

            double angle = strtod(value, &end);

            if (errno || *end || !isfinite(angle)) {
                fprintf(stderr, "invalid start_angle: %s\n", value);
                return false;
            }

            config->specific.nodes.start_angle = angle;
        }

        else if (config->type == CHART_NODES &&
                 strcmp(name, "advance_angle") == 0) {
            char *end;
            errno = 0;

            double angle = strtod(value, &end);

            if (errno || *end || !isfinite(angle)) {
                fprintf(stderr, "invalid advance_angle: %s\n", value);
                return false;
            }

            config->specific.nodes.advance_angle = angle;
            config->specific.nodes.has_advance_angle = true;
        }

        else if (config->type == CHART_HISTOGRAM &&
                 strcmp(name, "show_grid") == 0) {
            if (strcmp(value, "0") == 0) {
                config->specific.histogram.show_grid = false;
            } else if (strcmp(value, "1") == 0) {
                config->specific.histogram.show_grid = true;
            } else {
                fprintf(stderr, "show_grid must be 0 or 1\n");
                return false;
            }
        }

        else if (config->type == CHART_HISTOGRAM &&
                 strcmp(name, "show_axis_numbers") == 0) {
            if (strcmp(value, "0") == 0) {
                config->specific.histogram.show_axis_numbers = false;
            } else if (strcmp(value, "1") == 0) {
                config->specific.histogram.show_axis_numbers = true;
            } else {
                fprintf(stderr, "show_axis_numbers must be 0 or 1\n");
                return false;
            }
        }

        else if (config->type == CHART_HISTOGRAM &&
                strcmp(name, "x_range") == 0) {
            HistogramConfig *hist = &config->specific.histogram;

            if (!parse_range(value, &hist->x_min, &hist->x_max)) {
                fprintf(stderr, "invalid x_range: %s\n", value);
                return false;
            }

            hist->has_x_range = true;
        }

        else if (config->type == CHART_HISTOGRAM &&
                strcmp(name, "y_range") == 0) {
            HistogramConfig *hist = &config->specific.histogram;

            if (!parse_range(value, &hist->y_min, &hist->y_max)) {
                fprintf(stderr, "invalid y_range: %s\n", value);
                return false;
            }
            printf("Parsed y_range: %f to %f\n", hist->y_min, hist->y_max);

            hist->has_y_range = true;
        }

        else {
            fprintf(stderr, "unknown or incompatible setting: %s\n", name);
            return false;
        }
    }

    return true;
}

bool expect_word(Parser *p, const char *word) {
    skip_whitespace(p);

    size_t length = strlen(word);

    if (strncmp(p->source + p->position, word, length) != 0) {
        return false;
    }

    char next = p->source[p->position + length];

    if (isalnum((unsigned char)next) || next == '_') {
        return false;
    }

    p->position += length;
    return true;
}

bool expect(Parser *p, char c) {
    skip_whitespace(p);

    if (p->source[p->position] != c) {
        return false;
    }

    p->position++;
    return true;
}

bool expect_operator(Parser *p, const char *op) {
    skip_whitespace(p);

    size_t length = strlen(op);

    if (strncmp(p->source + p->position, op, length) != 0) {
        return false;
    }

    p->position += length;
    return true;
}

char *parse_name(Parser *p) {
    skip_whitespace(p);

    size_t start = p->position;
    size_t length = 0;

    bool quoted = p->source[p->position] == '"';

    if (quoted) {
        p->position++;
        start = p->position;

        while (p->source[p->position] &&
               p->source[p->position] != '"') {
            p->position++;
        }

        if (p->source[p->position] != '"') {
            return NULL;
        }

        length = p->position - start;
        p->position++;
    } else {
        while (p->source[p->position] &&
               !isspace((unsigned char)p->source[p->position]) &&
               p->source[p->position] != ',' &&
               p->source[p->position] != '}' &&
               p->source[p->position] != '-' &&
               p->source[p->position] != '<') {
            p->position++;
        }

        length = p->position - start;
    }

    if (length == 0) {
        return NULL;
    }

    char *name = malloc(length + 1);

    if (!name) {
        return NULL;
    }

    memcpy(name, p->source + start, length);
    name[length] = '\0';

    return name;
}

static bool parse_graph(
    Parser *p,
    Graph *graph,
    ChartConfig *config
) {
    Settings settings = {0};

    bool success = false;
    bool type_seen = false;
    bool setup_seen = false;
    bool title_seen = false;
    bool switch_seen = false;

    *config = (ChartConfig){0};

    config->type = CHART_NODES;
    config->global.show_identifiers = false;
    config->specific.nodes.start_angle = -90.0;
    config->specific.nodes.has_advance_angle = false;

    // Read directives
    while (true) {
        skip_whitespace(p);

        if (p->source[p->position] != '.') {
            break;
        }

        p->position++;

        if (expect_word(p, "type")) {
            if (type_seen) {
                fprintf(stderr, "duplicate .type declaration\n");
                goto cleanup;
            }

            type_seen = true;

            if (!expect(p, '{')) {
                fprintf(stderr, "invalid chart type declaration\n");
                goto cleanup;
            }

            if (expect_word(p, "nodes")) {
                config->type = CHART_NODES;
            } else if (expect_word(p, "histogram")) {
                config->type = CHART_HISTOGRAM;
            } else {
                fprintf(stderr, "invalid or unsupported chart type\n");
                goto cleanup;
            }

            if (!expect(p, '}')) {
                fprintf(stderr, "expected '}' after chart type\n");
                goto cleanup;
            }

        } else if (expect_word(p, "setup")) {
            if (setup_seen) {
                fprintf(stderr, "duplicate .setup block\n");
                goto cleanup;
            }

            setup_seen = true;

            if (!parse_setup(p, &settings)) {
                goto cleanup;
            }
        } else if (expect_word(p, "title")) {
            if (title_seen) {
                fprintf(stderr, "duplicate .title declaration\n");
                goto cleanup;
            }

            title_seen = true;
            config->title = parse_title(p);

            if (!config->title) {
                goto cleanup;
            }
        } else if (expect_word(p, "switch")) {
            if (switch_seen) {
                fprintf(stderr, "duplicate .switch declaration\n");
                goto cleanup;
            }

            switch_seen = true;

            skip_whitespace(p);

            // Optional empty braces: .switch {}
            if (p->source[p->position] == '{') {
                if (!expect(p, '{') || !expect(p, '}')) {
                    fprintf(stderr, "expected empty '{}' after .switch\n");
                    goto cleanup;
                }
            }
        } else {
            fprintf(stderr, "unknown directive\n");
            goto cleanup;
        }
    }

    if (!type_seen) {
        fprintf(stderr, "missing .type declaration\n");
        goto cleanup;
    }

    if (switch_seen) {
        if (config->type != CHART_HISTOGRAM) {
            fprintf(stderr, ".switch is only valid for histograms\n");
            goto cleanup;
        }

        config->specific.histogram.type = HISTOGRAM_DISTRIBUTION;
    }

    if (!apply_settings(&settings, config)) {
        goto cleanup;
    }

    switch (config->type) {
        case CHART_NODES:
            if (!node_graph_parse(p, graph)) {
                goto cleanup;
            }
            break;

        case CHART_HISTOGRAM:
            if (!histogram_graph_parse(p, graph, config)) {
                goto cleanup;
            }
            break;

        default:
            fprintf(stderr, "unsupported chart type\n");
            goto cleanup;
    }

    skip_whitespace(p);

    if (p->source[p->position] != '\0') {
        fprintf(stderr, "unexpected content after graph\n");
        goto cleanup;
    }

    success = true;

cleanup:
    for (int i = 0; i < settings.count; i++) {
        free(settings.items[i].name);
        free(settings.items[i].value);
    }

    free(settings.items);

    if (!success) {
        free(config->title);
        config->title = NULL;
    }

    return success;
}

bool parse_file(
    const char *filename,
    Graph **graph,
    ChartConfig **config
) {
    if (!graph || !config) {
        return false;
    }

    *graph = NULL;
    *config = NULL;

    FILE *file = fopen(filename, "rb");

    if (!file) {
        perror("could not open file");
        return false;
    }

    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return false;
    }

    long file_size = ftell(file);

    if (file_size < 0) {
        fclose(file);
        return false;
    }

    rewind(file);

    char *source = malloc((size_t)file_size + 1);

    if (!source) {
        fclose(file);
        return false;
    }

    size_t read_size = fread(source, 1, (size_t)file_size, file);

    if (ferror(file)) {
        free(source);
        fclose(file);
        return false;
    }

    source[read_size] = '\0';
    fclose(file);

    *graph = calloc(1, sizeof(Graph));
    *config = calloc(1, sizeof(ChartConfig));

    if (!*graph || !*config) {
        free(source);
        free(*graph);
        free(*config);
        *graph = NULL;
        *config = NULL;
        return false;
    }

    Parser parser = {
        .source = source,
        .position = 0
    };

    bool success = parse_graph(&parser, *graph, *config);

    free(source);

    if (!success) {
        free_graph(*graph, (*config)->type);
        free_chart_config(*config);

        *graph = NULL;
        *config = NULL;
    }

    return success;
}

void free_chart_config(ChartConfig *config) {
    if (!config)
        return;

    free(config->title);
    free(config);
}

void free_graph(Graph *graph, ChartType type) {
    if (!graph)
        return;

    switch (type) {
        case CHART_NODES:
            for (int i = 0; i < graph->data.nodes.node_count; i++) {
                free(graph->data.nodes.nodes[i].name);
                free(graph->data.nodes.nodes[i].identifier);
            }

            free(graph->data.nodes.nodes);
            free(graph->data.nodes.connections);
            break;

        case CHART_HISTOGRAM:
            for (int i = 0; i < graph->data.histogram.data.fuzzy.parameter_count; i++) {
                free(graph->data.histogram.data.fuzzy.parameters[i].name);
            }

            free(graph->data.histogram.data.fuzzy.parameters);
            break;

        case CHART_BAR:
            /* TODO: Add bar chart cleanup when implemented. */
            break;
    }

    free(graph);
}