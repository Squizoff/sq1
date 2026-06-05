#include "pch.h"

RGBA skyColor = { 255, 255, 255, 255 };
#define FOG_DENSITY 0.2f

struct Sprite {
    float distSq;
    float spriteX;
    float spriteY;
    int mapX, mapY;
    float floorZ;
};

struct WallHit {
    float dist;
    int mapX;
    int mapY;
    int side;
    float rayDirX;
    float rayDirY;
};

static std::vector<float> depthBuffer;
static std::vector<Sprite> spriteBuffer;
static std::vector<float> zbuffer;

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
#if 0
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

static void render_entities(const std::vector<float>& depthBuffer)
{
    spriteBuffer.clear();

    for (int i = 0; i < MAP_SIZE * MAP_SIZE; i++) {
        if (MAPDATA[i].type == 2) {
            int mapX = i % MAP_SIZE;
            int mapY = i / MAP_SIZE;

            const Cell& cell = MAPDATA[i];

            float spriteX = mapX + 0.5f - state.pos.x;
            float spriteY = mapY + 0.5f - state.pos.y;
            float distSq = spriteX * spriteX + spriteY * spriteY;

            spriteBuffer.push_back({ distSq, spriteX, spriteY, mapX, mapY, cell.floorZ });
        }
    }

    std::sort(spriteBuffer.begin(), spriteBuffer.end(), [](const Sprite& a, const Sprite& b) {
        return a.distSq > b.distSq;
        });

    for (const auto& spr : spriteBuffer) {
        float spriteX = spr.spriteX;
        float spriteY = spr.spriteY;

        float invDet = 1.0f / (state.plane.x * state.dir.y - state.dir.x * state.plane.y);
        float transformX = invDet * (state.dir.y * spriteX - state.dir.x * spriteY);
        float transformY = invDet * (-state.plane.y * spriteX + state.plane.x * spriteY);

        if (transformY <= 0.0f)
            continue;

        int spriteScreenX = (int)((SCREEN_WIDTH / 2.0f) * (1.0f + transformX / transformY));

        const float eyeZ = state.pos.z + 0.5f;
        const float entityHeight = 1.0f;

        float bottomScreenF = (SCREEN_HEIGHT / 2.0f)
            - ((spr.floorZ - eyeZ) / transformY) * SCREEN_HEIGHT
            + state.pitch;

        float topScreenF = (SCREEN_HEIGHT / 2.0f)
            - (((spr.floorZ + entityHeight) - eyeZ) / transformY) * SCREEN_HEIGHT
            + state.pitch;

        int drawStartY = (int)topScreenF;
        int drawEndY = (int)bottomScreenF;

        int spriteHeight = drawEndY - drawStartY;
        if (spriteHeight <= 0)
            continue;

        int spriteWidth = spriteHeight;

        drawStartY = std::max(drawStartY, 0);
        drawEndY = std::min(drawEndY, SCREEN_HEIGHT - 1);

        int drawStartX = -spriteWidth / 2 + spriteScreenX;
        int drawEndX = spriteWidth / 2 + spriteScreenX;

        drawStartX = std::max(drawStartX, 0);
        drawEndX = std::min(drawEndX, SCREEN_WIDTH - 1);

        int texWidth = state.tex_width[2];
        int texHeight = state.tex_height[2];

        for (int x = drawStartX; x < drawEndX; x++) {
            if (x < 0 || x >= SCREEN_WIDTH)
                continue;

            int texX = (int)((x - (-spriteWidth / 2.0f + spriteScreenX)) * texWidth / spriteWidth);
            texX = std::clamp(texX, 0, texWidth - 1);

            for (int y = drawStartY; y < drawEndY; y++) {
                int idx = y * SCREEN_WIDTH + x;

                if (transformY > depthBuffer[idx] * 1.05f)
                    continue;

                float t = (y - topScreenF) / (bottomScreenF - topScreenF);
                int texY = std::clamp((int)(t * texHeight), 0, texHeight - 1);

                RGBA color = get_texture_pixel(2, texX, texY);
                if (color.a == 0)
                    continue;

                color = apply_fog(color, transformY);
                color = apply_tonemap(color);

                uint32_t bgColor = state.pixels[idx];
                uint8_t bgR = (bgColor >> 16) & 0xFF;
                uint8_t bgG = (bgColor >> 8) & 0xFF;
                uint8_t bgB = bgColor & 0xFF;

                float alpha = color.a / 255.0f;
                uint8_t outR = (uint8_t)(color.r * alpha + bgR * (1.0f - alpha));
                uint8_t outG = (uint8_t)(color.g * alpha + bgG * (1.0f - alpha));
                uint8_t outB = (uint8_t)(color.b * alpha + bgB * (1.0f - alpha));

                state.pixels[idx] = (outB << 16) | (outG << 8) | outR;
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

static void render_floor(int horizon, std::vector<float>& depthBuffer)
{
    const int texWidth = state.tex_width[1];
    const int texHeight = state.tex_height[1];
    const float eyeZ = state.pos.z + 0.5f;

    for (int y = horizon; y < SCREEN_HEIGHT; ++y) {
        int p = y - horizon;
        if (p <= 0) p = 1;

        for (int x = 0; x < SCREEN_WIDTH; ++x) {
            float cameraX = 2.0f * x / (float)SCREEN_WIDTH - 1.0f;
            float rayDirX = state.dir.x + state.plane.x * cameraX;
            float rayDirY = state.dir.y + state.plane.y * cameraX;

            float floorZ = state.pos.z;

            float worldX = state.pos.x;
            float worldY = state.pos.y;
            int cellX = -1, cellY = -1;

            for (int iter = 0; iter < 2; ++iter) {
                float dist = (eyeZ - floorZ) * SCREEN_HEIGHT / (float)p;
                if (dist <= 0.01f)
                    break;

                worldX = state.pos.x + rayDirX * dist;
                worldY = state.pos.y + rayDirY * dist;

                cellX = (int)floorf(worldX);
                cellY = (int)floorf(worldY);

                if (cellX < 0 || cellY < 0 || cellX >= MAP_SIZE || cellY >= MAP_SIZE)
                    break;

                floorZ = MAPDATA[cellY * MAP_SIZE + cellX].floorZ;
            }

            if (cellX < 0 || cellY < 0 || cellX >= MAP_SIZE || cellY >= MAP_SIZE)
                continue;

            const Cell& cell = MAPDATA[cellY * MAP_SIZE + cellX];
            if (eyeZ <= cell.floorZ + 0.01f)
                continue;

            float dist = (eyeZ - cell.floorZ) * SCREEN_HEIGHT / (float)p;
            if (dist <= 0.01f)
                continue;

            int idx = y * SCREEN_WIDTH + x;
            depthBuffer[idx] = dist;

            worldX = state.pos.x + rayDirX * dist;
            worldY = state.pos.y + rayDirY * dist;

            float fracX = worldX - floorf(worldX);
            float fracY = worldY - floorf(worldY);

            if (fracX < 0.0f) fracX += 1.0f;
            if (fracY < 0.0f) fracY += 1.0f;

            int texX = (int)(fracX * texWidth);
            int texY = (int)(fracY * texHeight);

            texX = std::clamp(texX, 0, texWidth - 1);
            texY = std::clamp(texY, 0, texHeight - 1);

            RGBA color = get_texture_pixel(1, texX, texY);

            float heightDelta = cell.floorZ - state.pos.z;
            float shade = clamp(1.0f - fabsf(heightDelta) * 0.12f, 0.55f, 1.0f);
            color.r = (uint8_t)(color.r * shade);
            color.g = (uint8_t)(color.g * shade);
            color.b = (uint8_t)(color.b * shade);

            color = apply_tonemap(color);
            color = apply_fog(color, dist);
            color = apply_dynamic_lights(color, worldX, worldY);

            state.pixels[idx] = (color.b << 16) | (color.g << 8) | color.r;
        }
    }
}

static void render_other(std::vector<float>& depthBuffer)
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
    render_floor(horizon, depthBuffer);
}

static void render_walls(std::vector<float>& depthBuffer)
{
    zbuffer.resize(SCREEN_WIDTH);

    for (int x = 0; x < SCREEN_WIDTH; ++x) {
        float cameraX = 2.0f * x / SCREEN_WIDTH - 1.0f;

        float rayDirX = state.dir.x + (state.plane.x * cameraX);
        float rayDirY = state.dir.y + (state.plane.y * cameraX);

        int mapX = (int)state.pos.x;
        int mapY = (int)state.pos.y;

        float deltaDistX = (rayDirX == 0.0f) ? 1e30f : fabsf(1.0f / rayDirX);
        float deltaDistY = (rayDirY == 0.0f) ? 1e30f : fabsf(1.0f / rayDirY);

        int stepX = (rayDirX < 0.0f) ? -1 : 1;
        int stepY = (rayDirY < 0.0f) ? -1 : 1;

        float sideDistX = (rayDirX < 0.0f)
            ? (state.pos.x - mapX) * deltaDistX
            : (mapX + 1.0f - state.pos.x) * deltaDistX;

        float sideDistY = (rayDirY < 0.0f)
            ? (state.pos.y - mapY) * deltaDistY
            : (mapY + 1.0f - state.pos.y) * deltaDistY;

        std::vector<WallHit> columnHits;
        columnHits.reserve(6);

        while (true) {
            int side = 0;

            if (sideDistX < sideDistY) {
                sideDistX += deltaDistX;
                mapX += stepX;
                side = 0;
            }
            else {
                sideDistY += deltaDistY;
                mapY += stepY;
                side = 1;
            }

            if (mapX < 0 || mapX >= (int)MAP_SIZE || mapY < 0 || mapY >= (int)MAP_SIZE)
                break;

            const Cell& cell = MAPDATA[mapY * MAP_SIZE + mapX];
            if (cell.type && cell.type != 2 && cell.type != 3)
            {
                float dist = (side == 0) ? (sideDistX - deltaDistX) : (sideDistY - deltaDistY);
                if (dist > 0.01f)
                    columnHits.push_back({ dist, mapX, mapY, side, rayDirX, rayDirY });
            }

            if (sideDistX > 64.0f && sideDistY > 64.0f)
                break;
        }

        std::sort(columnHits.begin(), columnHits.end(),
            [](const WallHit& a, const WallHit& b) { return a.dist > b.dist; });

        for (const WallHit& h : columnHits) {
            const Cell& wallCell = MAPDATA[h.mapY * MAP_SIZE + h.mapX];

            float perpWallDist = h.dist;
            if (perpWallDist <= 0.01f)
                perpWallDist = 0.01f;

            if (perpWallDist < zbuffer[x])
                zbuffer[x] = perpWallDist;

            int texId = wallCell.type - 1;
            if (texId < 0)
                continue;

            int texW = state.tex_width[texId];
            int texH = state.tex_height[texId];

            float eyeZ = state.pos.z + 0.5f;
            float ceilDist = wallCell.ceilZ - eyeZ;
            float floorDist = -eyeZ;

            int ceilScreen = (int)((SCREEN_HEIGHT / 2.0f) - (ceilDist / perpWallDist) * SCREEN_HEIGHT);
            int floorScreen = (int)((SCREEN_HEIGHT / 2.0f) - (floorDist / perpWallDist) * SCREEN_HEIGHT);

            int rawStart = ceilScreen + (int)state.pitch;
            int rawEnd = floorScreen + (int)state.pitch - 1;

            int drawStart = std::max(rawStart, 0);
            int drawEnd = std::min(rawEnd, SCREEN_HEIGHT - 1);

            if (drawStart > drawEnd)
                continue;

            float wallScreenHeight = (float)(rawEnd - rawStart);
            if (wallScreenHeight <= 0.0f)
                continue;

            float wallHit = (h.side == 0)
                ? state.pos.y + perpWallDist * h.rayDirY
                : state.pos.x + perpWallDist * h.rayDirX;
            wallHit -= floorf(wallHit);

            int texX = (int)(wallHit * texW);
            if ((h.side == 0 && h.rayDirX > 0.0f) || (h.side == 1 && h.rayDirY < 0.0f))
                texX = texW - texX - 1;

            texX = std::clamp(texX, 0, texW - 1);

            for (int y = drawStart; y <= drawEnd; ++y) {
                int idx = y * SCREEN_WIDTH + x;

                if (perpWallDist > depthBuffer[idx])
                    continue;

                depthBuffer[idx] = perpWallDist;

                float wallWorldHeight = std::max(0.001f, wallCell.ceilZ);

                float t = (y - rawStart) / wallScreenHeight;

                float texCoord = t * wallWorldHeight;

                int texY = (int)(fmodf(texCoord, 1.0f) * texH);

                if (texY < 0)
                    texY += texH;

                RGBA color = get_texture_pixel(texId, texX, texY);
                color = apply_tonemap(color);
                color = apply_fog(color, perpWallDist);
                color = apply_dynamic_lights(color,
                    state.pos.x + h.rayDirX * perpWallDist,
                    state.pos.y + h.rayDirY * perpWallDist);

                if (h.side == 1) {
                    color.r >>= 1;
                    color.g >>= 1;
                    color.b >>= 1;
                }

                state.pixels[idx] = (color.b << 16) | (color.g << 8) | color.r;
            }
        }
    }

    render_entities(depthBuffer);
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
    const float eyeZ = state.pos.z + 0.5f;

    for (size_t i = 1; i < bulletTrail.size(); i++) {
        const auto& p1 = bulletTrail[i - 1];
        const auto& p2 = bulletTrail[i];

        auto project = [&](const v3& p, float& screenX, float& screenY, float& depth)
            {
                float spriteX = p.x - state.pos.x;
                float spriteY = p.y - state.pos.y;

                float transformX = invDet * (state.dir.y * spriteX - state.dir.x * spriteY);
                float transformY = invDet * (-state.plane.y * spriteX + state.plane.x * spriteY);

                if (transformY <= 0.01f) {
                    screenX = screenY = depth = -1;
                    return;
                }

                screenX = (SCREEN_WIDTH / 2.0f) * (1.0f + transformX / transformY);

                screenY = (SCREEN_HEIGHT / 2.0f)
                    - ((p.z - eyeZ) / transformY) * SCREEN_HEIGHT
                    + state.pitch;

                depth = transformY;
            };

        float x1, y1, d1, x2, y2, d2;
        project(p1, x1, y1, d1);
        project(p2, x2, y2, d2);

        if (d1 < 0 || d2 < 0)
            continue;

        RGBA baseColor = { 0, 255, 0, 255 };

        RGBA c1 = apply_fog(baseColor, d1);
        RGBA c2 = apply_fog(baseColor, d2);

        RGBA lineColor = {
            (uint8_t)((c1.r + c2.r) * 0.5f),
            (uint8_t)((c1.g + c2.g) * 0.5f),
            (uint8_t)((c1.b + c2.b) * 0.5f),
            255
        };

        draw_line((int)x1, (int)y1, (int)x2, (int)y2, lineColor);
    }
}

void render_loop(float deltaTime)
{
    // memset(state.pixels, 0, SCREEN_WIDTH * SCREEN_HEIGHT * sizeof(uint32_t));

    std::fill(depthBuffer.begin(), depthBuffer.end(), FLT_MAX);

    update_dynamic_lights(deltaTime);

    render_other(depthBuffer);
    render_walls(depthBuffer);
    render_bullet_trail();
    render_weapon();

    // apply_glitch();
    // apply_dither();
    onerender();
}

void render_init() {
    std::fill(zbuffer.begin(), zbuffer.end(), FLT_MAX);
    depthBuffer.resize(SCREEN_WIDTH * SCREEN_HEIGHT);
    spriteBuffer.reserve(256);
}