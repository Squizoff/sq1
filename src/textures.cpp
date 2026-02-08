#include "pch.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

RGBA get_texture_pixel(int tex_id, int x, int y)
{
    if (tex_id < 0 || tex_id >= count_t || !state.textures[tex_id]) {
        return { 255, 0, 255, 255 };
    }

    SDL_Surface* texture = state.textures[tex_id];
    if (!texture->pixels) {
        return { 255, 0, 0, 255 };
    }

    int texW = state.tex_width[tex_id];
    int texH = state.tex_height[tex_id];

    if (texW <= 0 || texH <= 0) {
        return { 255, 255, 0, 255 };
    }

    int texX = ((x % texW) + texW) % texW;
    int texY = ((y % texH) + texH) % texH;

    uint32_t* pixels = (uint32_t*)texture->pixels;
    uint32_t pixel = pixels[texY * texW + texX];

    return {
        static_cast<uint8_t>(pixel & 0xFF),
        static_cast<uint8_t>((pixel >> 8) & 0xFF),
        static_cast<uint8_t>((pixel >> 16) & 0xFF),
        static_cast<uint8_t>((pixel >> 24) & 0xFF)
    };
}

int load_textures()
{
    const char* texture_files[count_t] = { "wall1.png", "floor.png", "enemy.png", "wall1.png", "sky.png", "weapon.png" };

    for (int i = 0; i < count_t; i++) {
        int width, height, channels;
        uint8_t* pixels = stbi_load(texture_files[i], &width, &height, &channels, 4);
        if (!pixels) {
            std::cerr << "err loading: " << texture_files[i] << " " << stbi_failure_reason() << std::endl;
            return 0;
        }

        SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormat(0, width, height, 32, SDL_PIXELFORMAT_ABGR8888);
        if (!surface) {
            std::cerr << "err creating surface: " << SDL_GetError() << std::endl;
            stbi_image_free(pixels);
            return 0;
        }

        memcpy(surface->pixels, pixels, width * height * 4);
        stbi_image_free(pixels);

        state.textures[i] = surface;
        state.tex_width[i] = width;
        state.tex_height[i] = height;
    }

    return 1;
}
