#pragma clang diagnostic push
#pragma ide diagnostic ignored "cert-err58-cpp"

#include <iostream>
#include <chrono>
#include "src/Resources.h"
#include <SDL2/SDL.h>
#include <microgl/Canvas.h>
#include <microgl/vec2.h>
#include <microgl/PixelCoder.h>
#include <microgl/tesselation/FanTriangulation.h>

#define TEST_ITERATIONS 1
#define W 640*1
#define H 480*1

SDL_Window * window;
SDL_Renderer * renderer;
SDL_Texture * texture;

typedef Canvas<uint32_t, RGB888_PACKED_32> Canvas24Bit_Packed32;

Canvas24Bit_Packed32 * canvas;

Resources resources{};

void loop();
void init_sdl(int width, int height);

using namespace tessellation;

template <typename T>
void render_polygon(std::vector<vec2<T>> polygon);

float t = 0;

std::vector<vec2_f> poly_diamond() {
    vec2_f p0 = {100,300};
    vec2_f p1 = {300, 100};
    vec2_f p2 = {400, 300};
    vec2_f p3 = {300, 400};

    return {p0, p1, p2, p3};
}

void render() {
//    t+=.05f;
//    std::cout << t << std::endl;
    render_polygon(poly_diamond());
}


template <typename T>
void render_polygon(std::vector<vec2<T>> polygon) {
    using index = unsigned int;
    using tri = triangles::TrianglesIndices;

//    polygon[1].x = 140 + 20 +  t;
//    polygon[1].y = 140 + 20 -  t;
    canvas->clear(WHITE);

    FanTriangulation fan{true};

    uint8_t precision = 0;
    index size_indices = FanTriangulation::required_indices_size(polygon.size(), tri::TRIANGLES_FAN);
    index size_indices_with_boundary = FanTriangulation::required_indices_size(polygon.size(), tri::TRIANGLES_FAN_WITH_BOUNDARY);

    size_indices = size_indices_with_boundary;
    index indices[size_indices];

    fan.compute(polygon.data(),
            polygon.size(),
            indices,
                size_indices,
                tri::TRIANGLES_FAN_WITH_BOUNDARY
            );

    // draw triangles batch
    canvas->drawTriangles<blendmode::Normal, porterduff::SourceOverOnOpaque, false>(
            RED, polygon.data(),
            indices, size_indices,
            tri::TRIANGLES_FAN_WITH_BOUNDARY,
            120,
            precision);

    // draw triangulation
    canvas->drawTrianglesWireframe(BLACK, polygon.data(),
                                    indices, size_indices,
                                   TrianglesIndices::TRIANGLES_FAN_WITH_BOUNDARY,
                                   255, precision);

}

int main() {
    init_sdl(W, H);
    loop();
}

void init_sdl(int width, int height) {
    SDL_Init(SDL_INIT_VIDEO);

    window = SDL_CreateWindow("SDL2 Pixel Drawing", SDL_WINDOWPOS_UNDEFINED,
            SDL_WINDOWPOS_UNDEFINED, width, height, 0);
    renderer = SDL_CreateRenderer(window, -1, 0);
    texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGB888,
            SDL_TEXTUREACCESS_STATIC, width, height);

    canvas = new Canvas24Bit_Packed32(width, height, new RGB888_PACKED_32());

    resources.init();
}

int render_test(int N) {
    auto ms = std::chrono::milliseconds(1);
    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < N; ++i) {
        render();
    }

    auto end = std::chrono::high_resolution_clock::now();
    return (end-start)/(ms*N);
}

void loop() {
    bool quit = false;
    SDL_Event event;

    // 100 Quads
    int ms = render_test(TEST_ITERATIONS);
    cout << ms << endl;

    while (!quit)
    {
        SDL_PollEvent(&event);

        switch (event.type)
        {
            case SDL_QUIT:
                quit = true;
                break;
            case SDL_KEYUP:
                if( event.key.keysym.sym == SDLK_ESCAPE )
                    quit = true;
                break;
        }
//
        render();

        SDL_UpdateTexture(texture, nullptr, canvas->pixels(),
                canvas->width() * canvas->sizeofPixel());
        SDL_RenderClear(renderer);
        SDL_RenderCopy(renderer, texture, nullptr, nullptr);
        SDL_RenderPresent(renderer);
    }

    SDL_DestroyWindow(window);
    SDL_Quit();
}

#pragma clang diagnostic pop