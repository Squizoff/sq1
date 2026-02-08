#include "pch.h"

static int load_map(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "err loading map file " << filename << std::endl;
        return 0;
    }

    std::vector<std::string> lines;
    std::string line;

    while (std::getline(file, line)) {
        lines.push_back(line);
    }

    int mapSize = lines.size();
    delete[] MAPDATA;
    MAP_SIZE = mapSize;
    MAPDATA = new uint8_t[MAP_SIZE * MAP_SIZE];

    int spawnFound = 0;

    for (int y = 0; y < mapSize; y++) {
        for (int x = 0; x < mapSize; x++) {
            if (lines[y][x] == '1') {
                MAPDATA[y * MAP_SIZE + x] = 1;
            }
            else if (lines[y][x] == '2') {
                MAPDATA[y * MAP_SIZE + x] = 2;
            }
            else if (lines[y][x] == '3') {
                MAPDATA[y * MAP_SIZE + x] = 3;
                if (!spawnFound) {
                    state.pos = { static_cast<float>(x), static_cast<float>(y), 0 };
                    spawnFound = 1;
                }
            }
            else {
                MAPDATA[y * MAP_SIZE + x] = 0;
            }
        }
    }

    return 1;
}

int main() {
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        std::cerr << "SDL_Init failed: " << SDL_GetError() << std::endl;
        return 1;
    }

    state.window = SDL_CreateWindow("sq1",
                                    SDL_WINDOWPOS_CENTERED,
                                    SDL_WINDOWPOS_CENTERED,
                                    1280, 720,
                                    SDL_WINDOW_ALLOW_HIGHDPI);
    if (!state.window) {
        std::cerr << "Failed to create window: " << SDL_GetError() << std::endl;
        SDL_Quit();
        return 1;
    }

    int renderMethod = (USE_GPU == 1) ? SDL_RENDERER_ACCELERATED : SDL_RENDERER_SOFTWARE;
    state.renderer = SDL_CreateRenderer(state.window, -1, renderMethod);
    if (!state.renderer) {
        std::cerr << "Failed to create renderer: " << SDL_GetError() << std::endl;
        SDL_DestroyWindow(state.window);
        SDL_Quit();
        return 1;
    }
    
    state.texture = SDL_CreateTexture(state.renderer,
                                     SDL_PIXELFORMAT_ABGR8888,
                                     SDL_TEXTUREACCESS_STREAMING,
                                     SCREEN_WIDTH,
                                     SCREEN_HEIGHT);
    if (!state.texture) {
        std::cerr << "Failed to create texture: " << SDL_GetError() << std::endl;
        SDL_DestroyRenderer(state.renderer);
        SDL_DestroyWindow(state.window);
        SDL_Quit();
        return 1;
    }

    state.pos = { 0.0f, 0.0f, 0 };
    state.dir = { -1.0f, 0.0f, 0 };
    state.plane = { 0.0f, 0.66f, 0 };

    if (!load_textures()) return 1;
    if (!load_map("map.txt")) return 1;

    SDL_SetRelativeMouseMode(SDL_TRUE);

    float lightX = 8.0f;
    float lightDir = 1.0f;
    float lightSpeed = 1.5f;

    Uint32 frameStart;
    Uint32 lastTime = SDL_GetTicks();
    Uint32 lastFpsUpdate = lastTime;
    int frameCount = 0;
    float fps = 0.0f;

    int quit = 0;
    while (!quit) {
        frameStart = SDL_GetTicks();
        state.deltaTime = (frameStart - lastTime) / 1000.0f;
        lastTime = frameStart;

        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_QUIT) quit = 1;
        }

        const uint8_t* keystate = SDL_GetKeyboardState(NULL);
        update_player(keystate);

        lightX += lightDir * lightSpeed * state.deltaTime;
        if (lightX > 12.0f) lightDir = -1.0f;
        if (lightX < 4.0f) lightDir = 1.0f;

        dynamicLights.clear();
        add_dynamic_light(lightX, 8, 1.5f, { 255, 255, 255 }, 3.0f, CONSTANT);

        memset(state.pixels, 0, sizeof(state.pixels));
        render(state.deltaTime);

        frameCount++;
        if (frameStart - lastFpsUpdate >= 300) {
            fps = frameCount * (1000.0f / (frameStart - lastFpsUpdate));
            lastFpsUpdate = frameStart;
            frameCount = 0;

            std::stringstream title;
            title << "sq1 - FPS: " << static_cast<int>(fps);
            SDL_SetWindowTitle(state.window, title.str().c_str());
        }
    }

    for (int i = 0; i < 1; i++) {
        if (state.textures[i]) {
            SDL_FreeSurface(state.textures[i]);
        }
    }

    SDL_DestroyTexture(state.texture);
    SDL_DestroyRenderer(state.renderer);
    SDL_DestroyWindow(state.window);

    SDL_Quit();
    return 0;
}
