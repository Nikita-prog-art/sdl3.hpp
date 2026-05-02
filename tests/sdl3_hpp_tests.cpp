#include "sdl3.hpp"

#include <SDL3/SDL.h>

#include <array>
#include <cstddef>
#include <exception>
#include <iostream>
#include <type_traits>

namespace {

int failures = 0;

#define REQUIRE(condition)                                                                                             \
    do {                                                                                                               \
        if (!(condition)) {                                                                                            \
            std::cerr << __FILE__ << ':' << __LINE__ << ": requirement failed: " #condition << '\n';                   \
            ++failures;                                                                                                \
        }                                                                                                              \
    } while (false)

void test_version_and_error_helpers()
{
    static_assert(std::is_move_constructible_v<sdl::window>);
    static_assert(!std::is_copy_constructible_v<sdl::window>);
    static_assert(std::is_move_assignable_v<sdl::renderer>);
    static_assert(!std::is_copy_assignable_v<sdl::renderer>);

    const sdl::version compiled = sdl::compiled_version();
    const sdl::version linked = sdl::linked_version();
    REQUIRE(compiled.major == 3);
    REQUIRE(linked.major == 3);
    REQUIRE(sdl::to_string(compiled).starts_with("3."));

    sdl::clear_error();
    REQUIRE(sdl::current_error().empty());
    REQUIRE(sdl::try_check(true, "ok").has_value());
    REQUIRE(!sdl::try_check(false, "expected failure").has_value());

    REQUIRE(sdl::ticks().count() >= 0);
    REQUIRE(sdl::ticks_ns().count() >= 0);
    sdl::delay(std::chrono::milliseconds(0));
}

void test_properties()
{
    auto props = sdl::make_properties();
    int pointed_value = 42;

    props.set_pointer("pointer", &pointed_value);
    props.set_string("string", "hello");
    props.set_number("number", 123);
    props.set_float("float", 1.5f);
    props.set_boolean("boolean", true);

    REQUIRE(props.pointer("pointer") == &pointed_value);
    REQUIRE(props.string("string") == "hello");
    REQUIRE(props.number("number") == 123);
    REQUIRE(props.floating("float") == 1.5f);
    REQUIRE(props.boolean("boolean"));
    REQUIRE(props.type("number") == SDL_PROPERTY_TYPE_NUMBER);

    auto moved = std::move(props);
    REQUIRE(!props);
    REQUIRE(moved.number("number") == 123);
}

void test_io()
{
    std::array<std::byte, 4> source {
        std::byte {'S'},
        std::byte {'D'},
        std::byte {'L'},
        std::byte {'3'},
    };
    auto stream = sdl::io_from_const_memory(source.data(), source.size());

    REQUIRE(sdl::io_size(stream) == 4);
    REQUIRE(sdl::tell(stream) == 0);

    std::array<std::byte, 4> destination {};
    REQUIRE(sdl::read(stream, destination) == destination.size());
    REQUIRE(destination == source);
    REQUIRE(sdl::tell(stream) == 4);
    REQUIRE(sdl::seek(stream, 0, SDL_IO_SEEK_SET) == 0);
}

void test_surface()
{
    auto surface = sdl::make_surface(8, 4, SDL_PIXELFORMAT_RGBA32);
    REQUIRE(surface);
    REQUIRE((sdl::surface_size(surface) == sdl::size {8, 4}));
    REQUIRE(sdl::surface_pitch(surface) >= 8 * 4);
    REQUIRE(sdl::surface_pixels(surface) != nullptr);

    sdl::fill_surface_rect(surface, 0xff00ffffU);

    auto other = sdl::make_surface(8, 4, SDL_PIXELFORMAT_RGBA32);
    sdl::blit_surface(surface, other);
}

void test_events()
{
    sdl::init app(SDL_INIT_EVENTS);
    REQUIRE((sdl::was_init(SDL_INIT_EVENTS) & SDL_INIT_EVENTS) == SDL_INIT_EVENTS);

    SDL_Event event {};
    event.type = SDL_EVENT_USER;
    event.user.code = 7;
    sdl::push_event(event);

    auto polled = sdl::poll_event();
    REQUIRE(polled.has_value());
    REQUIRE(polled->type == SDL_EVENT_USER);
    REQUIRE(polled->user.code == 7);
}

void test_video_and_renderer()
{
    sdl::set_hint(SDL_HINT_VIDEO_DRIVER, "dummy");
    sdl::set_hint(SDL_HINT_RENDER_DRIVER, "software");

    sdl::init app(SDL_INIT_VIDEO);
    auto pair = sdl::make_window_and_renderer("sdl3.hpp tests", 64, 48, SDL_WINDOW_HIDDEN);

    REQUIRE(pair.window);
    REQUIRE(pair.renderer);
    REQUIRE(sdl::window_title(pair.window) == "sdl3.hpp tests");
    REQUIRE((sdl::window_size(pair.window) == sdl::size {64, 48}));
    REQUIRE(!sdl::renderer_name(pair.renderer).empty());

    sdl::set_render_draw_color(pair.renderer, sdl::color {10, 20, 30, 255});
    sdl::clear(pair.renderer);

    const sdl::frect rect {4.0f, 4.0f, 16.0f, 12.0f};
    sdl::set_render_draw_color(pair.renderer, sdl::color {255, 128, 0, 255});
    sdl::fill_rect(pair.renderer, rect);

    auto pixels = sdl::make_surface(16, 16, SDL_PIXELFORMAT_RGBA32);
    sdl::fill_surface_rect(pixels, 0xffffffffU);

    auto texture = sdl::make_texture_from_surface(pair.renderer, pixels);
    REQUIRE((sdl::texture_size(texture) == sdl::fsize {16.0f, 16.0f}));
    sdl::render_texture(pair.renderer, texture);
    sdl::present(pair.renderer);
}

} // namespace

int main()
{
    try {
        test_version_and_error_helpers();
        test_properties();
        test_io();
        test_surface();
        test_events();
        test_video_and_renderer();
    } catch (const sdl::error& err) {
        std::cerr << "sdl::error: " << err.what() << '\n';
        return 1;
    } catch (const std::exception& err) {
        std::cerr << "std::exception: " << err.what() << '\n';
        return 1;
    }

    if (failures != 0) {
        std::cerr << failures << " test requirement(s) failed\n";
        return 1;
    }

    return 0;
}
