#ifndef GRAVITY_H
#define GRAVITY_H

#include <SDL2/SDL.h>
#include <stdbool.h>

#define WINDOW_WIDTH 1280
#define WINDOW_HEIGHT 720
#define MAX_BODIES 16
#define TRAIL_LENGTH 240

extern const double G;
extern const double FIXED_DT;
extern const double MAX_FRAME_TIME;
extern const double SOFTENING;
extern const double SPAWN_VELOCITY_SCALE;
extern const double SPAWN_MASS_MIN;
extern const double SPAWN_MASS_MAX;
extern const double SPAWN_MASS_STEP;

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

typedef enum {
    SCENE_EMPTY = 0,
    SCENE_STARTER = 1,
    SCENE_CHAOTIC_3_BODY = 2,
    SCENE_BINARY_STARS = 3
} ScenePreset;

typedef struct {
    bool active;
    Vec2 start;
    Vec2 current;
    double mass;
    int color_index;
} SpawnState;

#endif
