#include "gravity.h"
#include "render.h"
#include "scenes.h"
#include "simulation.h"

#include <SDL2/SDL.h>
#include <math.h>
#include <stdio.h>

// Physics now runs in SI units, so the sim now uses explicit playback rate
// This is just says 1 real second corresponds to 1 simulated day.
static const double SIM_SECONDS_PER_REAL_SECOND = 86400.0;
static const double TIME_SCALE_OPTIONS[] = {0.125, 0.25, 0.5, 1.0, 2.0, 4.0, 8.0, 16.0};
static const int DEFAULT_TIME_SCALE_INDEX = 3;
static const double CAMERA_ZOOM_FACTOR = 1.25;
static const double CAMERA_MIN_METERS_PER_PIXEL = 1.0e5;
static const double CAMERA_MAX_METERS_PER_PIXEL = 5.0e10;
static const double CAMERA_KEY_PAN_PIXELS = 80.0;

static double clamp_double(double value, double min_value, double max_value) {
    if (value < min_value) {
        return min_value;
    }

    if (value > max_value) {
        return max_value;
    }

    return value;
}

static void reset_camera(Camera *camera) {
    camera->center = vec2(0.0, 0.0);
    camera->meters_per_pixel = VIEW_METERS_PER_PIXEL;
}

static void pan_camera_pixels(Camera *camera, double dx_pixels, double dy_pixels) {
    // Positive screen x is right, positive screen y is down.
    // Camera center uses world coordinates where positive y is up.
    camera->center.x += dx_pixels * camera->meters_per_pixel;
    camera->center.y -= dy_pixels * camera->meters_per_pixel;
}

static void zoom_camera_at(Camera *camera, int screen_x, int screen_y, double zoom_factor) {
    Vec2 world_before = screen_to_world(screen_x, screen_y, camera);

    camera->meters_per_pixel = clamp_double(
        camera->meters_per_pixel * zoom_factor,
        CAMERA_MIN_METERS_PER_PIXEL,
        CAMERA_MAX_METERS_PER_PIXEL
    );

    Vec2 world_after = screen_to_world(screen_x, screen_y, camera);
    camera->center = vec_add(camera->center, vec_sub(world_before, world_after));
}

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

    if (!init_render_resources()) {
        fprintf(stderr, "Failed to initialize render resources\n");
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    Simulation initial_state = {0};
    Simulation sim = {0};
    SpawnState spawn = {0};
    Camera camera = {0};
    ScenePreset current_scene = SCENE_STARTER;
    double accumulator = 0.0;
    double simulated_time_seconds = 0.0;
    int time_scale_index = DEFAULT_TIME_SCALE_INDEX;
    bool hud_visible = true;
    bool camera_dragging = false;
    int camera_drag_last_x = 0;
    int camera_drag_last_y = 0;

    activate_scene(current_scene, &initial_state, &sim, &spawn, &accumulator, &simulated_time_seconds);
    reset_camera(&camera);

    spawn.mass = EARTH_MASS;
    spawn.color_index = 0;

    bool running = true;
    bool paused = false;

    Uint64 previous_counter = SDL_GetPerformanceCounter();
    Uint64 performance_frequency = SDL_GetPerformanceFrequency();
    int time_scale_count = (int)(sizeof(TIME_SCALE_OPTIONS) / sizeof(TIME_SCALE_OPTIONS[0]));

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
                        activate_scene(current_scene, &initial_state, &sim, &spawn, &accumulator,
                                       &simulated_time_seconds);
                        reset_camera(&camera);
                        break;

                    case SDLK_h:
                        hud_visible = !hud_visible;
                        break;

                    case SDLK_c:
                        reset_camera(&camera);
                        break;

                    case SDLK_q:
                        zoom_camera_at(&camera, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2, CAMERA_ZOOM_FACTOR);
                        break;

                    case SDLK_e:
                        zoom_camera_at(&camera, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2, 1.0 / CAMERA_ZOOM_FACTOR);
                        break;

                    case SDLK_t:
                        time_scale_index = DEFAULT_TIME_SCALE_INDEX;
                        break;

                    case SDLK_0:
                        current_scene = SCENE_EMPTY;
                        activate_scene(current_scene, &initial_state, &sim, &spawn, &accumulator,
                                       &simulated_time_seconds);
                        reset_camera(&camera);
                        break;

                    case SDLK_1:
                        current_scene = SCENE_STARTER;
                        activate_scene(current_scene, &initial_state, &sim, &spawn, &accumulator,
                                       &simulated_time_seconds);
                        reset_camera(&camera);
                        break;

                    case SDLK_2:
                        current_scene = SCENE_CHAOTIC_3_BODY;
                        activate_scene(current_scene, &initial_state, &sim, &spawn, &accumulator,
                                       &simulated_time_seconds);
                        reset_camera(&camera);
                        break;

                    case SDLK_3:
                        current_scene = SCENE_BINARY_STARS;
                        activate_scene(current_scene, &initial_state, &sim, &spawn, &accumulator,
                                       &simulated_time_seconds);
                        reset_camera(&camera);
                        break;

                    case SDLK_LEFTBRACKET:
                        adjust_spawn_mass(&spawn, -SPAWN_MASS_STEP);
                        break;

                    case SDLK_RIGHTBRACKET:
                        adjust_spawn_mass(&spawn, SPAWN_MASS_STEP);
                        break;

                    case SDLK_MINUS:
                    case SDLK_KP_MINUS:
                        if (time_scale_index > 0) {
                            time_scale_index--;
                        }
                        break;

                    case SDLK_EQUALS:
                    case SDLK_KP_PLUS:
                        if (time_scale_index < time_scale_count - 1) {
                            time_scale_index++;
                        }
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
                    SDL_Keymod modifiers = SDL_GetModState();

                    if ((modifiers & KMOD_SHIFT) != 0) {
                        adjust_spawn_mass(&spawn, wheel_y * SPAWN_MASS_STEP);
                    } else {
                        int mouse_x;
                        int mouse_y;
                        double zoom_factor = pow(CAMERA_ZOOM_FACTOR, -wheel_y);

                        SDL_GetMouseState(&mouse_x, &mouse_y);
                        zoom_camera_at(&camera, mouse_x, mouse_y, zoom_factor);
                    }
                }
            }

            if (event.type == SDL_MOUSEBUTTONDOWN) {
                if (event.button.button == SDL_BUTTON_LEFT) {
                    spawn.active = true;
                    spawn.start = screen_to_world(event.button.x, event.button.y, &camera);
                    spawn.current = spawn.start;
                }

                if (event.button.button == SDL_BUTTON_RIGHT) {
                    spawn.active = false;
                }

                if (event.button.button == SDL_BUTTON_MIDDLE) {
                    camera_dragging = true;
                    camera_drag_last_x = event.button.x;
                    camera_drag_last_y = event.button.y;
                    spawn.active = false;
                }
            }

            if (event.type == SDL_MOUSEMOTION) {
                if (spawn.active) {
                    spawn.current = screen_to_world(event.motion.x, event.motion.y, &camera);
                }

                if (camera_dragging) {
                    int dx = event.motion.x - camera_drag_last_x;
                    int dy = event.motion.y - camera_drag_last_y;

                    pan_camera_pixels(&camera, -(double)dx, -(double)dy);
                    camera_drag_last_x = event.motion.x;
                    camera_drag_last_y = event.motion.y;
                }
            }

            if (event.type == SDL_MOUSEBUTTONUP && event.button.button == SDL_BUTTON_MIDDLE) {
                camera_dragging = false;
            }

            if (event.type == SDL_MOUSEBUTTONUP &&
                event.button.button == SDL_BUTTON_LEFT &&
                spawn.active) {
                spawn.current = screen_to_world(event.button.x, event.button.y, &camera);

                Vec2 launch_velocity = vec_scale(
                    vec_sub(spawn.current, spawn.start),
                    SPAWN_SPEED_PER_PIXEL / camera.meters_per_pixel
                );

                Body new_body = make_body(
                    spawn.start.x,
                    spawn.start.y,
                    launch_velocity.x,
                    launch_velocity.y,
                    spawn.mass,
                    radius_from_mass(spawn.mass),
                    EARTH_DENSITY,
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

        const Uint8 *keyboard = SDL_GetKeyboardState(NULL);
        double pan_pixels = CAMERA_KEY_PAN_PIXELS * frame_time * 8.0;

        if (keyboard[SDL_SCANCODE_A] || keyboard[SDL_SCANCODE_LEFT]) {
            pan_camera_pixels(&camera, -pan_pixels, 0.0);
        }

        if (keyboard[SDL_SCANCODE_D] || keyboard[SDL_SCANCODE_RIGHT]) {
            pan_camera_pixels(&camera, pan_pixels, 0.0);
        }

        if (keyboard[SDL_SCANCODE_W] || keyboard[SDL_SCANCODE_UP]) {
            pan_camera_pixels(&camera, 0.0, -pan_pixels);
        }

        if (keyboard[SDL_SCANCODE_S] || keyboard[SDL_SCANCODE_DOWN]) {
            pan_camera_pixels(&camera, 0.0, pan_pixels);
        }

        // Fixed timestep loop
        if (!paused) {
            // frame_time is real wall-clock time in seconds.
            // We scale it into simulated time before feeding the fixed-step accumulator.
            accumulator += frame_time * SIM_SECONDS_PER_REAL_SECOND * TIME_SCALE_OPTIONS[time_scale_index];

            while (accumulator >= FIXED_DT) {
                step_simulation(&sim, FIXED_DT);
                simulated_time_seconds += FIXED_DT;
                accumulator -= FIXED_DT;
            }
        }
        SimulationDiagnostics diagnostics = compute_diagnostics(&sim);
        update_window_title(window, &sim, &spawn, &camera, current_scene, paused,
                            TIME_SCALE_OPTIONS[time_scale_index]);
        render_simulation(
            renderer,
            &sim,
            &spawn,
            &diagnostics,
            simulated_time_seconds,
            &camera,
            current_scene,
            paused,
            hud_visible,
            TIME_SCALE_OPTIONS[time_scale_index],
            time_scale_index,
            time_scale_count
        );
    }
    shutdown_render_resources();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
