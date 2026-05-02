# sdl3.hpp

Header-only C++23 RAII wrapper for SDL 3.

## Usage

```cpp
#include <sdl3.hpp>

int main()
{
    sdl::init app(SDL_INIT_VIDEO);

    auto window = sdl::make_window("demo", 800, 600);
    auto renderer = sdl::make_renderer(window);

    sdl::set_render_draw_color(renderer, {20, 24, 28, 255});
    sdl::clear(renderer);
    sdl::present(renderer);
}
```

## Build Tests

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure
```

The test binary uses SDL's dummy video driver and software renderer, so it can
run in a headless environment when the SDL 3 package provides those backends.
