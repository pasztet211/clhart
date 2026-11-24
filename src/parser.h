#ifndef PARSER_H
#define PARSER_H

#include <stdbool.h>
#include <SDL2/SDL.h>

typedef struct ImageRegistry ImageRegistry;

typedef enum {
    CHART_NODES,
    CHART_HISTOGRAM,
    CHART_BAR,
    CHART_COLUMN,
    CHART_LINE,
    CHART_PIE,
    CHART_COMBINATION,
    CHART_DISPLAY
} ChartType;

typedef struct {
    bool show_identifiers;
    bool legend;
    bool no_underscore_names;
    char *xlabel;
    char *ylabel;
} GlobalConfig;

typedef struct {
    double start_angle;
    double advance_angle;

    bool has_start_angle;
    bool has_advance_angle;
} NodeConfig;

typedef enum {
    HISTOGRAM_FUZZY,
    HISTOGRAM_DISTRIBUTION
} HistogramType;

typedef struct {
    HistogramType type;

    bool show_grid;
    bool show_axis_numbers;
    bool fuzzy;

    double x_min;
    double x_max;
    double y_min;
    double y_max;

    bool has_x_range;
    bool has_y_range;
} HistogramConfig;

typedef struct {
    bool show_percentages;
    bool three_dimensional;
} PieConfig;

typedef struct {
    bool horizontal;
    bool show_values;
    bool show_axis_numbers;

    double y_min;
    double y_max;
    bool has_y_range;
} BarConfig;

typedef struct {
    bool show_values;
    bool show_axis_numbers;

    double y_min;
    double y_max;
    bool has_y_range;
} ColumnConfig;

typedef struct Graph Graph;
typedef struct ChartConfig ChartConfig;

typedef struct {
    bool show_sub_titles;
} CombinationConfig;

typedef enum {
    DISPLAY_FIT,
    DISPLAY_STRETCH,
    DISPLAY_NONE
} DisplayFit;

typedef struct {
    DisplayFit fit;
    int border_thickness;
    char border_color[7];
    bool has_border;
} DisplayConfig;

struct ChartConfig {
    ChartType type;

    GlobalConfig global;
    char *title;

    union {
        NodeConfig nodes;
        HistogramConfig histogram;
        PieConfig pie;
        BarConfig bar;
        CombinationConfig combination;
        DisplayConfig display;
        ColumnConfig column;
        ColumnConfig line;
    } specific;
};

typedef struct {
    char *name;
    char *identifier;
    char color[7];
    bool has_color;

    SDL_Texture *image;
} Node;

typedef struct {
    bool bidirectional;
    int from;
    int to;
} Connection;

typedef struct {
    char *name;

    double a;
    double b;
    double c;
    double d;

    bool trapezoidal;

    char color[7];
    bool has_color;
} FuzzyParameter;

typedef struct {
    char *name;
    double min;
    double max;
    double amount;
    char color[7];
    bool has_color;
    SDL_Texture *image;
} DistributionParameter;

typedef struct {
    union {
        struct {
            FuzzyParameter *parameters;
            int parameter_count;
        } fuzzy;

        struct {
            DistributionParameter *parameters;
            int parameter_count;
        } distribution;
    } data;
} HistogramGraph;

typedef struct {
    char *name;
    double value;

    char color[7];
    bool has_color;

    SDL_Texture *image;
} PieSlice;

typedef struct {
    PieSlice *slices;
    int slice_count;
} PieGraph;

typedef struct {
    char *name;
    double value;

    char color[7];
    bool has_color;

    SDL_Texture *image;
} Bar;

typedef struct {
    Bar *bars;
    int bar_count;
} BarGraph;

typedef struct {
    char *name;
    double *values;
    int value_count;

    char color[7];
    bool has_color;
} ColumnSeries;

typedef struct {
    char *name;
    SDL_Texture *image;
} ColumnCategory;

typedef struct {
    ColumnSeries *series;
    int series_count;

    ColumnCategory *categories;
    int category_count;
    bool has_categories;
} ColumnGraph;

typedef struct {
    Graph *graph;
    ChartConfig *config;
} CombinationChart;

typedef struct {
    CombinationChart *charts;
    int chart_count;
} CombinationGraph;

typedef struct {
    SDL_Texture *image;
} DisplayGraph;

struct Graph {
    union {
        struct {
            Node *nodes;
            int node_count;

            Connection *connections;
            int connection_count;
        } nodes;

        HistogramGraph histogram;
        PieGraph pie;
        BarGraph bar;
        CombinationGraph combination;
        DisplayGraph display;
        ColumnGraph column;
        ColumnGraph line;
    } data;

    ImageRegistry *images;
};

bool parse_file(
    const char *filename,
    Graph **graph,
    ChartConfig **config,
    SDL_Renderer *renderer
);

void free_graph(Graph *graph, ChartType type);
void free_chart_config(ChartConfig *config);

#endif