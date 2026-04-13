#ifndef RENDER_H
#define RENDER_H

#include "gravity.h"

void render_simulation(SDL_Renderer *renderer, const Simulation *sim, const SpawnState *spawn);
void update_window_title(SDL_Window *window, const Simulation *sim, const SpawnState *spawn,
                         ScenePreset scene, bool paused);

#endif
