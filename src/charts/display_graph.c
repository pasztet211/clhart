#include "display_graph.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

void display_graph_draw(
    SDL_Renderer *renderer,
    const Graph *graph,
    const ChartConfig *config,
    int width,
    int height
) {
    if (!renderer || !graph || !config || !graph->data.display.image) {
        fprintf(stderr, "cannot render a display without an image\n");
        return;
    }

    SDL_Texture *image = graph->data.display.image;
    int image_width;
    int image_height;

    if (SDL_QueryTexture(image, NULL, NULL, &image_width, &image_height) != 0) {
        fprintf(stderr, "could not query display image: %s\n", SDL_GetError());
        return;
    }

    if (image_width <= 0 || image_height <= 0 || width <= 0 || height <= 0) {
        fprintf(stderr, "display image or render area has invalid dimensions\n");
        return;
    }

    SDL_Rect destination = {0, 0, 0, 0};

    switch (config->specific.display.fit) {
        case DISPLAY_FIT: {
            double scale = fmin(
                (double)width / image_width,
                (double)height / image_height
            );
            int fitted_width = (int)lround(image_width * scale);
            int fitted_height = (int)lround(image_height * scale);
            if (fitted_width < 1)
                fitted_width = 1;
            if (fitted_height < 1)
                fitted_height = 1;

            destination = (SDL_Rect) {
                (width - fitted_width) / 2,
                (height - fitted_height) / 2,
                fitted_width,
                fitted_height
            };
            break;
        }

        case DISPLAY_STRETCH:
            destination = (SDL_Rect) {0, 0, width, height};
            break;

        case DISPLAY_NONE:
            destination = (SDL_Rect) {
                (width - image_width) / 2,
                (height - image_height) / 2,
                image_width,
                image_height
            };
            break;

        default:
            fprintf(stderr, "cannot render display with unknown fit mode\n");
            return;
    }

    if (SDL_RenderCopy(renderer, image, NULL, &destination) != 0) {
        fprintf(stderr, "could not render display image: %s\n", SDL_GetError());
        return;
    }

    const DisplayConfig *settings = &config->specific.display;
    if (!settings->has_border) {
        return;
    }

    unsigned int rgb = (unsigned int)strtoul(
        settings->border_color,
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

    int border_layers = settings->border_thickness;
    int max_layers = (destination.w < destination.h
        ? destination.w
        : destination.h) / 2;
    if (max_layers < 1)
        max_layers = 1;
    if (border_layers > max_layers)
        border_layers = max_layers;

    for (int i = 0; i < border_layers; i++) {
        SDL_Rect border = {
            destination.x + i,
            destination.y + i,
            destination.w - i * 2,
            destination.h - i * 2
        };

        if (SDL_RenderDrawRect(renderer, &border) != 0) {
            fprintf(stderr, "could not draw display border: %s\n", SDL_GetError());
            return;
        }
    }
}
