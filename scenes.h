#ifndef SCENES_H
#define SCENES_H

#include "gravity.h"

const char *scene_name(ScenePreset preset);
void activate_scene(ScenePreset preset, Simulation *initial_state, Simulation *sim,
                    SpawnState *spawn, double *accumulator, double *simulated_time_seconds);

#endif
