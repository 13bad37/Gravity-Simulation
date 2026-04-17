#include "simulation.h"

#include <math.h>

// The simulation now uses SI units internally:
// meters, kilograms, and seconds.
// Rendering is handled separately so the physics code can stay unit-clean.

const double G = 6.67430e-11;
const double FIXED_DT = 1800.0;
const double MAX_FRAME_TIME = 0.25;
const double SOFTENING = 1.0e7;
const double VIEW_METERS_PER_PIXEL = 5.0e8;
const double SPAWN_SPEED_PER_PIXEL = 300.0;
const double EARTH_MASS = 5.9722e24;
const double EARTH_RADIUS = 6.371e6;
const double EARTH_DENSITY = 5514.0;
const double SOLAR_MASS = 1.98847e30;
const double SOLAR_RADIUS = 6.9634e8;
const double AU = 1.495978707e11;
const double SPAWN_MASS_MIN = 2.9861e23;
const double SPAWN_MASS_MAX = 1.19444e26;
const double SPAWN_MASS_STEP = 1.49305e24;

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
    const double pi = 3.14159265358979323846;

    if (mass <= 0.0) {
        return 0.0;
    }

    // If we assume a default rocky density for spawned bodies:
    // m = rho * (4/3) * pi * r^3
    // so
    // r = cbrt((3m) / (4pi rho))
    return cbrt((3.0 * mass) / (4.0 * pi * EARTH_DENSITY));
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

SimulationDiagnostics compute_diagnostics(const Simulation *sim) {
    SimulationDiagnostics diagnostics = {0};

    for (int i = 0; i < sim->body_count; i++) {
        const Body *body = &sim->bodies[i];

        diagnostics.kinetic_energy += 0.5 * body->mass * vec_length_sq(body->velocity);
        diagnostics.total_momentum = vec_add(
            diagnostics.total_momentum,
            vec_scale(body->velocity, body->mass)
        );

        // In a 2D simulation, the angular momentum vector only has a z component.
        diagnostics.angular_momentum_z +=
            body->mass * ((body->position.x * body->velocity.y) -
                          (body->position.y * body->velocity.x));
    }

    for (int i = 0; i < sim->body_count; i++) {
        for (int j = i + 1; j < sim->body_count; j++) {
            Vec2 delta = vec_sub(sim->bodies[j].position, sim->bodies[i].position);
            double softened_distance = sqrt(vec_length_sq(delta) + (SOFTENING * SOFTENING));

            // Use the softened potential that matches the softened force law.
            diagnostics.potential_energy +=
                (-G * sim->bodies[i].mass * sim->bodies[j].mass) / softened_distance;
        }
    }

    diagnostics.total_energy = diagnostics.kinetic_energy + diagnostics.potential_energy;
    return diagnostics;
}

static void compute_accelerations(const Simulation *sim, Vec2 accelerations[MAX_BODIES]) {
    for (int i = 0; i < MAX_BODIES; i++) {
        accelerations[i] = vec2(0.0, 0.0);
    }

    // Compute each pair once and apply equal-and-opposite accelerations.
    for (int i = 0; i < sim->body_count; i++) {
        for (int j = i + 1; j < sim->body_count; j++) {
            Vec2 delta = vec_sub(sim->bodies[j].position, sim->bodies[i].position);

            // Softening keeps the force from going really high at tiny distances.
            double distance_sq = vec_length_sq(delta) + (SOFTENING * SOFTENING);
            double distance = sqrt(distance_sq);
            double inv_distance_cubed = 1.0 / (distance_sq * distance);

            // a = G * m_other * delta / |delta|^3
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
}
void step_simulation(Simulation *sim, double dt) {
    Vec2 accelerations_before[MAX_BODIES];
    Vec2 accelerations_after[MAX_BODIES];

    compute_accelerations(sim, accelerations_before);

    for (int i = 0; i < sim->body_count; i++) {
        Body *body = &sim->bodies[i];

        //Velo Verlet position step: x(t+dt) = x(t) + v(t)dt + 1/2 a(t)dt^2
        Vec2 position_step = vec_add(
            vec_scale(body->velocity, dt),
            vec_scale(accelerations_before[i], 0.5 * dt * dt)
        );

        body->position = vec_add(body->position, position_step);
    }
    compute_accelerations(sim, accelerations_after);
    for (int i = 0; i < sim->body_count; i++) {
        Body *body = &sim->bodies[i];

        //Velo verlet velocity step: v(t+dt) = v(t) + 1/2 (a_old + a_new) dt
        Vec2 average_accelration = vec_scale(
            vec_add(accelerations_before[i], accelerations_after[i]),
            0.5
        );
         body->velocity = vec_add(body->velocity, vec_scale(average_accelration, dt));
         push_trail_point(body);
    }
}
