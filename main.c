#include <SDL2/SDL.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>

#define WINDOW_WIDTH 1280
#define WINDOW_HEIGHT 720
#define MAX_BODIES 16
#define TRAIL_LENGTH 240

// Sim units, not real astronomical constants
// Since this is the first iteration, it's more of a feature since it keeps this version readable for me to extend later.

static const double G = 180.0;
static const double FIXED_DT = 1.0 / 120.0;
static const double MAX_FRAME_TIME = 0.25;
static const double SOFTENING = 12.0;
static const double SPAWN_VELOCITY_SCALE = 0.60;
static const double SPAWN_MASS_MIN = 6.0;
static const double SPAWN_MASS_MAX = 160.0;
static const double SPAWN_MASS_STEP = 4.0;

typedef struct {
    double x;
    double y;
} Vec2;

typedef struct {
    Vec2 position;
    Vec2 velocity;
    double mass;
    double radius;
    SDL_Color color;

    // Ring buffer for trail
    // Keep newest pos and wrap around when buffer fills
    Vec2 trail[TRAIL_LENGTH];
    int trail_next;
    int trail_count;
} Body;

typedef struct {
    Body bodies [MAX_BODIES];
    int body_count;
} Simulation;

typedef struct {
    bool active;
    Vec2 start;
    Vec2 current;
    double mass;
    int color_index;
} SpawnState;

static const SDL_Color SPAWN_PALETTE[] = {
    {120, 200, 255, 255},
    {255, 120, 150, 255},
    {160, 255, 190, 255},
    {255, 210, 120, 255},
    {180, 160, 255, 255}
};

static Vec2 vec2(double x, double y) {
    Vec2 value = {x, y};
    return value;
}

static Vec2 vec_add(Vec2 a, Vec2 b) {
    return vec2(a.x + b.x, a.y + b.y);
}

static Vec2 vec_sub(Vec2 a, Vec2 b) {
    return vec2(a.x - b.x, a.y -b.y);
}

static Vec2 vec_scale(Vec2 v, double scale) {
    return vec2(v.x * scale, v.y * scale);
}

static double vec_length_sq(Vec2 v) {
    return (v.x * v.x) + (v.y * v.y);
}

static double clamp_double(double value, double min_value, double max_value) {
    if (value < min_value) {
        return min_value;
    }
    if (value > max_value) {
        return max_value;
    }
    return value; 
 }
 
 static double radius_from_mass(double mass) {
    // The idea here is that mass tracks volume, and volume scales with r^3 so radius scales with the cube root of mass
    return fmax(4.0, cbrt(mass) * 1.8);
 }

 static SDL_Color current_spawn_color(const SpawnState *spawn) {
    int color_count = (int)(sizeof(SPAWN_PALETTE) / sizeof(SPAWN_PALETTE[0]));
    return SPAWN_PALETTE[spawn->color_index % color_count];
 }

 static void adjust_spawn_mass(SpawnState *spawn, double delta) {
    spawn->mass = clamp_double(spawn->mass + delta, SPAWN_MASS_MIN, SPAWN_MASS_MAX);
 }

 static bool add_body(Simulation *sim, Body body) {
    if (sim->body_count >= MAX_BODIES) {
        return false;
    }
    sim->bodies[sim->body_count] = body;
    sim->body_count++;
    return true;
 }

static void push_trail_point(Body *body) {
    body->trail[body->trail_next] = body->position;
    body->trail_next = (body->trail_next +1) % TRAIL_LENGTH;

    if (body->trail_count <TRAIL_LENGTH) {
        body->trail_count++;
    }
}

static void reset_trail(Body *body) {
    body->trail_next = 0;
    body->trail_count = 0;
    push_trail_point(body);
}

static Body make_body(double x, double y, double vx, double vy, double mass, double radius, SDL_Color color) {
    Body body = {0};
    body.position = vec2(x, y);
    body.velocity = vec2(vx, vy);
    body.mass = mass;
    body.radius = radius;
    body.color = color;
    reset_trail(&body);
    return body;
}

// For a near circular orbit around a big central mass: v = sqrt(G * M / r)
static double circular_orbit_speed(double central_mass, double orbital_radius) {
    return sqrt((G * central_mass)/ orbital_radius);
}

static void load_starter_scene(Simulation *sim) {
    const SDL_Color star_color = {255, 214, 140, 255};
    const SDL_Color blue_color = {120, 200, 255, 255};
    const SDL_Color red_color = {255, 120, 150, 255};

    const double center_x = WINDOW_WIDTH * 0.5;
    const double center_y = WINDOW_HEIGHT * 0.5;
    const double star_mass = 8000.0;

    Body star = make_body(center_x, center_y, 0.0, 0.0, star_mass, 16.0, star_color);

    const double orbit_a_radius = 210.0;
    const double orbit_a_speed = circular_orbit_speed(star_mass, orbit_a_radius);

    // Velo is perpendicular to the radius 
    Body planet_a = make_body(
        center_x + orbit_a_radius,
        center_y,
        0.0,
        -orbit_a_speed,
        14.0,
        6.0,
        blue_color
    );

    const double orbit_b_radius = 320.0;
    const double orbit_b_speed = circular_orbit_speed(star_mass, orbit_b_radius);

    // a little under circular speed so the orbit is a bit more interesting
    Body planet_b = make_body(
        center_x - orbit_b_radius,
        center_y, 
        0.0,
        orbit_b_speed * 0.92,
        26.0,
        8.0,
        red_color
    );
    sim->body_count = 3;
    sim->bodies[0] = star;
    sim->bodies[1] = planet_a;
    sim->bodies[2] = planet_b;

    // Giving the star the opposite momentum so the system starts closer to a balanced center of mass instead of cheating it as fixed. 

    Vec2 total_planet_momentum = vec2(0.0, 0.0);
    for (int i = 1; i < sim->body_count; i++) {
        Vec2 momentum = vec_scale(sim->bodies[i].velocity, sim->bodies[i].mass);
        total_planet_momentum = vec_add(total_planet_momentum, momentum);
    }

    sim->bodies[0].velocity = vec_scale(total_planet_momentum, -1.0 / sim->bodies[0].mass);

    for (int i = 0; i < sim->body_count; i++) {
        reset_trail(&sim->bodies[i]);
    }
}

static void step_simulation(Simulation *sim, double dt) {
    Vec2 accelerations[MAX_BODIES] = {0};

    // Pairwise gravity: compute each interaction once then update both bodies symmetrically
    for (int i = 0; i < sim->body_count; i++) {
        for (int j = i + 1; j < sim->body_count; j++) {
            Vec2 delta = vec_sub(sim->bodies[j].position, sim->bodies[i].position);

            // Softening keeps the force from going absurdly high at tiny distances
            double distance_sq = vec_length_sq(delta) + (SOFTENING * SOFTENING);
            double distance = sqrt(distance_sq);
            double inv_distance_cubed = 1.0 / (distance_sq * distance);

            // Vector version of gravity: a = G * m_other * delta / |delta|^3
            Vec2 direction_term = vec_scale(delta, inv_distance_cubed);
            
            accelerations[i] = vec_add(
                accelerations[i],
                vec_scale(direction_term, G * sim->bodies[j].mass)
            );
            accelerations[j] = vec_sub(
                accelerations[j],
                vec_scale(direction_term, G * sim->bodies[i].mass)
            );
        }
    }

    for (int i = 0; i < sim->body_count; i++){
        Body *body = &sim->bodies[i];

        //Semi-implicit Euler: first update velo from acceleration, then move using new velo
        body->velocity = vec_add(body->velocity, vec_scale(accelerations[i], dt));
        body->position = vec_add(body->position, vec_scale(body->velocity, dt));

        push_trail_point(body);
    }
}

static void draw_filled_circle(SDL_Renderer *renderer, int cx, int cy, int radius, SDL_Color color) {
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);

    for (int y = -radius; y <= radius; y++) {
        int x_span = (int)sqrt((double)((radius * radius)-(y * y)));
        SDL_RenderDrawLine(renderer, cx - x_span, cy + y, cx + x_span, cy + y);
    }
}

static void draw_trail(SDL_Renderer *renderer, const Body *body) {
    if (body->trail_count < 2) {
        return; 
    }
    int oldest = (body->trail_next - body->trail_count + TRAIL_LENGTH) % TRAIL_LENGTH;

    for (int i = 1; i < body->trail_count; i++) {
        int index_a = (oldest + i - 1) % TRAIL_LENGTH;
        int index_b = (oldest + i) % TRAIL_LENGTH;

        double t = (double)i / (double)(body->trail_count - 1);
        Uint8 alpha = (Uint8)(20.0 + (160.0 * t));

        SDL_SetRenderDrawColor(renderer, body->color.r, body->color.g, body->color.b, alpha);
        SDL_RenderDrawLine(
            renderer,
            (int)lround(body->trail[index_a].x),
            (int)lround(body->trail[index_a].y),
            (int)lround(body->trail[index_b].x),
            (int)lround(body->trail[index_b].y)
        );
    }
}

static void draw_spawn_preview(SDL_Renderer *renderer, const SpawnState *spawn) {
    if(!spawn->active) {
        return;
    }

    SDL_Color color = current_spawn_color(spawn);
    SDL_Color ghost_color = color;
    SDL_Color cursor_color = {255, 255, 255, 180};

    ghost_color.a = 120;

    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, 220);
    SDL_RenderDrawLine(
        renderer,
        (int)lround(spawn->start.x),
        (int)lround(spawn->start.y),
        (int)lround(spawn->current.x),
        (int)lround(spawn->current.y)
    );

    draw_filled_circle(
        renderer,
        (int)lround(spawn->start.x),
        (int)lround(spawn->start.y),
        (int)lround(radius_from_mass(spawn->mass)),
        ghost_color
    );

    draw_filled_circle(
        renderer,
        (int)lround(spawn->current.x),
        (int)lround(spawn->current.y),
        3,
        cursor_color
    );
}

static void render_simulation(SDL_Renderer *renderer, const Simulation *sim, const SpawnState *spawn) {
    SDL_SetRenderDrawColor(renderer, 8, 12, 20, 255);
    SDL_RenderClear(renderer);

    for (int i = 0; i < sim->body_count; i++) {
        draw_trail(renderer, &sim->bodies[i]);
    }

    for (int i=0; i < sim->body_count; i++) {
        const Body *body = &sim->bodies[i];
        draw_filled_circle(
            renderer,
            (int)lround(body->position.x),
            (int)lround(body->position.y),
            (int)lround(body->radius),
            body->color
        );
    }

    draw_spawn_preview(renderer, spawn);
    SDL_RenderPresent(renderer);
}

static void update_window_title(SDL_Window *window, const Simulation *sim, const SpawnState *spawn, bool paused) {
    char title[256];

    snprintf(
        title,
        sizeof(title),
        "Gravity Sim | %s | bodies %d/%d | spawn mass %0.0f | LMB drag spawn | wheel or [ ] mass",
        paused ? "paused" : "running",
        sim->body_count,
        MAX_BODIES,
        spawn->mass
    );
    SDL_SetWindowTitle(window, title);
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

    Simulation initial_state = {0};
    Simulation sim = {0};
    SpawnState spawn = {0};

    load_starter_scene(&initial_state);
    sim = initial_state;

    spawn.mass = 20.0;
    spawn.color_index = 0;

    bool running = true;
    bool paused = false;
    double accumulator = 0.0;

    Uint64 previous_counter = SDL_GetPerformanceCounter();
    Uint64 performance_frequency = SDL_GetPerformanceFrequency();

    puts("Controls:");
    puts("  Left click + drag - spawn a new body and set its launch velocity");
    puts("  Right click       - cancel the current spawn drag");
    puts("  Mouse wheel / [ ] - adjust spawn mass");
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
                        sim = initial_state;
                        accumulator = 0.0;
                        spawn.active = false;
                        spawn.color_index = 0;
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
                    int color_count = (int)(sizeof(SPAWN_PALETTE)/ sizeof(SPAWN_PALETTE[0]));
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
        update_window_title(window, &sim, &spawn, paused);
        render_simulation(renderer, &sim, &spawn);
    }
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
