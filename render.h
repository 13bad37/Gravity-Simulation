#ifndef RENDER_H
#define RENDER_H

#include "gravity.h"

bool init_render_resources(void);
void shutdown_render_resources(void);

void render_simulation(SDL_Renderer *renderer, const Simulation *sim, const SpawnState *spawn,
                       const SimulationDiagnostics *diagnostics, double simulated_time_seconds,
                       const Camera *camera, ScenePreset scene, bool paused, bool hud_visible,
                       double time_scale, int time_scale_index, int time_scale_count);
void update_window_title(SDL_Window *window, const Simulation *sim, const SpawnState *spawn,
                         const Camera *camera, ScenePreset scene, bool paused, double time_scale);
Vec2 screen_to_world(int screen_x, int screen_y, const Camera *camera);

#endif
