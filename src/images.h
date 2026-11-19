#ifndef IMAGE_H
#define IMAGE_H

#include <SDL2/SDL.h>
#include <stdbool.h>

#define MAX_IMAGES 100

typedef struct {
    char *name;
    SDL_Texture *texture;
} Image;

typedef struct ImageRegistry {
    Image images[MAX_IMAGES];
    int count;
} ImageRegistry;

void init_image_registry(ImageRegistry *registry);
bool add_image(ImageRegistry *registry, SDL_Renderer *renderer, const char *name, const char *path);
SDL_Texture *get_image(ImageRegistry *registry, const char *name);
void free_image_registry(ImageRegistry *registry);

#endif