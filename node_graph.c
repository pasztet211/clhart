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
    const char *color
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
        .has_color = color != NULL
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

bool node_graph_parse(Parser *parser, Graph *graph) {
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

        char *first = parse_name(parser);
        if (!first) {
            fprintf(stderr, "expected node name\n");
            goto cleanup;
        }

        char color[7] = {0};
        bool has_color = false;
        bool declaration = false;
        bool is_connection = false;
        bool bidirectional = false;
        bool reverse = false;

        if (expect_operator(parser, "---")) {
            declaration = true;
            char *identifier = parse_name(parser);

            if (!identifier) {
                fprintf(stderr, "expected identifier after '---'\n");
                free(first);
                goto cleanup;
            }

            if (!parse_optional_color(parser, color, &has_color)) {
                free(first);
                free(identifier);
                goto cleanup;
            }

            if (!add_node(
                graph,
                first,
                identifier,
                has_color ? color : NULL
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
            char *second = parse_name(parser);

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
            if (!parse_optional_color(parser, color, &has_color)) {
                free(first);
                goto cleanup;
            }

            if (!add_node(
                graph,
                first,
                NULL,
                has_color ? color : NULL
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

static Position get_node_position(
    int index,
    int node_count,
    const ChartConfig *config,
    int width,
    int height
) {
    double advance = config->specific.nodes.has_advance_angle
        ? config->specific.nodes.advance_angle
        : 360.0 / node_count;
    double angle = (
        config->specific.nodes.start_angle + index * advance
    ) * (PI / 180.0);

    return (Position) {
        .x = (int)(width / 2.0 + cos(angle) * LAYOUT_RADIUS),
        .y = (int)(height / 2.0 + sin(angle) * LAYOUT_RADIUS)
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
    double direction_y
) {
    const double length = 12.0;
    const double half_width = 6.0;
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
    Position to
) {
    double dx = to.x - from.x;
    double dy = to.y - from.y;
    double distance = sqrt(dx * dx + dy * dy);

    if (distance == 0.0) {
        return;
    }

    double direction_x = dx / distance;
    double direction_y = dy / distance;
    double start_x = from.x + direction_x * (NODE_RADIUS + 2);
    double start_y = from.y + direction_y * (NODE_RADIUS + 2);
    double end_x = to.x - direction_x * (NODE_RADIUS + 2);
    double end_y = to.y - direction_y * (NODE_RADIUS + 2);

    SDL_RenderDrawLine(
        renderer,
        (int)start_x,
        (int)start_y,
        (int)end_x,
        (int)end_y
    );

    draw_arrowhead(renderer, end_x, end_y, direction_x, direction_y);

    if (connection->bidirectional) {
        draw_arrowhead(renderer, start_x, start_y, -direction_x, -direction_y);
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
    set_color(renderer, 150, 155, 170);

    for (int i = 0; i < graph->data.nodes.connection_count; i++) {
        const Connection *connection = &graph->data.nodes.connections[i];
        Position from = get_node_position(
            connection->from,
            graph->data.nodes.node_count,
            config,
            width,
            height
        );
        Position to = get_node_position(
            connection->to,
            graph->data.nodes.node_count,
            config,
            width,
            height
        );

        draw_connection(renderer, connection, from, to);
    }

    for (int i = 0; i < graph->data.nodes.node_count; i++) {
        Position position = get_node_position(
            i,
            graph->data.nodes.node_count,
            config,
            width,
            height
        );

        set_color(renderer, 10, 10, 14);
        draw_filled_circle(
            renderer,
            position.x,
            position.y,
            NODE_RADIUS + 3
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
            NODE_RADIUS
        );

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
            position.y + NODE_RADIUS + 8
        );
    }
}
