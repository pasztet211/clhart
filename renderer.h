#ifndef RENDERER_H
#define RENDERER_H

#include <stdbool.h>
#include "parser.h"

bool render_graph(
    const Graph *graph,
    const char *font_path,
    const ChartConfig *config
);

#endif