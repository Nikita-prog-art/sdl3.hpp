#pragma once

#if defined(_MSVC_LANG)
#define SDL3_HPP_CPLUSPLUS _MSVC_LANG
#else
#define SDL3_HPP_CPLUSPLUS __cplusplus
#endif

#if SDL3_HPP_CPLUSPLUS < 202302L
#error "sdl3.hpp requires C++23 or newer"
#endif

#include <SDL3/SDL.h>

#include <chrono>
#include <compare>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

#ifndef __cpp_lib_expected
#error "sdl3.hpp requires a C++23 standard library with <expected>"
#endif

namespace sdl {

template <class T>
using result = std::expected<T, std::string>;

class error : public std::runtime_error {
public:
    explicit error(std::string message)
        : std::runtime_error(std::move(message))
    {
    }
};

[[nodiscard]] inline std::string current_error()
{
    const char* message = SDL_GetError();
    return message != nullptr ? std::string(message) : std::string();
}

inline void clear_error()
{
    SDL_ClearError();
}

[[nodiscard]] inline std::string make_error_message(std::string_view action)
{
    std::string detail = current_error();
    if (action.empty()) {
        return detail.empty() ? std::string("SDL error") : detail;
    }

    std::string message(action);
    if (!detail.empty()) {
        message += ": ";
        message += detail;
    }
    return message;
}

[[noreturn]] inline void throw_error(std::string_view action)
{
    throw error(make_error_message(action));
}

inline void check(bool ok, std::string_view action)
{
    if (!ok) {
        throw_error(action);
    }
}

[[nodiscard]] inline result<void> try_check(bool ok, std::string_view action)
{
    if (ok) {
        return {};
    }
    return std::unexpected(make_error_message(action));
}

template <class T>
[[nodiscard]] inline T* check_not_null(T* ptr, std::string_view action)
{
    if (ptr == nullptr) {
        throw_error(action);
    }
    return ptr;
}

template <class T>
[[nodiscard]] inline result<T*> try_not_null(T* ptr, std::string_view action)
{
    if (ptr != nullptr) {
        return ptr;
    }
    return std::unexpected(make_error_message(action));
}

[[nodiscard]] inline SDL_PropertiesID check_property_id(SDL_PropertiesID id, std::string_view action)
{
    if (id == 0) {
        throw_error(action);
    }
    return id;
}

namespace detail {

[[nodiscard]] inline int checked_count(std::size_t count, std::string_view action)
{
    if (count > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        throw error(std::string(action) + ": count does not fit in int");
    }
    return static_cast<int>(count);
}

} // namespace detail

template <class T, auto Destroy>
    requires requires(T* ptr) { Destroy(ptr); }
class resource {
public:
    using element_type = T;

    constexpr resource() noexcept = default;

    explicit constexpr resource(T* ptr) noexcept
        : ptr_(ptr)
    {
    }

    resource(const resource&) = delete;
    resource& operator=(const resource&) = delete;

    constexpr resource(resource&& other) noexcept
        : ptr_(std::exchange(other.ptr_, nullptr))
    {
    }

    constexpr resource& operator=(resource&& other) noexcept
    {
        if (this != &other) {
            reset(std::exchange(other.ptr_, nullptr));
        }
        return *this;
    }

    ~resource()
    {
        reset();
    }

    [[nodiscard]] constexpr T* get() const noexcept
    {
        return ptr_;
    }

    [[nodiscard]] constexpr explicit operator bool() const noexcept
    {
        return ptr_ != nullptr;
    }

    [[nodiscard]] constexpr T& operator*() const noexcept
    {
        return *ptr_;
    }

    [[nodiscard]] constexpr T* operator->() const noexcept
    {
        return ptr_;
    }

    [[nodiscard]] constexpr T* release() noexcept
    {
        return std::exchange(ptr_, nullptr);
    }

    constexpr void reset(T* ptr = nullptr) noexcept
    {
        if (ptr_ != nullptr) {
            static_cast<void>(Destroy(ptr_));
        }
        ptr_ = ptr;
    }

    friend constexpr bool operator==(const resource&, const resource&) noexcept = default;

private:
    T* ptr_ = nullptr;
};

using window = resource<SDL_Window, SDL_DestroyWindow>;
using renderer = resource<SDL_Renderer, SDL_DestroyRenderer>;
using texture = resource<SDL_Texture, SDL_DestroyTexture>;
using surface = resource<SDL_Surface, SDL_DestroySurface>;
using io_stream = resource<SDL_IOStream, SDL_CloseIO>;
using cursor = resource<SDL_Cursor, SDL_DestroyCursor>;

template <class T>
struct sdl_free_deleter {
    void operator()(T* ptr) const noexcept
    {
        SDL_free(ptr);
    }
};

template <class T>
using allocated_ptr = std::unique_ptr<T, sdl_free_deleter<T>>;

using allocated_string = allocated_ptr<char>;

struct version {
    int major = 0;
    int minor = 0;
    int patch = 0;

    friend constexpr auto operator<=>(const version&, const version&) noexcept = default;
};

[[nodiscard]] constexpr version compiled_version() noexcept
{
    return {SDL_MAJOR_VERSION, SDL_MINOR_VERSION, SDL_MICRO_VERSION};
}

[[nodiscard]] inline version linked_version() noexcept
{
    const int value = SDL_GetVersion();
    return {
        SDL_VERSIONNUM_MAJOR(value),
        SDL_VERSIONNUM_MINOR(value),
        SDL_VERSIONNUM_MICRO(value),
    };
}

[[nodiscard]] inline std::string to_string(version value)
{
    return std::to_string(value.major) + "." + std::to_string(value.minor) + "." + std::to_string(value.patch);
}

struct size {
    int width = 0;
    int height = 0;

    friend constexpr auto operator<=>(const size&, const size&) noexcept = default;
};

struct fsize {
    float width = 0.0f;
    float height = 0.0f;

    friend constexpr auto operator<=>(const fsize&, const fsize&) noexcept = default;
};

struct color {
    Uint8 r = 0;
    Uint8 g = 0;
    Uint8 b = 0;
    Uint8 a = SDL_ALPHA_OPAQUE;

    [[nodiscard]] constexpr SDL_Color sdl() const noexcept
    {
        return SDL_Color{r, g, b, a};
    }
};

using point = SDL_Point;
using fpoint = SDL_FPoint;
using rect = SDL_Rect;
using frect = SDL_FRect;

class init {
public:
    explicit init(SDL_InitFlags flags)
        : flags_(flags)
    {
        check(SDL_Init(flags), "SDL_Init");
        active_ = true;
    }

    init(const init&) = delete;
    init& operator=(const init&) = delete;

    init(init&& other) noexcept
        : flags_(std::exchange(other.flags_, 0))
        , active_(std::exchange(other.active_, false))
    {
    }

    init& operator=(init&& other) noexcept
    {
        if (this != &other) {
            reset();
            flags_ = std::exchange(other.flags_, 0);
            active_ = std::exchange(other.active_, false);
        }
        return *this;
    }

    ~init()
    {
        reset();
    }

    [[nodiscard]] SDL_InitFlags flags() const noexcept
    {
        return flags_;
    }

    [[nodiscard]] bool active() const noexcept
    {
        return active_;
    }

    void reset() noexcept
    {
        if (active_) {
            SDL_Quit();
            active_ = false;
            flags_ = 0;
        }
    }

private:
    SDL_InitFlags flags_ = 0;
    bool active_ = false;
};

class subsystem {
public:
    explicit subsystem(SDL_InitFlags flags)
        : flags_(flags)
    {
        check(SDL_InitSubSystem(flags), "SDL_InitSubSystem");
        active_ = true;
    }

    subsystem(const subsystem&) = delete;
    subsystem& operator=(const subsystem&) = delete;

    subsystem(subsystem&& other) noexcept
        : flags_(std::exchange(other.flags_, 0))
        , active_(std::exchange(other.active_, false))
    {
    }

    subsystem& operator=(subsystem&& other) noexcept
    {
        if (this != &other) {
            reset();
            flags_ = std::exchange(other.flags_, 0);
            active_ = std::exchange(other.active_, false);
        }
        return *this;
    }

    ~subsystem()
    {
        reset();
    }

    [[nodiscard]] SDL_InitFlags flags() const noexcept
    {
        return flags_;
    }

    [[nodiscard]] bool active() const noexcept
    {
        return active_;
    }

    void reset() noexcept
    {
        if (active_) {
            SDL_QuitSubSystem(flags_);
            active_ = false;
            flags_ = 0;
        }
    }

private:
    SDL_InitFlags flags_ = 0;
    bool active_ = false;
};

[[nodiscard]] inline SDL_InitFlags was_init(SDL_InitFlags flags = 0) noexcept
{
    return SDL_WasInit(flags);
}

inline void set_app_metadata(std::string_view name, std::string_view app_version, std::string_view identifier)
{
    std::string name_string(name);
    std::string version_string(app_version);
    std::string identifier_string(identifier);
    check(SDL_SetAppMetadata(name_string.c_str(), version_string.c_str(), identifier_string.c_str()), "SDL_SetAppMetadata");
}

inline void set_app_metadata_property(std::string_view name, std::string_view value)
{
    std::string name_string(name);
    std::string value_string(value);
    check(SDL_SetAppMetadataProperty(name_string.c_str(), value_string.c_str()), "SDL_SetAppMetadataProperty");
}

[[nodiscard]] inline std::string app_metadata_property(const char* name)
{
    const char* value = SDL_GetAppMetadataProperty(name);
    return value != nullptr ? std::string(value) : std::string();
}

inline void set_hint(const char* name, std::string_view value)
{
    std::string value_string(value);
    check(SDL_SetHint(name, value_string.c_str()), "SDL_SetHint");
}

class properties {
public:
    properties() = default;

    explicit properties(SDL_PropertiesID id) noexcept
        : id_(id)
    {
    }

    properties(const properties&) = delete;
    properties& operator=(const properties&) = delete;

    properties(properties&& other) noexcept
        : id_(std::exchange(other.id_, 0))
    {
    }

    properties& operator=(properties&& other) noexcept
    {
        if (this != &other) {
            reset();
            id_ = std::exchange(other.id_, 0);
        }
        return *this;
    }

    ~properties()
    {
        reset();
    }

    [[nodiscard]] SDL_PropertiesID get() const noexcept
    {
        return id_;
    }

    [[nodiscard]] explicit operator bool() const noexcept
    {
        return id_ != 0;
    }

    [[nodiscard]] SDL_PropertiesID release() noexcept
    {
        return std::exchange(id_, 0);
    }

    void reset(SDL_PropertiesID id = 0) noexcept
    {
        if (id_ != 0) {
            SDL_DestroyProperties(id_);
        }
        id_ = id;
    }

    [[nodiscard]] SDL_PropertyType type(const char* name) const
    {
        return SDL_GetPropertyType(id_, name);
    }

    void set_pointer(const char* name, void* value)
    {
        check(SDL_SetPointerProperty(id_, name, value), "SDL_SetPointerProperty");
    }

    void set_string(std::string_view name, std::string_view value)
    {
        std::string name_string(name);
        std::string value_string(value);
        check(SDL_SetStringProperty(id_, name_string.c_str(), value_string.c_str()), "SDL_SetStringProperty");
    }

    void set_number(const char* name, Sint64 value)
    {
        check(SDL_SetNumberProperty(id_, name, value), "SDL_SetNumberProperty");
    }

    void set_float(const char* name, float value)
    {
        check(SDL_SetFloatProperty(id_, name, value), "SDL_SetFloatProperty");
    }

    void set_boolean(const char* name, bool value)
    {
        check(SDL_SetBooleanProperty(id_, name, value), "SDL_SetBooleanProperty");
    }

    [[nodiscard]] void* pointer(const char* name, void* default_value = nullptr) const
    {
        return SDL_GetPointerProperty(id_, name, default_value);
    }

    [[nodiscard]] std::string string(const char* name, const char* default_value = "") const
    {
        const char* value = SDL_GetStringProperty(id_, name, default_value);
        return value != nullptr ? std::string(value) : std::string();
    }

    [[nodiscard]] Sint64 number(const char* name, Sint64 default_value = 0) const
    {
        return SDL_GetNumberProperty(id_, name, default_value);
    }

    [[nodiscard]] float floating(const char* name, float default_value = 0.0f) const
    {
        return SDL_GetFloatProperty(id_, name, default_value);
    }

    [[nodiscard]] bool boolean(const char* name, bool default_value = false) const
    {
        return SDL_GetBooleanProperty(id_, name, default_value);
    }

private:
    SDL_PropertiesID id_ = 0;
};

[[nodiscard]] inline properties make_properties()
{
    return properties(check_property_id(SDL_CreateProperties(), "SDL_CreateProperties"));
}

[[nodiscard]] inline window make_window(std::string_view title, int width, int height, SDL_WindowFlags flags = 0)
{
    std::string title_string(title);
    return window(check_not_null(SDL_CreateWindow(title_string.c_str(), width, height, flags), "SDL_CreateWindow"));
}

struct window_renderer {
    sdl::window window;
    sdl::renderer renderer;
};

[[nodiscard]] inline window_renderer make_window_and_renderer(
    std::string_view title,
    int width,
    int height,
    SDL_WindowFlags flags = 0)
{
    std::string title_string(title);
    SDL_Window* raw_window = nullptr;
    SDL_Renderer* raw_renderer = nullptr;
    check(
        SDL_CreateWindowAndRenderer(title_string.c_str(), width, height, flags, &raw_window, &raw_renderer),
        "SDL_CreateWindowAndRenderer");
    return {window(raw_window), renderer(raw_renderer)};
}

[[nodiscard]] inline SDL_WindowID window_id(const window& value)
{
    return SDL_GetWindowID(value.get());
}

[[nodiscard]] inline SDL_WindowFlags window_flags(const window& value)
{
    return SDL_GetWindowFlags(value.get());
}

inline void set_window_title(window& value, std::string_view title)
{
    std::string title_string(title);
    check(SDL_SetWindowTitle(value.get(), title_string.c_str()), "SDL_SetWindowTitle");
}

[[nodiscard]] inline std::string window_title(const window& value)
{
    const char* title = SDL_GetWindowTitle(value.get());
    return title != nullptr ? std::string(title) : std::string();
}

inline void set_window_size(window& value, int width, int height)
{
    check(SDL_SetWindowSize(value.get(), width, height), "SDL_SetWindowSize");
}

[[nodiscard]] inline size window_size(const window& value)
{
    size result;
    check(SDL_GetWindowSize(value.get(), &result.width, &result.height), "SDL_GetWindowSize");
    return result;
}

inline void show_window(window& value)
{
    check(SDL_ShowWindow(value.get()), "SDL_ShowWindow");
}

inline void hide_window(window& value)
{
    check(SDL_HideWindow(value.get()), "SDL_HideWindow");
}

[[nodiscard]] inline renderer make_renderer(window& value)
{
    return renderer(check_not_null(SDL_CreateRenderer(value.get(), nullptr), "SDL_CreateRenderer"));
}

[[nodiscard]] inline renderer make_renderer(window& value, std::string_view name)
{
    std::string name_string(name);
    return renderer(check_not_null(SDL_CreateRenderer(value.get(), name_string.c_str()), "SDL_CreateRenderer"));
}

[[nodiscard]] inline renderer make_renderer(SDL_Surface* value)
{
    return renderer(check_not_null(SDL_CreateSoftwareRenderer(value), "SDL_CreateSoftwareRenderer"));
}

[[nodiscard]] inline renderer make_renderer(surface& value)
{
    return make_renderer(value.get());
}

[[nodiscard]] inline renderer make_renderer(const properties& props)
{
    return renderer(check_not_null(SDL_CreateRendererWithProperties(props.get()), "SDL_CreateRendererWithProperties"));
}

[[nodiscard]] inline std::string renderer_name(const renderer& value)
{
    const char* name = SDL_GetRendererName(value.get());
    return name != nullptr ? std::string(name) : std::string();
}

[[nodiscard]] inline size render_output_size(const renderer& value)
{
    size result;
    check(SDL_GetRenderOutputSize(value.get(), &result.width, &result.height), "SDL_GetRenderOutputSize");
    return result;
}

inline void set_render_vsync(renderer& value, int vsync)
{
    check(SDL_SetRenderVSync(value.get(), vsync), "SDL_SetRenderVSync");
}

inline void set_render_draw_color(renderer& value, color value_color)
{
    check(
        SDL_SetRenderDrawColor(value.get(), value_color.r, value_color.g, value_color.b, value_color.a),
        "SDL_SetRenderDrawColor");
}

inline void set_render_draw_blend_mode(renderer& value, SDL_BlendMode blend_mode)
{
    check(SDL_SetRenderDrawBlendMode(value.get(), blend_mode), "SDL_SetRenderDrawBlendMode");
}

inline void clear(renderer& value)
{
    check(SDL_RenderClear(value.get()), "SDL_RenderClear");
}

inline void present(renderer& value)
{
    check(SDL_RenderPresent(value.get()), "SDL_RenderPresent");
}

inline void draw_point(renderer& value, float x, float y)
{
    check(SDL_RenderPoint(value.get(), x, y), "SDL_RenderPoint");
}

inline void draw_points(renderer& value, std::span<const fpoint> points)
{
    check(
        SDL_RenderPoints(value.get(), points.data(), detail::checked_count(points.size(), "SDL_RenderPoints")),
        "SDL_RenderPoints");
}

inline void draw_line(renderer& value, float x1, float y1, float x2, float y2)
{
    check(SDL_RenderLine(value.get(), x1, y1, x2, y2), "SDL_RenderLine");
}

inline void draw_lines(renderer& value, std::span<const fpoint> points)
{
    check(
        SDL_RenderLines(value.get(), points.data(), detail::checked_count(points.size(), "SDL_RenderLines")),
        "SDL_RenderLines");
}

inline void draw_rect(renderer& value, const frect* value_rect = nullptr)
{
    check(SDL_RenderRect(value.get(), value_rect), "SDL_RenderRect");
}

inline void draw_rect(renderer& value, const frect& value_rect)
{
    draw_rect(value, &value_rect);
}

inline void draw_rects(renderer& value, std::span<const frect> rects)
{
    check(
        SDL_RenderRects(value.get(), rects.data(), detail::checked_count(rects.size(), "SDL_RenderRects")),
        "SDL_RenderRects");
}

inline void fill_rect(renderer& value, const frect* value_rect = nullptr)
{
    check(SDL_RenderFillRect(value.get(), value_rect), "SDL_RenderFillRect");
}

inline void fill_rect(renderer& value, const frect& value_rect)
{
    fill_rect(value, &value_rect);
}

inline void fill_rects(renderer& value, std::span<const frect> rects)
{
    check(
        SDL_RenderFillRects(value.get(), rects.data(), detail::checked_count(rects.size(), "SDL_RenderFillRects")),
        "SDL_RenderFillRects");
}

inline void debug_text(renderer& value, float x, float y, std::string_view text)
{
    std::string text_string(text);
    check(SDL_RenderDebugText(value.get(), x, y, text_string.c_str()), "SDL_RenderDebugText");
}

[[nodiscard]] inline surface make_surface(int width, int height, SDL_PixelFormat format = SDL_PIXELFORMAT_RGBA32)
{
    return surface(check_not_null(SDL_CreateSurface(width, height, format), "SDL_CreateSurface"));
}

[[nodiscard]] inline surface make_surface_from(
    int width,
    int height,
    SDL_PixelFormat format,
    void* pixels,
    int pitch)
{
    return surface(check_not_null(SDL_CreateSurfaceFrom(width, height, format, pixels, pitch), "SDL_CreateSurfaceFrom"));
}

[[nodiscard]] inline size surface_size(const surface& value) noexcept
{
    SDL_Surface* raw = value.get();
    return raw != nullptr ? size{raw->w, raw->h} : size{};
}

[[nodiscard]] inline int surface_pitch(const surface& value) noexcept
{
    SDL_Surface* raw = value.get();
    return raw != nullptr ? raw->pitch : 0;
}

[[nodiscard]] inline SDL_PixelFormat surface_format(const surface& value) noexcept
{
    SDL_Surface* raw = value.get();
    return raw != nullptr ? raw->format : SDL_PIXELFORMAT_UNKNOWN;
}

[[nodiscard]] inline void* surface_pixels(const surface& value) noexcept
{
    SDL_Surface* raw = value.get();
    return raw != nullptr ? raw->pixels : nullptr;
}

class surface_lock {
public:
    explicit surface_lock(surface& value)
        : surface_(value.get())
    {
        check(SDL_LockSurface(surface_), "SDL_LockSurface");
    }

    surface_lock(const surface_lock&) = delete;
    surface_lock& operator=(const surface_lock&) = delete;

    surface_lock(surface_lock&& other) noexcept
        : surface_(std::exchange(other.surface_, nullptr))
    {
    }

    surface_lock& operator=(surface_lock&& other) noexcept
    {
        if (this != &other) {
            unlock();
            surface_ = std::exchange(other.surface_, nullptr);
        }
        return *this;
    }

    ~surface_lock()
    {
        unlock();
    }

    void unlock() noexcept
    {
        if (surface_ != nullptr) {
            SDL_UnlockSurface(surface_);
            surface_ = nullptr;
        }
    }

private:
    SDL_Surface* surface_ = nullptr;
};

inline void fill_surface_rect(surface& value, Uint32 pixel, const rect* value_rect = nullptr)
{
    check(SDL_FillSurfaceRect(value.get(), value_rect, pixel), "SDL_FillSurfaceRect");
}

inline void fill_surface_rect(surface& value, const rect& value_rect, Uint32 pixel)
{
    fill_surface_rect(value, pixel, &value_rect);
}

inline void blit_surface(surface& source, surface& destination, const rect* source_rect = nullptr, const rect* destination_rect = nullptr)
{
    check(SDL_BlitSurface(source.get(), source_rect, destination.get(), destination_rect), "SDL_BlitSurface");
}

[[nodiscard]] inline texture make_texture(
    renderer& value,
    SDL_PixelFormat format,
    SDL_TextureAccess access,
    int width,
    int height)
{
    return texture(check_not_null(SDL_CreateTexture(value.get(), format, access, width, height), "SDL_CreateTexture"));
}

[[nodiscard]] inline texture make_texture_from_surface(renderer& value, surface& source)
{
    return texture(check_not_null(SDL_CreateTextureFromSurface(value.get(), source.get()), "SDL_CreateTextureFromSurface"));
}

[[nodiscard]] inline fsize texture_size(const texture& value)
{
    fsize result;
    check(SDL_GetTextureSize(value.get(), &result.width, &result.height), "SDL_GetTextureSize");
    return result;
}

inline void set_texture_blend_mode(texture& value, SDL_BlendMode blend_mode)
{
    check(SDL_SetTextureBlendMode(value.get(), blend_mode), "SDL_SetTextureBlendMode");
}

inline void set_texture_scale_mode(texture& value, SDL_ScaleMode scale_mode)
{
    check(SDL_SetTextureScaleMode(value.get(), scale_mode), "SDL_SetTextureScaleMode");
}

inline void update_texture(texture& value, const void* pixels, int pitch, const rect* value_rect = nullptr)
{
    check(SDL_UpdateTexture(value.get(), value_rect, pixels, pitch), "SDL_UpdateTexture");
}

class texture_lock {
public:
    explicit texture_lock(texture& value, const rect* value_rect = nullptr)
        : texture_(value.get())
    {
        check(SDL_LockTexture(texture_, value_rect, &pixels_, &pitch_), "SDL_LockTexture");
    }

    texture_lock(const texture_lock&) = delete;
    texture_lock& operator=(const texture_lock&) = delete;

    texture_lock(texture_lock&& other) noexcept
        : texture_(std::exchange(other.texture_, nullptr))
        , pixels_(std::exchange(other.pixels_, nullptr))
        , pitch_(std::exchange(other.pitch_, 0))
    {
    }

    texture_lock& operator=(texture_lock&& other) noexcept
    {
        if (this != &other) {
            unlock();
            texture_ = std::exchange(other.texture_, nullptr);
            pixels_ = std::exchange(other.pixels_, nullptr);
            pitch_ = std::exchange(other.pitch_, 0);
        }
        return *this;
    }

    ~texture_lock()
    {
        unlock();
    }

    [[nodiscard]] void* pixels() const noexcept
    {
        return pixels_;
    }

    [[nodiscard]] int pitch() const noexcept
    {
        return pitch_;
    }

    void unlock() noexcept
    {
        if (texture_ != nullptr) {
            SDL_UnlockTexture(texture_);
            texture_ = nullptr;
            pixels_ = nullptr;
            pitch_ = 0;
        }
    }

private:
    SDL_Texture* texture_ = nullptr;
    void* pixels_ = nullptr;
    int pitch_ = 0;
};

inline void render_texture(renderer& value, texture& source, const frect* source_rect = nullptr, const frect* destination_rect = nullptr)
{
    check(SDL_RenderTexture(value.get(), source.get(), source_rect, destination_rect), "SDL_RenderTexture");
}

inline void render_texture(renderer& value, texture& source, const frect& source_rect, const frect& destination_rect)
{
    render_texture(value, source, &source_rect, &destination_rect);
}

[[nodiscard]] inline io_stream io_from_file(std::string_view path, std::string_view mode)
{
    std::string path_string(path);
    std::string mode_string(mode);
    return io_stream(check_not_null(SDL_IOFromFile(path_string.c_str(), mode_string.c_str()), "SDL_IOFromFile"));
}

[[nodiscard]] inline io_stream io_from_memory(void* memory, std::size_t size)
{
    return io_stream(check_not_null(SDL_IOFromMem(memory, size), "SDL_IOFromMem"));
}

[[nodiscard]] inline io_stream io_from_const_memory(const void* memory, std::size_t size)
{
    return io_stream(check_not_null(SDL_IOFromConstMem(memory, size), "SDL_IOFromConstMem"));
}

[[nodiscard]] inline io_stream dynamic_io()
{
    return io_stream(check_not_null(SDL_IOFromDynamicMem(), "SDL_IOFromDynamicMem"));
}

[[nodiscard]] inline Sint64 io_size(const io_stream& value)
{
    Sint64 result = SDL_GetIOSize(value.get());
    if (result < 0) {
        throw_error("SDL_GetIOSize");
    }
    return result;
}

[[nodiscard]] inline Sint64 seek(io_stream& value, Sint64 offset, SDL_IOWhence whence)
{
    Sint64 result = SDL_SeekIO(value.get(), offset, whence);
    if (result < 0) {
        throw_error("SDL_SeekIO");
    }
    return result;
}

[[nodiscard]] inline Sint64 tell(io_stream& value)
{
    Sint64 result = SDL_TellIO(value.get());
    if (result < 0) {
        throw_error("SDL_TellIO");
    }
    return result;
}

[[nodiscard]] inline std::size_t read(io_stream& value, std::span<std::byte> bytes)
{
    return SDL_ReadIO(value.get(), bytes.data(), bytes.size());
}

[[nodiscard]] inline std::size_t write(io_stream& value, std::span<const std::byte> bytes)
{
    return SDL_WriteIO(value.get(), bytes.data(), bytes.size());
}

inline void write_all(io_stream& value, std::span<const std::byte> bytes)
{
    const std::size_t written = write(value, bytes);
    if (written != bytes.size()) {
        throw_error("SDL_WriteIO");
    }
}

inline void flush(io_stream& value)
{
    check(SDL_FlushIO(value.get()), "SDL_FlushIO");
}

[[nodiscard]] inline std::optional<SDL_Event> poll_event()
{
    SDL_Event event {};
    if (SDL_PollEvent(&event)) {
        return event;
    }
    return std::nullopt;
}

[[nodiscard]] inline SDL_Event wait_event()
{
    SDL_Event event {};
    check(SDL_WaitEvent(&event), "SDL_WaitEvent");
    return event;
}

[[nodiscard]] inline std::optional<SDL_Event> wait_event_for(std::chrono::milliseconds timeout)
{
    SDL_Event event {};
    if (SDL_WaitEventTimeout(&event, static_cast<Sint32>(timeout.count()))) {
        return event;
    }
    return std::nullopt;
}

inline void push_event(SDL_Event event)
{
    check(SDL_PushEvent(&event), "SDL_PushEvent");
}

[[nodiscard]] inline allocated_string pref_path(std::string_view organization, std::string_view application)
{
    std::string organization_string(organization);
    std::string application_string(application);
    return allocated_string(check_not_null(
        SDL_GetPrefPath(organization_string.c_str(), application_string.c_str()),
        "SDL_GetPrefPath"));
}

[[nodiscard]] inline std::string base_path()
{
    const char* path = SDL_GetBasePath();
    return path != nullptr ? std::string(path) : std::string();
}

} // namespace sdl

#undef SDL3_HPP_CPLUSPLUS
