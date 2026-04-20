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
    return vec2(a.x - b.x, a.y - b.y);
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

double radius_from_mass_density(double mass, double density) {
    const double pi = 3.14159265358979323846;

    if (mass <= 0.0 || density <= 0.0) {
        return 0.0;
    }

    // m = rho * (4/3) * pi * r^3, so r = cbrt((3m) / (4pi rho)).
    return cbrt((3.0 * mass) / (4.0 * pi * density));
}

double density_from_mass_radius(double mass, double radius) {
    const double pi = 3.14159265358979323846;
    double volume;

    if (mass <= 0.0 || radius <= 0.0) {
        return 0.0;
    }

    volume = (4.0 / 3.0) * pi * radius * radius * radius;
    return mass / volume;
}

double radius_from_mass(double mass) {
    return radius_from_mass_density(mass, EARTH_DENSITY);
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
    body->trail_next = (body->trail_next + 1) % TRAIL_LENGTH;

    if (body->trail_count < TRAIL_LENGTH) {
        body->trail_count++;
    }
}

void reset_trail(Body *body) {
    body->trail_next = 0;
    body->trail_count = 0;
    push_trail_point(body);
}

Body make_body(double x, double y, double vx, double vy, double mass, double radius,
               double density, SDL_Color color) {
    Body body = {0};
    body.position = vec2(x, y);
    body.velocity = vec2(vx, vy);
    body.mass = mass;
    body.radius = radius;
    body.density = density;
    body.spin_angular_momentum = 0.0;
    body.color = color;
    reset_trail(&body);
    return body;
}

static double orbital_angular_momentum_z(const Body *body) {
    return body->mass * ((body->position.x * body->velocity.y) -
                         (body->position.y * body->velocity.x));
}

static double moment_of_inertia(const Body *body) {
    // Approximate each body as a solid sphere for spin bookkeeping.
    return 0.4 * body->mass * body->radius * body->radius;
}

static SDL_Color merge_colors(const Body *a, const Body *b, double merged_mass) {
    SDL_Color color = {0};
    double weight_a;
    double weight_b;

    if (merged_mass <= 0.0) {
        return a->color;
    }

    weight_a = a->mass / merged_mass;
    weight_b = b->mass / merged_mass;

    color.r = (Uint8)lround((a->color.r * weight_a) + (b->color.r * weight_b));
    color.g = (Uint8)lround((a->color.g * weight_a) + (b->color.g * weight_b));
    color.b = (Uint8)lround((a->color.b * weight_a) + (b->color.b * weight_b));
    color.a = 255;
    return color;
}

static void merge_body_pair(Simulation *sim, int index_a, int index_b) {
    Body *a = &sim->bodies[index_a];
    Body *b = &sim->bodies[index_b];
    Body merged = {0};
    double merged_mass = a->mass + b->mass;
    Vec2 merged_momentum;
    Vec2 merged_position;
    double volume_a;
    double volume_b;
    double merged_density;
    double total_angular_momentum;
    double merged_orbital_angular_momentum;
    int last_index = sim->body_count - 1;

    if (merged_mass <= 0.0) {
        return;
    }

    merged_momentum = vec_add(
        vec_scale(a->velocity, a->mass),
        vec_scale(b->velocity, b->mass)
    );

    merged_position = vec_scale(
        vec_add(
            vec_scale(a->position, a->mass),
            vec_scale(b->position, b->mass)
        ),
        1.0 / merged_mass
    );

    volume_a = (a->density > 0.0) ? (a->mass / a->density) : 0.0;
    volume_b = (b->density > 0.0) ? (b->mass / b->density) : 0.0;

    if (volume_a + volume_b > 0.0) {
        merged_density = merged_mass / (volume_a + volume_b);
    } else {
        merged_density = EARTH_DENSITY;
    }

    merged.mass = merged_mass;
    merged.position = merged_position;
    merged.velocity = vec_scale(merged_momentum, 1.0 / merged_mass);
    merged.density = merged_density;
    merged.radius = radius_from_mass_density(merged.mass, merged.density);
    merged.color = merge_colors(a, b, merged_mass);

    total_angular_momentum =
        orbital_angular_momentum_z(a) + a->spin_angular_momentum +
        orbital_angular_momentum_z(b) + b->spin_angular_momentum;

    merged_orbital_angular_momentum = orbital_angular_momentum_z(&merged);
    merged.spin_angular_momentum = total_angular_momentum - merged_orbital_angular_momentum;

    // Old trails no longer mean anything after a merge, so restart the trail.
    merged.trail_next = 0;
    merged.trail_count = 0;

    sim->bodies[index_a] = merged;

    if (index_b != last_index) {
        sim->bodies[index_b] = sim->bodies[last_index];
    }

    sim->body_count--;
}

static void resolve_collisions(Simulation *sim) {
    bool merged_any = true;

    while (merged_any) {
        merged_any = false;

        for (int i = 0; i < sim->body_count; i++) {
            for (int j = i + 1; j < sim->body_count; j++) {
                Vec2 delta = vec_sub(sim->bodies[j].position, sim->bodies[i].position);
                double collision_distance = sim->bodies[i].radius + sim->bodies[j].radius;

                if (vec_length_sq(delta) <= collision_distance * collision_distance) {
                    merge_body_pair(sim, i, j);
                    merged_any = true;
                    goto collision_pass_complete;
                }
            }
        }

collision_pass_complete:
        ;
    }
}

SimulationDiagnostics compute_diagnostics(const Simulation *sim) {
    SimulationDiagnostics diagnostics = {0};

    for (int i = 0; i < sim->body_count; i++) {
        const Body *body = &sim->bodies[i];
        double spin_inertia = moment_of_inertia(body);

        diagnostics.kinetic_energy += 0.5 * body->mass * vec_length_sq(body->velocity);

        if (spin_inertia > 0.0) {
            diagnostics.kinetic_energy +=
                (body->spin_angular_momentum * body->spin_angular_momentum) / (2.0 * spin_inertia);
        }

        diagnostics.total_momentum = vec_add(
            diagnostics.total_momentum,
            vec_scale(body->velocity, body->mass)
        );

        diagnostics.angular_momentum_z +=
            orbital_angular_momentum_z(body) + body->spin_angular_momentum;
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

        // Velocity Verlet position step:
        // x(t + dt) = x(t) + v(t)dt + 1/2 a(t)dt^2
        Vec2 position_step = vec_add(
            vec_scale(body->velocity, dt),
            vec_scale(accelerations_before[i], 0.5 * dt * dt)
        );

        body->position = vec_add(body->position, position_step);
    }

    compute_accelerations(sim, accelerations_after);

    for (int i = 0; i < sim->body_count; i++) {
        Body *body = &sim->bodies[i];

        // Velocity Verlet velocity step:
        // v(t + dt) = v(t) + 1/2 (a_old + a_new) dt
        Vec2 average_acceleration = vec_scale(
            vec_add(accelerations_before[i], accelerations_after[i]),
            0.5
        );

        body->velocity = vec_add(body->velocity, vec_scale(average_acceleration, dt));
    }

    // Merge overlapping bodies after the step which perfectly models inelastic collision where mass and linear momentum are conserved while mechanical energy can be lost
    resolve_collisions(sim);

    for (int i = 0; i < sim->body_count; i++) {
        push_trail_point(&sim->bodies[i]);
    }
}
