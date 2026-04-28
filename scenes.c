#include "scenes.h"

#include "simulation.h"

#include <math.h>

static void clear_simulation(Simulation *sim) {
    sim->body_count = 0;
}

static void set_zero_total_momentum(Simulation *sim) {
    Vec2 total_momentum = vec2(0.0, 0.0);
    double total_mass = 0.0;

    for (int i = 0; i < sim->body_count; i++) {
        Vec2 momentum = vec_scale(sim->bodies[i].velocity, sim->bodies[i].mass);
        total_momentum = vec_add(total_momentum, momentum);
        total_mass += sim->bodies[i].mass;
    }

    if (total_mass <= 0.0) {
        return;
    }

    Vec2 center_of_mass_velocity = vec_scale(total_momentum, 1.0 / total_mass);
    for (int i = 0; i< sim->body_count; i++) {
        sim->bodies[i].velocity = vec_sub(sim->bodies[i].velocity, center_of_mass_velocity);
    }
}

static void finalise_scene(Simulation *sim) {
    // Keeps whole system from drifting for no physical reason
    set_zero_total_momentum(sim);

    for (int i = 0; i < sim->body_count; i++) {
        reset_trail(&sim->bodies[i]);
    }
}

// For a near circular orbit around a big central mass: v = sqrt(G * M / r)
static double circular_orbit_speed(double central_mass, double orbital_radius) {
    return sqrt((G * central_mass)/ orbital_radius);
}

static void load_empty_scene(Simulation *sim) {
    clear_simulation(sim);
}

static void load_starter_scene(Simulation *sim) {
    const SDL_Color star_color = {255, 214, 140, 255};
    const SDL_Color blue_color = {120, 200, 255, 255};
    const SDL_Color red_color = {255, 120, 150, 255};

    const double mars_orbit_radius = 1.523679 * AU;
    const double earth_orbit_radius = AU;
    const double star_mass = SOLAR_MASS;

    Body star = make_body(
        0.0,
        0.0,
        0.0,
        0.0,
        BODY_TYPE_STAR,
        star_mass,
        SOLAR_RADIUS,
        star_color
    );

    const double orbit_a_speed = circular_orbit_speed(star_mass, earth_orbit_radius);

    // Earth-style orbit around the Sun.
    Body planet_a = make_body(
        earth_orbit_radius,
        0.0,
        0.0,
        orbit_a_speed,
        BODY_TYPE_ROCKY,
        EARTH_MASS,
        EARTH_RADIUS,
        blue_color
    );

    const double orbit_b_speed = circular_orbit_speed(star_mass, mars_orbit_radius);

    // Mars-style outer orbit in the opposite phase.
    Body planet_b = make_body(
        -mars_orbit_radius,
        0.0,
        0.0,
        -orbit_b_speed,
        BODY_TYPE_ROCKY,
        6.4171e23,
        3.3895e6,
        red_color
    );
    sim->body_count = 3;
    sim->bodies[0] = star;
    sim->bodies[1] = planet_a;
    sim->bodies[2] = planet_b;

    finalise_scene(sim);
}

static void load_chaotic_three_body_scene(Simulation *sim) {
    const SDL_Color color_a = {120, 200, 255, 255};
    const SDL_Color color_b = {255, 120, 150, 255};
    const SDL_Color color_c = {160, 255, 190, 255};

    const double mass = 0.65 * SOLAR_MASS;
    const double radius = 0.65 * SOLAR_RADIUS;

    clear_simulation(sim);

    sim->body_count = 3;
    sim->bodies[0] = make_body(
        -0.70 * AU,
        -0.15 * AU,
        12000.0,
        -14000.0,
        BODY_TYPE_STAR,
        mass,
        radius,
        color_a
    );
    sim->bodies[1] = make_body(
        0.70 * AU,
        -0.15 * AU,
        -11000.0,
        16000.0,
        BODY_TYPE_STAR,
        mass,
        radius,
        color_b
    );
    sim->bodies[2] = make_body(
        0.08 * AU,
        0.62 * AU,
        -5000.0,
        -3000.0,
        BODY_TYPE_STAR,
        mass,
        radius,
        color_c
    );

    finalise_scene(sim);
}

static void load_binary_stars_scene(Simulation *sim) {
    const SDL_Color star_a_color = {255, 214, 140, 255};
    const SDL_Color star_b_color = {255, 170, 120, 255};
    const SDL_Color planet_color = {120, 200, 255, 255};

    const double star_mass = 0.80 * SOLAR_MASS;
    const double star_radius = 0.80 * SOLAR_RADIUS;
    const double star_seperation = 1.20 * AU;

    // Equal mass binary: each star feels gravity from the other star and orbits barycenter, solving F = mv^2 / r -> v = sqrt(G*M/(2*d)) where M is the mass of the other star and D is the full seperation

    const double star_speed = sqrt((G * star_mass)/ (2.0 * star_seperation));

    const double planet_radius_from_center = 2.20 * AU;
    const double planet_speed = circular_orbit_speed(star_mass * 2.0, planet_radius_from_center) * 0.97;

    clear_simulation(sim);

    sim->body_count = 3;
    sim->bodies[0] = make_body(
        -(star_seperation * 0.5),
        0.0,
        0.0,
        -star_speed,
        BODY_TYPE_STAR,
        star_mass,
        star_radius,
        star_a_color
    );
    sim->bodies[1] = make_body(
        star_seperation *0.5,
        0.0,
        0.0,
        star_speed,
        BODY_TYPE_STAR,
        star_mass,
        star_radius,
        star_b_color
    );
    sim->bodies[2] = make_body(
        0.0,
        planet_radius_from_center,
        planet_speed,
        0.0,
        BODY_TYPE_GAS_GIANT,
        0.8 * JUPITER_MASS,
        0.9 * JUPITER_RADIUS,
        planet_color
    );
    finalise_scene(sim);
}

const char *scene_name(ScenePreset preset) {
    switch (preset) {
        case SCENE_EMPTY:
            return "empty";
        case SCENE_STARTER:
            return("starter");
        case SCENE_CHAOTIC_3_BODY:
            return"chaotic 3-body";
        case SCENE_BINARY_STARS:
            return "binary stars";
        default:
            return "unkown";
    }
}

static void load_scene(Simulation *sim, ScenePreset preset) {
    switch (preset) {
        case SCENE_EMPTY:
            load_empty_scene(sim);
            break;
        case SCENE_STARTER:
            load_starter_scene(sim);
            break;
        case SCENE_CHAOTIC_3_BODY:
            load_chaotic_three_body_scene(sim);
            break;
        case SCENE_BINARY_STARS:
            load_binary_stars_scene(sim);
            break;
        default:
            load_starter_scene(sim);
            break;
    }
}

void activate_scene(ScenePreset preset, Simulation *initial_state, Simulation *sim,
                    SpawnState *spawn, double *accumulator, double *simulated_time_seconds) {
    load_scene(initial_state, preset);
    *sim = *initial_state;
    *accumulator = 0.0;
    *simulated_time_seconds = 0.0;
    spawn->active = false;
}
