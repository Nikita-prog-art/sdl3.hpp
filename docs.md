# sdl3.hpp Documentation

`sdl3.hpp` - header-only C++23 wrapper для SDL 3. Он не заменяет SDL API целиком:
основная цель - дать безопасное RAII-владение SDL-ресурсами, удобную обработку
ошибок и короткие C++-обертки для часто используемых операций.

Библиотека находится в namespace `sdl` и напрямую использует типы SDL 3:
`SDL_InitFlags`, `SDL_WindowFlags`, `SDL_PixelFormat`, `SDL_Event`,
`SDL_FRect` и остальные SDL-типы можно передавать как обычно.

## Требования

- C++23.
- SDL 3.2 или новее.
- Стандартная библиотека с `<expected>`.

Подключение:

```cpp
#include <sdl3.hpp>
```

При прямой компиляции нужно добавить include path к корню репозитория и
слинковать SDL 3:

```sh
c++ -std=c++23 main.cpp -I. $(pkg-config --cflags --libs sdl3)
```

Через CMake:

```cmake
find_package(SDL3 REQUIRED CONFIG COMPONENTS SDL3)

add_executable(app main.cpp)
target_compile_features(app PRIVATE cxx_std_23)
target_include_directories(app PRIVATE path/to/sdl3.hpp)
target_link_libraries(app PRIVATE SDL3::SDL3)
```

Если используется CMake target из этого репозитория:

```cmake
add_subdirectory(path/to/sdl3.hpp)
target_link_libraries(app PRIVATE sdl3_hpp::sdl3_hpp)
```

## Быстрый Старт

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

Все фабрики ресурсов возвращают move-only RAII-объекты. Когда объект выходит из
области видимости, соответствующий `SDL_Destroy...` или `SDL_CloseIO`
вызывается автоматически.

## Инициализация

`sdl::init` вызывает `SDL_Init(flags)` в конструкторе и `SDL_Quit()` в
деструкторе:

```cpp
sdl::init app(SDL_INIT_VIDEO | SDL_INIT_AUDIO);
```

`sdl::subsystem` управляет отдельными подсистемами через `SDL_InitSubSystem()`
и `SDL_QuitSubSystem()`:

```cpp
sdl::subsystem video(SDL_INIT_VIDEO);
```

Дополнительные helpers:

```cpp
auto flags = sdl::was_init(SDL_INIT_VIDEO);
sdl::set_hint(SDL_HINT_VIDEO_DRIVER, "dummy");
sdl::set_app_metadata("My App", "1.0.0", "com.example.myapp");
sdl::set_app_metadata_property(SDL_PROP_APP_METADATA_TYPE_STRING, "application");
```

Hints, которые влияют на создание video/audio backend, обычно задаются до
`sdl::init`.

## Ошибки

Основной стиль API - исключения. Если SDL возвращает ошибку, wrapper бросает
`sdl::error`, унаследованный от `std::runtime_error`.

```cpp
try {
    sdl::init app(SDL_INIT_VIDEO);
    auto window = sdl::make_window("demo", 800, 600);
} catch (const sdl::error& err) {
    // err.what() содержит имя SDL-вызова и SDL_GetError()
}
```

Низкоуровневые helpers:

```cpp
std::string message = sdl::current_error();
sdl::clear_error();
sdl::check(SDL_ClearError(), "SDL_ClearError");
```

Для кода, где удобнее `std::expected`, есть alias и helpers:

```cpp
sdl::result<void> ok = sdl::try_check(true, "operation");
auto ptr = sdl::try_not_null(SDL_GetWindowFromID(id), "SDL_GetWindowFromID");
```

Большинство готовых wrapper-функций сейчас используют исключения, а не
возвращают `sdl::result<T>`.

## Владение Ресурсами

Типы ресурсов:

| Тип wrapper | SDL resource | Destructor |
| --- | --- | --- |
| `sdl::window` | `SDL_Window*` | `SDL_DestroyWindow` |
| `sdl::renderer` | `SDL_Renderer*` | `SDL_DestroyRenderer` |
| `sdl::texture` | `SDL_Texture*` | `SDL_DestroyTexture` |
| `sdl::surface` | `SDL_Surface*` | `SDL_DestroySurface` |
| `sdl::io_stream` | `SDL_IOStream*` | `SDL_CloseIO` |
| `sdl::cursor` | `SDL_Cursor*` | `SDL_DestroyCursor` |

Все эти типы move-only:

```cpp
auto window = sdl::make_window("demo", 640, 480);
auto moved = std::move(window);

if (!window) {
    // window больше не владеет SDL_Window*
}
```

Общие методы ресурса:

```cpp
resource.get();      // raw pointer без передачи владения
resource.release();  // raw pointer с передачей владения вызывающему коду
resource.reset();    // уничтожить текущий ресурс
```

Важно сохранять обычный порядок SDL lifetime. Например, renderer должен быть
уничтожен до window. В C++ локальные переменные уничтожаются в обратном порядке:

```cpp
auto window = sdl::make_window("demo", 640, 480);
auto renderer = sdl::make_renderer(window);
// renderer уничтожится первым, window - вторым
```

`sdl::make_window_and_renderer()` возвращает `sdl::window_renderer`, где порядок
полей уже подобран так, чтобы renderer уничтожался перед window.

## Window И Renderer

Создание окна:

```cpp
auto window = sdl::make_window("demo", 800, 600, SDL_WINDOW_RESIZABLE);
```

Создание окна и renderer одним SDL-вызовом:

```cpp
auto app = sdl::make_window_and_renderer("demo", 800, 600);
sdl::set_render_draw_color(app.renderer, {0, 0, 0, 255});
sdl::clear(app.renderer);
sdl::present(app.renderer);
```

Работа с window:

```cpp
auto id = sdl::window_id(window);
auto flags = sdl::window_flags(window);
auto title = sdl::window_title(window);
auto size = sdl::window_size(window);

sdl::set_window_title(window, "new title");
sdl::set_window_size(window, 1024, 768);
sdl::show_window(window);
sdl::hide_window(window);
```

Создание renderer:

```cpp
auto renderer = sdl::make_renderer(window);             // SDL выбирает backend
auto opengl = sdl::make_renderer(window, "opengl");     // конкретный backend
auto software = sdl::make_renderer(surface);            // software renderer
```

Renderer helpers:

```cpp
std::string name = sdl::renderer_name(renderer);
sdl::size output = sdl::render_output_size(renderer);
sdl::set_render_vsync(renderer, 1);
```

## Рисование

Wrapper покрывает базовые функции SDL renderer:

```cpp
sdl::set_render_draw_color(renderer, {30, 30, 30, 255});
sdl::clear(renderer);

sdl::set_render_draw_color(renderer, {255, 128, 0, 255});
sdl::draw_point(renderer, 10.0f, 10.0f);
sdl::draw_line(renderer, 20.0f, 20.0f, 120.0f, 40.0f);

sdl::frect rect {40.0f, 50.0f, 200.0f, 120.0f};
sdl::draw_rect(renderer, rect);
sdl::fill_rect(renderer, rect);

sdl::debug_text(renderer, 12.0f, 12.0f, "debug text");
sdl::present(renderer);
```

Для массивов используются `std::span`:

```cpp
std::array<sdl::fpoint, 3> points {{
    {10.0f, 10.0f},
    {20.0f, 30.0f},
    {40.0f, 15.0f},
}};

sdl::draw_lines(renderer, points);
```

Типы `sdl::point`, `sdl::fpoint`, `sdl::rect`, `sdl::frect` являются aliases к
`SDL_Point`, `SDL_FPoint`, `SDL_Rect`, `SDL_FRect`.

## Surface

Создание surface:

```cpp
auto surface = sdl::make_surface(320, 200, SDL_PIXELFORMAT_RGBA32);
```

Доступ к базовым свойствам:

```cpp
sdl::size size = sdl::surface_size(surface);
int pitch = sdl::surface_pitch(surface);
SDL_PixelFormat format = sdl::surface_format(surface);
void* pixels = sdl::surface_pixels(surface);
```

Заполнение и blit:

```cpp
sdl::fill_surface_rect(surface, 0xff00ffffU);

auto target = sdl::make_surface(320, 200, SDL_PIXELFORMAT_RGBA32);
sdl::blit_surface(surface, target);
```

Scoped lock:

```cpp
{
    sdl::surface_lock lock(surface);
    // Работайте с surface pixels.
} // SDL_UnlockSurface вызывается автоматически
```

`sdl::make_surface_from()` создает wrapper над `SDL_CreateSurfaceFrom()`.
Пиксельная память при этом остается владением вызывающего кода, как и в SDL.

## Texture

Создание texture:

```cpp
auto texture = sdl::make_texture(
    renderer,
    SDL_PIXELFORMAT_RGBA32,
    SDL_TEXTUREACCESS_STREAMING,
    256,
    256);
```

Из surface:

```cpp
auto image = sdl::make_surface(64, 64, SDL_PIXELFORMAT_RGBA32);
sdl::fill_surface_rect(image, 0xffffffffU);

auto texture = sdl::make_texture_from_surface(renderer, image);
```

Размер и параметры:

```cpp
sdl::fsize size = sdl::texture_size(texture);
sdl::set_texture_blend_mode(texture, SDL_BLENDMODE_BLEND);
sdl::set_texture_scale_mode(texture, SDL_SCALEMODE_NEAREST);
```

Обновление:

```cpp
sdl::update_texture(texture, pixels, pitch);
```

Scoped lock для streaming textures:

```cpp
{
    sdl::texture_lock lock(texture);
    void* pixels = lock.pixels();
    int pitch = lock.pitch();
    // Запишите пиксели.
} // SDL_UnlockTexture вызывается автоматически
```

Рендер texture:

```cpp
sdl::render_texture(renderer, texture);

sdl::frect src {0.0f, 0.0f, 32.0f, 32.0f};
sdl::frect dst {100.0f, 100.0f, 64.0f, 64.0f};
sdl::render_texture(renderer, texture, src, dst);
```

## IOStream

Фабрики:

```cpp
auto file = sdl::io_from_file("data.bin", "rb");
auto memory = sdl::io_from_memory(buffer.data(), buffer.size());
auto const_memory = sdl::io_from_const_memory(data.data(), data.size());
auto dynamic = sdl::dynamic_io();
```

Чтение и запись:

```cpp
std::array<std::byte, 128> buffer {};
std::size_t read = sdl::read(file, buffer);

std::span<const std::byte> bytes = buffer;
sdl::write_all(dynamic, bytes);
sdl::flush(dynamic);
```

Позиционирование:

```cpp
Sint64 size = sdl::io_size(file);
Sint64 pos = sdl::tell(file);
sdl::seek(file, 0, SDL_IO_SEEK_SET);
```

`sdl::read()` и `sdl::write()` возвращают фактическое количество байт, как SDL.
`sdl::write_all()` бросает `sdl::error`, если записано меньше, чем запрошено.

## Events

```cpp
sdl::init app(SDL_INIT_EVENTS);

while (auto event = sdl::poll_event()) {
    if (event->type == SDL_EVENT_QUIT) {
        break;
    }
}
```

Ожидание:

```cpp
SDL_Event event = sdl::wait_event();

auto maybe_event = sdl::wait_event_for(std::chrono::milliseconds(100));
if (maybe_event) {
    // event received
}
```

Push:

```cpp
SDL_Event event {};
event.type = SDL_EVENT_USER;
event.user.code = 42;
sdl::push_event(event);
```

## Properties

`sdl::properties` владеет `SDL_PropertiesID`, созданным через
`SDL_CreateProperties()`.

```cpp
auto props = sdl::make_properties();

props.set_string("name", "renderer");
props.set_number("width", 800);
props.set_float("scale", 2.0f);
props.set_boolean("enabled", true);

std::string name = props.string("name");
Sint64 width = props.number("width");
bool enabled = props.boolean("enabled");
```

Pointer property:

```cpp
int value = 10;
props.set_pointer("value", &value);
auto* raw = static_cast<int*>(props.pointer("value"));
```

Для renderer properties можно использовать raw SDL property names:

```cpp
auto props = sdl::make_properties();
props.set_pointer(SDL_PROP_RENDERER_CREATE_WINDOW_POINTER, window.get());
props.set_string(SDL_PROP_RENDERER_CREATE_NAME_STRING, "software");

auto renderer = sdl::make_renderer(props);
```

## Filesystem Helpers

```cpp
std::string base = sdl::base_path();

auto pref = sdl::pref_path("ExampleOrg", "ExampleApp");
std::string path = pref.get();
```

`sdl::pref_path()` возвращает `sdl::allocated_string`, то есть `std::unique_ptr`
с deleter на `SDL_free`.

## Version Helpers

```cpp
sdl::version compiled = sdl::compiled_version();
sdl::version linked = sdl::linked_version();

std::string text = sdl::to_string(linked);
```

`compiled_version()` показывает версию headers, с которыми компилируется код.
`linked_version()` показывает версию SDL library, с которой программа реально
запущена.

## Headless Тестирование

Для CI или headless окружения часто удобно использовать dummy video driver и
software renderer:

```cpp
sdl::set_hint(SDL_HINT_VIDEO_DRIVER, "dummy");
sdl::set_hint(SDL_HINT_RENDER_DRIVER, "software");

sdl::init app(SDL_INIT_VIDEO);
auto pair = sdl::make_window_and_renderer("test", 64, 48, SDL_WINDOW_HIDDEN);
```

Тесты в репозитории используют именно этот подход.

## Запуск Тестов

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure
```

Тестовый бинарник проверяет:

- move-only RAII-типы;
- error helpers;
- properties;
- `SDL_IOStream`;
- `SDL_Surface`;
- event queue;
- hidden window и renderer через dummy/software backend.

## Ограничения

- Это не полная C++-обвязка всего SDL 3 API.
- Wrapper не скрывает SDL-типы и SDL-константы. Это намеренно: можно смешивать
  `sdl3.hpp` и прямые SDL-вызовы.
- Готовые функции в основном бросают `sdl::error`; `sdl::result<T>` пока
  предоставлен как низкоуровневая опция для пользовательских wrapper-слоев.
- Потоковая безопасность остается такой же, как у соответствующих SDL-вызовов.
  Если SDL требует main thread, wrapper не меняет это требование.

