#ifndef GRAVITY_H
#define GRAVITY_H

#include <SDL2/SDL.h>
#include <stdbool.h>

#define WINDOW_WIDTH 1280
#define WINDOW_HEIGHT 720
#define MAX_BODIES 16
#define TRAIL_LENGTH 2000

extern const double G;
extern const double FIXED_DT;
extern const double MAX_FRAME_TIME;
extern const double SOFTENING;
extern const double VIEW_METERS_PER_PIXEL;
extern const double SPAWN_SPEED_PER_PIXEL;
extern const double SPAWN_MASS_MIN;
extern const double SPAWN_MASS_MAX;
extern const double SPAWN_MASS_STEP;
extern const double EARTH_MASS;
extern const double EARTH_RADIUS;
extern const double EARTH_DENSITY;
extern const double SOLAR_MASS;
extern const double SOLAR_RADIUS;
extern const double AU;

typedef struct {
    double x;
    double y;
} Vec2;

typedef struct {
    Vec2 position;
    Vec2 velocity;
    double mass;
    double radius;
    double density;
    double spin_angular_momentum;
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
    double kinetic_energy;
    double potential_energy;
    double total_energy;
    Vec2 total_momentum;
    double angular_momentum_z;
} SimulationDiagnostics;

typedef struct 
{
    SimulationDiagnostics diagnostics;
    double energy_scale;
    double momentum_scale;
    double angular_momentum_scale;
    bool valid;
} DiagnosticsBaseline;

typedef struct 
{
    double energy_relative;
    double momentum_relative;
    double angular_momentum_relative;
} SimulationDrift; 


typedef enum {
    SCENE_EMPTY = 0,
    SCENE_STARTER = 1,
    SCENE_CHAOTIC_3_BODY = 2,
    SCENE_BINARY_STARS = 3
} ScenePreset;

typedef enum {
    INTEGRATOR_SEMI_IMPLICIT_EULER = 0,
    INTEGRATOR_VELOCITY_VERLET = 1,
    INTEGRATOR_RK4 = 2,
    INTEGRATOR_COUNT = 3
} IntegratorMode;

typedef struct {
    bool active;
    Vec2 start;
    Vec2 current;
    double mass;
    int color_index;
} SpawnState;

typedef struct {
    Vec2 center;
    double meters_per_pixel;
} Camera;

#endif
