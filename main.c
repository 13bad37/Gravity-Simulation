#include "gravity.h"
#include "render.h"
#include "scenes.h"
#include "simulation.h"

#include <SDL2/SDL.h>
#include <stdio.h>

int main(void) {
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    SDL_Window *window = SDL_CreateWindow(
        "Gravity Sim",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        WINDOW_WIDTH,
        WINDOW_HEIGHT,
        SDL_WINDOW_SHOWN
    );

    SDL_Renderer *renderer = SDL_CreateRenderer(
        window,
        -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC
    );
    // Use software rendering if hardware rendering isn't an option

    if (renderer == NULL) {
        renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
    }

    if (renderer == NULL) {
        fprintf(stderr, "SDL_CreateRenderer failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    Simulation initial_state = {0};
    Simulation sim = {0};
    SpawnState spawn = {0};
    ScenePreset current_scene = SCENE_STARTER;
    double accumulator = 0.0;

    activate_scene(current_scene, &initial_state, &sim, &spawn, &accumulator);

    spawn.mass = 20.0;
    spawn.color_index = 0;

    bool running = true;
    bool paused = false;

    Uint64 previous_counter = SDL_GetPerformanceCounter();
    Uint64 performance_frequency = SDL_GetPerformanceFrequency();

    puts("Controls:");
    puts("  Left click + drag - spawn a new body and set its launch velocity");
    puts("  Right click       - cancel the current spawn drag");
    puts("  Mouse wheel / [ ] - adjust spawn mass");
    puts("  0/1/2/3 - empty / starter/ 3-body / binary stars");
    puts("  Space  - pause / resume");
    puts("  R      - reset scene");
    puts("  Esc    - quit");

    while (running) {
        Uint64 current_counter = SDL_GetPerformanceCounter();
        double frame_time = (double)(current_counter - previous_counter) / (double)performance_frequency;
        previous_counter = current_counter;

        //Big frame skips can happen when moving windows or debugging so clamping stops one bad frtame from ruining the physics.
        if (frame_time > MAX_FRAME_TIME) {
            frame_time = MAX_FRAME_TIME;
        }
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = false;
            }
            if (event.type == SDL_KEYDOWN && event.key.repeat == 0) {
                switch (event.key.keysym.sym) {
                    case SDLK_ESCAPE:
                        running = false;
                        break;

                    case SDLK_SPACE:
                        paused = !paused;
                        break;

                    case SDLK_r:
                        activate_scene(current_scene, &initial_state, &sim, &spawn, &accumulator);
                        break;

                    case SDLK_0:
                        current_scene = SCENE_EMPTY;
                        activate_scene(current_scene, &initial_state, &sim, &spawn, &accumulator);
                        break;

                    case SDLK_1:
                        current_scene = SCENE_STARTER;
                        activate_scene(current_scene, &initial_state, &sim, &spawn, &accumulator);
                        break;

                    case SDLK_2:
                        current_scene = SCENE_CHAOTIC_3_BODY;
                        activate_scene(current_scene, &initial_state, &sim, &spawn, &accumulator);
                        break;

                    case SDLK_3:
                        current_scene = SCENE_BINARY_STARS;
                        activate_scene(current_scene, &initial_state, &sim, &spawn, &accumulator);
                        break;

                    case SDLK_LEFTBRACKET:
                        adjust_spawn_mass(&spawn, -SPAWN_MASS_STEP);
                        break;

                    case SDLK_RIGHTBRACKET:
                        adjust_spawn_mass(&spawn, SPAWN_MASS_STEP);
                        break;

                    default:
                        break;
                }
            }
            if (event.type == SDL_MOUSEWHEEL) {
                int wheel_y = event.wheel.y;

                if (event.wheel.direction == SDL_MOUSEWHEEL_FLIPPED) {
                    wheel_y = -wheel_y;
                }

                if (wheel_y != 0) {
                    adjust_spawn_mass(&spawn, wheel_y * SPAWN_MASS_STEP);
                }
            }

            if (event.type == SDL_MOUSEBUTTONDOWN) {
                if (event.button.button == SDL_BUTTON_LEFT) {
                    spawn.active = true;
                    spawn.start = vec2((double)event.button.x, (double)event.button.y);
                    spawn.current = spawn.start;
                }

                if (event.button.button == SDL_BUTTON_RIGHT) {
                    spawn.active = false;
                }
            }

            if (event.type == SDL_MOUSEMOTION && spawn.active) {
                spawn.current = vec2((double)event.motion.x, (double)event.motion.y);
            }

            if (event.type == SDL_MOUSEBUTTONUP &&
                event.button.button == SDL_BUTTON_LEFT &&
                spawn.active) {
                spawn.current = vec2((double)event.button.x, (double)event.button.y);

                Vec2 launch_velocity = vec_scale(
                    vec_sub(spawn.current, spawn.start),
                    SPAWN_VELOCITY_SCALE
                );

                Body new_body = make_body(
                    spawn.start.x,
                    spawn.start.y,
                    launch_velocity.x,
                    launch_velocity.y,
                    spawn.mass,
                    radius_from_mass(spawn.mass),
                    current_spawn_color(&spawn)
                );

                if (!add_body(&sim, new_body)) {
                    fprintf(stderr, "Body limit reached (%d max)\n", MAX_BODIES);
                } else {
                    int color_count = spawn_color_count();
                    spawn.color_index = (spawn.color_index + 1) % color_count;
                }

                spawn.active = false;
            }
        }
        // Fixed timestep loop
        if (!paused) {
            accumulator += frame_time;

            while (accumulator >= FIXED_DT) {
                step_simulation(&sim, FIXED_DT);
                accumulator -= FIXED_DT;
            }
        }
        update_window_title(window, &sim, &spawn, current_scene, paused);
        render_simulation(renderer, &sim, &spawn);
    }
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
