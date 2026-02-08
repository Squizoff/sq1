#include "pch.h"

RGBA skyColor = { 255, 255, 255, 255 };
#define FOG_DENSITY 0.2f

static RGBA apply_fog(RGBA color, float distance)
{
#if 1
    float fog_factor = std::min(1.0f, clamp(distance * FOG_DENSITY, 0.0f, 1.0f));
    color.r = static_cast<uint8_t>(color.r * (1 - fog_factor));
    color.g = static_cast<uint8_t>(color.g * (1 - fog_factor));
    color.b = static_cast<uint8_t>(color.b * (1 - fog_factor));
#endif
    return color;
}

static int trace(float startX, float startY, float dirX, float dirY, float maxDist)
{
    v3 mapPos = {
        static_cast<float>((int)startX),
        static_cast<float>((int)startY),
        0.0f
    };

    v3 deltaDist = { fabsf(1.0f / dirX), fabsf(1.0f / dirY), 0 };
    v3 step = {
        static_cast<float>((dirX < 0) ? -1 : 1),
        static_cast<float>((dirY < 0) ? -1 : 1),
        0.0f
    };

    v3 sideDist = {
        (dirX < 0) ? (startX - mapPos.x) * deltaDist.x
                   : (mapPos.x + 1 - startX) * deltaDist.x,
        (dirY < 0) ? (startY - mapPos.y) * deltaDist.y
                   : (mapPos.y + 1 - startY) * deltaDist.y,
        0
    };

    while (maxDist > 0) {
        if (sideDist.x < sideDist.y) {
            sideDist.x += deltaDist.x;
            mapPos.x += step.x;
        } else {
            sideDist.y += deltaDist.y;
            mapPos.y += step.y;
        }

        if (MAPDATA[(int)mapPos.y * MAP_SIZE + (int)mapPos.x] == 1) {
            return 0;
        }
        maxDist -= 1.0f;
    }

    return 1;
}

static int is_visible(float targetX, float targetY)
{
    float deltaX = targetX - state.pos.x;
    float deltaY = targetY - state.pos.y;
    float distSq = deltaX * deltaX + deltaY * deltaY;

    const float maxViewDistSq = 15.0f * 15.0f;
    if (distSq > maxViewDistSq)
        return 0;

    float invDist = 1.0f / sqrtf(distSq);
    return trace(state.pos.x, state.pos.y, deltaX * invDist, deltaY * invDist, sqrtf(distSq));
}

static void update_dynamic_lights(float deltaTime)
{
    for (DLight& light : dynamicLights) {
        light.time += deltaTime;

        const char flickerPattern[] = "mmamammmmammamamaaamammma";
        int patternLength = sizeof(flickerPattern) - 1;
        int patternIndex = (int)(light.time * 10) % patternLength;

        switch (light.pattern) {
        case CONSTANT:
            break;
        case PULSE:
            light.intensity = 0.5f + 0.5f * sin(light.time * 2.0f);
            break;
        case FLICKER:
            if (flickerPattern[patternIndex] == 'm') {
                light.intensity = 1.0f;
            } else {
                light.intensity = 0.0f;
            }
            break;
        }
    }
}

static RGBA apply_dynamic_lights(RGBA color, float pixelX, float pixelY)
{
    for (const DLight& light : dynamicLights) {
        float dx = pixelX - light.x;
        float dy = pixelY - light.y;
        float dist2 = dx * dx + dy * dy;
        float radius2 = light.radius * light.radius;

        if (dist2 < radius2) {
            float influence = ((radius2 - dist2) * (radius2 - dist2)) / radius2;

            color.r = clamp(color.r + (light.color.r * influence) / radius2, 0.0f, 255.0f);
            color.g = clamp(color.g + (light.color.g * influence) / radius2, 0.0f, 255.0f);
            color.b = clamp(color.b + (light.color.b * influence) / radius2, 0.0f, 255.0f);
        }
    }
    return color;
}

static RGBA apply_tonemap(RGBA color)
{
#if 1
    float influenceFactor = 1.0f * 0.5f;
    float brightness = (color.r + color.g + color.b) / 3.0f;
    float skyBrightness = (skyColor.r + skyColor.g + skyColor.b) / 3.0f;
    float skyBrightnessFactor = 1.0f - (skyBrightness / 255.0f);
    influenceFactor *= skyBrightnessFactor;
    float threshold = 20.0f;
    if (brightness < threshold) {
        return color;
    }

    color.r = static_cast<uint8_t>(color.r * (1 - influenceFactor) + skyColor.r * influenceFactor);
    color.g = static_cast<uint8_t>(color.g * (1 - influenceFactor) + skyColor.g * influenceFactor);
    color.b = static_cast<uint8_t>(color.b * (1 - influenceFactor) + skyColor.b * influenceFactor);
#endif

    return color;
}

void apply_dither()
{
    static const uint8_t bayer_matrix[4][4] = {
        { 0, 32, 8, 40 },
        { 48, 16, 56, 24 },
        { 12, 44, 4, 36 },
        { 60, 28, 52, 20 }
    };

    for (int y = 0; y < SCREEN_HEIGHT; y++) {
        for (int x = 0; x < SCREEN_WIDTH; x++) {
            uint32_t color = state.pixels[y * SCREEN_WIDTH + x];

            uint8_t r = (color >> 16) & 0xFF;
            uint8_t g = (color >> 8) & 0xFF;
            uint8_t b = color & 0xFF;

            uint8_t bayerValue = bayer_matrix[y & 3][x & 3];
            int dither_offset = bayerValue - 31;

            r = clamp(r + (r * dither_offset >> 6), 0, 255);
            g = clamp(g + (g * dither_offset >> 6), 0, 255);
            b = clamp(b + (b * dither_offset >> 6), 0, 255);

            r = (r / 32) * 32;
            g = (g / 32) * 32;
            b = (b / 32) * 32;

            state.pixels[y * SCREEN_WIDTH + x] = (r << 16) | (g << 8) | b;
        }
    }
}

void apply_glitch()
{
    for (int y = 0; y < SCREEN_HEIGHT; y++) {
        if (rand() % 10 < 2) {
            int shift = (rand() % 20) - 10;
            for (int x = SCREEN_WIDTH - 1; x >= 0; x--) {
                int newX = x + shift;
                if (newX >= 0 && newX < SCREEN_WIDTH) {
                    state.pixels[y * SCREEN_WIDTH + newX] = state.pixels[y * SCREEN_WIDTH + x];
                }
            }
        }
    }

    for (int i = 0; i < SCREEN_WIDTH * SCREEN_HEIGHT; i++) {
        if (rand() % 50 == 0) {
            uint32_t color = state.pixels[i];
            uint8_t r = (color >> 16) & 0xFF;
            uint8_t g = (color >> 8) & 0xFF;
            uint8_t b = color & 0xFF;

            r = (r + rand() % 100 - 50) & 0xFF;
            g = (g + rand() % 100 - 50) & 0xFF;
            b = (b + rand() % 100 - 50) & 0xFF;

            state.pixels[i] = (r << 16) | (g << 8) | b;
        }
    }
}

static void onerender()
{
    SDL_UpdateTexture(state.texture, NULL, state.pixels, SCREEN_WIDTH * 4);
    SDL_RenderCopyEx(state.renderer, state.texture, NULL, NULL, 0.0, NULL, SDL_FLIP_NONE);
    SDL_RenderPresent(state.renderer);
}

static void render_entities()
{
    for (int i = 0; i < MAP_SIZE * MAP_SIZE; i++) {
        if (MAPDATA[i] == 2) {
            int mapX = i % MAP_SIZE;
            int mapY = i / MAP_SIZE;

            float spriteX = mapX + 0.5f - state.pos.x;
            float spriteY = mapY + 0.5f - state.pos.y;

            if (!is_visible(mapX, mapY)) {
                continue;
            }

            float invDet = 1.0f / (state.plane.x * state.dir.y - state.dir.x * state.plane.y);
            float transformX = invDet * (state.dir.y * spriteX - state.dir.x * spriteY);
            float transformY = invDet * (-state.plane.y * spriteX + state.plane.x * spriteY);

            if (transformY <= 0)
                continue;

            int spriteScreenX = (int)(((float)SCREEN_WIDTH / 2) * (1 + transformX / transformY));
            int spriteHeight = abs((int)((float)SCREEN_HEIGHT / transformY));
            int drawStartY = -((float)spriteHeight / 2) + ((float)SCREEN_HEIGHT / 2) + state.pitch;
            drawStartY = std::max(drawStartY, 0);
            int drawEndY = ((float)spriteHeight / 2) + ((float)SCREEN_HEIGHT / 2) + state.pitch;
            drawEndY = std::min(drawEndY, SCREEN_HEIGHT - 1);

            int spriteWidth = abs((int)(SCREEN_HEIGHT / transformY));
            int drawStartX = -spriteWidth / 2 + spriteScreenX;
            drawStartX = std::max(drawStartX, 0);
            int drawEndX = spriteWidth / 2 + spriteScreenX;
            drawEndX = std::min(drawEndX, SCREEN_WIDTH - 1);

            int texWidth = state.tex_width[2];
            int texHeight = state.tex_height[2];

            for (int x = drawStartX; x < drawEndX; x++) {
                int texX = (int)((x - ((float)-spriteWidth / 2 + spriteScreenX)) * texWidth / (float)spriteWidth);
                if (texX < 0)
                    texX = 0;
                if (texX >= texWidth)
                    texX = texWidth - 1;

                for (int y = drawStartY; y < drawEndY; y++) {
                    float realSpriteHeight = SCREEN_HEIGHT / transformY;
                    float texPos = ((y - SCREEN_HEIGHT / 2.0f) + (realSpriteHeight / 2.0f) - state.pitch) * texHeight / realSpriteHeight;
                    int texY = (int)texPos;
                    if (texY < 0)
                        texY = 0;
                    if (texY >= texHeight)
                        texY = texHeight - 1;

                    float spriteDist = transformY;
                    if (spriteDist < 0.05f) {
                        continue;
                    }

                    RGBA color = get_texture_pixel(2, texX, texY);
                    color = apply_fog(color, spriteDist);
                    color = apply_tonemap(color);

                    if (color.a > 0) {
                        uint32_t bgColor = state.pixels[y * SCREEN_WIDTH + x];
                        uint8_t bgR = bgColor & 0xFF;
                        uint8_t bgG = (bgColor >> 8) & 0xFF;
                        uint8_t bgB = (bgColor >> 16) & 0xFF;

                        float alpha = color.a / 255.0f;

                        uint8_t outR = (uint8_t)(color.r * alpha + bgR * (1.0f - alpha));
                        uint8_t outG = (uint8_t)(color.g * alpha + bgG * (1.0f - alpha));
                        uint8_t outB = (uint8_t)(color.b * alpha + bgB * (1.0f - alpha));

                        state.pixels[y * SCREEN_WIDTH + x] = (outB << 16) | (outG << 8) | outR;
                    }
                }
            }
        }
    }
}

static void render_sky(int skyHeight, float viewAngle)
{
    const int texWidth = state.tex_width[4];
    const int texHeight = state.tex_height[4];
    const int baseSkyHeight = SCREEN_HEIGHT / 2;
    const float texPerPixel = 1.0f / SCREEN_WIDTH;

    for (int screenY = 0; screenY < skyHeight; screenY++) {
        int texY_unclamped = screenY - state.pitch;
        if (texY_unclamped < 0)
            texY_unclamped = 0;
        else if (texY_unclamped >= baseSkyHeight)
            texY_unclamped = baseSkyHeight - 1;

        int texY = (texY_unclamped * texHeight) / baseSkyHeight;

        for (int x = 0; x < SCREEN_WIDTH; x++) {
            float texU = viewAngle - ((float)x * texPerPixel);
            if (texU < 0.0f)
                texU += 1.0f;
            else if (texU >= 1.0f)
                texU -= 1.0f;

            int texX = (int)(texU * texWidth);

            RGBA color = get_texture_pixel(4, texX, texY);
            uint32_t pixel = (color.b << 16) | (color.g << 8) | color.r;

            state.pixels[screenY * SCREEN_WIDTH + x] = pixel;
        }
    }
}

static void render_floor(int horizon)
{
    for (int y = horizon; y < SCREEN_HEIGHT; y++) {
        int p = y - horizon;
        if (p == 0)
            p = 1;

        float rowDist = (0.5f * SCREEN_HEIGHT) / p;

        float floorX = state.pos.x + rowDist * (state.dir.x - state.plane.x);
        float floorY = state.pos.y + rowDist * (state.dir.y - state.plane.y);

        float floorStepX = 2.0f * rowDist * state.plane.x / SCREEN_WIDTH;
        float floorStepY = 2.0f * rowDist * state.plane.y / SCREEN_WIDTH;

        for (int x = 0; x < SCREEN_WIDTH; x++) {
            int texWidth = state.tex_width[1];
            int texHeight = state.tex_height[1];

            int texX = ((int)(floorX * texWidth)) % texWidth;
            if (texX < 0)
                texX += texWidth;

            int texY = ((int)(floorY * texHeight)) % texHeight;
            if (texY < 0)
                texY += texHeight;

            RGBA color = get_texture_pixel(1, texX, texY);
            color = apply_tonemap(color);
            color = apply_fog(color, rowDist);
            color = apply_dynamic_lights(color, floorX, floorY);

            state.pixels[y * SCREEN_WIDTH + x] = (color.b << 16) | (color.g << 8) | color.r;

            floorX += floorStepX;
            floorY += floorStepY;
        }
    }
}

static void render_other()
{
    const int baseSkyHeight = SCREEN_HEIGHT / 2;
    int horizon = baseSkyHeight + state.pitch;
    if (horizon < 0)
        horizon = 0;
    if (horizon > SCREEN_HEIGHT)
        horizon = SCREEN_HEIGHT;

    float yaw = atan2f(state.dir.y, state.dir.x);
    float viewAngle = yaw / (2.0f * M_PI);
    if (viewAngle < 0.0f)
        viewAngle += 1.0f;

    render_sky(horizon, viewAngle);
    render_floor(horizon);
}

static void render_walls()
{
    for (int x = 0; x < SCREEN_WIDTH; ++x) {
        int cameraX_fixed = ((2 * x) << 16) / SCREEN_WIDTH - (1 << 16);

        float rayDirX = state.dir.x + ((state.plane.x * cameraX_fixed) / 65536.0f);
        float rayDirY = state.dir.y + ((state.plane.y * cameraX_fixed) / 65536.0f);

        int mapX = (int)state.pos.x;
        int mapY = (int)state.pos.y;

        float deltaDistX = (rayDirX == 0) ? 1e30f : fabsf(1.0f / rayDirX);
        float deltaDistY = (rayDirY == 0) ? 1e30f : fabsf(1.0f / rayDirY);

        float sideDistX, sideDistY;
        int stepX = (rayDirX < 0) ? -1 : 1;
        int stepY = (rayDirY < 0) ? -1 : 1;

        sideDistX = (rayDirX < 0)
            ? (state.pos.x - mapX) * deltaDistX
            : (mapX + 1.0f - state.pos.x) * deltaDistX;

        sideDistY = (rayDirY < 0)
            ? (state.pos.y - mapY) * deltaDistY
            : (mapY + 1.0f - state.pos.y) * deltaDistY;

        int hit = 0, side = 0;
        while (!hit) {
            if (sideDistX < sideDistY) {
                sideDistX += deltaDistX;
                mapX += stepX;
                side = 0;
            } else {
                sideDistY += deltaDistY;
                mapY += stepY;
                side = 1;
            }

            if (mapX < 0 || mapX >= (int)MAP_SIZE || mapY < 0 || mapY >= (int)MAP_SIZE)
                break;

            int tile = MAPDATA[mapY * MAP_SIZE + mapX];
            if (tile && tile != 3 && tile != 2)
                hit = 1;
        }

        if (!hit)
            continue;

        float perpWallDist = (side == 0)
            ? (sideDistX - deltaDistX)
            : (sideDistY - deltaDistY);

        if (perpWallDist <= 0.01f)
            perpWallDist = 0.01f;

        int lineHeight = (int)(SCREEN_HEIGHT / perpWallDist);
        int drawStart = (SCREEN_HEIGHT >> 1) - (lineHeight >> 1) + state.pitch;
        int drawEnd = drawStart + lineHeight;

        if (drawStart < 0)
            drawStart = 0;
        if (drawEnd >= SCREEN_HEIGHT)
            drawEnd = SCREEN_HEIGHT - 1;

        float wallHit = (side == 0)
            ? state.pos.y + perpWallDist * rayDirY
            : state.pos.x + perpWallDist * rayDirX;
        wallHit -= (int)wallHit;

        int texId = MAPDATA[mapY * MAP_SIZE + mapX] - 1;
        int texW = state.tex_width[texId];
        int texH = state.tex_height[texId];

        int texX = (int)(wallHit * texW);
        if ((side == 0 && rayDirX > 0) || (side == 1 && rayDirY < 0))
            texX = texW - texX - 1;

        float step = (float)texH / lineHeight;
        float texPos = (drawStart - SCREEN_HEIGHT / 2.0f + lineHeight / 2.0f - state.pitch) * step;

        for (int y = drawStart; y <= drawEnd; ++y) {
            int texY = (int)texPos;
            texPos += step;

            texY = (texY < 0) ? 0 : ((texY >= texH) ? texH - 1 : texY);

            RGBA color = get_texture_pixel(texId, texX, texY);
            color = apply_tonemap(color);
            color = apply_fog(color, perpWallDist);
            color = apply_dynamic_lights(color, state.pos.x + rayDirX * perpWallDist, state.pos.y + rayDirY * perpWallDist);
            if (side == 1) {
                color.r >>= 1;
                color.g >>= 1;
                color.b >>= 1;
            }

            state.pixels[y * SCREEN_WIDTH + x] = (color.b << 16) | (color.g << 8) | color.r;
        }
    }
}

static void render_weapon()
{
    SDL_Surface* weaponTexture = state.textures[5];
    if (!weaponTexture || !weaponTexture->pixels) {
        return;
    }

    int weaponWidth = state.tex_width[5];
    int weaponHeight = state.tex_height[5];

    float desiredScreenRatio = 0.3f;

    float scale = (SCREEN_WIDTH * desiredScreenRatio) / weaponWidth;

    if (scale < 0.5f)
        scale = 0.5f;
    if (scale > 2.5f)
        scale = 2.5f;

    int newWeaponWidth = (int)(weaponWidth * scale);
    int newWeaponHeight = (int)(weaponHeight * scale);

    int xOffset = (SCREEN_WIDTH - newWeaponWidth) / 2;
    int yOffset = SCREEN_HEIGHT - newWeaponHeight;

    uint32_t* weaponPixels = (uint32_t*)weaponTexture->pixels;

    for (int y = 0; y < newWeaponHeight; y++) {
        for (int x = 0; x < newWeaponWidth; x++) {
            int origX = (int)(x / scale);
            int origY = (int)(y / scale);

            if (origX < 0 || origX >= weaponWidth || origY < 0 || origY >= weaponHeight)
                continue;

            uint32_t color = weaponPixels[origY * weaponWidth + origX];

            uint8_t a = (color >> 24) & 0xFF;
            if (a == 0)
                continue;

            uint8_t r = (color >> 16) & 0xFF;
            uint8_t g = (color >> 8) & 0xFF;
            uint8_t b = color & 0xFF;

            RGBA weaponColor = { b, g, r, a };
            weaponColor = apply_tonemap(weaponColor);
            weaponColor = apply_dynamic_lights(weaponColor, state.pos.x, state.pos.y);

            int pixelX = x + xOffset;
            int pixelY = y + yOffset;

            if (pixelX < 0 || pixelX >= SCREEN_WIDTH || pixelY < 0 || pixelY >= SCREEN_HEIGHT)
                continue;

            state.pixels[pixelY * SCREEN_WIDTH + pixelX] = (weaponColor.b << 16) | (weaponColor.g << 8) | weaponColor.r;
        }
    }
}

static void draw_line(int x0, int y0, int x1, int y1, RGBA color)
{
    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy, e2;

    while (1) {
        if (x0 >= 0 && x0 < SCREEN_WIDTH && y0 >= 0 && y0 < SCREEN_HEIGHT) {
            uint32_t* dst_pixel = &state.pixels[y0 * SCREEN_WIDTH + x0];
            uint32_t dst_color = *dst_pixel;

            uint8_t dst_r = (dst_color >> 16) & 0xFF;
            uint8_t dst_g = (dst_color >> 8) & 0xFF;
            uint8_t dst_b = dst_color & 0xFF;

            float alpha = color.a / 255.0f;

            uint8_t out_r = (uint8_t)(color.r * alpha + dst_r * (1.0f - alpha));
            uint8_t out_g = (uint8_t)(color.g * alpha + dst_g * (1.0f - alpha));
            uint8_t out_b = (uint8_t)(color.b * alpha + dst_b * (1.0f - alpha));

            *dst_pixel = (out_r << 16) | (out_g << 8) | out_b;
        }
        if (x0 == x1 && y0 == y1)
            break;
        e2 = 2 * err;
        if (e2 >= dy) {
            err += dy;
            x0 += sx;
        }
        if (e2 <= dx) {
            err += dx;
            y0 += sy;
        }
    }
}

static void render_bullet_trail()
{
    if (bulletTrail.size() < 2)
        return;

    float invDet = 1.0f / (state.plane.x * state.dir.y - state.dir.x * state.plane.y);

    for (size_t i = 1; i < bulletTrail.size(); i++) {
        auto& pos1 = bulletTrail[i - 1];
        auto& pos2 = bulletTrail[i];

        float spriteX1 = pos1.x - state.pos.x;
        float spriteY1 = pos1.y - state.pos.y;

        float spriteX2 = pos2.x - state.pos.x;
        float spriteY2 = pos2.y - state.pos.y;

        float transformX1 = invDet * (state.dir.y * spriteX1 - state.dir.x * spriteY1);
        float transformY1 = invDet * (-state.plane.y * spriteX1 + state.plane.x * spriteY1);

        float transformX2 = invDet * (state.dir.y * spriteX2 - state.dir.x * spriteY2);
        float transformY2 = invDet * (-state.plane.y * spriteX2 + state.plane.x * spriteY2);

        if (transformY1 <= 0 || transformY2 <= 0)
            continue;

        int screenX1 = (int)(((float)SCREEN_WIDTH / 2) * (1 + transformX1 / transformY1));
        int screenX2 = (int)(((float)SCREEN_WIDTH / 2) * (1 + transformX2 / transformY2));

        int screenY1 = (int)((float)SCREEN_HEIGHT / 2 + state.pitch);
        int screenY2 = (int)((float)SCREEN_HEIGHT / 2 + state.pitch);

        RGBA baseColor = { 0, 255, 0, 255 };

        RGBA color1 = apply_fog(baseColor, transformY1);
        RGBA color2 = apply_fog(baseColor, transformY2);

        RGBA lineColor;
        lineColor.r = (color1.r + color2.r) / 2;
        lineColor.g = (color1.g + color2.g) / 2;
        lineColor.b = (color1.b + color2.b) / 2;
        lineColor.a = (color1.a + color2.a) / 2;

        draw_line(screenX1, screenY1, screenX2, screenY2, lineColor);
    }
}

void render(float deltaTime)
{
    memset(state.pixels, 0, SCREEN_WIDTH * SCREEN_HEIGHT * sizeof(uint32_t));

    update_dynamic_lights(deltaTime);

    render_other();
    render_walls();
    render_entities();
    render_bullet_trail();
    render_weapon();

    // apply_glitch();
    // apply_dither();
    onerender();
}
