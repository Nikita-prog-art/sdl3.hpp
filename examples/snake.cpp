#include <sdl3.hpp>

#include <algorithm>
#include <chrono>
#include <exception>
#include <iterator>
#include <iostream>
#include <random>
#include <string>
#include <vector>

namespace {

using namespace std::chrono_literals;

constexpr int grid_width = 32;
constexpr int grid_height = 24;
constexpr int cell_size = 24;
constexpr int header_height = 32;
constexpr int window_width = grid_width * cell_size;
constexpr int window_height = header_height + grid_height * cell_size;
constexpr auto tick_rate = 110ms;

struct cell {
    int x = 0;
    int y = 0;

    friend constexpr bool operator==(const cell&, const cell&) noexcept = default;
};

enum class direction {
    up,
    down,
    left,
    right,
};

[[nodiscard]] constexpr bool opposite(direction a, direction b) noexcept
{
    return (a == direction::up && b == direction::down) ||
        (a == direction::down && b == direction::up) ||
        (a == direction::left && b == direction::right) ||
        (a == direction::right && b == direction::left);
}

[[nodiscard]] constexpr cell advance(cell value, direction dir) noexcept
{
    switch (dir) {
    case direction::up:
        --value.y;
        break;
    case direction::down:
        ++value.y;
        break;
    case direction::left:
        --value.x;
        break;
    case direction::right:
        ++value.x;
        break;
    }
    return value;
}

[[nodiscard]] constexpr sdl::frect cell_rect(cell value, float inset = 2.0f) noexcept
{
    return {
        static_cast<float>(value.x * cell_size) + inset,
        static_cast<float>(header_height + value.y * cell_size) + inset,
        static_cast<float>(cell_size) - inset * 2.0f,
        static_cast<float>(cell_size) - inset * 2.0f,
    };
}

class snake_game {
public:
    snake_game()
        : rng_(std::random_device {}())
    {
        reset();
    }

    [[nodiscard]] bool running() const noexcept
    {
        return running_;
    }

    void handle(const SDL_Event& event)
    {
        if (event.type == SDL_EVENT_QUIT) {
            running_ = false;
            return;
        }

        if (event.type != SDL_EVENT_KEY_DOWN || event.key.repeat) {
            return;
        }

        switch (event.key.key) {
        case SDLK_ESCAPE:
            running_ = false;
            break;
        case SDLK_SPACE:
        case SDLK_R:
            if (game_over_) {
                reset();
            }
            break;
        case SDLK_UP:
        case SDLK_W:
            queue_direction(direction::up);
            break;
        case SDLK_DOWN:
        case SDLK_S:
            queue_direction(direction::down);
            break;
        case SDLK_LEFT:
        case SDLK_A:
            queue_direction(direction::left);
            break;
        case SDLK_RIGHT:
        case SDLK_D:
            queue_direction(direction::right);
            break;
        default:
            break;
        }
    }

    void update()
    {
        if (game_over_) {
            return;
        }

        current_direction_ = queued_direction_;
        const cell next = advance(snake_.front(), current_direction_);
        const bool grows = next == food_;

        if (out_of_bounds(next) || hits_body(next, grows)) {
            game_over_ = true;
            return;
        }

        snake_.insert(snake_.begin(), next);

        if (grows) {
            ++score_;
            spawn_food();
        } else {
            snake_.pop_back();
        }
    }

    void render(sdl::renderer& renderer) const
    {
        sdl::set_render_draw_color(renderer, {18, 20, 24, 255});
        sdl::clear(renderer);

        draw_header(renderer);
        draw_board(renderer);
        draw_food(renderer);
        draw_snake(renderer);

        if (game_over_) {
            draw_game_over(renderer);
        }

        sdl::present(renderer);
    }

private:
    void reset()
    {
        score_ = 0;
        game_over_ = false;
        current_direction_ = direction::right;
        queued_direction_ = direction::right;

        const cell head {grid_width / 2, grid_height / 2};
        snake_.clear();
        snake_.push_back(head);
        snake_.push_back({head.x - 1, head.y});
        snake_.push_back({head.x - 2, head.y});
        snake_.push_back({head.x - 3, head.y});

        spawn_food();
    }

    void queue_direction(direction next) noexcept
    {
        if (!opposite(current_direction_, next) && !opposite(queued_direction_, next)) {
            queued_direction_ = next;
        }
    }

    [[nodiscard]] bool out_of_bounds(cell value) const noexcept
    {
        return value.x < 0 || value.y < 0 || value.x >= grid_width || value.y >= grid_height;
    }

    [[nodiscard]] bool contains(cell value) const
    {
        return std::find(snake_.begin(), snake_.end(), value) != snake_.end();
    }

    [[nodiscard]] bool hits_body(cell value, bool include_tail) const
    {
        if (snake_.size() <= 1) {
            return false;
        }

        auto first = std::next(snake_.begin());
        auto last = snake_.end();
        if (!include_tail) {
            --last;
        }
        return std::find(first, last, value) != last;
    }

    void spawn_food()
    {
        if (static_cast<int>(snake_.size()) >= grid_width * grid_height) {
            game_over_ = true;
            return;
        }

        std::uniform_int_distribution<int> x_dist(0, grid_width - 1);
        std::uniform_int_distribution<int> y_dist(0, grid_height - 1);

        do {
            food_ = {x_dist(rng_), y_dist(rng_)};
        } while (contains(food_));
    }

    void draw_header(sdl::renderer& renderer) const
    {
        sdl::set_render_draw_color(renderer, {29, 34, 42, 255});
        sdl::fill_rect(
            renderer,
            sdl::frect {0.0f, 0.0f, static_cast<float>(window_width), static_cast<float>(header_height)});

        sdl::set_render_draw_color(renderer, {230, 234, 241, 255});
        sdl::debug_text(renderer, 10.0f, 10.0f, "Score: " + std::to_string(score_));

        if (!game_over_) {
            sdl::debug_text(renderer, 120.0f, 10.0f, "Arrows/WASD to move, Esc to quit");
        }
    }

    void draw_board(sdl::renderer& renderer) const
    {
        sdl::set_render_draw_color(renderer, {22, 27, 34, 255});
        sdl::fill_rect(
            renderer,
            sdl::frect {
                0.0f,
                static_cast<float>(header_height),
                static_cast<float>(window_width),
                static_cast<float>(window_height - header_height),
            });

        sdl::set_render_draw_color(renderer, {35, 41, 50, 255});
        for (int y = 0; y < grid_height; ++y) {
            for (int x = 0; x < grid_width; ++x) {
                sdl::draw_rect(renderer, cell_rect({x, y}, 0.5f));
            }
        }
    }

    void draw_food(sdl::renderer& renderer) const
    {
        sdl::set_render_draw_color(renderer, {230, 72, 72, 255});
        sdl::fill_rect(renderer, cell_rect(food_, 4.0f));
    }

    void draw_snake(sdl::renderer& renderer) const
    {
        if (snake_.empty()) {
            return;
        }

        sdl::set_render_draw_color(renderer, {132, 204, 22, 255});
        sdl::fill_rect(renderer, cell_rect(snake_.front(), 2.0f));

        sdl::set_render_draw_color(renderer, {74, 222, 128, 255});
        for (auto it = std::next(snake_.begin()); it != snake_.end(); ++it) {
            sdl::fill_rect(renderer, cell_rect(*it, 3.0f));
        }
    }

    void draw_game_over(sdl::renderer& renderer) const
    {
        const sdl::frect panel {
            static_cast<float>(window_width / 2 - 160),
            static_cast<float>(window_height / 2 - 32),
            320.0f,
            64.0f,
        };

        sdl::set_render_draw_color(renderer, {12, 14, 18, 230});
        sdl::fill_rect(renderer, panel);

        sdl::set_render_draw_color(renderer, {245, 247, 250, 255});
        sdl::debug_text(renderer, panel.x + 24.0f, panel.y + 18.0f, "Game over - Space/R to restart");
        sdl::debug_text(renderer, panel.x + 24.0f, panel.y + 36.0f, "Esc to quit");
    }

    bool running_ = true;
    bool game_over_ = false;
    int score_ = 0;
    direction current_direction_ = direction::right;
    direction queued_direction_ = direction::right;
    std::vector<cell> snake_;
    cell food_;
    std::mt19937 rng_;
};

} // namespace

int main()
{
    try {
        sdl::set_app_metadata("sdl3.hpp snake", "1.0.0", "com.example.sdl3-hpp.snake");

        sdl::init app(SDL_INIT_VIDEO);
        auto window_and_renderer = sdl::make_window_and_renderer("sdl3.hpp snake", window_width, window_height);
        sdl::set_render_draw_blend_mode(window_and_renderer.renderer, SDL_BLENDMODE_BLEND);

        snake_game game;
        auto previous = sdl::ticks();
        auto accumulator = 0ms;

        while (game.running()) {
            while (auto event = sdl::poll_event()) {
                game.handle(*event);
            }

            const auto now = sdl::ticks();
            accumulator += now - previous;
            previous = now;

            while (accumulator >= tick_rate) {
                game.update();
                accumulator -= tick_rate;
            }

            game.render(window_and_renderer.renderer);
            sdl::delay(8ms);
        }
    } catch (const sdl::error& err) {
        std::cerr << "SDL error: " << err.what() << '\n';
        return 1;
    } catch (const std::exception& err) {
        std::cerr << "Error: " << err.what() << '\n';
        return 1;
    }
}
