#define STB_IMAGE_IMPLEMENTATION
#include "../stb_image.h"

#include "images.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static SDL_Texture *load_image(SDL_Renderer *renderer, const char *path)
{
    int width, height, channels;
    char *full_path = _fullpath(NULL, path, 0);

    if (!full_path) {
        fprintf(stderr, "image loading failed: could not resolve path '%s'\n", path);
        return NULL;
    }

    unsigned char *pixels = stbi_load(
        full_path,
        &width,
        &height,
        &channels,
        STBI_rgb_alpha
    );

    free(full_path);

    if (!pixels) {
        fprintf(stderr, "image loading failed: %s\n", stbi_failure_reason());
        return NULL;
    }

    SDL_Surface *surface = SDL_CreateRGBSurfaceWithFormatFrom(
        pixels,
        width,
        height,
        32,
        width * 4,
        SDL_PIXELFORMAT_RGBA32
    );

    if (!surface) {
        stbi_image_free(pixels);
        return NULL;
    }

    SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surface);

    SDL_FreeSurface(surface);
    stbi_image_free(pixels);

    return texture;
}

void init_image_registry(ImageRegistry *registry)
{
    if (!registry)
        return;

    *registry = (ImageRegistry){0};
}

bool add_image(ImageRegistry *registry, SDL_Renderer *renderer,
               const char *name, const char *path)
{
    if (!registry || !renderer || !name || !path)
        return false;

    if (registry->count >= MAX_IMAGES)
        return false;

    if (get_image(registry, name))
        return false;

    SDL_Texture *texture = load_image(renderer, path);

    if (!texture)
        return false;

    char *image_name = malloc(strlen(name) + 1);

    if (!image_name) {
        SDL_DestroyTexture(texture);
        return false;
    }

    strcpy(image_name, name);

    registry->images[registry->count++] = (Image) {
        .name = image_name,
        .texture = texture
    };

    return true;
}

SDL_Texture *get_image(ImageRegistry *registry, const char *name)
{
    if (!registry || !name)
        return NULL;

    for (int i = 0; i < registry->count; i++) {
        if (strcmp(registry->images[i].name, name) == 0)
            return registry->images[i].texture;
    }

    return NULL;
}

void free_image_registry(ImageRegistry *registry)
{
    if (!registry)
        return;

    for (int i = 0; i < registry->count; i++) {
        free(registry->images[i].name);
        SDL_DestroyTexture(registry->images[i].texture);
    }

    *registry = (ImageRegistry){0};
}