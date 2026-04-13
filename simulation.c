#include "simulation.h"

#include <math.h>

// Sim units, not real astronomical constants
// Since this is the first iteration, it's more of a feature since it keeps this version readable for me to extend later.

const double G = 180.0;
const double FIXED_DT = 1.0 / 120.0;
const double MAX_FRAME_TIME = 0.25;
const double SOFTENING = 12.0;
const double SPAWN_VELOCITY_SCALE = 0.60;
const double SPAWN_MASS_MIN = 6.0;
const double SPAWN_MASS_MAX = 160.0;
const double SPAWN_MASS_STEP = 4.0;

static const SDL_Color SPAWN_PALETTE[] = {
    {120, 200, 255, 255},
    {255, 120, 150, 255},
    {160, 255, 190, 255},
    {255, 210, 120, 255},
    {180, 160, 255, 255}
};

Vec2 vec2(double x, double y) {
    Vec2 value = {x, y};
    return value;
}

Vec2 vec_add(Vec2 a, Vec2 b) {
    return vec2(a.x + b.x, a.y + b.y);
}

Vec2 vec_sub(Vec2 a, Vec2 b) {
    return vec2(a.x - b.x, a.y -b.y);
}

Vec2 vec_scale(Vec2 v, double scale) {
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

double radius_from_mass(double mass) {
    // The idea here is that mass tracks volume, and volume scales with r^3 so radius scales with the cube root of mass
    return fmax(4.0, cbrt(mass) * 1.8);
}

SDL_Color current_spawn_color(const SpawnState *spawn) {
    int color_count = (int)(sizeof(SPAWN_PALETTE) / sizeof(SPAWN_PALETTE[0]));
    return SPAWN_PALETTE[spawn->color_index % color_count];
}

int spawn_color_count(void) {
    return (int)(sizeof(SPAWN_PALETTE) / sizeof(SPAWN_PALETTE[0]));
}

void adjust_spawn_mass(SpawnState *spawn, double delta) {
    spawn->mass = clamp_double(spawn->mass + delta, SPAWN_MASS_MIN, SPAWN_MASS_MAX);
}

bool add_body(Simulation *sim, Body body) {
    if (sim->body_count >= MAX_BODIES) {
        return false;
    }
    sim->bodies[sim->body_count] = body;
    sim->body_count++;
    return true;
}

void push_trail_point(Body *body) {
    body->trail[body->trail_next] = body->position;
    body->trail_next = (body->trail_next +1) % TRAIL_LENGTH;

    if (body->trail_count <TRAIL_LENGTH) {
        body->trail_count++;
    }
}

void reset_trail(Body *body) {
    body->trail_next = 0;
    body->trail_count = 0;
    push_trail_point(body);
}

Body make_body(double x, double y, double vx, double vy, double mass, double radius, SDL_Color color) {
    Body body = {0};
    body.position = vec2(x, y);
    body.velocity = vec2(vx, vy);
    body.mass = mass;
    body.radius = radius;
    body.color = color;
    reset_trail(&body);
    return body;
}

void step_simulation(Simulation *sim, double dt) {
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
