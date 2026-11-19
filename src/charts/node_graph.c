#include "node_graph.h"
#include "parser_internal.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    char *from;
    char *to;
    bool bidirectional;
    bool reverse;
} PendingConnection;

#define NODE_RADIUS 25
#define LAYOUT_RADIUS 260
#define PI 3.14159265358979323846

typedef struct {
    int x;
    int y;
} Position;

static bool add_node(
    Graph *graph,
    char *name,
    char *identifier,
    const char *color,
    SDL_Texture *image
) {
    for (int i = 0; i < graph->data.nodes.node_count; i++) {
        if (strcmp(graph->data.nodes.nodes[i].name, name) == 0) {
            fprintf(stderr, "duplicate node: %s\n", name);
            return false;
        }

        if (identifier &&
            graph->data.nodes.nodes[i].identifier &&
            strcmp(graph->data.nodes.nodes[i].identifier, identifier) == 0) {
            fprintf(stderr, "duplicate identifier: %s\n", identifier);
            return false;
        }
    }

    Node *nodes = realloc(
        graph->data.nodes.nodes,
        sizeof(Node) * (graph->data.nodes.node_count + 1)
    );

    if (!nodes) {
        return false;
    }

    graph->data.nodes.nodes = nodes;

    Node node = {
        .name = name,
        .identifier = identifier,
        .color = {0},
        .has_color = color != NULL,
        .image = image
    };

    if (color) {
        strcpy(node.color, color);
    }

    graph->data.nodes.nodes[graph->data.nodes.node_count++] = node;
    return true;
}

static bool add_pending(
    PendingConnection **connections,
    int *count,
    char *from,
    char *to,
    bool bidirectional,
    bool reverse
) {
    PendingConnection *new_connections = realloc(
        *connections,
        sizeof(PendingConnection) * (*count + 1)
    );

    if (!new_connections) {
        return false;
    }

    *connections = new_connections;
    (*connections)[(*count)++] = (PendingConnection) {
        .from = from,
        .to = to,
        .bidirectional = bidirectional,
        .reverse = reverse
    };

    return true;
}

static int resolve_node(const Graph *graph, const char *reference) {
    int result = -1;

    for (int i = 0; i < graph->data.nodes.node_count; i++) {
        const Node *node = &graph->data.nodes.nodes[i];
        const char *reference_name =
            node->identifier ? node->identifier : node->name;

        if (strcmp(reference_name, reference) == 0) {
            if (result != -1) {
                return -2;
            }

            result = i;
        }
    }

    return result;
}

static bool add_connection(
    Graph *graph,
    int from,
    int to,
    bool bidirectional
) {
    if (from == to) {
        fprintf(stderr, "self-connections are not supported\n");
        return false;
    }

    for (int i = 0; i < graph->data.nodes.connection_count; i++) {
        Connection *connection = &graph->data.nodes.connections[i];
        bool same_pair =
            (connection->from == from && connection->to == to) ||
            (connection->from == to && connection->to == from);

        if (same_pair &&
            (connection->bidirectional || bidirectional ||
             (connection->from == from && connection->to == to))) {
            fprintf(stderr, "duplicate connection\n");
            return false;
        }
    }

    Connection *connections = realloc(
        graph->data.nodes.connections,
        sizeof(Connection) * (graph->data.nodes.connection_count + 1)
    );

    if (!connections) {
        return false;
    }

    graph->data.nodes.connections = connections;
    graph->data.nodes.connections[graph->data.nodes.connection_count++] = (Connection) {
        .bidirectional = bidirectional,
        .from = from,
        .to = to
    };

    return true;
}

static bool parse_optional_color(
    Parser *parser,
    char color[7],
    bool *has_color
) {
    skip_whitespace(parser);
    *has_color = false;

    if (parser->source[parser->position] != ':') {
        return true;
    }

    parser->position++;

    if (parser->source[parser->position] != '#') {
        fprintf(stderr, "expected '#' in color\n");
        return false;
    }

    parser->position++;

    for (int i = 0; i < 6; i++) {
        char c = parser->source[parser->position];

        if (!isxdigit((unsigned char)c)) {
            fprintf(stderr, "invalid hexadecimal color\n");
            return false;
        }

        color[i] = c;
        parser->position++;
    }

    if (parser->source[parser->position] != ':') {
        fprintf(stderr, "expected ':' after color\n");
        return false;
    }

    parser->position++;
    color[6] = '\0';
    *has_color = true;
    return true;
}

static bool parse_node_attributes(
    Parser *parser,
    char color[7],
    bool *has_color,
    SDL_Texture **image
) {
    bool image_seen = false;
    bool color_seen = false;

    *has_color = false;
    *image = NULL;

    while (true) {
        skip_whitespace(parser);

        if (!image_seen &&
            strncmp(parser->source + parser->position, "\"*)", 3) == 0) {
            *image = parse_image_reference(parser);

            if (!*image) {
                return false;
            }

            image_seen = true;
        } else if (!color_seen &&
                   parser->source[parser->position] == ':') {
            if (!parse_optional_color(parser, color, has_color)) {
                return false;
            }

            color_seen = true;
        } else {
            return true;
        }
    }
}

bool node_graph_parse(
    Parser *parser,
    Graph *graph,
    ChartConfig *config
) {
    PendingConnection *pending = NULL;
    int pending_count = 0;
    bool success = false;

    if (!expect_word(parser, "nodes")) {
        fprintf(stderr, "expected 'nodes' block\n");
        goto cleanup;
    }

    if (!expect(parser, '{')) {
        fprintf(stderr, "expected '{' after nodes\n");
        goto cleanup;
    }

    while (true) {
        skip_whitespace(parser);

        if (parser->source[parser->position] == '}') {
            parser->position++;
            break;
        }

        char *first = parse_name(parser, config);
        if (!first) {
            fprintf(stderr, "expected node name\n");
            goto cleanup;
        }

        char color[7] = {0};
        bool has_color = false;
        SDL_Texture *image = NULL;
        bool declaration = false;
        bool is_connection = false;
        bool bidirectional = false;
        bool reverse = false;

        if (expect_operator(parser, "---")) {
            declaration = true;
            char *identifier = parse_name(parser, config);

            if (!identifier) {
                fprintf(stderr, "expected identifier after '---'\n");
                free(first);
                goto cleanup;
            }

            if (!parse_node_attributes(
                parser,
                color,
                &has_color,
                &image
            )) {
                free(first);
                free(identifier);
                goto cleanup;
            }

            if (!add_node(
                graph,
                first,
                identifier,
                has_color ? color : NULL,
                image
            )) {
                free(first);
                free(identifier);
                goto cleanup;
            }
        } else if (expect_operator(parser, "<->")) {
            is_connection = true;
            bidirectional = true;
        } else if (expect_operator(parser, "-->")) {
            is_connection = true;
        } else if (expect_operator(parser, "<--")) {
            is_connection = true;
            reverse = true;
        }

        if (is_connection) {
            const char *operator_name = bidirectional
                ? "<->"
                : (reverse ? "<--" : "-->");
            char *second = parse_name(parser, config);

            if (!second) {
                fprintf(
                    stderr,
                    "expected node reference after '%s'\n",
                    operator_name
                );
                free(first);
                goto cleanup;
            }

            if (!add_pending(
                &pending,
                &pending_count,
                first,
                second,
                bidirectional,
                reverse
            )) {
                free(first);
                free(second);
                goto cleanup;
            }
        } else if (!declaration &&
                   parser->source[parser->position] != '-' &&
                   parser->source[parser->position] != '<') {
            if (!parse_node_attributes(
                parser,
                color,
                &has_color,
                &image
            )) {
                free(first);
                goto cleanup;
            }

            if (!add_node(
                graph,
                first,
                NULL,
                has_color ? color : NULL,
                image
            )) {
                free(first);
                goto cleanup;
            }
        } else if (!declaration) {
            fprintf(stderr, "expected node attributes or connection operator\n");
            free(first);
            goto cleanup;
        }

        skip_whitespace(parser);

        if (parser->source[parser->position] == ',') {
            parser->position++;
            continue;
        }

        if (parser->source[parser->position] == '}') {
            parser->position++;
            break;
        }

        fprintf(stderr, "expected ',' or '}'\n");
        goto cleanup;
    }

    for (int i = 0; i < pending_count; i++) {
        int from = resolve_node(graph, pending[i].from);
        int to = resolve_node(graph, pending[i].to);

        if (from < 0 || to < 0) {
            fprintf(
                stderr,
                "invalid node reference: %s %s %s\n",
                pending[i].from,
                pending[i].bidirectional ? "<->" :
                    (pending[i].reverse ? "<--" : "-->"),
                pending[i].to
            );
            goto cleanup;
        }

        if (pending[i].reverse) {
            int temporary = from;
            from = to;
            to = temporary;
        }

        if (!add_connection(
            graph,
            from,
            to,
            pending[i].bidirectional
        )) {
            goto cleanup;
        }
    }

    success = true;

cleanup:
    for (int i = 0; i < pending_count; i++) {
        free(pending[i].from);
        free(pending[i].to);
    }

    free(pending);
    return success;
}

static void set_color(SDL_Renderer *renderer, int r, int g, int b) {
    SDL_SetRenderDrawColor(renderer, r, g, b, 255);
}

#define NODES_PER_RING 13
#define MIN_LAYOUT_RADIUS 90

static Position get_node_position(
    int index,
    int node_count,
    const ChartConfig *config,
    int width,
    int height,
    double scale
) {
    int ring = index / NODES_PER_RING;
    int index_in_ring = index % NODES_PER_RING;

    int ring_count = (node_count + NODES_PER_RING - 1)
        / NODES_PER_RING;

    int nodes_in_ring = node_count - ring * NODES_PER_RING;

    if (nodes_in_ring > NODES_PER_RING)
        nodes_in_ring = NODES_PER_RING;

    double radius = LAYOUT_RADIUS * scale;

    if (ring_count > 1) {
        radius -= ring * 85 * scale;
    }

    double advance = config->specific.nodes.has_advance_angle
        ? config->specific.nodes.advance_angle
        : 360.0 / nodes_in_ring;

    double angle = (
        config->specific.nodes.start_angle + index_in_ring * advance
    ) * (PI / 180.0);

    return (Position) {
        .x = (int)(width / 2.0 + cos(angle) * radius),
        .y = (int)(height / 2.0 + sin(angle) * radius)
    };
}

static void draw_filled_circle(
    SDL_Renderer *renderer,
    int cx,
    int cy,
    int radius
) {
    for (int y = -radius; y <= radius; y++) {
        int x = (int)sqrt(radius * radius - y * y);
        SDL_RenderDrawLine(renderer, cx - x, cy + y, cx + x, cy + y);
    }
}

static void draw_image_in_circle(
    SDL_Renderer *renderer,
    SDL_Texture *image,
    int cx,
    int cy,
    int radius
) {
    if (radius <= 0) {
        return;
    }

#if SDL_VERSION_ATLEAST(2, 0, 18)
    enum { SEGMENTS = 48 };
    SDL_Vertex vertices[SEGMENTS + 2];
    int indices[SEGMENTS * 3];
    const SDL_Color white = {255, 255, 255, 255};

    vertices[0].position = (SDL_FPoint){(float)cx, (float)cy};
    vertices[0].color = white;
    vertices[0].tex_coord = (SDL_FPoint){0.5f, 0.5f};

    for (int i = 0; i <= SEGMENTS; i++) {
        double angle = 2.0 * PI * i / SEGMENTS;
        float u = (float)(0.5 + cos(angle) * 0.5);
        float v = (float)(0.5 + sin(angle) * 0.5);
        int vertex = i + 1;

        vertices[vertex].position = (SDL_FPoint) {
            (float)cx + (float)(cos(angle) * radius),
            (float)cy + (float)(sin(angle) * radius)
        };
        vertices[vertex].color = white;
        vertices[vertex].tex_coord = (SDL_FPoint){u, v};

        if (i > 0) {
            int triangle = (i - 1) * 3;
            indices[triangle] = 0;
            indices[triangle + 1] = i;
            indices[triangle + 2] = i + 1;
        }
    }

    if (SDL_RenderGeometry(
            renderer,
            image,
            vertices,
            SEGMENTS + 2,
            indices,
            SEGMENTS * 3
        ) != 0) {
        fprintf(stderr, "could not render node image: %s\n", SDL_GetError());
    }
#else
    int texture_width;
    int texture_height;

    if (SDL_QueryTexture(image, NULL, NULL, &texture_width, &texture_height) != 0) {
        fprintf(stderr, "could not query node image: %s\n", SDL_GetError());
        return;
    }

    int diameter = radius * 2;

    for (int y = -radius; y <= radius; y++) {
        int half_width = (int)sqrt(radius * radius - y * y);
        double left = (double)(radius - half_width) / diameter;
        double right = (double)(radius + half_width + 1) / diameter;
        double top = (double)(y + radius) / diameter;
        double bottom = (double)(y + radius + 1) / diameter;
        int source_x = (int)floor(left * texture_width);
        int source_y = (int)floor(top * texture_height);
        int source_right = (int)ceil(right * texture_width);
        int source_bottom = (int)ceil(bottom * texture_height);

        if (source_x >= texture_width)
            source_x = texture_width - 1;
        if (source_y >= texture_height)
            source_y = texture_height - 1;
        if (source_right > texture_width)
            source_right = texture_width;
        if (source_bottom > texture_height)
            source_bottom = texture_height;
        if (source_right <= source_x)
            source_right = source_x + 1;
        if (source_bottom <= source_y)
            source_bottom = source_y + 1;

        SDL_Rect source = {
            source_x,
            source_y,
            source_right - source_x,
            source_bottom - source_y
        };
        SDL_Rect destination = {
            cx - half_width,
            cy + y,
            half_width * 2 + 1,
            1
        };

        if (SDL_RenderCopy(renderer, image, &source, &destination) != 0) {
            fprintf(stderr, "could not render node image: %s\n", SDL_GetError());
            return;
        }
    }
#endif
}

static void draw_text(
    SDL_Renderer *renderer,
    TTF_Font *font,
    const char *text,
    int x,
    int y
) {
    SDL_Color color = {230, 230, 230, 255};
    SDL_Surface *surface = TTF_RenderUTF8_Blended(font, text, color);

    if (!surface) {
        return;
    }

    SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surface);

    if (texture) {
        SDL_Rect destination = {
            x - surface->w / 2,
            y,
            surface->w,
            surface->h
        };

        SDL_RenderCopy(renderer, texture, NULL, &destination);
        SDL_DestroyTexture(texture);
    }

    SDL_FreeSurface(surface);
}

static void draw_arrowhead(
    SDL_Renderer *renderer,
    double tip_x,
    double tip_y,
    double direction_x,
    double direction_y,
    double scale
) {
    const double length = 12.0 * scale;
    const double half_width = 6.0 * scale;
    double base_x = tip_x - direction_x * length;
    double base_y = tip_y - direction_y * length;
    double perpendicular_x = -direction_y * half_width;
    double perpendicular_y = direction_x * half_width;

    SDL_RenderDrawLine(
        renderer,
        (int)tip_x,
        (int)tip_y,
        (int)(base_x + perpendicular_x),
        (int)(base_y + perpendicular_y)
    );
    SDL_RenderDrawLine(
        renderer,
        (int)tip_x,
        (int)tip_y,
        (int)(base_x - perpendicular_x),
        (int)(base_y - perpendicular_y)
    );
}

static void draw_connection(
    SDL_Renderer *renderer,
    const Connection *connection,
    Position from,
    Position to,
    double from_radius,
    double to_radius,
    double scale
) {
    double dx = to.x - from.x;
    double dy = to.y - from.y;
    double distance = sqrt(dx * dx + dy * dy);

    if (distance == 0.0) {
        return;
    }

    double direction_x = dx / distance;
    double direction_y = dy / distance;
    double start_x = from.x + direction_x * (from_radius + 2 * scale);
    double start_y = from.y + direction_y * (from_radius + 2 * scale);
    double end_x = to.x - direction_x * (to_radius + 2 * scale);
    double end_y = to.y - direction_y * (to_radius + 2 * scale);

    SDL_RenderDrawLine(
        renderer,
        (int)start_x,
        (int)start_y,
        (int)end_x,
        (int)end_y
    );

    draw_arrowhead(
        renderer, end_x, end_y, direction_x, direction_y, scale
    );

    if (connection->bidirectional) {
        draw_arrowhead(
            renderer, start_x, start_y, -direction_x, -direction_y, scale
        );
    }
}

void node_graph_draw(
    SDL_Renderer *renderer,
    TTF_Font *font,
    const Graph *graph,
    const ChartConfig *config,
    int width,
    int height
) {
    double scale = fmin(
        width / 1000.0,
        height / 800.0
    );

    if (scale <= 0.0)
        return;

    set_color(renderer, 150, 155, 170);

    for (int i = 0; i < graph->data.nodes.connection_count; i++) {
        const Connection *connection = &graph->data.nodes.connections[i];
        int from_ring = connection->from / NODES_PER_RING;
        int to_ring = connection->to / NODES_PER_RING;
        int from_radius = (int)lround(
            (NODE_RADIUS - from_ring * 5) * scale
        );
        int to_radius = (int)lround(
            (NODE_RADIUS - to_ring * 5) * scale
        );

        if (from_radius < 1)
            from_radius = 1;

        if (to_radius < 1)
            to_radius = 1;

        Position from = get_node_position(
            connection->from,
            graph->data.nodes.node_count,
            config,
            width,
            height,
            scale
        );
        Position to = get_node_position(
            connection->to,
            graph->data.nodes.node_count,
            config,
            width,
            height,
            scale
        );

        draw_connection(
            renderer,
            connection,
            from,
            to,
            from_radius,
            to_radius,
            scale
        );
    }

    for (int i = 0; i < graph->data.nodes.node_count; i++) {
        Position position = get_node_position(
            i,
            graph->data.nodes.node_count,
            config,
            width,
            height,
            scale
        );
        int ring = i / NODES_PER_RING;
        int node_radius = (int)lround(
            (NODE_RADIUS - ring * 5) * scale
        );

        if (node_radius < 1)
            node_radius = 1;

        set_color(renderer, 10, 10, 14);
        draw_filled_circle(
            renderer,
            position.x,
            position.y,
            node_radius + 3
        );

        if (graph->data.nodes.nodes[i].has_color) {
            unsigned int hex;
            sscanf(graph->data.nodes.nodes[i].color, "%6x", &hex);

            set_color(
                renderer,
                (hex >> 16) & 0xFF,
                (hex >> 8) & 0xFF,
                hex & 0xFF
            );
        } else {
            set_color(renderer, 90, 150, 220);
        }

        draw_filled_circle(
            renderer,
            position.x,
            position.y,
            node_radius
        );

        const Node *node = &graph->data.nodes.nodes[i];

        if (node->image) {
            draw_image_in_circle(
                renderer,
                node->image,
                position.x,
                position.y,
                node_radius - 2
            );
        }

        const char *label = graph->data.nodes.nodes[i].name;

        if (config->global.show_identifiers &&
            graph->data.nodes.nodes[i].identifier) {
            label = graph->data.nodes.nodes[i].identifier;
        }

        draw_text(
            renderer,
            font,
            label,
            position.x,
            position.y + node_radius + (int)lround(8 * scale)
        );
    }
}
